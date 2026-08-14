#include "InverterSdo.h"

#if INVERTER_SDO_POLL

#include <string.h>

#include "us_ticker_api.h"

#include "pinout.h"      // canBus
#include "Telemetry.h"

namespace {

/*
 * CanSdo's command bytes, composed from the CANopen bitfields in libopeninv/src/cansdo.cpp:
 * SDO_READ is (2 << 5), and the reply adds EXPEDITED (1 << 1) and SIZE_SPECIFIED (1).
 *
 * There is deliberately no SDO_WRITE constant here. A write can change inverter parameters --
 * potmode among them -- and can be persisted to flash; leaving the constant out means the
 * write path cannot be reached from this file by a one-character edit.
 */
const uint8_t SDO_READ       = 0x40;
const uint8_t SDO_ABORT      = 0x80;

// Address a parameter by unique id, not by array position. Positions shift between firmware
// builds; ids do not. This is the same trap that made `can tx dir` fail when 5.35 renamed the
// parameter to `seldir`.
const uint16_t SDO_INDEX_PARAM_UID = 0x2100;

/*
 * Paced off the microsecond ticker, NOT off a count of main-loop iterations.
 *
 * Counting iterations is the obvious implementation and it is wrong here. The main loop's
 * pacing line is
 *
 *     ThisThread::sleep_for(MAIN_PERIOD - (t.read_ms() % MAIN_PERIOD));
 *
 * which is intended to hold a 50 ms cadence, and does not: measured on the car over an 89.5 s
 * window against the firmware's own clock, the loop iterates every 36.4 ms. A tick-counted
 * interval therefore runs ~37% fast -- the first build of this file asked for 1000 ms and got
 * 728, which would have made the active tier 13.7 Hz rather than the 10 Hz the bus budget was
 * written against. The CSV's 500 ms cadence is exact in the same capture, so this is specific
 * to the main loop, not to the clock.
 *
 * us_ticker_read() is a free-running 32-bit 1 MHz counter, the same source CanRx.h timestamps
 * frames with. It wraps every ~71.6 minutes, so every comparison below is an unsigned
 * subtraction against an interval rather than a "<" against an absolute time.
 */
const uint32_t US_ACTIVE = INVERTER_SDO_PERIOD_ACTIVE_MS * 1000u;
const uint32_t US_IDLE   = INVERTER_SDO_PERIOD_IDLE_MS * 1000u;
const uint32_t US_STALE  = INVERTER_STATE_STALE_MS * 1000u;

/*
 * What to poll, and why each id is here.
 *
 *   pot          2015   raw throttle channel 1. No periodic route at all.
 *   pot2         2016   raw throttle channel 2. Same.
 *   regenpreset  2051   stripped from the map; the VCU sends it but never sees it applied.
 *
 * canio (2022) is deliberately absent: the same six inputs are now periodic as the din_* bits in
 * 0x002, so polling it would spend bus on information already in the log. cruisespeed (2041) is
 * absent because din_cruise reads 0 on this car, so cruise is inert.
 *
 * NOTE THE ID OF regenpreset. The write-up that specified this poll gave 2018, which in this
 * build is `seldir` -- a direction enum. Reading it would have succeeded, returned a plausible
 * small number, and logged it in a field labelled as a regen percentage, which is the exact
 * failure the id-versus-position choice above exists to avoid. These three are transcribed from
 * VALUE_ENTRY in stm32-sine/include/param_prj.h on the car-914 branch, not from prose.
 */
struct PollTarget {
    uint16_t id;
    bool     disabled;   // latched by an abort; see inverter_sdo_on_frame()
};

PollTarget targets[] = {
    { 2015, false },     // pot
    { 2016, false },     // pot2
    { 2051, false },     // regenpreset
};
const uint8_t NUM_TARGETS = sizeof(targets) / sizeof(targets[0]);

uint8_t  nextTarget        = 0;

// One request outstanding at a time. Bursting all three would mean a single lost reply
// desynchronises the pairing for the rest of the round.
bool     outstanding       = false;
uint8_t  outstandingTarget = 0;
uint16_t outstandingIndex  = 0;
uint8_t  outstandingSub    = 0;

// Last opmode seen in 0x002. 0 is Off, which is also what an inverter that has never spoken
// looks like -- the conservative default, since Off selects the slow tier.
uint8_t  opmode = 0;

// Timestamps, in us_ticker_read() units. Both start at zero, which is a valid time; the first
// tick therefore sees a large elapsed value and polls immediately, and opmode is already Off, so
// a boot with no inverter present starts on the slow tier rather than the fast one.
uint32_t lastPollUs  = 0;
uint32_t lastStateUs = 0;

inline uint16_t uid_index(uint16_t paramId) { return SDO_INDEX_PARAM_UID | (paramId >> 8); }
inline uint8_t  uid_sub(uint16_t paramId)   { return (uint8_t)(paramId & 0xFF); }

void send_read(uint8_t target)
{
    const uint16_t paramId = targets[target].id;
    const uint16_t index   = uid_index(paramId);
    const uint8_t  sub     = uid_sub(paramId);

    // cmd | index lo | index hi | subIndex | four data bytes, zero on a read.
    uint8_t p[8];
    memset(p, 0, sizeof(p));
    p[0] = SDO_READ;
    p[1] = (uint8_t)(index & 0xFF);
    p[2] = (uint8_t)(index >> 8);
    p[3] = sub;

    if (!canBus->write(CANMessage(INV_SDO_REQ_ID, p, 8))) {
        // The peripheral refused it, so no reply is coming and no request is outstanding. Not
        // counted separately: nothing reached the log either, so "request refused" and "request
        // sent but unanswered" are the same observable -- a gap in the rotation.
        return;
    }

    // Echo it into the log. A CAN node does not receive its own frames, so without this the
    // record would show the inverter's replies with nothing that provoked them. Emitted at
    // transmit request, so it is evidence of intent rather than proof of arbitration success.
    telemetry_echo_frame(INV_SDO_REQ_ID, p, 8);

    outstanding       = true;
    outstandingTarget = target;
    outstandingIndex  = index;
    outstandingSub    = sub;
}

} // namespace

void inverter_sdo_on_frame(const CANMessage &msg)
{
    if (msg.format != CANStandard) {
        return;
    }

    // Byte 2 of the inverter state frame is opmode. This is the only thing the poll rate depends
    // on, and it is read rather than inferred: if the inverter's map is ever changed so 0x002
    // stops arriving, opmode simply goes stale and the rate falls back to the idle tier.
    if (msg.id == INV_STATE_ID) {
        if (msg.len >= 3) {
            opmode      = msg.data[2];
            lastStateUs = us_ticker_read();
        }
        return;
    }

    if (msg.id != INV_SDO_RESP_ID || msg.len < 8 || !outstanding) {
        return;
    }

    // Pair on the echoed index/subIndex rather than on arrival order. A late reply, or one
    // provoked by something else talking SDO to node 1 on the same bus, then cannot be mistaken
    // for the answer to this request.
    const uint16_t index = (uint16_t)msg.data[1] | ((uint16_t)msg.data[2] << 8);
    if (index != outstandingIndex || msg.data[3] != outstandingSub) {
        return;
    }

    if (msg.data[0] == SDO_ABORT) {
        // The id is wrong for this firmware build (0x06020000) or out of range (0x06090030).
        // Retrying cannot succeed and would only spend bus, so retire the id from the rotation
        // and report it once. Reported rather than silent because a retired id means the poll is
        // quietly doing less than it claims to.
        const uint32_t code = (uint32_t)msg.data[4]
                            | ((uint32_t)msg.data[5] << 8)
                            | ((uint32_t)msg.data[6] << 16)
                            | ((uint32_t)msg.data[7] << 24);
        targets[outstandingTarget].disabled = true;
        telemetry_emit_diag(BMS_DIAG_SDO_ABORT,
                            targets[outstandingTarget].id,
                            (uint16_t)(code & 0xFFFF),
                            (uint16_t)(code >> 16));
    }

    // Either way the transaction is closed. A successful value needs nothing done to it here:
    // the reply frame was already forwarded into the log by the main loop's proxy, and that
    // raw exchange is the whole record this poll exists to produce. Decoding -- including the
    // s32fp divide-by-32 described in the header -- belongs in the consumer.
    outstanding = false;
}

void inverter_sdo_tick()
{
    const uint32_t now = us_ticker_read();

    // Expire the opmode reading rather than letting it persist: an inverter that goes quiet
    // mid-drive would otherwise leave the poll latched at the fast rate forever. Unsigned
    // subtraction, so the ticker's ~71.6 minute wrap needs no special case.
    if ((now - lastStateUs) > US_STALE) {
        opmode = 0;
    }

    const uint32_t interval = (opmode != 0) ? US_ACTIVE : US_IDLE;
    if ((now - lastPollUs) < interval) {
        return;
    }
    // Stamped from `now` rather than accumulated, so a late call cannot bank credit and fire a
    // burst of catch-up requests onto the bus.
    lastPollUs = now;

    // A request still outstanding at the next poll instant never got its reply. Drop the sample;
    // do not retry inside the interval, which would double the request rate exactly when the
    // link is already struggling. The loss is visible in the log as a request with no reply
    // after it, which is why there is no separate counter for it.
    outstanding = false;

    // Round-robin, skipping ids an abort has retired. If every id is retired there is nothing
    // left to ask for and the poll goes quiet rather than spinning.
    for (uint8_t i = 0; i < NUM_TARGETS; i++) {
        const uint8_t candidate = (uint8_t)((nextTarget + i) % NUM_TARGETS);
        if (!targets[candidate].disabled) {
            nextTarget = (uint8_t)((candidate + 1) % NUM_TARGETS);
            send_read(candidate);
            return;
        }
    }
}

#endif // INVERTER_SDO_POLL

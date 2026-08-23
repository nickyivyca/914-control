#include "Telemetry.h"

#if SLCAN_MODE

#include <math.h>
#include <string.h>

#include "CanRx.h"
#include "Slcan.h"
#include "pinout.h"
#include "schema_id.h"

namespace {

/*
 * Every scale factor below has a matching declaration in generate_914_dbc.py. Where the two
 * could disagree silently, the units are spelled out here, because the failure mode is not a
 * crash -- it is a log full of numbers that are wrong by a factor of ten and look plausible.
 *
 * Internal units, for reference:
 *   packVoltage    mV (sum of the cells in series, so ~336600 for a full pack)
 *   totalCurrent   mA
 *   joules         J
 *   allVoltages    mV
 *   allTemperatures  degC as float
 *   dieTemps       whole degC, stored in an int8_t (signed: the part's range is -40 to +125 C)
 *   heatsinktemp   whole degC (Main.cpp already divides the inverter's 0.1 degC by 10)
 */

inline void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

inline void put_i16(uint8_t *p, int16_t v)
{
    put_u16(p, (uint16_t)v);
}

inline void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

// Saturating rather than wrapping. A pegged counter reads as "at least this many", which is
// still true; a wrapped one reads as a small number, which is a lie.
inline uint16_t sat_u16(uint32_t v)
{
    return (v > 0xFFFFu) ? 0xFFFFu : (uint16_t)v;
}

inline int16_t sat_i16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

// Thermistor readings are floats and can be NaN if a channel is open; isnan() is already
// checked before storing, but a stale slot never written is whatever it was initialised to.
inline int16_t temp_to_deci(float degC)
{
    if (isnan(degC)) {
        return 0;
    }
    return sat_i16((int32_t)lroundf(degC * 10.0f));
}

} // namespace

// ---------------------------------------------------------------- fast tier

void telemetry_emit_status(const BmsStatusFields &f)
{
    uint8_t p[8];
    p[0] = f.faultLatched;
    p[1] = f.faultNow;
    p[2] = f.status;
    p[3] = f.ioexpOut;
    p[4] = (uint8_t)(MCP_BMS_THREAD_MASK);
    put_u16(&p[5], f.errCount);
    p[7] = f.ioexpIn;
    slcan_emit(TLM_ID_STATUS, p, 8, true);
}

void telemetry_emit_knobs(const BmsKnobFields &f)
{
    uint8_t p[8];
    put_u16(&p[0], f.knob1Raw);
    put_u16(&p[2], f.knob2Raw);
    put_u16(&p[4], f.knob3Raw);
    p[6] = f.gpiPortB;
    p[7] = f.gpiMask;
    slcan_emit(TLM_ID_KNOBS, p, 8, true);
}

void telemetry_emit_pack(const batterydata_t &d)
{
    uint8_t p[8];
    put_u16(&p[0], sat_u16(d.packVoltage / 100));            // mV -> 0.1 V
    put_i16(&p[2], sat_i16(d.totalCurrent / 10));            // mA -> 0.01 A
    p[4] = d.soc;
    p[5] = d.numBalancing;
    // Power is deliberately absent: it is V * I and belongs in the decoder, not on the wire.
    int64_t centiKwh = d.joules / 36000;                     // J -> 0.01 kWh
    if (centiKwh < 0) centiKwh = 0;
    if (centiKwh > 0xFFFF) centiKwh = 0xFFFF;
    put_u16(&p[6], (uint16_t)centiKwh);
    slcan_emit(TLM_ID_PACK, p, 8, true);
}

void telemetry_emit_cell_summary(const batterysummary_t &s)
{
    uint8_t p[8];
    put_u16(&p[0], s.minVoltage);
    p[2] = s.minVoltage_cell;
    put_u16(&p[3], s.maxVoltage);
    p[5] = s.maxVoltage_cell;
    // Average, min and max travel together so a low cell and a high cell are always
    // distinguishable from one frame. See the note in Data.h on why the divisor is 84.
    put_u16(&p[6], s.avgVoltage);
    slcan_emit(TLM_ID_CELL_SUM, p, 8, true);
}

void telemetry_emit_temp_summary(const batterysummary_t &s, int16_t heatsinkTempDegC)
{
    uint8_t p[8];
    put_i16(&p[0], temp_to_deci(s.minTemp));
    p[2] = s.minTemp_box;
    put_i16(&p[3], temp_to_deci(s.maxTemp));
    p[5] = s.maxTemp_box;
    // The inverter reports 0.1 degC and Main.cpp divides by 10 for the display, so scale back
    // up rather than inventing a second unit for the same quantity.
    put_i16(&p[6], sat_i16((int32_t)heatsinkTempDegC * 10));
    slcan_emit(TLM_ID_TEMP_SUM, p, 8, true);
}

// ---------------------------------------------------------------- slow tier

void telemetry_emit_slow(const batterydata_t &d, uint32_t maxLatencyUs)
{
    uint8_t p[8];

    // Cell voltages, 4 per frame. The flat 0..167 index matches the DBC's Cell_000..Cell_167
    // and the balancing mask; allVoltages is [string][cell within string], so the two strings
    // sit end to end. A trailing partial frame is zero-padded and its unused signals simply do
    // not exist in the DBC.
    for (uint16_t frame = 0; frame < TLM_CELL_FRAMES; frame++) {
        memset(p, 0, sizeof(p));
        for (uint8_t k = 0; k < 4; k++) {
            uint16_t cell = frame * 4 + k;
            if (cell < TLM_NUM_CELLS) {
                put_u16(&p[k * 2],
                        d.allVoltages[cell / TLM_CELLS_PER_STR][cell % TLM_CELLS_PER_STR]);
            }
        }
        slcan_emit(TLM_ID_CELL_BASE + frame, p, 8, true);
    }

    // Thermistors, 4 per frame at 0.1 degC -- genuinely that precise, unlike the die temps.
    for (uint16_t frame = 0; frame < TLM_THERM_FRAMES; frame++) {
        memset(p, 0, sizeof(p));
        for (uint8_t k = 0; k < 4; k++) {
            uint16_t idx = frame * 4 + k;
            if (idx < NUM_CHIPS) {
                put_i16(&p[k * 2], temp_to_deci(d.allTemperatures[idx]));
            }
        }
        slcan_emit(TLM_ID_THERM_BASE + frame, p, 8, true);
    }

    // Die temperatures, 8 per frame at whole degrees. The source is a uint8_t computed as
    // raw*0.0001/0.0076 - 276, so encoding at 0.1 degC would invent precision that is not
    // there. Reinterpreted as int8: the subtraction wraps for sub-zero readings and the
    // two's-complement bit pattern recovers them.
    for (uint16_t frame = 0; frame < TLM_DIE_FRAMES; frame++) {
        memset(p, 0, sizeof(p));
        for (uint8_t k = 0; k < 8; k++) {
            uint16_t idx = frame * 8 + k;
            if (idx < NUM_CHIPS) {
                p[k] = (uint8_t)d.dieTemps[idx];
            }
        }
        slcan_emit(TLM_ID_DIE_BASE + frame, p, 8, true);
    }

    // Link health. The latency is the caller's read-and-clear maximum over this logging
    // interval; a lifetime maximum would be pinned by the worst moment since boot and would
    // never move again.
    memset(p, 0, sizeof(p));
    put_u16(&p[0], sat_u16(canRxSwDrops));
    put_u16(&p[2], sat_u16(canRxHwOverruns));
    put_u16(&p[4], sat_u16(canRxQueuePeak));
    put_u16(&p[6], sat_u16(maxLatencyUs));
    slcan_emit(TLM_ID_LINK, p, 8, true);

    // Balancing mask, one bit per cell, indexed identically to the cell voltages above. The
    // mask is a byte array in the same bit order the DBC expects (bit n of byte b is cell
    // b*8+n), so each frame is a straight slice of it.
    for (uint16_t frame = 0; frame < TLM_BAL_FRAMES; frame++) {
        memset(p, 0, sizeof(p));
        for (uint8_t k = 0; k < 8; k++) {
            uint16_t b = frame * 8 + k;
            if (b < TLM_BAL_BYTES) {
                p[k] = d.balanceMask[b];
            }
        }
        slcan_emit(TLM_ID_BAL_BASE + frame, p, 8, true);
    }
}

// ---------------------------------------------------------------- schema and diagnostics

void telemetry_emit_schema()
{
    uint8_t p[8];
    memset(p, 0, sizeof(p));
    p[0] = SCHEMA_MAJOR;
    p[1] = SCHEMA_MINOR;
    put_u32(&p[2], SCHEMA_HASH);
    slcan_emit(TLM_ID_SCHEMA, p, 8, true);
}

void telemetry_emit_diag(uint8_t code, uint16_t arg1, uint16_t arg2, uint16_t arg3)
{
    uint8_t p[8];
    p[0] = code;
    put_u16(&p[1], arg1);
    put_u16(&p[3], arg2);
    put_u16(&p[5], arg3);
    p[7] = 0;
    slcan_emit(TLM_ID_DIAG, p, 8, true);
}

void telemetry_echo_frame(uint32_t id, const uint8_t *data, uint8_t len)
{
    slcan_emit(id, data, len, false);
}

#endif // SLCAN_MODE

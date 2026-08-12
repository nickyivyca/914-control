#include <iostream>

#include "mbed.h"
#include "rtos.h"

#include "pinout.h"
#include "Slcan.h"

namespace {

const char HEXD[] = "0123456789ABCDEF";

// Guards a whole frame, not a byte. See the note in Slcan.h -- interleaving two threads
// mid-frame would produce spliced records indistinguishable from the link corruption being
// measured, which is how the 2021 logs got their `ERR:` messages embedded in CSV rows.
Mutex s_lock;

uint16_t s_crc = 0xFFFF;      // CCITT, over every ASCII byte of the current block
uint16_t s_frames = 0;        // frames in the current block
uint16_t s_seqCounter = 0;    // increments per sequence frame, wraps

inline void crc_byte(uint8_t b)
{
    s_crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; i++) {
        s_crc = (s_crc & 0x8000) ? (uint16_t)((s_crc << 1) ^ 0x1021) : (uint16_t)(s_crc << 1);
    }
}

// Builds the frame into a local buffer and writes it once. A single write keeps the frame
// atomic below the mutex as well as above it, and avoids the many-small-writes pattern that
// the CSV used.
size_t build(char *out, uint32_t id, const uint8_t *data, uint8_t dlc, bool extended)
{
    size_t n = 0;
    if (extended) {
        out[n++] = 'T';
        for (int shift = 28; shift >= 0; shift -= 4) {
            out[n++] = HEXD[(id >> shift) & 0xF];
        }
    } else {
        out[n++] = 't';
        for (int shift = 8; shift >= 0; shift -= 4) {
            out[n++] = HEXD[(id >> shift) & 0xF];
        }
    }
    out[n++] = HEXD[dlc & 0xF];
    for (uint8_t i = 0; i < dlc; i++) {
        out[n++] = HEXD[(data[i] >> 4) & 0xF];
        out[n++] = HEXD[data[i] & 0xF];
    }
    out[n++] = '\r';
    return n;
}

// Emit the integrity frame and start a new block. Caller must hold s_lock.
void emit_sequence_locked()
{
    uint8_t payload[8];
    // Little-endian, like every other frame in the map. This was big-endian in the first
    // prototype, which made the one frame that is supposed to police the stream the only one
    // that disagreed with the DBC about byte order.
    payload[0] = (uint8_t)(s_seqCounter & 0xFF);
    payload[1] = (uint8_t)(s_seqCounter >> 8);
    payload[2] = (uint8_t)(s_frames & 0xFF);
    payload[3] = (uint8_t)(s_frames >> 8);
    payload[4] = (uint8_t)(s_crc & 0xFF);
    payload[5] = (uint8_t)(s_crc >> 8);
    payload[6] = 0;
    payload[7] = 0;

    // The sequence frame reports the block before it, and its own bytes are excluded from
    // every block -- it is written directly rather than through slcan_emit(), so it neither
    // accumulates into the CRC nor counts towards s_frames. The host must skip it the same
    // way: CRC covers exactly the frames between two sequence frames.
    s_seqCounter++;
    s_crc = 0xFFFF;
    s_frames = 0;

    char line[32];
    size_t n = build(line, SLCAN_SEQ_ID, payload, 8, true);
    std::cout.write(line, n);
}

} // namespace

void slcan_init()
{
    s_crc = 0xFFFF;
    s_frames = 0;
    s_seqCounter = 0;
}

void slcan_emit(uint32_t id, const uint8_t *data, uint8_t dlc, bool extended)
{
    char line[32];
    if (dlc > 8) {
        dlc = 8;
    }
    size_t n = build(line, id, data, dlc, extended);

    s_lock.lock();
    for (size_t i = 0; i < n; i++) {
        crc_byte((uint8_t)line[i]);
    }
    s_frames++;
    std::cout.write(line, n);

    // Block boundary. Checked here so that every emitted frame is counted exactly once no
    // matter which thread produced it.
    if (s_frames >= SLCAN_SEQ_INTERVAL) {
        emit_sequence_locked();
    }
    s_lock.unlock();
}

void slcan_emit_sequence()
{
    s_lock.lock();
    emit_sequence_locked();
    s_lock.unlock();
}

uint16_t slcan_frames_since_sequence()
{
    return s_frames;
}

void slcan_lock()
{
    s_lock.lock();
}

void slcan_unlock()
{
    s_lock.unlock();
}

// ---------------------------------------------------------------- host -> bus

#if SLCAN_HOST_TX

volatile uint32_t slcanHostTxFrames = 0;
volatile uint32_t slcanHostTxErrors = 0;

namespace {

// Longest command is 'T' + 8 id + 1 dlc + 16 data = 26, plus slack for junk.
char s_cmd[40];
size_t s_cmdLen = 0;
bool s_overrun = false;

FileHandle *s_in = nullptr;

// Replies go out under the same lock as frames. Without it an ack lands inside a telemetry
// frame and the host rejects that frame -- the exact splice the mutex exists to prevent.
// Reply bytes are deliberately NOT fed to crc_byte: the integrity CRC covers emitted frames
// only, so a host that CRCs frame bytes still matches. Same rule as the dual-emit CSV.
void reply(const char *s, size_t n)
{
    s_lock.lock();
    std::cout.write(s, n);
    s_lock.unlock();
}

inline void reply_ok()   { reply("\r", 1); }
inline void reply_err()  { reply("\a", 1); }

int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Parse n hex digits starting at p. Returns false on any non-hex digit.
bool hexfield(const char *p, size_t n, uint32_t &out)
{
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) {
        int d = hexval(p[i]);
        if (d < 0) return false;
        v = (v << 4) | (uint32_t)d;
    }
    out = v;
    return true;
}

// t<3 id><dlc><data>  /  T<8 id><dlc><data>  /  r,R the same without data.
void handle_transmit(const char *cmd, size_t len)
{
    const bool extended = (cmd[0] == 'T' || cmd[0] == 'R');
    const bool remote   = (cmd[0] == 'r' || cmd[0] == 'R');
    const size_t idlen  = extended ? 8 : 3;

    if (len < 1 + idlen + 1) { reply_err(); return; }

    uint32_t id, dlc;
    if (!hexfield(cmd + 1, idlen, id))          { reply_err(); return; }
    if (!hexfield(cmd + 1 + idlen, 1, dlc))     { reply_err(); return; }
    if (dlc > 8)                                { reply_err(); return; }

    uint8_t data[8] = {0};
    if (!remote) {
        // The frame's length is fixed by its DLC. Enforcing that here is what stops a
        // truncated command being transmitted as a short frame with garbage payload.
        if (len != 1 + idlen + 1 + dlc * 2)     { reply_err(); return; }
        for (uint32_t i = 0; i < dlc; i++) {
            uint32_t b;
            if (!hexfield(cmd + 1 + idlen + 1 + i * 2, 2, b)) { reply_err(); return; }
            data[i] = (uint8_t)b;
        }
    } else if (len != 1 + idlen + 1) {
        reply_err();
        return;
    }

    CANMessage msg;
    msg.id     = id;
    msg.len    = (uint8_t)dlc;
    msg.format = extended ? CANExtended : CANStandard;
    msg.type   = remote ? CANRemote : CANData;
    for (uint32_t i = 0; i < dlc; i++) {
        msg.data[i] = data[i];
    }

    if (canBus->write(msg)) {
        slcanHostTxFrames++;
        // Echo it into the log. The VCU cannot receive its own transmission, so without this
        // the record shows the inverter's SDO replies with nothing that provoked them.
        slcan_emit(id, data, (uint8_t)dlc, extended);
        reply_ok();
    } else {
        slcanHostTxErrors++;
        reply_err();
    }
}

void handle_command(const char *cmd, size_t len)
{
    if (len == 0) { reply_ok(); return; }

    switch (cmd[0]) {
        case 't': case 'T': case 'r': case 'R':
            handle_transmit(cmd, len);
            break;

        // Open, close, bitrate, BTR, filter mask/code, timestamps. Accepted and ignored: the
        // bus is already up at CAN_FREQUENCY and the VCU is a node on it, not a dongle that
        // can go offline. Answering ACK rather than BELL matters -- python-can's slcan backend
        // sends C, S, O on open and treats a BELL as a fatal error.
        case 'O': case 'C': case 'S': case 's':
        case 'M': case 'm': case 'Z':
            reply_ok();
            break;

        case 'V': reply("V1013\r", 6); break;   // hardware/software version
        case 'N': reply("N914C\r", 6); break;   // serial number
        case 'F': reply("F00\r", 4); break;     // status flags, none latched

        default:
            reply_err();
            break;
    }
}

} // namespace

void slcan_input_init()
{
    // The console file handle, not the `serial` object: with stdio-buffered-serial the console
    // owns the UART's receive interrupt and buffers into its own queue, so reading the raw
    // UnbufferedSerial would race it and lose bytes.
    s_in = mbed_file_handle(STDIN_FILENO);
    if (s_in != nullptr) {
        s_in->set_blocking(false);
    }
    s_cmdLen = 0;
    s_overrun = false;
}

void slcan_poll_input()
{
    if (s_in == nullptr) {
        return;
    }

    char c;
    while (s_in->read(&c, 1) == 1) {
        if (c == '\r' || c == '\n') {
            if (s_overrun) {
                // The command was longer than any valid one, so it is junk however it ends.
                // Report it rather than acting on a truncated prefix.
                reply_err();
            } else {
                s_cmd[s_cmdLen] = '\0';
                handle_command(s_cmd, s_cmdLen);
            }
            s_cmdLen = 0;
            s_overrun = false;
        } else if (s_cmdLen < sizeof(s_cmd) - 1) {
            s_cmd[s_cmdLen++] = c;
        } else {
            s_overrun = true;
        }
    }
}

#endif // SLCAN_HOST_TX

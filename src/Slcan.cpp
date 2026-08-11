#include <iostream>

#include "mbed.h"
#include "rtos.h"

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

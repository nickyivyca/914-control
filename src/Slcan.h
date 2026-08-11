#ifndef SLCAN_H
#define SLCAN_H

#include "mbed.h"
#include "config.h"

/*
 * SLCAN (LAWICEL ASCII) transmit encoding, plus the integrity frame the plain format lacks.
 *
 * Why this format: it is the one CAN-over-serial standard that needs no hardware change here,
 * and it is understood as-is by python-can (slcan), SavvyCAN and can-utils. Frames are
 *
 *     t<3 hex id><dlc><data bytes as hex>\r        standard, 11-bit
 *     T<8 hex id><dlc><data bytes as hex>\r        extended, 29-bit
 *
 * The 11/29-bit split carries meaning here: standard IDs are frames that were really on the
 * vehicle bus, extended IDs are synthetic -- values the VCU knows internally and that no CAN
 * controller ever saw. One DBC can then describe both without ambiguity.
 *
 * WHAT THIS DOES NOT INHERIT: CAN's own 15-bit CRC. That is checked and stripped by the CAN
 * controller before can_read() returns, so it only ever protected the hop from another ECU to
 * this chip -- which is not where the corruption is. Synthetic frames never had one at all.
 * Between this chip and the host there is no checksum in the SLCAN format whatsoever.
 *
 * What the format does give, for free, is structure: a frame's length is fixed by its DLC and
 * it terminates at '\r', so any inserted or deleted byte makes the length wrong and the frame
 * is rejected. That covers the single-byte insertion failure mode. It does NOT cover a byte
 * substituted into another valid hex digit, and it cannot see whole frames going missing.
 *
 * So a sequence frame is emitted periodically on SLCAN_SEQ_ID carrying a counter, the number
 * of frames since the last one, and a CRC16 over every ASCII byte emitted since the last one.
 * Counter gaps expose lost frames; the CRC exposes substitutions the structural check misses.
 * Both live inside a normal CAN frame, so other tools simply see an ID they can ignore.
 */

// Synthetic telemetry lives in a reserved extended-ID block. The low byte selects the channel.
#define SLCAN_SYNTH_BASE   0x1F000000u

// The integrity frame. Deliberately the top of the extended range so it sorts last and is
// obviously not vehicle data.
#define SLCAN_SEQ_ID       0x1FFFFFFFu

void slcan_init();

// Emit one frame. Whole-frame atomic: telemetry comes from the BMS thread and forwarded bus
// traffic from the main loop, and two threads interleaving mid-frame would manufacture exactly
// the corruption this exists to measure.
//
// Closes the block and emits the integrity frame automatically once SLCAN_SEQ_INTERVAL frames
// have gone out. Counting here rather than in the callers is what makes forwarded bus traffic
// covered: the main loop forwards frames without any idea of the block structure, and when the
// BMS thread owned the counter those frames sat inside blocks whose declared frame count did
// not include them.
void slcan_emit(uint32_t id, const uint8_t *data, uint8_t dlc, bool extended);

// Emit the sequence/CRC frame and start a new block. Called automatically by slcan_emit(); only
// needed directly to force a block boundary.
void slcan_emit_sequence();

// Frames emitted since the last sequence frame, for the caller's pacing decisions.
uint16_t slcan_frames_since_sequence();

// Hold the frame lock across something that is not a frame. The only use is the dual-emit
// verification build, where a CSV record is written into the same stream: without the lock a
// forwarded frame from the main loop can land in the middle of the CSV line, which is the
// splice this whole design exists to prevent. Bytes written between these are NOT accumulated
// into the block CRC, so a host must exclude non-frame bytes when checking it.
void slcan_lock();
void slcan_unlock();

#endif // SLCAN_H

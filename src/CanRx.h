#ifndef CAN_RX_H
#define CAN_RX_H

#include "mbed.h"
#include "us_ticker_api.h"
#include "config.h"

/*
 * The CAN receive queue and its loss instrumentation.
 *
 * canqueue is the only elastic buffer in the whole receive path. The LPC17xx has no receive
 * FIFO to fall back on -- can_read() gates on GSR bit 0, a single "message waiting" flag, and
 * releases one message at a time via CMR = 0x04. The ISR must do the read (returning without
 * reading re-asserts the interrupt immediately; see IsrSafeCAN.h), so everything downstream
 * of the ISR depends on this queue absorbing whatever arrives between main-loop drains.
 *
 * Frames can be lost in two places, and both are silent by default:
 *
 *   below the ISR   a frame arrives before the ISR services the previous one. The LPC17xx
 *                   reports this as a Data Overrun -> canRxHwOverruns.
 *   above the ISR   the queue fills before the main loop drains it. CircularBuffer::push()
 *                   overwrites the oldest entry with no error and no counter -> canRxSwDrops.
 *
 * Both should read zero. The point is positive evidence that a capture is complete rather
 * than absence of evidence that it is not.
 */

// A received frame plus the time it was taken out of the hardware receive buffer.
//
// The main loop drains up to MAIN_PERIOD (50 ms) of frames in one burst, so a timestamp taken
// at pop time would collapse a whole burst onto near-identical values and lose the real
// inter-frame timing. us_ticker_read() is a single read of a free-running 32-bit 1 MHz timer
// (targets/TARGET_NXP/TARGET_LPC17XX/us_ticker.c) -- no locks, safe from an ISR. It wraps
// every ~71.6 minutes, so consumers must compare with unsigned subtraction, never with <.
struct TimestampedCANMessage {
    CANMessage msg;
    uint32_t   rxTimeUs;
};

// 20 bytes: CANMessage is 16 -- id 4, data[8], len 1, format 1, type 1, one byte of tail
// padding -- plus the 4-byte timestamp. format and type are enums, which would be 4 bytes
// each under the generic ABI; they are 1 here because arm-none-eabi-gcc defaults to
// -fshort-enums. Asserted rather than assumed, because CAN_RX_QUEUE_DEPTH is sized against
// the AHB SRAM bank below and a silent change here would silently overrun it.
static_assert(sizeof(TimestampedCANMessage) == 20,
              "TimestampedCANMessage changed size; re-check CAN_RX_QUEUE_DEPTH against IRAM2");

/*
 * The queue lives in the AHB SRAM bank (IRAM2), not in the main SRAM the rest of the program
 * uses. At CAN_RX_QUEUE_DEPTH 512 it is 14 KB, and IRAM1 is 32 KB with ~60% already spoken
 * for -- it does not fit there. IRAM2 carries no static data at all today; the linker script
 * gives it entirely to `.AHBSRAM_bss` (this section) followed by heap. That heap is only the
 * *second* sbrk region: _sbrk fills the 12.7 KB IRAM1 heap first and falls through to IRAM2
 * only on exhaustion (platform/source/mbed_retarget.cpp:1492), which this program has never
 * reached. Placing the queue here therefore costs nothing that is presently in use, and
 * still leaves ~18 KB of overflow heap behind it.
 *
 * The section is NOLOAD, so the storage is not initialised from flash -- but CircularBuffer
 * has a real constructor, which runs from .init_array at startup like any other global.
 */
extern __attribute__((section("AHBSRAM")))
CircularBuffer<TimestampedCANMessage, CAN_RX_QUEUE_DEPTH> canqueue;

// Frames overwritten in canqueue because the main loop had not drained it in time.
extern volatile uint32_t canRxSwDrops;

// Frames lost inside the CAN peripheral because the ISR did not read the previous one in
// time. Counted from CAN::DoIrq; see IsrSafeCAN::clearDataOverrun().
extern volatile uint32_t canRxHwOverruns;

// Deepest observed occupancy of canqueue, in entries. This is what says whether
// CAN_RX_QUEUE_DEPTH is generous or merely lucky.
extern volatile uint32_t canRxQueuePeak;

// Longest time any frame sat in canqueue between the ISR pushing it and the main loop
// popping it, in microseconds. Bounded by the drain interval in normal operation, so a
// value near or above MAIN_PERIOD*1000 means the main loop, not the queue, is the limit.
extern volatile uint32_t canRxMaxLatencyUs;

#endif // CAN_RX_H

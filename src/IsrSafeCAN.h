#ifndef ISR_SAFE_CAN_H
#define ISR_SAFE_CAN_H

#include "mbed.h"

/*
 * mbed's CAN::read() takes a Mutex, but canRX() runs in CAN interrupt context,
 * where acquiring a mutex traps. The chain, still intact in Mbed CE 7.0.0:
 *   CAN::read()      -> lock()                      drivers/source/CAN.cpp:97, :206
 *   CAN::lock()      -> _mutex.lock()
 *   Mutex::lock()    -> osMutexAcquire(); any non-osOK raises a FATAL
 *                       MBED_ERROR_CODE_MUTEX_LOCK_FAILED
 *                                                   rtos/source/Mutex.cpp:65-75
 *   osMutexAcquire() -> returns osErrorISR when IsException() || IsIrqMasked()
 *                                        cmsis/CMSIS-RTX/Source/rtx_mutex.c:658
 *
 * Why the read must happen in the ISR at all: on LPC17xx the receive interrupt
 * stays asserted until the receive buffer is released, and the only thing that
 * releases it is `obj->dev->CMR = 0x04` inside can_read() itself
 * (targets/TARGET_NXP/TARGET_LPC17XX/can_api.c:427). Returning from the ISR
 * without reading re-triggers it immediately. Deferring the read to a thread
 * therefore also requires masking and unmasking the CAN interrupt around it.
 *
 * Origin: ARMmbed/mbed-os issue #5374 (2017-10-24). The read()-only unlock was
 * posted by GitHub user omdathetkan on 2018-01-24, who noted it "can be done
 * without modifying the mbed-os files by subclassing it in the application and
 * adding a CAN::readUnsafe()" -- which is what this class does. Arm's
 * SenRamakri recommended an event/mailbox reader thread instead; omdathetkan
 * countered with the interrupt-clearing constraint described above.
 *   https://github.com/ARMmbed/mbed-os/issues/5374#issuecomment-360232458
 *
 * Upstream status: fixed by PR #14688, merged 2021-07-12, released in mbed-os
 * 6.13.0. That fix has two halves, and only one reaches this target -- the
 * deferred can_read is STM/bxCAN-specific (the generic driver has no thread or
 * semaphore machinery), so on LPC1768 the only thing that shipped is the
 * RawCAN class, which Mbed CE does carry at drivers/include/drivers/RawCAN.h.
 *
 * Why not just use RawCAN: it overrides lock()/unlock() for the whole object,
 * so write() becomes unlocked too. This project's earlier hand-patch of
 * mbed-os/drivers/source/CAN.cpp unlocked read() only and kept write() locked,
 * and that asymmetry is preserved here. It costs nothing today (BmsThread is
 * the only writer) but keeps writers serialised if a second TX path is ever
 * added -- e.g. the planned CAN telemetry/proxy work.
 *
 * The remaining race -- an unlocked ISR read against a locked thread write --
 * is benign on the LPC17xx HAL: read and write touch disjoint registers except
 * CMR, where each is a single store of self-clearing command bits using
 * disjoint bit patterns (0x04 release-rx vs 0x21 transmit-request), and
 * can_enable()'s read-modify-write on MOD is idempotent in both paths.
 */
class IsrSafeCAN : public mbed::CAN {
public:
    using mbed::CAN::CAN;

    /*
     * Lock-free equivalent of CAN::read(), for use from an ISR.
     * Returns 1 on a message read, 0 on none, matching CAN::read().
     */
    int readNoLock(mbed::CANMessage &msg, int handle = 0)
    {
        return can_read(&_can, &msg, handle);
    }

    /*
     * Clear the peripheral's Data Overrun Status flag. Call this from a CAN::DoIrq handler.
     *
     * Per UM10360 (LPC176x/5x user manual): the Data Overrun Interrupt in ICR is set on the
     * 0 -> 1 transition of the Data Overrun Status bit in SR/GSR, and DOS is cleared only by
     * writing the Clear Data Overrun command, CMR bit 3. Reading ICR -- which can_irq_n()
     * does before dispatching -- clears the interrupt flag but not DOS. So without this call
     * DOS latches after the first overrun, no further 0 -> 1 transition can occur, and the
     * counter would read 1 forever no matter how many frames were actually lost. Nothing in
     * mbed-os writes CMR bit 3 anywhere, which is why this has to live here.
     *
     * Reaching _can.dev makes this LPC17xx-specific, unlike readNoLock() above. That is fine
     * for this project but is the line to fix first if it is ever ported.
     *
     * NOT YET OBSERVED ON HARDWARE -- an overrun has never been provoked on this car. The
     * register semantics above are from the manual, not from a measurement.
     */
    void clearDataOverrun()
    {
        _can.dev->CMR = (1 << 3);
    }
};

#endif // ISR_SAFE_CAN_H

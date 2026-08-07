#ifndef ISR_SAFE_CAN_H
#define ISR_SAFE_CAN_H

#include "mbed.h"

/*
 * mbed's CAN::read() takes a Mutex, but canRX() runs in CAN interrupt context,
 * where acquiring a mutex traps. The pre-Mbed-CE build worked around this by
 * editing mbed-os/drivers/source/CAN.cpp to comment out the lock()/unlock()
 * calls in read() only -- write() kept its mutex. That patched file was kept at
 * the repo root as CAN.cpp and hand-copied into the (gitignored) mbed-os tree.
 *
 * Mbed CE still locks in read(), and a patch inside a submodule would be silently
 * lost on update. can_t _can is protected, so a thin subclass can provide the
 * lock-free read directly and leave every other CAN method -- including write()
 * and its mutex -- exactly as upstream.
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
};

#endif // ISR_SAFE_CAN_H

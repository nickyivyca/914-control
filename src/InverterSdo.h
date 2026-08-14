#ifndef INVERTER_SDO_H
#define INVERTER_SDO_H

#include "mbed.h"

#include "config.h"

/*
 * Polling the inverter for parameters the periodic CAN map cannot carry.
 *
 * WHY THIS EXISTS. stm32-sine's UpgradeParameters() runs unconditionally from main() on every
 * boot and deletes the CAN map entries for pot, pot2, canio, cruisespeed and regenpreset. Those
 * five are the payload of a hard-coded control frame on `controlid`, protected by a dual sequence
 * counter and a CRC; the generic CanMap has neither, so the firmware refuses to let an
 * unprotected path to a throttle be recreated. That reasoning is sound for *receive*, but
 * CanMap::Remove() takes no direction argument -- it deletes the first match in either map -- so
 * a transmit-only telemetry mapping is collateral damage of a guard aimed at receive.
 *
 * Confirmed on the car 2026-08-13: all five were added, saved (both "CANMAP stored" and
 * "Parameters stored"), power cycled, and exactly those that had been mapped came back missing.
 *
 * So the raw throttle ADC digits have no periodic-frame route on stock firmware. They are still
 * readable on demand over openinverter's SDO server, which bypasses the CAN map entirely. This
 * module is that read.
 *
 * THE PROTOCOL. CANopen-style expedited request/response, 8 bytes packed little-endian:
 *
 *     cmd(1) | index(2) | subIndex(1) | data(4)
 *
 *     request   0x601   40 <index lo> <index hi> <sub> 00 00 00 00
 *     reply     0x581   43 <index lo> <index hi> <sub> <int32 little-endian>
 *     abort     0x581   80 <index lo> <index hi> <sub> <abort code>
 *
 * Parameters are named by UNIQUE ID, not by array position. Two forms exist and only one is safe
 * across firmware builds:
 *
 *     index 0x2000, subIndex = position in the parameter table    fragile -- positions shift
 *     index 0x2100 | (id >> 8), subIndex = id & 0xFF              stable -- use this
 *
 * The firmware reconstructs it as Param::NumFromId(subIndex + ((index & 0xFF) << 8)), so for
 * pot (id 2015 = 0x07DF) the request carries index 0x2107, subIndex 0xDF.
 *
 * THE FIXED-POINT TRAP, for whatever decodes the reply. The payload is Param::Get(), which
 * returns s32fp -- fixed point with 5 fractional bits. The value on the wire is the real value
 * times 32, and it is SIGNED: potnom = -12.09 arrives as a negative int32 in two's complement,
 * not as a large positive one. Divide by 32 with an arithmetic shift or a signed divide. Using
 * >> 5 on an unsigned type decodes every regen reading as roughly +134 million.
 *
 * This module does not decode it. The raw exchange is logged as two CAN frames and paired in
 * post-processing, which is honest about what SDO is: request/response, not periodic, with a
 * 24-bit index/subIndex pair selecting the meaning that a DBC multiplexer cannot express.
 *
 * WHAT THIS IS NOT. A diagnostic path, and nothing in the control loop may depend on it. SDO
 * carries no sequence counter, no CRC and no timeout semantics; a lost reply is a lost sample and
 * nothing more. It is also READ-ONLY BY CONSTRUCTION -- there is no write path in this file. An
 * SDO *write* can change inverter parameters, potmode among them, and can be persisted to flash;
 * a read cannot.
 */

// 0x600 + nodeid / 0x580 + nodeid, the pair CanSdo registers as a user message filter.
#define INV_SDO_REQ_ID   (0x600u + INVERTER_NODE_ID)
#define INV_SDO_RESP_ID  (0x580u + INVERTER_NODE_ID)

// The inverter state frame, 0x002. Polled rate depends on opmode, which arrives in its byte 2.
#define INV_STATE_ID     0x002u

#if INVERTER_SDO_POLL

// Feed every frame the main loop drains from the CAN receive queue. Two things are picked out:
// opmode from 0x002, which sets the poll rate, and replies on 0x581, which close a request.
// Everything else is ignored.
void inverter_sdo_on_frame(const CANMessage &msg);

// Call once per main-loop iteration. Owns the pacing, the round-robin and the give-up rule.
void inverter_sdo_tick();

#else

inline void inverter_sdo_on_frame(const CANMessage &) {}
inline void inverter_sdo_tick() {}

#endif // INVERTER_SDO_POLL

#endif // INVERTER_SDO_H

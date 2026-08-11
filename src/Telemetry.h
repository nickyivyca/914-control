#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "mbed.h"

#include "config.h"
#include "Data.h"

/*
 * The synthetic-telemetry CAN map.
 *
 * This file is the firmware half of notes/can-id-allocation.md in the 914 notes repo; the other
 * half is notes/artifacts/generate_914_dbc.py, which emits 914-telemetry.dbc and schema_id.h.
 * The two must agree byte for byte. They are kept honest by the schema hash: the generator
 * digests the map's structure and writes it into schema_id.h, this firmware broadcasts it, and
 * a consumer compares it against the DBC it loaded. Editing one side without regenerating the
 * other therefore shows up as a hash mismatch in the log rather than as plausible wrong numbers.
 *
 * Conventions, all of which the DBC also encodes:
 *   - Extended 29-bit id  =>  synthetic. A value the VCU knows internally that no CAN
 *                             controller ever saw.
 *   - Standard 11-bit id  =>  the frame really was on the vehicle bus.
 *   - Little-endian (Intel) throughout.
 *
 * Two rate tiers, both derived from the existing print constants so that charging slows the
 * telemetry the same way it already slows the CSV:
 *   - Fast: one per cell-sense scan  (10 Hz driving, 2 Hz charging)
 *   - Slow: one per logging scan     (CELL_SENSE_FREQUENCY / CELL_PRINT_MULTIPLE)
 */

// ---------------------------------------------------------------- ids

#define TLM_ID_STATUS      0x1F000000u
#define TLM_ID_PACK        0x1F000001u
#define TLM_ID_CELL_SUM    0x1F000002u
#define TLM_ID_TEMP_SUM    0x1F000003u
#define TLM_ID_CELL_BASE   0x1F000010u
#define TLM_ID_THERM_BASE  0x1F000040u
#define TLM_ID_DIE_BASE    0x1F000050u
#define TLM_ID_LINK        0x1F000060u
#define TLM_ID_BAL_BASE    0x1F000070u
#define TLM_ID_DIAG        0x1F0000F0u
#define TLM_ID_SCHEMA      0x1FFFFFFEu

// The VCU's own transmission to the inverter. A CAN node does not receive its own frames, so
// this one is echoed into the log deliberately at transmit request -- see telemetry_echo_frame().
#define TLM_ID_VCU_CONTROL 0x03Fu

// Derived frame counts. These are what walk one block into the next when the pack grows, so the
// static_asserts below stand in for the ID-collision guard the generator already has.
#define TLM_NUM_CELLS      (NUM_CHIPS * NUM_CELLS_PER_CHIP)
#define TLM_CELLS_PER_STR  (TLM_NUM_CELLS / NUM_STRINGS)
#define TLM_CELL_FRAMES    ((TLM_NUM_CELLS + 3) / 4)
#define TLM_THERM_FRAMES   ((NUM_CHIPS + 3) / 4)
#define TLM_DIE_FRAMES     ((NUM_CHIPS + 7) / 8)
#define TLM_BAL_BYTES      ((TLM_NUM_CELLS + 7) / 8)
#define TLM_BAL_FRAMES     ((TLM_NUM_CELLS + 63) / 64)

static_assert(TLM_ID_CELL_BASE + TLM_CELL_FRAMES <= TLM_ID_THERM_BASE,
              "cell-voltage frames have outgrown their block and now collide with the "
              "thermistor block; move TLM_ID_THERM_BASE and the blocks after it, and make the "
              "same move in generate_914_dbc.py");
static_assert(TLM_ID_THERM_BASE + TLM_THERM_FRAMES <= TLM_ID_DIE_BASE,
              "thermistor frames collide with the die-temperature block");
static_assert(TLM_ID_DIE_BASE + TLM_DIE_FRAMES <= TLM_ID_LINK,
              "die-temperature frames collide with the link-health frame");
static_assert(TLM_ID_BAL_BASE + TLM_BAL_FRAMES <= TLM_ID_DIAG,
              "balancing-mask frames collide with the diagnostic frame");

// ---------------------------------------------------------------- enums

/*
 * The five fault sources that exist in the code today. Both a latched and an instantaneous copy
 * are transmitted: the dash lamp follows the instantaneous state while the inverter's 50%
 * throttle limit follows the latched one, so a single-cycle sag half-throttles the car with
 * nothing but a lamp flicker. Logging both is what makes that divergence visible.
 *
 * Only three bits are spare, and a new fault must take bit n in the latched byte and bit n in
 * the instantaneous byte together -- adding to one without the other breaks the correspondence
 * the frame exists to expose.
 */
enum BmsFaultBit : uint8_t {
  BMS_FAULT_CELL_OVERVOLTAGE  = 0,  // >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH
  BMS_FAULT_CELL_UNDERVOLTAGE = 1,  // <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW
  BMS_FAULT_OVERTEMP          = 2,  // > BMS_TEMPERATURE_THRESHOLD, charging only
  BMS_FAULT_STRING_IMBALANCE  = 3,  // > BMS_STRING_DIFFERENCE_THRESHOLD
  BMS_FAULT_PEC               = 4,  // LTC6813 read failed its PEC
};

// Byte 2 of BmsStatus is full. The next status bit needs a new frame at 0x1F000004, which is
// free and costs 27 bytes per emission -- cheaper than overloading a hardware mirror byte.
enum BmsStatusBit : uint8_t {
  BMS_STATUS_DISCHARGING    = 0,
  BMS_STATUS_CHARGE_SWITCH  = 1,
  BMS_STATUS_CHARGE_EN      = 2,
  BMS_STATUS_CONTACTORS     = 3,
  BMS_STATUS_BALANCING      = 4,
  BMS_STATUS_SOC_RESERVE    = 5,
  BMS_STATUS_VCHECK_OK      = 6,
  BMS_STATUS_STRINGCHECK_OK = 7,
};

// Coded rather than free text. Text would need multi-frame reassembly and decodes to nothing
// useful in a DBC tool; a code plus arguments is one frame with a VAL_ table. This still
// satisfies the rule that an error report never depends on not disturbing the data stream,
// because it is its own frame and cannot corrupt a data frame the way the 2021 mid-record
// `ERR:` prints did. Keep in step with the VAL_ table in generate_914_dbc.py.
enum BmsDiagCode : uint8_t {
  BMS_DIAG_NONE            = 0,
  BMS_DIAG_PEC_FAILURE     = 1,
  BMS_DIAG_BMS_FAULT       = 2,
  BMS_DIAG_THREAD_START    = 3,
  BMS_DIAG_INVALID_MESSAGE = 4,
};

// ---------------------------------------------------------------- status frame inputs

struct BmsStatusFields {
  uint8_t  faultLatched;
  uint8_t  faultNow;
  uint8_t  status;
  uint8_t  ioexpOut;    // the literal byte handed to MCP23017::write_mask()
  uint8_t  ioexpIn;     // MCP23017 port B; reserved, zero unless TELEMETRY_READ_GPIO_INPUTS
  uint16_t errCount;
};

// ---------------------------------------------------------------- emission

#if SLCAN_MODE

void telemetry_emit_status(const BmsStatusFields &f);
void telemetry_emit_pack(const batterydata_t &d);
void telemetry_emit_cell_summary(const batterysummary_t &s);
void telemetry_emit_temp_summary(const batterysummary_t &s, int16_t heatsinkTempDegC);

// Cell voltages, thermistors, die temperatures, link health and the balancing mask -- the whole
// slow tier in one call, emitted on the logging scan only.
//
// maxLatencyUs is passed in rather than read here because canRxMaxLatencyUs is a read-and-clear
// maximum with exactly one owner. When both this and the CSV read it, each clears it for the
// other and both report a maximum over part of the interval -- measured on the car in the
// dual-emit build, where the frame said 60775 us and the CSV said 49709 us for the same scan.
// Neither was the true maximum.
void telemetry_emit_slow(const batterydata_t &d, uint32_t maxLatencyUs);

void telemetry_emit_schema();
void telemetry_emit_diag(uint8_t code, uint16_t arg1, uint16_t arg2, uint16_t arg3);

// Echo a frame the VCU is transmitting. Evidence of intent, not proof of arbitration success --
// see the comment on TLM_ID_VCU_CONTROL.
void telemetry_echo_frame(uint32_t id, const uint8_t *data, uint8_t len);

#else

// Compiled out entirely in CSV builds, so the call sites in BmsThread.cpp need no #if around
// them and the two configurations cannot drift apart.
inline void telemetry_emit_status(const BmsStatusFields &) {}
inline void telemetry_emit_pack(const batterydata_t &) {}
inline void telemetry_emit_cell_summary(const batterysummary_t &) {}
inline void telemetry_emit_temp_summary(const batterysummary_t &, int16_t) {}
inline void telemetry_emit_slow(const batterydata_t &, uint32_t) {}
inline void telemetry_emit_schema() {}
inline void telemetry_emit_diag(uint8_t, uint16_t, uint16_t, uint16_t) {}
inline void telemetry_echo_frame(uint32_t, const uint8_t *, uint8_t) {}

#endif // SLCAN_MODE

#endif // TELEMETRY_H

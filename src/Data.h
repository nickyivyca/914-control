#pragma once

#include "config.h"
#include "mbed.h"

#ifndef MSG_QUEUE_SIZE
#define MSG_QUEUE_SIZE 16
#endif

typedef struct {
  uint16_t allVoltages[NUM_STRINGS][NUM_CHIPS * NUM_CELLS_PER_CHIP];
  float allTemperatures[NUM_CHIPS];
  uint8_t dieTemps[NUM_CHIPS];
  int stringCurrents[NUM_STRINGS];
  uint8_t numBalancing;
  // One bit per cell, flat index 0..(NUM_CHIPS*NUM_CELLS_PER_CHIP-1), bit n of byte b being
  // cell b*8+n -- the same indexing as minVoltage_cell and as the wire format, so the bit
  // ordering cannot drift from the voltage ordering.
  //
  // This is *commanded* discharge state, not current flow: discharge is muted around the
  // measurement, which is why the voltage readings are not skewed by it. It is a snapshot of
  // one scan rather than a union across the scans in a logging interval, so that it is
  // coherent with the cell voltages recorded beside it.
  uint8_t balanceMask[(NUM_CHIPS * NUM_CELLS_PER_CHIP + 7) / 8];
  int totalCurrent;
  uint32_t timestamp;
  unsigned int packVoltage;
  int64_t joules;
  uint8_t soc;
} batterydata_t;

typedef struct {
  uint16_t VAC;
  uint16_t IAC;
} chargerdata_t;

typedef struct {
  int16_t heatsinktemp;
} inverterdata_t;

typedef struct {
  uint16_t minVoltage;
  uint8_t minVoltage_cell;
  uint16_t maxVoltage;
  uint8_t maxVoltage_cell;
  // Mean cell voltage in mV, computed the way the live display does it: pack voltage divided
  // by the number of cells *in series*, which is 84 and not 168. The two strings are in
  // parallel. The commented-out display block above the live one divides by 168 and would read
  // half the true value.
  uint16_t avgVoltage;
  float minTemp;
  uint8_t minTemp_box;
  float maxTemp;
  uint8_t maxTemp_box;
  int totalCurrent;
  unsigned int totalVoltage;
  int64_t joules;
  uint32_t timestamp;
  uint8_t soc;
  uint8_t numBalancing;

} batterysummary_t;

typedef struct {
  thread_message msg_event;
} mail_t;

typedef struct {
  batterydata_t batterydata;
  batterysummary_t batterysummary;
  Mutex mutex;
} batterycomm_t;


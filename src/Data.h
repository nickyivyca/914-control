#pragma once

#include "config.h"
#include "mbed.h"

#ifndef MSG_QUEUE_SIZE
#define MSG_QUEUE_SIZE 16
#endif

typedef struct {
  uint16_t allVoltages[NUM_CHIPS * NUM_CELLS_PER_CHIP];
  float allTemperatures[NUM_CHIPS];
  uint8_t dieTemps[NUM_CHIPS];
  uint8_t numBalancing;
  int totalCurrent;
  uint32_t timestamp;
  unsigned int packVoltage;
} batterydata_t;

typedef struct {
  uint16_t minVoltage;
  uint8_t minVoltage_cell;
  uint16_t maxVoltage;
  uint8_t maxVoltage_cell;
  float minTemp;
  uint8_t minTemp_box;
  float maxTemp;
  uint8_t maxTemp_box;
  int totalCurrent;
  unsigned int totalVoltage;
  uint32_t timestamp;

} batterysummary_t;

typedef struct {
  thread_message msg_event;
} mail_t;

typedef struct {
  batterydata_t batterydata;
  batterysummary_t batterysummary;
  Mutex mutex;
} batterycomm_t;


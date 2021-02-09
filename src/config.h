#pragma once

#include "mbed.h"


// Global pointer to serial object
//
// This allows for all files to access the serial output
extern Serial* serial;

// Global pointer to can bus object
//
// This allows for all files to access the can bus output
extern CAN* canBus;


// Number of 6813 chips on isospi bus
#ifndef NUM_CHIPS
#define NUM_CHIPS 2
#endif

#ifndef CELL_SENSE_FREQUENCY
#define CELL_SENSE_FREQUENCY 20
#endif

#ifndef MAIN_PERIOD
#define MAIN_PERIOD 10
#endif

/*#ifndef DEBUGN
#define DEBUGN
#endif*/

// Number of cells per chip
#ifndef NUM_CELLS_PER_CHIP
#define NUM_CELLS_PER_CHIP 14
#endif

const int BMS_CELL_MAP[18] = {0, 1, 2, 3, 4, -1, 5, 6, 7, 8, 9, -1, 10, 11, 12, 13, -1, -1};

enum thread_message {INIT_ALL, NEW_CELL_DATA, // main
  BMS_INIT, BMS_READ, ENABLE_BALANCING, DISABLE_BALANCING, // bms thread
  DATA_INIT, DATA_DATA, DATA_SUMMARY};    // data thread


//
// IO Configuration
//

// Charger output
//
// To be pulled high to enable charger
#ifndef PIN_CHARGER_CONTROL
#define PIN_CHARGER_CONTROL NC
#endif

// Charger output
//
// Controls BMS fault light on dash (and beeper?)
#ifndef PIN_BMS_FAULT_CONTROL
#define PIN_BMS_FAULT_CONTROL NC
#endif

// Throttle input
#ifndef PIN_SIG_THROTTLE
#define PIN_SIG_THROTTLE NC
#endif
// 'Brake' input
#ifndef PIN_SIG_BRAKE
#define PIN_SIG_BRAKE NC
#endif

// Upper threshold when fault will be thrown for cell voltage
//
// Units: millivolts
#ifndef BMS_FAULT_VOLTAGE_THRESHOLD_HIGH
#define BMS_FAULT_VOLTAGE_THRESHOLD_HIGH 4200
#endif

// Lower threshold when fault will be thrown for cell voltage
//
// Units: millivolts
#ifndef BMS_FAULT_VOLTAGE_THRESHOLD_LOW
#define BMS_FAULT_VOLTAGE_THRESHOLD_LOW 3000
#endif

// Threshold when cells will be discharged when discharging is enabled.
//
// Units: millivolts
#ifndef BMS_DISCHARGE_THRESHOLD
#define BMS_DISCHARGE_THRESHOLD 15
#endif


//
// SPI Configuration
//

// SPI master out slave in
#ifndef PIN_6820_SPI_MOSI
#define PIN_6820_SPI_MOSI p5
#endif

// SPI master in slave out
#ifndef PIN_6820_SPI_MISO
#define PIN_6820_SPI_MISO p6
#endif

// SPI clock
#ifndef PIN_6820_SPI_SCLK
#define PIN_6820_SPI_SCLK p7
#endif

// SPI chip select
#ifndef PIN_6820_SPI_SSEL
#define PIN_6820_SPI_SSEL p8
#endif


//
// CAN Configuration
//

// CAN TX pin to transceiver
#ifndef PIN_CAN_TX
#define PIN_CAN_TX p30
#endif

// CAN RX pin from transceiver
#ifndef PIN_CAN_RX
#define PIN_CAN_RX p29
#endif

// CAN frequency to used
// default: 500k
#ifndef CAN_FREQUENCY
#define CAN_FREQUENCY 250000
#endif


#pragma once

#include "mbed.h"


// Global pointer to serial object
//
// This allows for all files to access the serial output
extern Serial* serial;
extern Serial* serial2;
extern Serial* displayserial;


#ifndef PIN_SERIAL2_TX
#define PIN_SERIAL2_TX p9
#endif
#ifndef PIN_SERIAL2_RX
#define PIN_SERIAL2_RX p10
#endif
#ifndef PIN_DISPLAY_TX
#define PIN_DISPLAY_TX p9
#endif
#ifndef PIN_DISPLAY_RX
#define PIN_DISPLAY_RX p10
#endif

// Global pointer to can bus object
//
// This allows for all files to access the can bus output
extern CAN* canBus;


// Number of 6813 chips on isospi bus
#ifndef NUM_CHIPS
#define NUM_CHIPS 6
#endif

// Nominal range of the LEM sensor
#ifndef ISENSE_RANGE
#define ISENSE_RANGE 200.0
#endif

// Chip number for where the LEM is plugged in, 0 indexed
#ifndef ISENSE_LOCATION
#define ISENSE_LOCATION 5
#endif

#ifndef CELL_SENSE_FREQUENCY
#define CELL_SENSE_FREQUENCY 10
#endif

#ifndef MAIN_PERIOD
#define MAIN_PERIOD 10
#endif

// #ifndef TESTBALANCE
// #define TESTBALANCE
// #endif

/*#ifndef DEBUGN
#define DEBUGN
#endif*/

// Number of cells per chip
#ifndef NUM_CELLS_PER_CHIP
#define NUM_CELLS_PER_CHIP 14
#endif

const int BMS_CELL_MAP[18] = {0, 1, 2, 3, 4, -1, 5, 6, 7, 8, 9, -1, 10, 11, 12, 13, -1, -1};

enum thread_message {INIT_ALL, NEW_CELL_DATA, BATT_ERR, // to main
  BMS_INIT, BMS_READ, ENABLE_BALANCING, DISABLE_BALANCING, // to bms thread
  DATA_INIT, DATA_DATA, DATA_SUMMARY, DATA_ERR};    // to data thread

// Value for 100% on the bar on the display
#ifndef DISP_FULL_SCALE
#define DISP_FULL_SCALE 80000
#endif

// Divide out for 20 characters width
#ifndef DISP_PER_BOX
#define DISP_PER_BOX (DISP_FULL_SCALE/20)
#endif


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


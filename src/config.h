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
// Controls BMS fault light on dash
#ifndef PIN_BMS_FAULT_CONTROL
#define PIN_BMS_FAULT_CONTROL NC
#endif



// Contactor outputs
//
//
#ifndef PIN_CONTACTOR_1_CONTROL
#define PIN_CONTACTOR_1_CONTROL NC
#endif
#ifndef PIN_CONTACTOR_2_CONTROL
#define PIN_CONTACTOR_2_CONTROL NC
#endif
#ifndef PIN_CONTACTOR_3_CONTROL
#define PIN_CONTACTOR_3_CONTROL NC
#endif

// Throttle input
#ifndef PIN_SIG_THROTTLE
#define PIN_SIG_THROTTLE NC
#endif
// 'Brake' input
#ifndef PIN_SIG_BRAKE
#define PIN_SIG_BRAKE NC
#endif

// Current input
//
// Input from analog current sensors
#ifndef PIN_SIG_CURRENT_1
#define PIN_SIG_CURRENT_1 NC
#endif
#ifndef PIN_SIG_CURRENT_2
#define PIN_SIG_CURRENT_1 NC
#endif
#ifndef PIN_SIG_CURRENT_3
#define PIN_SIG_CURRENT_1 NC
#endif


//
// SPI Configuration
//

// SPI master out slave in
#ifndef PIN_6820_SPI_MOSI
#define PIN_SPI_MOSI p5
#endif

// SPI master in slave out
#ifndef PIN_6820_SPI_MISO
#define PIN_SPI_MISO p6
#endif

// SPI clock
#ifndef PIN_6820_SPI_SCLK
#define PIN_SPI_SCLK p7
#endif

// SPI chip select
#ifndef PIN_6820_SPI_SSEL
#define PIN_SPI_SSEL p8
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


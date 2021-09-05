#pragma once

#include "mbed.h"

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




// Global pointer to serial object
//
// This allows for all files to access the serial output
extern Serial* serial;
extern Serial* displayserial;



// Global pointer to can bus object
//
// This allows for all files to access the can bus output
extern CAN* canBus;

#ifndef PIN_DO_BATTCONTACTOR
#define PIN_DO_BATTCONTACTOR p26
#endif

#ifndef PIN_DO_BATTCONTACTOR2
#define PIN_DO_BATTCONTACTOR2 p25
#endif

#ifndef PIN_DO_DRIVEENABLE
#define PIN_DO_DRIVEENABLE p24
#endif

#ifndef PIN_DO_CHARGEENABLE
#define PIN_DO_CHARGEENABLE p23
#endif

#ifndef PIN_DI_CHARGESWITCH
#define PIN_DI_CHARGESWITCH p17
#endif

extern DigitalOut* led1;
extern DigitalOut* led2;
extern DigitalOut* led3;
extern DigitalOut* led4;

extern DigitalOut* DO_BattContactor;
extern DigitalOut* DO_BattContactor2;
extern DigitalOut* DO_DriveEnable;
extern DigitalOut* DO_ChargeEnable;

extern DigitalIn* DI_ChargeSwitch;
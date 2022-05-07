#pragma once

#include "mbed.h"
#include "MCP23017.h"

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
// I2C Configuration
//

// I2C SDA
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA p28
#endif

// I2C SCL
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL p27
#endif

#ifndef MCP_ADDRESS
#define MCP_ADDRESS (0x20 << 1)
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
extern BufferedSerial* serial;
extern BufferedSerial* displayserial;



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

#ifndef PIN_PWM_TACH
#define PIN_PWM_TACH p22
#endif

#ifndef PIN_PWM_FUELGAUGE
#define PIN_PWM_FUELGAUGE p21
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

extern MCP23017* ioexp;

extern PwmOut* fuelgauge;
extern PwmOut* tach;


#ifndef MCP_PIN_BIT
#define MCP_PIN_BIT(x) (1 << x)
#endif

#ifndef MCP_PIN_BMSERR
#define MCP_PIN_BMSERR 7
#endif

#ifndef MCP_PIN_EGR
#define MCP_PIN_EGR 4
#endif

#ifndef MCP_PIN_G // Light labeled G on leftmost gauge
#define MCP_PIN_G 0
#endif

#ifndef MCP_PIN_BIGB // Big light labeled B on leftmost gauge
#define MCP_PIN_BIGB 5
#endif

#ifndef MCP_PIN_LOWFUEL
#define MCP_PIN_LOWFUEL 3
#endif

#ifndef MCP_BMS_THREAD_MASK
#define MCP_BMS_THREAD_MASK MCP_PIN_BIT(MCP_PIN_LOWFUEL) | MCP_PIN_BIT(MCP_PIN_BMSERR) | MCP_PIN_BIT(MCP_PIN_EGR)
#endif

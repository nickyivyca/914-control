#pragma once

#include "mbed.h"
#include "MCP23017.h"
#include "IsrSafeCAN.h"

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
#define PIN_CAN_TX p29
#endif

// CAN RX pin from transceiver
#ifndef PIN_CAN_RX
#define PIN_CAN_RX p30
#endif

// CAN frequency to used
// default: 500k
#ifndef CAN_FREQUENCY
#define CAN_FREQUENCY 500000
#endif




// Global pointer to serial object
//
// This allows for all files to access the serial output
extern UnbufferedSerial* serial;
extern BufferedSerial* displayserial;



// Global pointer to can bus object
//
// This allows for all files to access the can bus output
extern IsrSafeCAN* canBus;

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

// Brake switch input
#ifndef PIN_DI_BRAKESWITCH
#define PIN_DI_BRAKESWITCH p18
#endif

// Reverse light input
#ifndef PIN_DI_REVERSESWITCH
#define PIN_DI_REVERSESWITCH p19
#endif

#ifndef PIN_DO_TACH
#define PIN_DO_TACH p22
#endif

#ifndef PIN_PWM_FUELGAUGE
#define PIN_PWM_FUELGAUGE p21
#endif

#ifndef PIN_ANALOG_KNOB1
#define PIN_ANALOG_KNOB1 p15
#endif

#ifndef PIN_ANALOG_KNOB2
#define PIN_ANALOG_KNOB2 p16
#endif

// p15, p16 and p20 are the only analog pins free on this board. The LPC1768 exposes six
// (p15-p20) and p17/p18/p19 are already the charge, brake and reverse digital inputs, so the
// three dash knobs can only be on these three. Which physical knob is on which of them is a
// separate question, and one the bring-up capture answers.
#ifndef PIN_ANALOG_KNOB3
#define PIN_ANALOG_KNOB3 p20
#endif

extern DigitalOut* led1;
extern DigitalOut* led2;
extern DigitalOut* led3;
extern DigitalOut* led4;

extern DigitalOut* DO_Tach;

extern DigitalOut* DO_BattContactor;
extern DigitalOut* DO_BattContactor2;
extern DigitalOut* DO_DriveEnable;
extern DigitalOut* DO_ChargeEnable;

extern DigitalIn* DI_ChargeSwitch;
extern DigitalIn* DI_BrakeSwitch;
extern DigitalIn* DI_ReverseSwitch;

extern AnalogIn* knob1;
extern AnalogIn* knob2;
extern AnalogIn* knob3;

extern MCP23017* ioexp;

extern PwmOut* fuelgauge;


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

// Parenthesised: this is mirrored into byte 4 of the BmsStatus telemetry frame, where it is
// used in an expression rather than passed straight to write_mask(), and an unparenthesised
// chain of `|` binds wrong there.
#ifndef MCP_BMS_THREAD_MASK
#define MCP_BMS_THREAD_MASK (MCP_PIN_BIT(MCP_PIN_LOWFUEL) | MCP_PIN_BIT(MCP_PIN_BMSERR) | MCP_PIN_BIT(MCP_PIN_EGR) | MCP_PIN_BIT(MCP_PIN_G))
#endif

//
// MCP23017 input side, port B. Byte 7 of the BmsStatus frame carries these, and
// TELEMETRY_READ_GPIO_INPUTS (or TELEMETRY_EMIT_KNOBS) is what turns the read on.
//
// Both switch assignments below were confirmed on the car 2026-08-23 by toggling each one and
// watching which bit moved -- they are measurements, not the guesses the names imply:
//
//   pin 11 -> the MIDDLE knob's switch (p15), which selects CC vs CV termination
//   pin 12 -> the RIGHT knob's switch (p16), currently unmapped
//
// Polarity is inverted from the pull-to-ground reading the pullups suggest: pushed IN reads 1,
// pulled OUT reads 0. See KNOB_SW_PUSHED_IN in config.h.
//
// Pins 13-15 are configured as inputs but have no pullup and nothing connected. They float, and
// one of them was seen toggling on its own during a capture. Do not read meaning into them.
//
#ifndef MCP_PIN_KNOB1SW
#define MCP_PIN_KNOB1SW 11
#endif

#ifndef MCP_PIN_KNOB2SW
#define MCP_PIN_KNOB2SW 12
#endif

#ifndef MCP_BMS_THREAD_READ_MASK
#define MCP_BMS_THREAD_READ_MASK (MCP_PIN_BIT(MCP_PIN_KNOB1SW) | MCP_PIN_BIT(MCP_PIN_KNOB2SW))
#endif

// Every pin the MCP23017 is actually configured to read, which is wider than the two the knob
// switches are believed to be on. Main.cpp calls config(0b1111100000000000, ...), making pins
// 11-15 inputs, with the 100k pullups enabled on 11 and 12 only.
//
// Bring-up reads all five rather than the two, because "the switch is not on the pin we expected"
// and "the switch is on the pin we expected and open" are the same reading through the narrow
// mask. Pins 13-15 have no pullup and will float, so treat them as suspect unless they track a
// switch cleanly.
#ifndef MCP_BMS_THREAD_READ_MASK_ALL
#define MCP_BMS_THREAD_READ_MASK_ALL 0xF800
#endif

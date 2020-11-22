#pragma once

#include "LTC681xBus.h"

#include "config.h"

class LTC6813 {
 public:
  enum class GPIOOutputState : uint8_t {
    // States with pull up resistor
    kLow = 0,
    kHigh = 1,
    // States with no pull up (inputs)
    kPullDown = 0,
    kPassive = 1
  };
  enum class ReferencePowerOff : uint8_t {
    kAfterConversions = 0,
    kWatchdogTimeout = 1
  };
  enum class DischargeTimerEnable : uint8_t { kDisabled = 0, kEnabled = 1 };
  enum class AdcModeOption : uint8_t { kDefault = 0, kLowSpeed = 1 };
  union DischargeState {
    struct {
      uint8_t cell18 : 1, cell17 : 1, cell16 : 1, cell15 : 1, cell14 : 1,
          cell13 : 1, cell12 : 1, cell11 : 1, cell10 : 1, cell9 : 1,
          cell8 : 1, cell7 : 1, cell6 : 1, cell5 : 1, cell4 : 1, cell3 : 1,
          cell2 : 1, cell1 : 1;
    };
    uint32_t value;
  };
  // Discharge Timeout in minutes
  enum class DischargeTimeoutValue : uint8_t {
    kDisabled = 0,
    k0_5,
    k1,
    k2,
    k3,
    k4,
    k5,
    k10,
    k15,
    k20,
    k30,
    k40,
    k60,
    k75,
    k90,
    k120
  };

  class Configuration {
   public:
    GPIOOutputState gpio9;
    GPIOOutputState gpio8;
    GPIOOutputState gpio7;
    GPIOOutputState gpio6;
    GPIOOutputState gpio5;
    GPIOOutputState gpio4;
    GPIOOutputState gpio3;
    GPIOOutputState gpio2;
    GPIOOutputState gpio1;
    ReferencePowerOff referencePowerOff;
    DischargeTimerEnable dischargeTimerEnabled;
    AdcModeOption adcMode;
    // Voltage = (val + 1) • 16 • 100μV
    uint16_t undervoltageComparison : 12;
    uint16_t overvoltageComparison : 12;
    DischargeState dischargeState;
    DischargeTimeoutValue dischargeTimeout;
    uint8_t forceDigitalRedundancyFailure : 1;
    uint8_t redundancyPathSelection : 2;
    uint8_t enableDischargeTimerMonitor : 1;
    uint8_t DCCGPIO9PullDownEnable : 1;
  };

  LTC6813(LTC681xBus &bus);
  Configuration &getConfig();
  void updateConfig();
  void readConfig();

  void getVoltages(uint16_t voltages[NUM_CHIPS][18]);
  uint16_t *getGpio();
  uint16_t *getGpioPin(GpioSelection pin);

 private:
  LTC681xBus &m_bus;
  Configuration m_config;
};

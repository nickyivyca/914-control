#include "LTC6813.h"

#include "mbed.h"
#include "rtos.h"

#include "LTC681xBus.h"

#include "config.h"

LTC6813::LTC6813(LTC681xBus &bus) : m_bus(bus) {
  m_config =
    Configuration{.gpio9 = GPIOOutputState::kPassive,
                  .gpio8 = GPIOOutputState::kPassive,
                  .gpio7 = GPIOOutputState::kPassive,
                  .gpio6 = GPIOOutputState::kPassive,
                  .gpio5 = GPIOOutputState::kPassive,
                  .gpio4 = GPIOOutputState::kPassive,
                  .gpio3 = GPIOOutputState::kPassive,
                  .gpio2 = GPIOOutputState::kPassive,
                  .gpio1 = GPIOOutputState::kPassive,
                  .referencePowerOff = ReferencePowerOff::kAfterConversions,
                  .dischargeTimerEnabled = DischargeTimerEnable::kDisabled,
                  .adcMode = AdcModeOption::kDefault,
                  .undervoltageComparison = 0,
                  .overvoltageComparison = 0,
                  .dischargeState = {.value = 0},
                  .dischargeTimeout = DischargeTimeoutValue::kDisabled};
}

void LTC6813::updateConfig() {
  // Create configuration data to write
  uint8_t config[6];
  config[0] = (uint8_t)m_config.gpio5 << 7
    | (uint8_t)m_config.gpio4 << 6
    | (uint8_t)m_config.gpio3 << 5
    | (uint8_t)m_config.gpio2 << 4
    | (uint8_t)m_config.gpio1 << 3
    | (uint8_t)m_config.referencePowerOff << 2
    | (uint8_t)m_config.dischargeTimerEnabled << 1
    | (uint8_t)m_config.adcMode;
  config[1] = m_config.undervoltageComparison & 0xFF;
  config[2] = ((m_config.undervoltageComparison >> 8) & 0x0F)
    | (((uint8_t)m_config.overvoltageComparison & 0x0F) << 4);
  config[3] = (m_config.overvoltageComparison >> 4) & 0xFF;
  config[4] = m_config.dischargeState.value & 0xFF;
  config[5] = (((uint8_t)m_config.dischargeTimeout & 0x0F) << 4)
    | ((m_config.dischargeState.value >> 8) & 0x0F);

  LTC681xBus::Command cmd = LTC681xBus::buildBroadcastCommand(WriteConfigurationGroupA());
  m_bus.sendCommandWithData(cmd, config);

  config[0] = ((m_config.dischargeState.value >> 8) & 0xF0)
    | (uint8_t)m_config.gpio9 << 3
    | (uint8_t)m_config.gpio8 << 2
    | (uint8_t)m_config.gpio7 << 1
    | (uint8_t)m_config.gpio6;
  config[1] = m_config.forceDigitalRedundancyFailure << 6 // MSB taken up by read only mute bit
    | m_config.redundancyPathSelection << 4
    | m_config.enableDischargeTimerMonitor << 3
    | m_config.DCCGPIO9PullDownEnable << 2
    | ((m_config.dischargeState.value >> 16) & 0x03);
  // configgroupb[2-5] are read only

  cmd = LTC681xBus::buildBroadcastCommand(WriteConfigurationGroupB());

  m_bus.sendCommandWithData(cmd, config);
}

LTC6813::Configuration &LTC6813::getConfig() { return m_config; }

void LTC6813::getVoltages(uint16_t voltages[NUM_CHIPS][18]) {
  auto cmd = StartCellVoltageADC(AdcMode::k7k, false, CellSelection::kAll);
  m_bus.sendCommand(LTC681xBus::buildBroadcastCommand(cmd));

  // Wait 2 ms for ADC to finish
  ThisThread::sleep_for(2); // TODO: Change. Use adc status polling?

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[6][NUM_CHIPS][8];

  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupA()), 
    (uint8_t**)rxbuf[0]);
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupB()), 
    (uint8_t**)rxbuf[1]);
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupC()), 
    (uint8_t**)rxbuf[2]);
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupD()), 
    (uint8_t**)rxbuf[3]);
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupE()), 
    (uint8_t**)rxbuf[4]);
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupF()), 
    (uint8_t**)rxbuf[5]);

  // Voltage = val • 100μV
  uint8_t cellCursor = 0;

  for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
    for (unsigned int j = 0; j < 6; j++) { // iterate through each cell voltage group
      for (unsigned int i = 0; i < 6; i+= 2) {
        voltages[k][cellCursor] = ((uint16_t)rxbuf[j][k][i]) | ((uint16_t)rxbuf[j][k][i + 1] << 8);
        cellCursor++;
      }
    }
    cellCursor = 0;
  }

  //return voltages;
}

uint16_t *LTC6813::getGpio() {
  // TODO: update this based on making getVoltages work
  auto cmd = StartGpioADC(AdcMode::k7k, GpioSelection::kAll);
  m_bus.sendCommand(LTC681xBus::buildBroadcastCommand(cmd));

  // Wait 15 ms for ADC to finish
  ThisThread::sleep_for(5); // TODO: This could be done differently

  uint8_t rxbuf[8 * 2];

  //m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupA()), rxbuf);
  //m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupB()), rxbuf + 8);

  uint16_t *voltages = new uint16_t[5];

  for (unsigned int i = 0; i < sizeof(rxbuf); i++) {
    // Skip over PEC
    if (i % 8 == 6 || i % 8 == 7) continue;

    // Skip over odd bytes
    if (i % 2 == 1) continue;

    // Wack shit to skip over PEC
    voltages[(i / 2) - (i / 8)] = ((uint16_t)rxbuf[i]) | ((uint16_t)rxbuf[i + 1] << 8);
  }

  return voltages;
}

uint16_t *LTC6813::getGpioPin(GpioSelection pin) {
  auto cmd = StartGpioADC(AdcMode::k7k, pin);
  m_bus.sendCommand(LTC681xBus::buildBroadcastCommand(cmd));

  // Wait 5 ms for ADC to finish
  ThisThread::sleep_for(pin == GpioSelection::kAll ? 15 : 5); // TODO: Change to polling

  uint8_t rxbuf[8 * 2];

  //m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupA()), rxbuf);
  //m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupB()), rxbuf + 8);

  uint16_t *voltages = new uint16_t[5];

  for (unsigned int i = 0; i < sizeof(rxbuf); i++) {
    // Skip over PEC
    if (i % 8 == 6 || i % 8 == 7) continue;

    // Skip over odd bytes
    if (i % 2 == 1) continue;

    // Wack shit to skip over PEC
    voltages[(i / 2) - (i / 8)] = ((uint16_t)rxbuf[i]) | ((uint16_t)rxbuf[i + 1] << 8);
  }

  return voltages;
}

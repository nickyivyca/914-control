#include "LTC6813.h"

#include "mbed.h"
#include "rtos.h"

#include "LTC681xBus.h"

LTC6813::LTC6813(LTC681xBus &bus, uint8_t id) : m_bus(bus) {
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

  LTC681xBus::Command cmd = LTC681xBus::buildBroadcastCommand(WriteConfigurationGroupB());

  m_bus.sendCommandWithData(cmd, config);
}

LTC6813::Configuration &LTC6813::getConfig() { return m_config; }
// TODO: read back config, reading mute bit

uint16_t *LTC6813::getVoltages() {
  // TODO: figure out how a broadcast read works
  auto cmd = StartCellVoltageADC(AdcMode::k7k, false, CellSelection::kAll);
  m_bus.sendCommand(LTC681xBus::buildAddressedCommand(m_id, cmd));

  // Wait 2 ms for ADC to finish
  ThisThread::sleep_for(2); // TODO: Change

  // 4  * (Register of 6 Bytes + PEC)
  uint8_t rxbuf[8 * 4];

  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadCellVoltageGroupA()), rxbuf);
  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadCellVoltageGroupB()), rxbuf + 8);
  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadCellVoltageGroupC()), rxbuf + 16);
  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadCellVoltageGroupD()), rxbuf + 24);

  // Voltage = val • 100μV
  uint16_t *voltages = new uint16_t[12];
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

uint16_t *LTC6813::getGpio() {
  auto cmd = StartGpioADC(AdcMode::k7k, GpioSelection::kAll);
  m_bus.sendCommand(LTC681xBus::buildAddressedCommand(m_id, cmd));

  // Wait 15 ms for ADC to finish
  ThisThread::sleep_for(5); // TODO: This could be done differently

  uint8_t rxbuf[8 * 2];

  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupA()), rxbuf);
  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupB()), rxbuf + 8);

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
  m_bus.sendCommand(LTC681xBus::buildAddressedCommand(m_id, cmd));

  // Wait 5 ms for ADC to finish
  ThisThread::sleep_for(pin == GpioSelection::kAll ? 15 : 5); // TODO: Change to polling

  uint8_t rxbuf[8 * 2];

  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupA()), rxbuf);
  m_bus.readCommand(LTC681xBus::buildAddressedCommand(m_id, ReadAuxiliaryGroupB()), rxbuf + 8);

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

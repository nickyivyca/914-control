#include "LTC6813.h"

#include "mbed.h"
#include "rtos.h"

#include "LTC681xBus.h"

#include "config.h"
#include <bitset>
#include <iostream>

#include <DigitalOut.h>

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
/*#ifdef DEBUGN
    serial->printf("Updating config\n");
#endif*/
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
/*#ifdef DEBUGN
    serial->printf("Sent config A\n");
    for (unsigned int i = 0; i < 6; i++) {
      std::bitset<8> c(config[i]);
      std::cout << c << '\n';
    }
#endif*/

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
/*#ifdef DEBUGN
    serial->printf("Sent config B\n");
    for (unsigned int i = 0; i < 6; i++) {
      std::bitset<8> c(config[i]);
      std::cout << c << '\n';
    }
#endif*/
}

LTC6813::Configuration &LTC6813::getConfig() { return m_config; }

void LTC6813::readConfig() {

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[NUM_CHIPS][8];

  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadConfigurationGroupA()), 
    rxbuf);

  std::cout << "Reading cfg A: \n";
  for (int i = 0; i < 6; i++) {
    std::bitset<8> c(rxbuf[0][i]);
    std::cout << "Byte: " << i << ' ' << c << '\n';
    //serial->printf("Byte: %d: 0x%x\r\n", i, data[i]);
  }

  //return voltages;
}

void LTC6813::getVoltages(uint16_t voltages[NUM_CHIPS][18]) {
  Timer t;
  t.start();
  m_bus.sendCommandNoRelease(LTC681xBus::buildBroadcastCommand
    (StartCellVoltageADC(AdcMode::k7k, false, CellSelection::kAll)));
  //m_bus.sendCommandNoRelease(LTC681xBus::buildBroadcastCommand(PollADC()));

  /* 6813 datasheet Page 58
  If the bottom device communicates in isoSPI mode,
isoSPI data pulses are sent to the device to update the 
conversion status. Using LTC6820, this can be achieved
by just clocking its SCK pin. The conversion status is
valid only after the bottom LTC6813-1 device receives N
isoSPI data pulses and the status gets updated for every
isoSPI data pulse that follows. The device returns a low
data pulse if any of the devices in the stack is busy performing 
conversions and returns a high data pulse if all
the devices are free.*/
  static constexpr char allones = 0xff;
  m_bus.m_spiDriver->write(&allones, 1, NULL, 0);
  m_bus.m_spiDriver->write(&allones, 1, NULL, 0);
  //serial->printf("Wrote 3x allones, checking now\n");
  char checkbuf = 0;
  for (unsigned int i = 0; i < 200; i++) {
    m_bus.m_spiDriver->write(&allones, 1, &checkbuf, 1);
    //std::bitset<8> c(checkbuf);    
    //std::cout << i << ": " << c << '\n';
    if (checkbuf) {
      break;
    }
  }
  m_bus.releaseSpi();
  t.stop();

  //std::cout << t.read_ms() << '\n';
  std::cout << t.read_us() << '\n';

  // Wait 2 ms for ADC to finish
  ThisThread::sleep_for(10); // TODO: make the above polling actually work

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[6][NUM_CHIPS][8];

  m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupA()), 
    rxbuf[0]);
  ThisThread::sleep_for(1);
  //m_bus.releaseSpi();
  m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupB()), 
    rxbuf[1]);
  ThisThread::sleep_for(1);
  m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupC()), 
    rxbuf[2]);
  ThisThread::sleep_for(1);
  //m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupD()), 
    rxbuf[3]);
  ThisThread::sleep_for(1);
  //m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupE()), 
    rxbuf[4]);
  ThisThread::sleep_for(1);
  m_bus.wakeupChainSpi();
  m_bus.readWholeChainCommand(LTC681xBus::buildBroadcastCommand(ReadCellVoltageGroupF()), 
    rxbuf[5]);
  //ThisThread::sleep_for(1);

  // Voltage = val • 100μV
  uint8_t cellCursor = 0;

  for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
    for (unsigned int j = 0; j < 6; j++) { // iterate through each cell voltage group
      for (unsigned int i = 0; i < 6; i+= 2) {
        voltages[k][cellCursor] = ((uint16_t)rxbuf[j][k][i]) | ((uint16_t)rxbuf[j][k][i + 1] << 8);
        //std::cout << (int)cellCursor << ": " << voltages[k][cellCursor] << '\n';
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

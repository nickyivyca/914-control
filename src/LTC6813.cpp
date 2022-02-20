#include "LTC6813.h"

#include "mbed.h"
#include "rtos.h"

#include "LTC681xBus.h"

#include "config.h"
#include <bitset>
#include <iostream>

#include <DigitalOut.h>

LTC6813::LTC6813() {
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

LTC6813::Configuration &LTC6813::getConfig() { return m_config; }



LTC6813Bus::LTC6813Bus(LTC681xBus::LTC681xChainBus<N_chips> &bus) : m_bus(bus) {};

void LTC6813Bus::updateConfig() {
/*#ifdef DEBUGN
    serial->printf("Updating config\n");
#endif*/
  // Create configuration data to write
  uint8_t configA[NUM_CHIPS][6];
  uint8_t configB[NUM_CHIPS][6];
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    LTC6813::Configuration& conf = m_chips[i].getConfig();
    configA[i][0] = (uint8_t)conf.gpio5 << 7
      | (uint8_t)conf.gpio4 << 6
      | (uint8_t)conf.gpio3 << 5
      | (uint8_t)conf.gpio2 << 4
      | (uint8_t)conf.gpio1 << 3
      | (uint8_t)conf.referencePowerOff << 2
      | (uint8_t)conf.dischargeTimerEnabled << 1
      | (uint8_t)conf.adcMode;
    configA[i][1] = conf.undervoltageComparison & 0xFF;
    configA[i][2] = ((conf.undervoltageComparison >> 8) & 0x0F)
      | (((uint8_t)conf.overvoltageComparison & 0x0F) << 4);
    configA[i][3] = (conf.overvoltageComparison >> 4) & 0xFF;
    configA[i][4] = conf.dischargeState.value & 0xFF;
    configA[i][5] = (((uint8_t)conf.dischargeTimeout & 0x0F) << 4)
      | ((conf.dischargeState.value >> 8) & 0x0F);

  configB[i][0] = ((conf.dischargeState.value >> 8) & 0xF0)
    | (uint8_t)conf.gpio9 << 3
    | (uint8_t)conf.gpio8 << 2
    | (uint8_t)conf.gpio7 << 1
    | (uint8_t)conf.gpio6;
  configB[i][1] = conf.forceDigitalRedundancyFailure << 6 // MSB taken up by read only mute bit
    | conf.redundancyPathSelection << 4
    | conf.enableDischargeTimerMonitor << 3
    | conf.DCCGPIO9PullDownEnable << 2
    | ((conf.dischargeState.value >> 16) & 0x03);
  // configgroupb[2-5] are read only
  }
/*#ifdef DEBUGN
    serial->printf("Sent config A\n");
    for (unsigned int i = 0; i < 6; i++) {
      std::bitset<8> c(config[i]);
      std::cout << c << '\n';
    }
#endif*/

  LTC681xBus::BusCommand cmd = LTC681xBus::BuildChainBusCommand(WriteConfigurationGroupA());
  m_bus.SendDataCommand(cmd, configA);

  cmd = LTC681xBus::BuildChainBusCommand(WriteConfigurationGroupB());
  m_bus.SendDataCommand(cmd, configB);
/*#ifdef DEBUGN
    serial->printf("Sent config B\n");
    for (unsigned int i = 0; i < 6; i++) {
      std::bitset<8> c(config[i]);
      std::cout << c << '\n';
    }
#endif*/
}

void LTC6813Bus::readConfig() {

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[NUM_CHIPS][8];

  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadConfigurationGroupA()), 
    rxbuf);

  std::cout << "Reading cfg A: \n";
  for (int i = 0; i < 6; i++) {
    std::bitset<8> c(rxbuf[0][i]);
    std::cout << "Byte: " << i << ' ' << c << '\n';
    //serial->printf("Byte: %d: 0x%x\r\n", i, data[i]);
  }

  //return voltages;
}

void LTC6813Bus::getVoltages(uint16_t voltages[NUM_CHIPS][18]) {
  //Timer t;
  //t.start();
  m_bus.SendCommandAndPoll(LTC681xBus::BuildChainBusCommand
    (StartCellVoltageADC(AdcMode::k7k, false, CellSelection::kAll)));
  //t.stop();

  //std::cout << t.read_ms() << '\n';
  //std::cout << "Voltage: " << t.read_us() << '\n';

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[6][NUM_CHIPS][8];

  //m_bus.wakeupChainSpi();
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupA()), 
    rxbuf[0]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupB()), 
    rxbuf[1]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupC()), 
    rxbuf[2]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupD()), 
    rxbuf[3]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupE()), 
    rxbuf[4]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupF()), 
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
}

void LTC6813Bus::getGpio(uint16_t voltages[NUM_CHIPS][9]) {
  //Timer t;
  //t.start();
  m_bus.SendCommandAndPoll(LTC681xBus::BuildChainBusCommand
    (StartGpioADC(AdcMode::k7k, GpioSelection::kAll)));
  //t.stop();
  //std::cout << t.read_us() << "us" << '\n';
  //ThisThread::sleep_for(20);

  // [4 aux voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[4][NUM_CHIPS][8];

  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadAuxiliaryGroupA()), 
    rxbuf[0]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadAuxiliaryGroupB()), 
    rxbuf[1]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadAuxiliaryGroupC()), 
    rxbuf[2]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadAuxiliaryGroupD()), 
    rxbuf[3]);

  uint8_t measCursor = 0;

  for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
    for (unsigned int j = 0; j < 4; j++) { // iterate through each measurement voltage group
      for (unsigned int i = 0; i < 6; i+= 2) {
        voltages[k][measCursor] = ((uint16_t)rxbuf[j][k][i]) | ((uint16_t)rxbuf[j][k][i + 1] << 8);
        //std::cout << (int)measCursor << ": " << voltages[k][measCursor] << '\n';
        measCursor++;
      }
    }
    measCursor = 0;
  }
}

uint8_t LTC6813Bus::getCombined(uint16_t cellVoltages[NUM_CHIPS][18], uint16_t adcVoltages[NUM_CHIPS][2]) {
  //Timer t;
  //t.start();
  m_bus.SendCommandAndPoll(LTC681xBus::BuildChainBusCommand
    (StartCombinedCellVoltageGpioConversion(AdcMode::k7k, false)));
  //t.stop();

  //std::cout << t.read_ms() << '\n';
  //std::cout << "Voltage: " << t.read_us() << '\n';

  // [6 voltage groups][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[7][NUM_CHIPS][8];

  //m_bus.wakeupChainSpi();
  uint8_t pecStatuses = 0;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupA()), 
    rxbuf[0]);
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupB()), 
    rxbuf[1])<<1;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupC()), 
    rxbuf[2])<<2;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupD()), 
    rxbuf[3])<<3;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupE()), 
    rxbuf[4])<<4;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadCellVoltageGroupF()), 
    rxbuf[5])<<5;
  pecStatuses |= m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadAuxiliaryGroupA()), 
    rxbuf[6])<<6;
  //ThisThread::sleep_for(1);

  // Voltage = val • 100μV
  uint8_t measCursor = 0;
  if (!pecStatuses) {
    for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
      for (unsigned int j = 0; j < 6; j++) { // iterate through each cell voltage group
        for (unsigned int i = 0; i < 6; i+= 2) {
          cellVoltages[k][measCursor] = ((uint16_t)rxbuf[j][k][i]) | ((uint16_t)rxbuf[j][k][i + 1] << 8);
          //std::cout << (int)cellCursor << ": " << voltages[k][cellCursor] << '\n';
          measCursor++;
        }
      }
      measCursor = 0;
    // Register A has 3 GPIOs of data but we only want the first 2
      for (unsigned int i = 0; i < 4; i+= 2) {
        adcVoltages[k][measCursor] = ((uint16_t)rxbuf[6][k][i]) | ((uint16_t)rxbuf[6][k][i + 1] << 8);
        //std::cout << (int)measCursor << ": " << voltages[k][measCursor] << '\n';
        measCursor++;
      }
      measCursor = 0;
    }
  } 
  return pecStatuses;
}

void LTC6813Bus::getStatus(LTC6813::Status statuses[NUM_CHIPS]) {
  //Timer t;
  //t.start();
  m_bus.SendCommandAndPoll(LTC681xBus::BuildChainBusCommand
    (StartStatusGroupConversion(AdcMode::k7k, StatusGroupSelection::kAll)));

  // [2 status registers][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[2][NUM_CHIPS][8];

  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadStatusGroupA()), 
    rxbuf[0]);
  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadStatusGroupB()), 
    rxbuf[1]);

  for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
    uint16_t raw_sum = rxbuf[0][k][0] | (rxbuf[0][k][1] << 8);
    statuses[k].sumAllCells = raw_sum * 30.0 / 10.0;
    uint16_t raw_temp = rxbuf[0][k][2] | (rxbuf[0][k][3] << 8);
    statuses[k].internalTemperature = raw_temp * (0.0001)/(0.0076) - 276.0;

    uint16_t raw_va = rxbuf[0][k][4] | (rxbuf[0][k][5] << 8);
    statuses[k].voltageAnalog = raw_va / 10.0;
    uint16_t raw_vd = rxbuf[1][k][0] | (rxbuf[1][k][1] << 8);
    statuses[k].voltageDigital = raw_vd / 10.0;

    statuses[k].thermalShutDown = rxbuf[1][k][5] & 1;
  }
}
void LTC6813Bus::getDieTemps(uint8_t dieTemps[NUM_CHIPS]) {
  //Timer t;
  //t.start();
  m_bus.SendCommandAndPoll(LTC681xBus::BuildChainBusCommand
    (StartStatusGroupConversion(AdcMode::k7k, StatusGroupSelection::kITMP)));

  // [2 status registers][each chip in chain][Register of 6 Bytes + PEC]
  uint8_t rxbuf[NUM_CHIPS][8];

  m_bus.SendReadCommand(LTC681xBus::BuildChainBusCommand(ReadStatusGroupA()), 
    rxbuf);

  for (unsigned int k = 0; k < NUM_CHIPS; k++) { // iterate over each chip's worth of data
    uint16_t raw_temp = rxbuf[k][2] | (rxbuf[k][3] << 8);
    dieTemps[k] = (uint8_t)(raw_temp * (0.0001)/(0.0076) - 276.0);
  }
}

void LTC6813Bus::muteDischarge() {
  m_bus.sendCommand(LTC681xBus::BuildChainBusCommand(MuteDischarge()));
}
void LTC6813Bus::unmuteDischarge() {
  m_bus.sendCommand(LTC681xBus::BuildChainBusCommand(UnmuteDischarge()));
}
/*uint16_t *LTC6813::getGpioPin(GpioSelection pin) {
  auto cmd = StartGpioADC(AdcMode::k7k, pin);
  m_bus.sendCommand(LTC681xBus::BuildChainBusCommand(cmd));

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
};*/


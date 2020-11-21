#include "LTC681xBus.h"

// TODO: stdout should not be in this header
#include "config.h"

#include "mbed.h"

constexpr uint16_t LTC681xBus::crc15Table[256];

//DigitalOut chipselect(p8);

LTC681xBus::Command LTC681xBus::buildAddressedCommand(uint8_t addr, LTC681xCommand const &cmd) {
  Command out;

  out.addrMode = (uint8_t)0x01;
  out.address = addr;
  out.command = cmd.toValue();

  return out;
}

LTC681xBus::Command LTC681xBus::buildBroadcastCommand(LTC681xCommand const &cmd) {
  Command out;

  out.addrMode = 0x00;
  out.address = 0x00;
  out.command = cmd.toValue();

  return out;
}

LTC681xBus::LTC681xBus(SPI *spiDriver) : m_spiDriver(spiDriver) {
  //chipselect = 1;
}

void LTC681xBus::acquireSpi() {
  m_spiDriver->select();
  //chipselect = 0;
}

void LTC681xBus::releaseSpi() {
  m_spiDriver->deselect();
  //chipselect = 1;
}

void LTC681xBus::wakeupSpi() {
  acquireSpi();
  releaseSpi();
}

void LTC681xBus::wakeupChainSpi() {
  // Tready: 10us
  // Twake: 400us
  // Pulse IsoSPI the number of chips, with delay for Twake in between each
  for (unsigned int i = 0; i < NUM_CHIPS; i++) {
    wakeupSpi();
    // Threads can't delay for microseconds it seems?
    ThisThread::sleep_for(1);
  }
}

void LTC681xBus::send(uint8_t txCmd[2]) {
  uint16_t cmdPec = calculatePec(2, txCmd);
  uint8_t cmd[4] = {txCmd[1], txCmd[0],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};

#ifdef DEBUG
  for (int i = 0; i < 4; i++) {
    serial->printf("CMD: %d: 0x%x\r\n", i, cmd[i]);
  }
#endif

  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  releaseSpi();
}

void LTC681xBus::sendData(uint8_t txCmd[2], uint8_t txData[6]) {
  uint16_t cmdPec = calculatePec(2, txCmd);
  uint8_t cmd[4] = {txCmd[1], txCmd[0],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};

  uint8_t data[8];
  for (int i = 0; i < 6; i++) {
    data[i] = txData[i];
  }
  uint16_t dataPec = calculatePec(6, txData);
  data[6] = (uint8_t)(dataPec >> 8);
  data[7] = (uint8_t)(dataPec);

#ifdef DEBUG
  for (int i = 0; i < 4; i++) {
    serial->printf("CMD: %d: 0x%x\r\n", i, cmd[i]);
  }
  for (int i = 0; i < 8; i++) {
    serial->printf("Byte: %d: 0x%x\r\n", i, data[i]);
  }
#endif

  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  m_spiDriver->write((const char *)data, 8, NULL, 0);
  releaseSpi();
}

void LTC681xBus::sendCommand(Command txCmd) {
  uint8_t cmdCode[2] = {(uint8_t)(txCmd.value >> 8), (uint8_t)(txCmd.value)};
  uint16_t cmdPec = calculatePec(2, cmdCode);
  uint8_t cmd[4] = {cmdCode[0], cmdCode[1],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};

  //wakeupSpi();
  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  releaseSpi();
}

void LTC681xBus::sendCommandWithData(Command txCmd, uint8_t txData[6]) {
  uint8_t cmdCode[2] = {(uint8_t)(txCmd.value >> 8), (uint8_t)(txCmd.value)};
  uint16_t cmdPec = calculatePec(2, cmdCode);
  uint8_t cmd[4] = {cmdCode[0], cmdCode[1],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};

  uint8_t data[8];
  for (int i = 0; i < 6; i++) {
    data[i] = txData[i];
  }
  uint16_t dataPec = calculatePec(6, txData);
  data[6] = (uint8_t)(dataPec >> 8);
  data[7] = (uint8_t)(dataPec);

#ifdef DEBUG
  for (int i = 0; i < 4; i++) {
    serial->printf("CMD: %d: 0x%x\r\n", i, cmd[i]);
  }
  for (int i = 0; i < 8; i++) {
    serial->printf("Byte: %d: 0x%x\r\n", i, data[i]);
  }
  serial->printf("pec: 0x%x\r\n", dataPec);
#endif

  //wakeupSpi();
  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  m_spiDriver->write((const char *)data, 8, NULL, 0);
  releaseSpi();
}

void LTC681xBus::readCommand(Command txCmd, uint8_t *rxbuf) {
  uint8_t cmdCode[2] = {(uint8_t)(txCmd.value >> 8), (uint8_t)(txCmd.value)};
  uint16_t cmdPec = calculatePec(2, cmdCode);
  uint8_t cmd[4] = {cmdCode[0], cmdCode[1],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};
#ifdef DEBUG
  for (int i = 0; i < 4; i++) {
    serial->printf("CMD: %d: 0x%x\r\n", i, cmd[i]);
  }
#endif

  //wakeupSpi();
  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  m_spiDriver->write(NULL, 0, (char *)rxbuf, 8);
  releaseSpi();

#ifdef DEBUG
  for (int i = 0; i < 8; i++) {
    serial->printf("READ: %d: 0x%x\r\n", i, rxbuf[i]);
  }
#endif

  uint16_t dataPec = calculatePec(6, rxbuf);
  bool goodPec = ((uint8_t)(dataPec >> 8)) == rxbuf[6] && ((uint8_t)dataPec) == rxbuf[7];
  if (!goodPec) {
    // TODO: return error or throw out read result
    serial->printf("ERR: Bad PEC on read. Computed: 0x%x. Actual: 0x%x\r\n", dataPec, (uint16_t)(rxbuf[6] << 8 | rxbuf[7]));
  }
}

void LTC681xBus::readWholeChainCommand(Command txCmd, uint8_t **rxbuf) {
  uint8_t cmdCode[2] = {(uint8_t)(txCmd.value >> 8), (uint8_t)(txCmd.value)};
  uint16_t cmdPec = calculatePec(2, cmdCode);
  uint8_t cmd[4] = {cmdCode[0], cmdCode[1],
                    (uint8_t)(cmdPec >> 8),
                    (uint8_t)(cmdPec)};
#ifdef DEBUG
  for (int i = 0; i < 4; i++) {
    serial->printf("CMD: %d: 0x%x\r\n", i, cmd[i]);
  }
#endif

  //wakeupSpi();
  acquireSpi();
  m_spiDriver->write((const char *)cmd, 4, NULL, 0);
  for (int i = 0; i < NUM_CHIPS; i++) {
    m_spiDriver->write(NULL, 0, (char *)&rxbuf[i], 8);
  }
  releaseSpi();

#ifdef DEBUG
  for (int j = 0; j < NUM_CHIPS; j++) {
    for (int i = 0; i < 8; i++) {
      serial->printf("READ: %d: 0x%x\r\n", i, rxbuf[j][i]);
    }
  ]
#endif

  for (unsigned int i = 0; i < NUM_CHIPS; i++) {
    uint16_t dataPec = calculatePec(6, rxbuf[i]);
    bool goodPec = ((uint8_t)(dataPec >> 8)) == rxbuf[i][6] && ((uint8_t)dataPec) == rxbuf[i][7];
    if (!goodPec) {
      // TODO: return error or throw out read result
      serial->printf("ERR: Bad PEC on read on chip %d. Computed: 0x%x. Actual: 0x%x\r\n",
        i, dataPec, (uint16_t)(rxbuf[i][6] << 8 | rxbuf[i][7]));
    }
  }
}

uint16_t LTC681xBus::calculatePec(uint8_t length, uint8_t *data) {
  uint16_t remainder = 16;
  uint16_t addr;

  for (uint8_t i = 0; i < length; i++) {
    // Calculate pec table address
    addr = ((remainder >> 7) ^ data[i]) & 0xff;

    remainder = (remainder << 8) ^ crc15Table[addr];
  }
  return (remainder << 1);
}
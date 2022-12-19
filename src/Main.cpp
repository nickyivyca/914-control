#include "config.h"
#include "pinout.h"

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>

#include "mbed.h"
#include "rtos.h"

#include "LTC681xChainBus.h"
#include "LTC6813.h"
#include "BmsThread.h"
#include "DataThread.h"
#include "Data.h"

#include "MCP23017.h"


UnbufferedSerial* serial;
BufferedSerial* displayserial;
CAN* canBus;

MCP23017* ioexp;

PwmOut* fuelgauge;
PwmOut* tach;

DigitalOut* led1;
DigitalOut* led2;
DigitalOut* led3;
DigitalOut* led4;
DigitalOut* DO_BattContactor;
DigitalOut* DO_BattContactor2;
DigitalOut* DO_DriveEnable;
DigitalOut* DO_ChargeEnable;

DigitalIn* DI_ChargeSwitch;

AnalogIn* knob1;

batterycomm_t data_main;
batterycomm_t data_bms;
batterycomm_t data_data;

Mail<mail_t, MSG_QUEUE_SIZE> inbox_main;
Mail<mail_t, MSG_QUEUE_SIZE> inbox_data;
Mail<mail_t, MSG_QUEUE_SIZE> inbox_bms;

void initIO();

int main() {
  // Init all io pins
  initIO();

  //uint8_t blink = 0;

 /*float fueltest = 1;
  uint32_t tachtest = 4000;
  uint8_t soctest = 0;
  float tachdutytest = 1;*/

  SPI* spiDriver = new SPI(PIN_6820_SPI_MOSI,
                           PIN_6820_SPI_MISO,
                           PIN_6820_SPI_SCLK,
                           PIN_6820_SPI_SSEL,
                           use_gpio_ssel);
  spiDriver->format(8, 0);
  auto ltcBus = LTC681xChainBus<NUM_CHIPS>(spiDriver);
  LTC6813Bus ltc6813Bus = LTC6813Bus(ltcBus);

  Thread bmsThreadThread;
  BMSThread bmsThread(&inbox_main, &inbox_bms, &ltcBus, &ltc6813Bus, &data_bms);
  bmsThreadThread.start(callback(&BMSThread::startThread, &bmsThread));
  DataThread dataThread(&inbox_main, &inbox_data, &data_data);

  osThreadSetPriority(osThreadGetId(), osPriorityHigh7);
  mail_t *msg_init = inbox_data.alloc();
  msg_init->msg_event = DATA_INIT;
  inbox_data.put(msg_init);
  Timer t;
  t.start();
  while (1) {
    while(!inbox_main.empty()) {
      osEvent evt = inbox_main.get();

      if (evt.status == osEventMail) {
        //std::cout << "Received some mail\n";
        mail_t *msg = (mail_t *)evt.value.p;


        switch(msg->msg_event) {
          case INIT_ALL:
            std::cout << "Main thread init\n";
            break;
          case NEW_CELL_DATA:
            {
              //uint32_t copystart = t.read_ms();
              //std::cout << "New BMS data received\n";
              data_main.mutex.lock();
              data_bms.mutex.lock();
              memcpy(&data_main.batterysummary, &data_bms.batterysummary, sizeof(data_main.batterysummary));
              memcpy(&data_main.batterydata, &data_bms.batterydata, sizeof(data_main.batterydata));
              data_bms.mutex.unlock();
              data_data.mutex.lock();
              memcpy(&data_data.batterysummary, &data_main.batterysummary, sizeof(data_data.batterysummary));
              memcpy(&data_data.batterydata, &data_main.batterydata, sizeof(data_data.batterydata));
              data_data.mutex.unlock();

              data_main.mutex.unlock();
              //std::cout << "Copy took " << (t.read_ms() - copystart) << "ms. Telling data thread about it\n";

              mail_t *msg_out = inbox_data.alloc();
              msg_out->msg_event = DATA_DATA;
              //msg_out->msg_event = DATA_SUMMARY;
              inbox_data.put(msg_out);
              msg_out = inbox_data.alloc();
              msg_out->msg_event = DATA_SUMMARY;
              //ThisThread::sleep_for(40);
              inbox_data.put(msg_out);
            }
            break;
          case BATT_ERR:
            {
              /*mail_t *msg_out = inbox_data.alloc();
              msg_out->msg_event = DATA_ERR;
              inbox_data.put(msg_out);     */         
            }
            break;
          default:
            std::cout << "Invalid message received in Main thread!\n";
            break;
        }
        inbox_main.free(msg);

      }

    }

    //std::cout << "Analog reading: " << knob1->read_u16() << "\n";

    /*fuelgauge->write(fueltest);
    fueltest -= 0.05;
    if (fueltest < 0.5) {
      fueltest = 1;
    }*/

    // default (0.5?) duty cycle
    // 10000us = ~3000rpm
    // 15000us = norpm
    // 6000us = ~4800rpm
    // 12500 = ~23-2400rpm
    // 

    //float tachperiod = 1.0/tachtest;
    //tach->period_us(tachtest);

    //tach->period_us(15000);
    //tach->pulsewidth_us(3000);
    /*fuelgauge->write(0.5 + (0.005*soctest));
    //tach->write(tachdutytest);
    // tachtest += 100;
    // if (tachtest > 20000) {
    //   tachtest = 4000;
    // }
    tachdutytest -= 0.005;
    if (tachdutytest < 0.4) {
      tachdutytest = 1;
    }
    if (soctest < 15) {
      ioexp->write_mask(1 << MCP_PIN_LOWFUEL, 0xff);
    } else if (soctest >= 15 && soctest < 30) {      
      ioexp->write_mask(1 << MCP_PIN_G, 0xff);
    } else if (soctest >= 30 && soctest < 45) {      
      ioexp->write_mask(1 << MCP_PIN_BIGB, 0xff);
    } else if (soctest >= 45 && soctest < 60) {      
      ioexp->write_mask(1 << MCP_PIN_EGR, 0xff);
    } else if (soctest >= 60 && soctest < 61) {      
      ioexp->write_mask(1 << MCP_PIN_BMSERR, 0xff);
    } else {
      ioexp->write_mask(0, 0xff);
    }
    soctest++;
    if (soctest > 100) {
      soctest = 0;
    }
    std::cout << "soc: " << (int)soctest << "\n";*/

    /*char i2ctest;
    const uint8_t addr_iodir = 0x00;
    i2ctest = ioexp->readRegister(IODIR);
    std::cout << "Read: " << std::hex << (int)i2ctest << "\n";*/


    /*uint32_t readbase = t.read_us();
    if (ioexp->read_bit(15)) {
      std::cout << "\nB7 high\n";
    } else {
      std::cout << "\nB7 low\n";
    }
    if (ioexp->read_bit(14)) {
      std::cout << "B6 high\n";
    } else {
      std::cout << "B6 low\n";
    }
    if (ioexp->read_bit(13)) {
      std::cout << "B5 high\n";
    } else {
      std::cout << "B5 low\n";
    }
    if (ioexp->read_bit(12)) {
      std::cout << "B4 high\n";
    } else {
      std::cout << "B4 low\n";
    }

    std::cout << "4x read time: " << (int)(t.read_us() - readbase) << "\n";*/


    //std::cout << "Looping main while " << (MAIN_PERIOD - (t.read_ms()%MAIN_PERIOD)) << '\n';

    /*blink++;
    std::cout << "blink: " << (int)blink << " Output driven\n";
    *led4 = blink%2;
    *DO_BattContactor = blink%2;*/

    /*std::cout << "Charge switch status: " << (int)*DI_ChargeSwitch << "\n";
    *led2 = *DI_ChargeSwitch;*/

    ThisThread::sleep_for(MAIN_PERIOD - (t.read_ms()%MAIN_PERIOD));
  }
}

void initIO() {
  //serial = new MODSERIAL(USBTX, USBRX, 1024, 32);
  serial = new UnbufferedSerial(USBTX, USBRX, 460800);
  //serial->set_dma_usage_tx(DMA_USAGE_ALWAYS);

  //serial2 = new Serial(PIN_SERIAL2_TX, PIN_SERIAL2_RX, 230400);
  displayserial = new BufferedSerial(PIN_DISPLAY_TX, PIN_DISPLAY_RX, 19200);
  //displayserial->set_dma_usage_tx(DMA_USAGE_ALWAYS);

  //serial->printf("INIT\n");
  
  canBus = new CAN(PIN_CAN_TX, PIN_CAN_RX, CAN_FREQUENCY);
  led1 = new DigitalOut(LED1);
  led2 = new DigitalOut(LED2);
  led3 = new DigitalOut(LED3);
  led4 =  new DigitalOut(LED4);
  DO_BattContactor = new DigitalOut(PIN_DO_BATTCONTACTOR);
  DO_BattContactor2 = new DigitalOut(PIN_DO_BATTCONTACTOR2);
  DO_DriveEnable = new DigitalOut(PIN_DO_DRIVEENABLE);
  DO_ChargeEnable = new DigitalOut(PIN_DO_CHARGEENABLE);

  DI_ChargeSwitch = new DigitalIn(PIN_DI_CHARGESWITCH);

  ioexp = new MCP23017(PIN_I2C_SDA, PIN_I2C_SCL, MCP_ADDRESS, 1000000);
  ioexp->config(0b1111100000000000, 0b0001100000000000, 0);

  ioexp->write_mask(0, 0xff);

  fuelgauge = new PwmOut(PIN_PWM_FUELGAUGE);
  //tach = new PwmOut(PIN_PWM_TACH); //config on tach TBD

  fuelgauge->period_ms(1);
  //tach->period_ms(2);
  fuelgauge->write(0.1);
  //tach->write(0.5);

  knob1 = new AnalogIn(PIN_ANALOG_KNOB1);


  *DO_BattContactor = 0;
  *DO_BattContactor2 = 0;
  *DO_DriveEnable = 0;
  *DO_ChargeEnable = 0;
}

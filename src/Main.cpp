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


BufferedSerial* serial;
BufferedSerial* displayserial;
CAN* canBus;

MCP23017* ioexp;

DigitalOut* led1;
DigitalOut* led2;
DigitalOut* led3;
DigitalOut* led4;
DigitalOut* DO_BattContactor;
DigitalOut* DO_BattContactor2;
DigitalOut* DO_DriveEnable;
DigitalOut* DO_ChargeEnable;

DigitalIn* DI_ChargeSwitch;

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

  uint8_t blink = 0;

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
    if (!inbox_main.empty()) {
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

    /*char i2ctest;
    const uint8_t addr_iodir = 0x00;
    i2ctest = ioexp->readRegister(IODIR);
    std::cout << "Read: " << std::hex << (int)i2ctest << "\n";*/



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
  serial = new BufferedSerial(USBTX, USBRX, 230400);

  //serial2 = new Serial(PIN_SERIAL2_TX, PIN_SERIAL2_RX, 230400);
  displayserial = new BufferedSerial(PIN_DISPLAY_TX, PIN_DISPLAY_RX, 19200);

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

  ioexp = new MCP23017(PIN_I2C_SDA, PIN_I2C_SCL, MCP_ADDRESS);

  *DO_BattContactor = 0;
  *DO_BattContactor2 = 0;
  *DO_DriveEnable = 0;
  *DO_ChargeEnable = 0;
}

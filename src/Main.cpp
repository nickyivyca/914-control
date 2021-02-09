#include "config.h"

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>

#include "mbed.h"
#include "rtos.h"

#include "LTC681xBus.h"
#include "LTC6813.h"
#include "BmsThread.h"
#include "DataThread.h"
#include "Data.h"


Serial* serial;
CAN* canBus;

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

  //ThisThread::sleep_for(1000);

  SPI* spiDriver = new SPI(PIN_6820_SPI_MOSI,
                           PIN_6820_SPI_MISO,
                           PIN_6820_SPI_SCLK,
                           PIN_6820_SPI_SSEL,
                           use_gpio_ssel);
  spiDriver->format(8, 0);
  LTC681xBus ltcBus = LTC681xBus(spiDriver);
  LTC6813Bus ltc6813Bus = LTC6813Bus(ltcBus);

  BMSThread bmsThread(&inbox_main, &inbox_bms, &ltcBus, &ltc6813Bus, &data_bms, CELL_SENSE_FREQUENCY);
  DataThread dataThread(&inbox_main, &inbox_data, &data_data);

  DigitalOut led(LED1);
  osThreadSetPriority(osThreadGetId(), osPriorityHigh7);
  // Flash LEDs to indicate startup
  /*for (int i = 0; i < 7; i++) {
    led = 1;
    ThisThread::sleep_for(500);
    led = 0;
    ThisThread::sleep_for(500);
  }*/

  /*CANMessage msg;
  while (1) {
    if (canBus->read(msg)) {
      serial->printf("Received with ID %#010x length %d\n", msg.id, msg.len);
      if (msg.len == 8) {
        switch (msg.id) {
          case 0x10088A9E:
            {
              unsigned int voltage = msg.data[0] & msg.data[1]<<8;
              unsigned int current = msg.data[2] & msg.data[3]<<8;
              char temperature = msg.data[4];
              unsigned char state = msg.data[5];
              unsigned char errors = msg.data[6];
              serial->printf("Voltage: %d\nCurrent: %d\nTemperature: %d\nState: %#01x\nErrors: %#01x\n", voltage, current, temperature, state, errors);
              break;
            }
          case 0x10098A9E:
            {
              unsigned int speed = msg.data[0] & msg.data[1]<<8;
              unsigned int mileage = msg.data[2] & msg.data[3]<<8;
              int torque = (msg.data[4] & msg.data[5]<<8);
              serial->printf("RPM: %d\nMileage: %d\nTorque: %d\n", speed, mileage, torque);
              break;
            }
          default:
            break;
        }
      }
    }
    // Sleep 100 secs
    //ThisThread::sleep_for(100 * 1000);
  }*/
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
              data_bms.mutex.unlock();
              data_data.mutex.lock();
              memcpy(&data_data.batterysummary, &data_main.batterysummary, sizeof(data_data.batterysummary));
              data_data.mutex.unlock();

              data_main.mutex.unlock();
              //std::cout << "Copy took " << (t.read_ms() - copystart) << "ms. Telling data thread about it\n";

              mail_t *msg_out = inbox_data.alloc();
              msg_out->msg_event = DATA_SUMMARY;
              inbox_data.put(msg_out);
            }
            break;
          default:
            std::cout << "Invalid message received in Main thread!\n";
            break;
        }
        inbox_main.free(msg);

      }
    }
    //std::cout << "Looping main while " << (MAIN_PERIOD - (t.read_ms()%MAIN_PERIOD)) << '\n';
    ThisThread::sleep_for(MAIN_PERIOD - (t.read_ms()%MAIN_PERIOD));
  }
}

void initIO() {
  serial = new Serial(USBTX, USBRX);
  serial->baud(230400);
  //serial->printf("INIT\n");
  
  canBus = new CAN(PIN_CAN_TX, PIN_CAN_RX, CAN_FREQUENCY);
}

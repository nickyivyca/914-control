#include "config.h"

#include "mbed.h"
#include "rtos.h"


Serial* serial;
CAN* canBus;

void initIO();

int main() {
  // Init all io pins
  initIO();

  ThisThread::sleep_for(1000);

  DigitalOut led(LED1);
  // Flash LEDs to indicate startup
  for (int i = 0; i < 4; i++) {
    led = 1;
    ThisThread::sleep_for(50);
    led = 0;
    ThisThread::sleep_for(50);
  }

  CANMessage msg;
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
  }
}

void initIO() {
  serial = new Serial(USBTX, USBRX);
  serial->printf("INIT\n");
  
  canBus = new CAN(PIN_CAN_TX, PIN_CAN_RX, CAN_FREQUENCY);  
}

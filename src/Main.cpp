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

  while (1) {
    // Sleep 100 secs
    ThisThread::sleep_for(100 * 1000);
  }
}

void initIO() {
  serial = new Serial(USBTX, USBRX);
  serial->printf("INIT\n");
  
  canBus = new CAN(PIN_CAN_TX, PIN_CAN_RX, CAN_FREQUENCY);  
}

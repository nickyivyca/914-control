#pragma once

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>

#include "mbed.h"
#include "rtos.h"
#include "Mail.h"

#include "config.h"
//#include "Can.h"
#include "pinout.h"
#include "LTC6813.h"
#include "LTC681xBus.h"
#include "Data.h"

class BMSThread {
 public:

  BMSThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
  LTC681xBus* bus, LTC6813Bus* bus_6813);

  // Function to allow for starting threads from static context
  static void startThread(BMSThread *p) {
    p->threadWorker();
  }

  // crc function for CAN calculation - use lowest byte
  static uint32_t crc32_word(uint32_t Crc, uint32_t Data)
  {
    int i;

    Crc = Crc ^ Data;

    for(i=0; i<32; i++)
      if (Crc & 0x80000000)
        Crc = (Crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
      else
        Crc = (Crc << 1);

    return(Crc);
  }

 private:
  Mail<mail_t, MSG_QUEUE_SIZE>* m_inbox;
  Mail<mail_t, MSG_QUEUE_SIZE>* m_outbox;
  LTC681xBus* m_bus;
  LTC6813Bus* m_6813bus;

  void throwBmsFault();
  void threadWorker();

};

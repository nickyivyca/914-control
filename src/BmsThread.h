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
#include "Telemetry.h"

class BMSThread {
 public:

  BMSThread(Mail<mail_t, MSG_QUEUE_SIZE>* inbox_main, Mail<chargerdata_t, MSG_QUEUE_SIZE>* inbox_charger, 
  Mail<inverterdata_t, MSG_QUEUE_SIZE>* inbox_inverter, LTC681xBus* bus, LTC6813Bus* bus_6813);

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
  Mail<mail_t, MSG_QUEUE_SIZE>* m_inbox_main;
  Mail<chargerdata_t, MSG_QUEUE_SIZE>* m_inbox_charger;
  Mail<inverterdata_t, MSG_QUEUE_SIZE>* m_inbox_inverter;
  LTC681xBus* m_bus;
  LTC6813Bus* m_6813bus;

  // Every fault now names itself. The bit feeds both halves of the BmsStatus telemetry frame --
  // a latched byte and a this-cycle byte -- which is what makes it visible when a single-cycle
  // sag half-throttles the car via the latched path while the dash lamp only flickers.
  //
  // This does not yet replace voltagecheckOK / stringcheckOK / faultThrown. Those carry latch
  // semantics that feed the inverter's throttle limit, so unpicking them is a separate change
  // from making the faults observable.
  void throwBmsFault(BmsFaultBit fault);
  void threadWorker();

  // Charger command frames on CAN 0x102 and 0x103. See the definition in BmsThread.cpp for
  // the field layout, the CC/CV mode split, and why 0x103 is not gated on the charge switch.
  void sendChargerFrames();

};

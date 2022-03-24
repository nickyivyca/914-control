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
  LTC681xBus* bus, LTC6813Bus* bus_6813, batterycomm_t* datacomm);

  // Function to allow for starting threads from static context
  static void startThread(BMSThread *p) {
    p->threadWorker();
  }

 private:
  Mail<mail_t, MSG_QUEUE_SIZE>* m_inbox;
  Mail<mail_t, MSG_QUEUE_SIZE>* m_outbox;
  LTC681xBus* m_bus;
  LTC6813Bus* m_6813bus;
  Mutex* m_mutex;
  batterydata_t* m_batterydata;
  batterysummary_t* m_batterysummary;

  void throwBmsFault();
  void threadWorker();

};

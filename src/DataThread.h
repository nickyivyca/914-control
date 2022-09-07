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

class DataThread {
  public:
    DataThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
      batterycomm_t* datacomm);

  // Function to allow for starting threads from static context
    static void startThread(DataThread *p) {
      p->threadWorker();
    }

  private:
    Thread m_thread;
    Mail<mail_t, MSG_QUEUE_SIZE>* m_outbox;
    Mail<mail_t, MSG_QUEUE_SIZE>* m_inbox;
    batterydata_t* m_data;
    batterysummary_t* m_summary;
    Mutex* m_mutex;

    void threadWorker();

};

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
  
  // Things that need to go away
  bool m_discharging = false;

  void throwBmsFault();
  void threadWorker();

  const uint16_t SoC_lookup[102] = {
    3341,
    3364,
    3386,
    3407,
    3421,
    3429,
    3436,
    3446,
    3447,
    3448,
    3456,
    3464,
    3472,
    3481,
    3488,
    3495,
    3502,
    3509,
    3515,
    3521,
    3527,
    3532,
    3537,
    3542,
    3546,
    3550,
    3554,
    3558,
    3561,
    3566,
    3571,
    3575,
    3579,
    3583,
    3589,
    3593,
    3597,
    3601,
    3605,
    3609,
    3612,
    3614,
    3619,
    3621,
    3624,
    3630,
    3635,
    3640,
    3646,
    3652,
    3657,
    3662,
    3667,
    3673,
    3679,
    3686,
    3692,
    3698,
    3704,
    3713,
    3721,
    3729,
    3738,
    3749,
    3759,
    3768,
    3777,
    3789,
    3800,
    3809,
    3820,
    3831,
    3843,
    3853,
    3864,
    3876,
    3889,
    3900,
    3912,
    3925,
    3937,
    3948,
    3960,
    3973,
    3985,
    3997,
    4009,
    4021,
    4033,
    4045,
    4057,
    4070,
    4081,
    4093,
    4107,
    4120,
    4133,
    4148,
    4165,
    4182,
    4199,
    4200 // 101%
  };

};

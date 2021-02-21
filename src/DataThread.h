#pragma once

#include <string>
#include <iostream>
#include <sstream>

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "Data.h"

class DataThread {
  public:
    DataThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
      batterycomm_t* datacomm) : m_outbox(outbox), m_inbox(inbox) {

      m_mutex = &datacomm->mutex;
      m_data = &datacomm->batterydata;
      m_summary = &datacomm->batterysummary;

      m_thread.start(callback(&DataThread::startThread, this));
      m_thread.set_priority(osPriorityLow);
    }

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

    void threadWorker() {
      Timer t;
      t.start();
      uint32_t prevTime = 0;

      while (true) {
        std::stringstream printbuff;
        if (!m_inbox->empty()) {
          osEvent evt = m_inbox->get();

          if (evt.status == osEventMail) {
            //std::cout << "Received some mail in Data thread\n";
            mail_t *msg = (mail_t *)evt.value.p;


            uint32_t startTime = t.read_ms();
            switch(msg->msg_event) {
              case DATA_INIT:
                //std::cout << "Data thread received init\n";

                // Print CSV header
                std::cout << "time_millis,packVoltage,totalCurrent,kW";
                for (uint16_t i = 0; i < NUM_CHIPS/2; i++) {
                  for (uint16_t j = 1; j <= NUM_CELLS_PER_CHIP*2; j++) {
                    std::cout << ",V_" << (char)('A'+i) << j;
                  }
                }
                for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                  std::cout << ",T_" << (char)('A'+(i/2)) << (i%2)+1;
                }
                std::cout << '\n';
                break;
              case DATA_DATA:
                {
                  float totalCurrent_scaled = ((float)m_data->totalCurrent)/1000.0;
                  //std::cout << "Data thread received full data\n";
                  // Print line of CSV data
                  std::cout << m_data->timestamp << ',' << m_data->packVoltage/1000.0 << ',' << totalCurrent_scaled << ',' << totalCurrent_scaled * m_data->packVoltage / 1000000.0;
                  for (uint16_t i = 0; i < NUM_CHIPS * NUM_CELLS_PER_CHIP; i++) {
                    std::cout << ',' << m_data->allVoltages[i];
                  }
                  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                    std::cout << ',' << m_data->allTemperatures[i];
                  }
                  std::cout << '\n';
                }
                break;
              case DATA_SUMMARY:
                //std::cout << "Data thread received summary\n";
                {
                  float totalVoltage_scaled = ((float)m_summary->totalVoltage)/1000.0;
                  float totalCurrent_scaled = ((float)m_summary->totalCurrent)/1000.0;



                  printbuff << "Pack Voltage: " << ceil(totalVoltage_scaled * 10.0) / 10.0 << "V"  // round to 1 decimal place
                  << " Current: " << totalCurrent_scaled << "A"
                  << "\nPower: " << ceil(totalCurrent_scaled * (totalVoltage_scaled * 10.0) / 1000.0) / 10.0 << "kW"  // round to 1 decimal place, scale to kW
                  << "\nMax Cell: " << m_summary->maxVoltage << " " << (char)('A'+(m_summary->maxVoltage_cell/28)) << (m_summary->maxVoltage_cell%28)+1
                  << " Min Cell: " << m_summary->minVoltage << " " << (char)('A'+(m_summary->minVoltage_cell/28)) << (m_summary->minVoltage_cell%28)+1
                  << " Avg Cell: " << m_summary->totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS)
                  << "\nMax Temp: " << m_summary->maxTemp << " " << (char)('A'+(m_summary->maxTemp_box/2)) << (m_summary->maxTemp_box%2)+1
                  << " Min Temp: " << m_summary->minTemp << " " << (char)('A'+(m_summary->minTemp_box/2)) << (m_summary->minTemp_box%2)+1;
                  printbuff << "\n\n";
                  //std::cout << printbuff.str();
                  serial2->printf(printbuff.str().c_str());
                }
                break;
              default:
                std::cout << "Invalid message received in Data thread!\n";
                break;
            }

            m_inbox->free(msg);
            /*if ((t.read_ms() - startTime) != prevTime) {
              prevTime = t.read_ms() - startTime;
              std::cout << "Data loop time: " << prevTime << "ms\n";
            }*/
          }
        ThisThread::sleep_for(1);
        }
      }

    }
};
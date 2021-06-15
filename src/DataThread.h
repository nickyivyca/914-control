#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip> 

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

      uint32_t errCount = 0;

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
                {
                  //std::cout << "Data thread received init\n";

                  // Print CSV header
                  printbuff << "time_millis,packVoltage,totalCurrent,kW";
                  for (uint16_t i = 0; i < NUM_CHIPS/2; i++) {
                    for (uint16_t j = 1; j <= NUM_CELLS_PER_CHIP*2; j++) {
                      printbuff << ",V_" << (char)('A'+i) << j;
                    }
                  }
                  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                    printbuff << ",T_" << (char)('A'+(i/2)) << (i%2)+1;
                  }
                  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                    printbuff << ",dieTemp_" << (char)('A'+(i/2)) << (i%2)+1;
                  }
                  printbuff << ",numBalancing\n";
                  std::cout << printbuff.str();
                  //serial2->printf(printbuff.str().c_str());

                  // Init display
                  displayserial->putc(0x0C);
                  ThisThread::sleep_for(5);
                  displayserial->putc(0x11); // Backlight on
                  displayserial->putc(0x16); // Cursor off, no blink

                  // add custom characters
                  uint8_t customchar = 0b00010000;
                  uint8_t charindex = 0xf8;
                  displayserial->putc(0x94);// move to second row to test characters
                  displayserial->putc(charindex);
                  for (uint8_t j = 0; j < 8; j++) {
                    displayserial->putc(0);
                  }
                  charindex++;
                  for (uint8_t i = 0; i < 5; i++) {
                    displayserial->putc(charindex);
                    for (uint8_t j = 0; j < 8; j++) {
                      displayserial->putc(customchar);
                    }
                    customchar |= (customchar >> 1);
                    charindex++;

                    //std::cout << "sending custom char " << (int)i << '\n';
                    //displayserial->putc((int)i);
                  }
                  /*for (uint8_t i = 0; i < 6; i++) {
                    displayserial->putc(i);
                  }*/

                  //displayserial->putc(4);
                }
                break;
              case DATA_DATA:
                {
                  float totalCurrent_scaled = ((float)m_data->totalCurrent)/1000.0;
                  //std::cout << "Data thread received full data\n";
                  // Print line of CSV data
                  printbuff << std::fixed << std::setprecision(1) << m_data->timestamp << ',' << m_data->packVoltage/1000.0 << ',' << totalCurrent_scaled << ',' << totalCurrent_scaled * m_data->packVoltage / 1000000.0;
                  for (uint16_t i = 0; i < NUM_CHIPS * NUM_CELLS_PER_CHIP; i++) {
                    printbuff << ',' << m_data->allVoltages[i];
                  }
                  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                    printbuff << ',' << m_data->allTemperatures[i];
                  }
                  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                    printbuff << ',' << (int)m_data->dieTemps[i];
                  }
                  printbuff << ',' << (int)m_data->numBalancing;
                  printbuff << '\n';
                  std::cout << printbuff.str();
                  //serial2->printf(printbuff.str().c_str());
                }
                break;
              case DATA_SUMMARY:
                //std::cout << "Data thread received summary\n";
                {
                  /*float totalVoltage_scaled = ((float)m_summary->totalVoltage)/1000.0;
                  float totalCurrent_scaled = ((float)m_summary->totalCurrent)/1000.0;



                  printbuff << std::fixed << std::setprecision(1) << "Pack Voltage: " << totalVoltage_scaled << "V"  // round to 1 decimal place
                  << " Current: " << totalCurrent_scaled << "A"
                  << "\nPower: " << totalCurrent_scaled * (totalVoltage_scaled) / 1000.0 << "kW"  // scale to kW
                  << "\nMax Cell: " << m_summary->maxVoltage << " " << (char)('A'+(m_summary->maxVoltage_cell/28)) << (m_summary->maxVoltage_cell%28)+1
                  << " Min Cell: " << m_summary->minVoltage << " " << (char)('A'+(m_summary->minVoltage_cell/28)) << (m_summary->minVoltage_cell%28)+1
                  << " Avg Cell: " << m_summary->totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS)
                  << "\nMax Temp: " << m_summary->maxTemp << " " << (char)('A'+(m_summary->maxTemp_box/2)) << (m_summary->maxTemp_box%2)+1
                  << " Min Temp: " << m_summary->minTemp << " " << (char)('A'+(m_summary->minTemp_box/2)) << (m_summary->minTemp_box%2)+1;
                  printbuff << "\n\n";
                  std::cout << printbuff.str();
                  printbuff.str("");*/



                  printbuff << setw(3) << m_summary->totalCurrent/1000 << "A " << setw(3) << m_summary->totalVoltage/1000 << "V " 
                  << "\r-:" << setw(3) << m_summary->minVoltage/10 << " +:" << setw(3) << m_summary->maxVoltage/10
                  << " A:" << setw(3) << m_summary->totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS)/10
                  /*<< "\r " << (char)('A'+(m_summary->maxVoltage_cell/28)) << setw(2) << (m_summary->maxVoltage_cell%28)+1
                  << " " << (char)('A'+(m_summary->maxVoltage_cell/28)) << setw(2) << (m_summary->maxVoltage_cell%28)+1*/
                  << "\r+: " << setw(2) << (int)round(m_summary->maxTemp) << " " << (char)('A'+(m_summary->maxTemp_box/2)) << (m_summary->maxTemp_box%2)+1
                  << " -: " << setw(2) << (int)round(m_summary->minTemp) << " " << (char)('A'+(m_summary->minTemp_box/2)) << (m_summary->minTemp_box%2)+1;


                  //serial2->printf(printbuff.str().c_str());


                  displayserial->putc(0x80); // move to 0,0

                  int64_t power = m_summary->totalCurrent*((int64_t)m_summary->totalVoltage)/1000000;
                  // Guards display overflow
                  if (power < 0) {
                    power = 0;
                  }
                  uint8_t fullcount = power/DISP_PER_BOX; //80kw/20 character width;
                  // Limits bar to not go further than it's supposed to
                  if (fullcount > 19) {
                    fullcount = 19;
                  }
                  for (uint8_t i = 0; i < fullcount; i++) {
                    displayserial->putc(0x5);
                  }
                  // Scale remainder 0-5 for end of the bar
                  uint8_t finalchar = (uint8_t)(((power+DISP_PER_BOX)%DISP_PER_BOX)/(DISP_PER_BOX/5));
                  displayserial->putc(finalchar);
                  if (errCount == 800) {
                    errCount = 0;
                  }

                  for (uint8_t i = 0; i < (19 - fullcount); i++) {
                    displayserial->putc(0);
                  }

                  displayserial->putc(0x94); // move to 1,0
                  displayserial->printf(printbuff.str().c_str());
                  //std::cout << m_summary->totalCurrent << "A " << m_summary->totalVoltage/1000 << "V " << "Calc: " << m_summary->totalCurrent*(int32_t)m_summary->totalVoltage/1000000 << " " << power/1000 << "kW fullcount: " << (int)fullcount << " final: " << (int)finalchar << " time: " <<  t.read_ms()-startTime << '\n';
                }
                break;
              case DATA_ERR:
                std::cout << "Data thread received error!\n";
                {
                  errCount++;



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
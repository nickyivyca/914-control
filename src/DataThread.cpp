// #include <string>
// #include <iostream>
// #include <sstream>
// #include <iomanip> 

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "DataThread.h"


DataThread::DataThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
  batterycomm_t* datacomm) : m_outbox(outbox), m_inbox(inbox) {

  m_mutex = &datacomm->mutex;
  m_data = &datacomm->batterydata;
  m_summary = &datacomm->batterysummary;

  m_thread.start(callback(&DataThread::startThread, this));
  m_thread.set_priority(osPriorityLow);
}

void DataThread::threadWorker() {
  Timer t;
  t.start();
  //uint32_t prevTime = 0;

  uint32_t errCount = 0;

  while (true) {
    while(!m_inbox->empty()) {
      osEvent evt = m_inbox->get();

      if (evt.status == osEventMail) {
        //std::cout << "Received some mail in Data thread\n";
        mail_t *msg = (mail_t *)evt.value.p;

        m_mutex->lock();


        uint32_t startTime = t.read_us();
        switch(msg->msg_event) {
          case DATA_INIT:
            {
              //uint32_t curtime = t.read_us();
              //std::cout << "Data thread received init\n";

              // Print CSV header
              //std::cout << "time_millis,packVoltage";
              printf("time_millis,packVoltage");
              for (uint16_t i = 0; i < NUM_STRINGS; i++) {
                //std::cout << ",current" << i;
                printf(",current%d", i);
              }
              //std::cout << ",kW,Whr,soc";
              printf(",kW,Whr,soc");
              //serial->printf(printbuff.str().c_str());
              /*std::cout << printbuff.str();
              printbuff.str(std::string());*/
              for (uint16_t i = 0; i < NUM_CHIPS/2; i++) {
                for (uint16_t j = 1; j <= NUM_CELLS_PER_CHIP*2; j++) {
                  //std::cout << ",V_" << (char)('A'+i) << j;
                  printf(",V_%c%d", (char)('A'+i), j);
                }
                //std::cout << "Length: " << strlen(printbuff.str().c_str()) << "\n";
                /*std::cout << printbuff.str();
                printbuff.str(std::string());*/
                //ThisThread::sleep_for(5);
              }
              //serial->printf(printbuff.str().c_str());
              //std::cout << printbuff.str();
              //printbuff.str(std::string());
              for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                //std::cout << ",T_" << (char)('A'+(i/2)) << (i%2)+1;
                printf(",T_%c%d", (char)('A'+(i/2)), (i%2)+1);
              }
              for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                //std::cout << ",dieTemp_" << (char)('A'+(i/2)) << (i%2)+1;
                printf(",dieTemp_%c%d", (char)('A'+(i/2)), (i%2)+1);
              }
              //std::cout << ",numBalancing,errCount\n";
              printf(",numBalancing,errCount\n");


              //std::cout << "Init Print time: " << (t.read_us() - curtime) << "us \n";
              // ~32ms with printf

              //serial->printf(printbuff.str().c_str());
              /*std::cout << printbuff.str();
              printbuff.str(std::string());*/
              //serial2->printf(printbuff.str().c_str());

              // Init display
              /*displayserial->putc(0x0C);
              ThisThread::sleep_for(5);
              displayserial->putc(0x11); // Backlight on
              displayserial->putc(0x16); // Cursor off, no blink*/

              uint8_t dispinit[3] = {0x0C, 0x11, 0x16};

              displayserial->write(dispinit, 1);
              ThisThread::sleep_for(5);
              displayserial->write(&dispinit[1], 2);

              // add custom characters
              uint8_t customchar = 0b00010000;
              //uint8_t charindex = 0xf8;
              //displayserial->putc(0x94);// move to second row to test characters
              uint8_t charinit[9] = {0xf8, 0,0,0,0,0,0,0,0};
              displayserial->write(charinit, 9);
              //displayserial->putc(charindex);
              /*for (uint8_t j = 0; j < 8; j++) {
                displayserial->putc(0);
              }*/
              //charindex++;
              charinit[0]++;
              for (uint8_t i = 0; i < 5; i++) {
                //displayserial->putc(charindex);
                for (uint8_t j = 0; j < 8; j++) {
                  //displayserial->putc(customchar);
                  charinit[j+1] = customchar;
                }
                displayserial->write(charinit, 9);

                customchar |= (customchar >> 1);
                charinit[0]++;
                //charindex++;

                //std::cout << "sending custom char " << (int)i << '\n';
                //displayserial->putc((int)i);
              }

              /*for (uint8_t i = 0; i < 6; i++) {
                displayserial->putc(i);
              }*/

              //displayserial->putc(4);
              //std::cout << "Init Print time: " << (t.read_us() - curtime) << "us \n";
            }
            break;
          case DATA_DATA:
            {
              //uint32_t curtime = t.read_us();
              //printbuff.str(std::string());
              float totalCurrent_scaled = ((float)m_data->totalCurrent)/1000.0;
              //std::cout << "Data thread received full data\n";
              // Print line of CSV data
              //std::cout << std::fixed << std::setprecision(1) << m_data->timestamp << ',' << m_data->packVoltage/1000.0 ;
              printf("%lu,%d", m_data->timestamp, m_data->packVoltage/1000);
              for (uint16_t i = 0; i < NUM_STRINGS; i++) {
                //std::cout << ',' << ((float)m_data->stringCurrents[i])/1000.0;
                printf(",%.1f", ((float)m_data->stringCurrents[i])/1000.0);
              }
              //std::cout << ',' << totalCurrent_scaled * m_data->packVoltage / 1000000.0 << ',' << m_data->joules/3600 << ',' << (int)m_data->soc;
              printf(",%.2f,%lld,%d", totalCurrent_scaled * m_data->packVoltage / 1000000.0, m_data->joules/3600, (int)m_data->soc);
              for (uint8_t j = 0; j < NUM_STRINGS; j++) {
                for (uint16_t i = 0; i < NUM_CHIPS / NUM_STRINGS * NUM_CELLS_PER_CHIP; i++) {
                  //std::cout << ',' << m_data->allVoltages[j][i];
                  printf(",%d", m_data->allVoltages[j][i]);
                }
                //std::cout << printbuff.str();

                //ThisThread::sleep_for(30);
              }
              for (uint16_t i = 0; i < NUM_CHIPS; i++) { 
                //std::cout << ',' << m_data->allTemperatures[i];
                printf(",%.1f", m_data->allTemperatures[i]);
              }
              for (uint16_t i = 0; i < NUM_CHIPS; i++) {
                //std::cout << ',' << (int)m_data->dieTemps[i]; 
                printf(",%d", (int)m_data->dieTemps[i]);
                //std::cout << "chiptemp\n";
              }
              //std::cout << ',' << (int)m_data->numBalancing;
              //std::cout << ',' << (int)errCount;
              //std::cout << '\n';
              printf(",%d,%d\n", (int)m_data->numBalancing, (int)errCount);
              //uint32_t curtime = t.read_us();

              //std::cout << "Data Print time: " << (t.read_us() - curtime) << "us \n";
            }
            break;
          case DATA_SUMMARY:
            {
              //uint32_t curtime = t.read_us();
              //std::cout << "Data thread received summary\n";

              //std::stringstream printbuffstream;
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
              //uint32_t curtime = t.read_us();

              float kwh = ((float)m_summary->joules)/3600000.0;



              //uint16_t avgcell = 

              /*printbuffstream.setf(ios::fixed,ios::floatfield);

              //printbuff.str(std::string());

              printbuffstream << setw(3) << m_summary->totalCurrent/1000 << "A " << setw(3) << m_summary->totalVoltage/1000 << "V" 
              << setw(6) << setprecision(1) << std::showpoint << std::right << kwh << "kWhr"
              << "\r-:" << setw(3) << m_summary->minVoltage/10 << " +:" << setw(3) << m_summary->maxVoltage/10
              << " A:" << setw(3) << m_summary->totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS/NUM_STRINGS)/10

              << "\r+: " << setw(2) << (int)round(m_summary->maxTemp) << " " << (char)('A'+(m_summary->maxTemp_box/2)) << (m_summary->maxTemp_box%2)+1
              << " -: " << setw(2) << (int)round(m_summary->minTemp) << " " << (char)('A'+(m_summary->minTemp_box/2)) << (m_summary->minTemp_box%2)+1 << " ";*/

              char printbuff[61];

              sprintf(printbuff, "%c%4dA %3dV%6.1fkWhr\r-:%3d +:%3d A:%3d   \r+:%3d %c%d -:%3d %c%d z", 0x80, m_summary->totalCurrent/1000, m_summary->totalVoltage/1000,
               kwh, m_summary->minVoltage/10, (m_summary->maxVoltage/10)%512, m_summary->totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS/NUM_STRINGS)/10,
                (int)round(m_summary->maxTemp), (char)('A'+(m_summary->maxTemp_box/2)), (m_summary->maxTemp_box%2)+1,
                (int)round(m_summary->minTemp), (char)('A'+(m_summary->minTemp_box/2)), (m_summary->minTemp_box%2)+1);

              //printf("")



              //std::cout.setf(ios::fixed,ios::floatfield);
              //std::cout << std::showpoint << setprecision(1) << setw(6) << kwh << "kWhr \n";
              //serial2->printf(printbuff.str().c_str());

              uint8_t dispprint[22] = {0x80, // Move to 0,0
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Block for current bar
                0x94}; // Move to 1,0

              //displayserial->putc(0x80); // move to 0,0

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
                //displayserial->putc(0x5);
                dispprint[i+1] = 0x5;
              }
              // Scale remainder 0-5 for end of the bar
              uint8_t finalchar = (uint8_t)(((power+DISP_PER_BOX)%DISP_PER_BOX)/(DISP_PER_BOX/5));
              //displayserial->putc(finalchar);
              dispprint[fullcount+1] = finalchar;
              if (errCount == 800) {
                errCount = 0;
              }

              /*for (uint8_t i = 0; i < (19 - fullcount); i++) {
                displayserial->putc(0);
              }*/
              //uint32_t curtime = t.read_us();
              //displayserial->write(dispprint, 22);
              //ThisThread::sleep_for(10);
              /*std::cout << "Display len: " << strlen(printbuff.str().c_str()) << "\n";
              std::cout << "Display: " << printbuff.str().c_str() << "\n";*/
              // https://stackoverflow.com/questions/1374468/stringstream-string-and-char-conversion-confusion
              //const std::string& dispbuff = printbuff.str();
              //const char* dispbuff_cstr = dispbuff.c_str();
              //std::cout << "Display len: " << strlen(dispbuff_cstr) << "\n";
              //std::cout << "Display: " << dispbuff_cstr << "\n";


              //std::cout << "Aout to print to display\n";

              //displayserial->write(printbuff.str().c_str(), strlen(printbuff.str().c_str()));

              //displayserial->write(dispbuff_cstr, strlen(dispbuff_cstr));
              //displayserial->write(printbuff, strlen(printbuff));
              displayserial->write(printbuff, 1);
              //displayserial->write(&printbuff[1], 18);
              displayserial->write(&printbuff[41], 20);
              //serial->write(dispbuff_cstr, strlen(dispbuff_cstr));
              //printbuff.str(std::string());
              //ThisThread::sleep_for(20);

              //std::cout << "Printed to display\n";

              //std::cout << "Print time: " << (t.read_us() - curtime) << "us \n";
              //displayserial->printf(printbuff.str().c_str());


              //std::cout << "Print time: " << (t.read_us() - curtime) << "us \n";

              //std::cout << "Display Print time: " << (t.read_us() - curtime) << "us \n";
              //std::cout << m_summary->totalCurrent << "A " << m_summary->totalVoltage/1000 << "V " << "Calc: " << m_summary->totalCurrent*(int32_t)m_summary->totalVoltage/1000000 << " " << power/1000 << "kW fullcount: " << (int)fullcount << " final: " << (int)finalchar << " time: " <<  t.read_ms()-startTime << '\n';
            }
            break;
          case DATA_ERR:
            //std::cout << "Data thread received error!\n";
            {
              errCount++;



            }
            break;
          default:
            //std::cout << "Invalid message received in Data thread!\n";
            printf("Invalid message received in Data thread!\n");
            break;
        }

        m_inbox->free(msg);

        m_mutex->unlock();
        /*if ((t.read_ms() - startTime) != prevTime) {
          prevTime = t.read_ms() - startTime;
          std::cout << "Data loop time: " << prevTime << "ms\n";
        }*/
      }
    ThisThread::sleep_for(1);
    }
  }

}
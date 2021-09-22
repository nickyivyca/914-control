#pragma once

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <bitset>

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "pinout.h"
#include "LTC6813.h"
#include "LTC681xBus.h"
#include "Data.h"



/*DigitalOut* DO_BattContactor;
DigitalOut* DO_ChargeEnable;

DigitalOut* led1;
DigitalOut* led2;
DigitalOut* led3;
DigitalOut* led4;

DigitalIn* DI_ChargeSwitch;*/

class BMSThread {
 public:
 BMSThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
  LTC681xBus* bus, LTC6813Bus* bus_6813, batterycomm_t* datacomm, unsigned int frequency) : 
   m_inbox(inbox), m_outbox(outbox), m_bus(bus), m_6813bus(bus_6813) {
    m_delay = 1000 / frequency;
    //m_chip = new LTC6813(*bus);
    //m_6813bus = new LTC6813Bus(*bus);
    /*for (int i = 0; i < NUM_CHIPS; i++) {
      m_chips.push_back(LTC6813(*bus, i));
    }*/
    m_mutex = &datacomm->mutex;
    m_batterydata = &datacomm->batterydata;
    m_batterysummary = &datacomm->batterysummary;
    //m_bus->wakeupChainSpi();
    //m_6813bus->updateConfig();

    m_thread.start(callback(&BMSThread::startThread, this));
  }
  static void startThread(BMSThread *p) {
    p->threadWorker();
  }

 private:
  //Thread m_thread(osPriorityHigh, OS_STACK_SIZE*2);
  Thread m_thread;
  unsigned int m_delay;
  Mail<mail_t, MSG_QUEUE_SIZE>* m_inbox;
  Mail<mail_t, MSG_QUEUE_SIZE>* m_outbox;
  LTC681xBus* m_bus;
  LTC6813Bus* m_6813bus;
  Mutex* m_mutex;
  batterydata_t* m_batterydata;
  batterysummary_t* m_batterysummary;

  //std::vector<LTC6813> m_chips;
  bool m_discharging = true;
  uint16_t voltages[NUM_CHIPS][18];
  uint16_t gpio_adc[NUM_CHIPS][2];
  //uint8_t dieTemps[NUM_CHIPS];

  float currentZero;
  bool currentZeroed = false;
  bool startedUp = false;
  bool voltagecheckOK = true;
  bool faultThrown = false;

  enum state {INIT, RUN, FAULT};

  state currentState = INIT;
  state nextState = INIT;


  void throwBmsFault() {
    m_discharging = false;
    *DO_ChargeEnable = 0;
    faultThrown = true;
    *led2 = 0;
    *led4 = 1;
  }
  void threadWorker() {

    uint8_t balance_index = 0;
    uint16_t prevMinVoltage = BMS_FAULT_VOLTAGE_THRESHOLD_HIGH;

    /*std::cout << "time_millis,totalCurrent";
    for (uint16_t i = 0; i < NUM_CHIPS/2; i++) {
      for (uint16_t j = 1; j <= NUM_CELLS_PER_CHIP*2; j++) {
        std::cout << ",V_" << (char)('A'+i) << j;
      }
    }
    for (uint16_t i = 0; i < NUM_CHIPS; i++) {
      std::cout << ",T_" << (char)('A'+(i/2)) << (i%2)+1;
    }*/
    std::cout << '\n';
    Timer t;
    t.start();

    uint32_t prevTime = 0;
    m_batterysummary->joules = 0;



    while (true) {
      uint32_t startTime = t.read_ms();

      uint32_t allBanksVoltage = 0;
      uint16_t minVoltage = 0xFFFF;
      uint8_t minVoltage_cell = 255;
      uint16_t maxVoltage = 0x0000;
      uint8_t maxVoltage_cell = 255;
      float minTemp = std::numeric_limits<float>::max();
      uint8_t minTemp_box = 255;
      float maxTemp = std::numeric_limits<float>::min();
      uint8_t maxTemp_box = 255;
      unsigned int totalVoltage = 0;
      int totalCurrent = 0;
      m_batterydata->numBalancing = 0;
      //systime_t timeStart = chVTGetSystemTime();
      // Should be changed to ticker

      /*switch(currentState) {
        case INIT:
          // run open wire checks, etc
          // zero current sensor?

          nextState = currentState;
          break;
        case RUN:

      }*/
      for (uint8_t i = 0; i < NUM_CHIPS; i++) {
        // Get a reference to the config for toggling gpio
        LTC6813::Configuration& conf = m_6813bus->m_chips[i].getConfig();
        // Turn on status LED
        conf.gpio4 = LTC6813::GPIOOutputState::kLow;
        conf.referencePowerOff = LTC6813::ReferencePowerOff::kWatchdogTimeout;

#ifdef TESTBALANCE
        unsigned int index = BMS_CELL_MAP[balance_index];
        conf.dischargeState.value |= (1 << balance_index);
        if (balance_index) {
          conf.dischargeState.value |= (0 << balance_index-1);
        } else {
          conf.dischargeState.value |= (0 << 17);          
        }
#endif
      }

      
#ifdef TESTBALANCE
      //std::cout << "Balance index: " << (int)balance_index << "\n";
      balance_index++;
      if (balance_index == 18) {
        balance_index = 0;
      }
#endif
      m_bus->wakeupChainSpi();
      m_6813bus->muteDischarge();
      m_6813bus->updateConfig();
      uint8_t pecStatus = m_6813bus->getCombined(voltages, gpio_adc);
      m_batterydata->timestamp = t.read_ms();
      m_batterysummary->timestamp = t.read_ms();
      m_6813bus->unmuteDischarge();

      m_6813bus->getDieTemps(m_batterydata->dieTemps);



      //std::cout << "Current raw: " << gpio_adc[0][2] << '\n';
      for (uint8_t i = 0; i < NUM_CHIPS; i++) {
        // Get a reference to the config for toggling gpio
        LTC6813::Configuration& conf = m_6813bus->m_chips[i].getConfig();
        //ThisThread::sleep_for(400);
        // Turn off status LED
        conf.gpio4 = LTC6813::GPIOOutputState::kHigh;
        // Set power off after conversions?
        //conf.referencePowerOff = LTC6813::ReferencePowerOff::kAfterConversions;
      }
      //std::cout << "NTC raw: " << gpio_adc[0][0] << ' ' << gpio_adc[0][1]  << '\n';
      
      //m_bus->wakeupChainSpi();
      m_6813bus->updateConfig();

      // Done with communication at this point
      // Now time to crunch numbers


      if (!pecStatus) {
        m_mutex->lock();
        for (unsigned int i = 0; i < NUM_CHIPS; i++) {

          //serial->printf("Chip %d: Die Temp: %d\n", i, dieTemps[i]);

          /*std::cout << "Sum: " << statuses[i].sumAllCells
          << "\nInternal Temp: " << statuses[i].internalTemperature
          << "\nAnalog Reference: " << statuses[i].voltageAnalog
          << "\nDigital Reference: " << statuses[i].voltageDigital
          << "\n";*/
          //totalVoltage += statuses[i].sumAllCells;


          // Process voltages
          //serial->printf("Voltages: ");
          for (int j = 0; j < 18; j++) {
            uint16_t voltage = voltages[i][j] / 10;


            int index = BMS_CELL_MAP[j];
            if (index != -1) {
              m_batterydata->allVoltages[(NUM_CELLS_PER_CHIP * i) + index] = voltage;
              totalVoltage += voltage;

              if (voltage < minVoltage && voltage != 0) {
                minVoltage = voltage;
                minVoltage_cell = index + 14*i;
              }
              if (voltage > maxVoltage) {
                maxVoltage = voltage;
                maxVoltage_cell = index + 14*i;
              }
              //totalVoltage += voltage;
              //serial->printf("%dmV ", voltage);

              if (voltage >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH) {
                // Set fault line
                //serial->printf("***** BMS LOW VOLTAGE FAULT *****\nVoltage at %d\n\n", voltage);
                throwBmsFault();
                voltagecheckOK = false;
              }
              if (voltage <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW) {
                // Set fault line
                //serial->printf("***** BMS HIGH VOLTAGE FAULT *****\nVoltage at %d\n\n", voltage);
                throwBmsFault();
                voltagecheckOK = false;
              }

              // Discharge cells if enabled

              LTC6813::Configuration& conf = m_6813bus->m_chips[i].getConfig();
              if(*DI_ChargeSwitch && !faultThrown && m_discharging) {
                if((voltage > prevMinVoltage) && (voltage - prevMinVoltage > BMS_DISCHARGE_THRESHOLD)) {
                  // Discharge

                  //serial->printf("DISCHARGE CHIP: %d CELL: %d: %dmV (%dmV)\n", i, index, voltage, (voltage - prevMinVoltage));

                  // Enable discharging
                  conf.dischargeState.value |= (1 << j);
                  m_batterydata->numBalancing++;
                } else {
                  // Disable discharging
                  conf.dischargeState.value &= ~(1 << j);
                }
              } else {
                // Disable discharging
                conf.dischargeState.value &= ~(1 << j);
              }
            }
          }
          //serial->printf("\n");
          // Calculate current sensor
          if (i == ISENSE_LOCATION) {
            if (!currentZeroed) {
              currentZero = gpio_adc[i][0]/10000.0;
              currentZeroed = true;
            }
            // replace 2.497 with zero'd value from startup? maybe use ref
            int current = (ISENSE_RANGE * (gpio_adc[i][0]/10000.0 - currentZero) / 0.625) * 1000.0; // unit mA
            totalCurrent += current;
            //std::cout << "Current: " << current << '\n';
          }
          // Calculate thermistors: present on even chips (lower chip of each box)
          if (!(i % 2)) {
            for (uint8_t j = 0; j < 2; j++) {
              // calculate resistance from voltage
              float thermvolt = gpio_adc[i][j]/10000.0;
              float resistance = (4700.0 * thermvolt)/(5.0 - thermvolt);
              //std::cout << "Calculated resistance " << j+1 << ":  " << resistance << "\n";

              // https://github.com/panStamp/thermistor/blob/master/thermistor.cpp
              float steinhart;
              steinhart = resistance / 10000.0;     // (R/Ro)
              steinhart = log(steinhart);                  // ln(R/Ro)
              steinhart /= 3380.0;                   // 1/B * ln(R/Ro)
              steinhart += 1.0 / (25.0 + 273.15); // + (1/To)
              steinhart = 1.0 / steinhart;                 // Invert
              steinhart -= 273.15;                         // convert to C
              steinhart = ceil(steinhart * 10.0) / 10.0;   // round to 1 decimal place


              if (!isnan(steinhart)) {
                m_batterydata->allTemperatures[i+j] = steinhart;
                //std::cout << "NTC " << j+1 << ": " << steinhart << '\n';
                if (steinhart < minTemp && steinhart != 0){
                  minTemp = steinhart;
                  minTemp_box = j + i;
                }
                if (steinhart > maxTemp) {
                  maxTemp = steinhart;
                  maxTemp_box = j + i;
                }
                // max temp check
                if (steinhart > BMS_TEMPERATURE_THRESHOLD && *DI_ChargeSwitch) {
                  throwBmsFault();
                }
              }

            }
          }
        }
      

        /*float totalVoltage_scaled = ((float)totalVoltage)/1000.0;
        float totalCurrent_scaled = ((float)totalCurrent)/1000.0;

        std::cout << "Pack Voltage: " << ceil(totalVoltage_scaled * 10.0) / 10.0 << "V"  // round to 1 decimal place
        << " Current: " << totalCurrent_scaled << "A"
        << "\nPower: " << ceil(totalCurrent_scaled * (totalVoltage_scaled * 10.0) / 1000.0) / 10.0 << "kW"  // round to 1 decimal place, scale to kW
        << "\nMax Cell: " << maxVoltage << " " << (char)('A'+(maxVoltage_cell/28)) << (maxVoltage_cell%28)+1
        << " Min Cell: " << minVoltage << " " << (char)('A'+(minVoltage_cell/28)) << (minVoltage_cell%28)+1
        << " Avg Cell: " << (totalVoltage_scaled/(NUM_CELLS_PER_CHIP*NUM_CHIPS))
        << "\nMax Temp: " << maxTemp << " " << (char)('A'+(maxTemp_box/2)) << (maxTemp_box%2)+1
        << " Min Temp: " << minTemp << " " << (char)('A'+(minTemp_box/2)) << (minTemp_box%2)+1;
        std::cout << '\n';
        std::cout << '\n';*/

        m_batterysummary->minVoltage = minVoltage;
        m_batterysummary->minVoltage_cell = minVoltage_cell;
        m_batterysummary->maxVoltage = maxVoltage;
        m_batterysummary->maxVoltage_cell = maxVoltage_cell;
        m_batterysummary->minTemp = minTemp;
        m_batterysummary->minTemp_box = minTemp_box;
        m_batterysummary->maxTemp = maxTemp;
        m_batterysummary->maxTemp_box = maxTemp_box;
        m_batterysummary->totalCurrent = totalCurrent;
        m_batterysummary->totalVoltage = totalVoltage;

        m_batterysummary->joules += ((totalCurrent/1000) * ((int32_t)(totalVoltage/1000))/CELL_SENSE_FREQUENCY);
        m_batterydata->joules = m_batterysummary->joules;

        m_batterydata->totalCurrent = totalCurrent;
        m_batterydata->packVoltage = totalVoltage;

        prevMinVoltage = minVoltage;

        m_mutex->unlock();

        mail_t *msg = m_outbox->alloc();
        msg->msg_event = NEW_CELL_DATA;
        m_outbox->put(msg);

        *led3 = 0;

        if (!startedUp && voltagecheckOK) {
          *DO_BattContactor = 1;
          startedUp = true;
          *led1 = 1;
          /**msg = m_outbox->alloc();
          msg->msg_event = BATT_STARTUP;
          m_outbox->put(msg);*/
        }
        if (*DI_ChargeSwitch && voltagecheckOK && !faultThrown) {
          *DO_ChargeEnable = 1;
          *led2 = 1;
          /**msg = m_outbox->alloc();
          msg->msg_event = CHARGE_ENABLED;
          m_outbox->put(msg);*/
        } else {
          *DO_ChargeEnable = 0;
          *led2 = 0;          
        }
      } else {
        std::bitset<8> pecprint(pecStatus);

        mail_t *msg = m_outbox->alloc();
        msg->msg_event = BATT_ERR;
        m_outbox->put(msg);
        //std::cout << "PEC error! " << pecprint << '\n';
        *led3 = 1;
      }


      /*uint16_t mean = (totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS));
      float standardDeviation = 0;

      for(uint8_t i = 0; i < NUM_CELLS_PER_CHIP*NUM_CHIPS; i++) {
        standardDeviation += pow(allVoltages[i] - mean, 2);
      }

      std::cout << "CellV Standard deviation: " << sqrt(standardDeviation/(NUM_CELLS_PER_CHIP*NUM_CHIPS)) << '\n';*/



      /*std::cout << t.read_ms() << ',' << totalCurrent_scaled;
      for (uint16_t i = 0; i < NUM_CHIPS * NUM_CELLS_PER_CHIP; i++) {
        std::cout << ',' << allVoltages[i];
      }
      for (uint16_t i = 0; i < NUM_CHIPS; i++) {
        std::cout << ',' << allTemperatures[i];
      }
      std::cout << '\n';*/


      // Turn off status LED
      /*conf.gpio4 = LTC6813::GPIOOutputState::kHigh;
      m_bus->wakeupChainSpi();
      m_chip->updateConfig();*/

      //serial->printf("Total Voltage: %dmV\n",
      //         totalVoltage);*/
      /*serial->printf("Min Voltage: %dmV\n",
               minVoltage);
      serial->printf("Max Voltage: %dmV\n",
               maxVoltage);*/
      //delete voltages;

      
      /*for (unsigned int j = 0; j < BMS_BANK_TEMP_COUNT; j++) {
        auto temp = convertTemp(temperatures[j] / 10);
        allTemps[(BMS_BANK_TEMP_COUNT * i) + j] = temp;

        temp.map([&](auto t){
            if (t < minTemp) minTemp = t;
            if (t > maxTemp) maxTemp = t;
          });
      }*/
      /*
      allBanksVoltage += totalVoltage;

      averageVoltage = allBanksVoltage / (NUM_CHIPS * NUM_CELLS_PER_CHIP);
      prevMinVoltage = minVoltage;*/

      /*serial->printf("Temperatures: \n");
      for(int i = 0; i < BMS_BANK_COUNT * BMS_BANK_CELL_COUNT; i++){
        allTemps[i].map_or_else([&](auto temp) {
            if (temp >= BMS_FAULT_TEMP_THRESHOLD_HIGH) {
              serial->printf("***** BMS HIGH TEMP FAULT *****\nTemp at %d\n\n", temp);
              throwBmsFault();
            } else if (temp <= BMS_FAULT_TEMP_THRESHOLD_LOW) {
              serial->printf("***** BMS LOW TEMP FAULT *****\nTemp at %d\n\n", temp);
              throwBmsFault();
            }

            serial->printf("%3d ", temp);
          },
          [&]() {
            serial->printf("ERR ");
            //serial->printf("***** BMS INVALID TEMP FAULT *****\n");
            //throwBmsFault();
          });
        if((i + 1) % BMS_BANK_CELL_COUNT == 0)
          serial->printf("\n");
      }*/

      /*canBus->write(BMSStatMessage(allBanksVoltage / 10, maxVoltage, minVoltage, maxTemp, minTemp));
      
      // Send CAN
      for (size_t i = 0; i < BMS_BANK_COUNT; i++) {
        // Convert from optional temp values to values with default of -127 (to indicate error)
        auto temps = std::array<int8_t, BMS_BANK_TEMP_COUNT>();
        std::transform(allTemps.begin() + (BMS_BANK_TEMP_COUNT * i),
                       allTemps.begin() + (BMS_BANK_TEMP_COUNT * (i + 1)),
                       temps.begin(),
                       [](tl::optional<int8_t> t) { return t.value_or(-127); });

        canBus->write(BMSTempMessage(i, (uint8_t*)temps.data()));
      }

      for (size_t i = 0; i < 7; i++) {
        canBus->write(BMSVoltageMessage(i, allVoltages + (4 * i)));
      }*/

      // Compute time elapsed since beginning of measurements and sleep for
      // m_delay accounting for elapsed time
      // TODO: use a hardware timer or a virtual timer or literally anything
      // else. kek.
      //unsigned int timeElapsed = TIME_I2MS(chVTTimeElapsedSinceX(timeStart));
#ifdef DEBUG
      //serial->printf("BMS Thread time elapsed: %dms\n", timeElapsed);
#endif
      //ThisThread::sleep_for(m_delay*3);

      //std::cout << "m_delay: " << m_delay << " Delaying for: " << (m_delay - (t.read_ms()%m_delay));
      /*if ((t.read_ms() - startTime) != prevTime) {
        prevTime = t.read_ms() - startTime;
        std::cout << "BMS loop time: " << prevTime << "ms\n";
      }*/

      ThisThread::sleep_for(m_delay - (t.read_ms()%m_delay));
    }
  }
};

#pragma once

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "LTC6813.h"
#include "LTC681xBus.h"

class BMSThread {
 public:
 BMSThread(LTC681xBus* bus, unsigned int frequency) : m_bus(bus) {
    m_delay = 1000 / frequency;
    m_chip = new LTC6813(*bus);
    m_bus->wakeupChainSpi();
    m_chip->updateConfig();
    m_thread.start(callback(&BMSThread::startThread, this));
  }
  static void startThread(BMSThread *p) {
    p->threadWorker();
  }

 private:
  Thread m_thread;
  unsigned int m_delay;
  LTC681xBus* m_bus;
  LTC6813* m_chip;
  bool m_discharging = false;
  uint16_t voltages[NUM_CHIPS][18];
  uint16_t gpio_adc[NUM_CHIPS][9];
  uint16_t allVoltages[NUM_CHIPS * NUM_CELLS_PER_CHIP];

  enum state {INIT, RUN, FAULT};

  state currentState = INIT;
  state nextState = INIT; 


  void throwBmsFault() {
    //m_discharging = false;
    //bmsFault->write(0);
    //chargerControl->write(1);

  }
  void threadWorker() {
    uint16_t averageVoltage = -1;
    uint16_t prevMinVoltage = -1;

    while (true) {
      //systime_t timeStart = chVTGetSystemTime();
      // Should be changed to ticker

      uint32_t allBanksVoltage = 0;
      uint16_t minVoltage = 0xFFFF;
      uint16_t maxVoltage = 0x0000;
      int8_t minTemp = INT8_MAX;
      int8_t maxTemp = INT8_MIN;

      /*switch(currentState) {
        case INIT:
          // run open wire checks, etc
          // zero current sensor?

          nextState = currentState;
          break;
        case RUN:

      }*/

      // Get a reference to the config for toggling gpio
      LTC6813::Configuration& conf = m_chip->getConfig();

      // Turn on status LED
      conf.gpio4 = LTC6813::GPIOOutputState::kLow;
      conf.referencePowerOff = LTC6813::ReferencePowerOff::kWatchdogTimeout;
      m_bus->wakeupChainSpi();
      m_chip->updateConfig();
      //m_chip->readConfig();

      m_bus->wakeupChainSpi();
      //std::cout << "Getting voltages\n";
      m_chip->getVoltages(voltages);
      m_bus->wakeupChainSpi();
      //std::cout << "Getting GPIOs\n";
      m_chip->getGpio(gpio_adc);
      //std::cout << "Current raw: " << gpio_adc[0][2] << '\n';
      std::cout << "NTC raw: " << gpio_adc[0][0] << ' ' << gpio_adc[0][1]  << '\n';

      //ThisThread::sleep_for(400);
      // Turn off status LED
      conf.gpio4 = LTC6813::GPIOOutputState::kHigh;
      
      //conf.referencePowerOff = LTC6813::ReferencePowerOff::kAfterConversions;
      m_bus->wakeupChainSpi();
      m_chip->updateConfig();

      // Done with communication at this point
      // Now time to crunch numbers

      for (unsigned int i = 0; i < NUM_CHIPS; i++) {

        //serial->printf("Chip %d:\n", i);

        // Process voltages
        unsigned int totalVoltage = 0;
        serial->printf("Voltages: ");
        for (int j = 0; j < 18; j++) {
          uint16_t voltage = voltages[i][j] / 10;


          unsigned int index = BMS_CELL_MAP[j];
          if (index != -1) {
            //allVoltages[(BMS_BANK_CELL_COUNT * i) + index] = voltage;

            if (voltage < minVoltage && voltage != 0) minVoltage = voltage;
            if (voltage > maxVoltage) maxVoltage = voltage;

            totalVoltage += voltage;
            serial->printf("%dmV ", voltage);

            // if (voltage >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH) {
            //   // Set fault line
            //   serial->printf("***** BMS LOW VOLTAGE FAULT *****\nVoltage at %d\n\n", voltage);
            //   throwBmsFault();
            // }
            // if (voltage <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW) {
            //   // Set fault line
            //   serial->printf("***** BMS HIGH VOLTAGE FAULT *****\nVoltage at %d\n\n", voltage);
            //   throwBmsFault();
            // }

            // Discharge cells if enabled
            if(m_discharging) {
              if((voltage > prevMinVoltage) && (voltage - prevMinVoltage > BMS_DISCHARGE_THRESHOLD)) {
                // Discharge

                serial->printf("DISCHARGE CELL %d: %dmV (%dmV)\n", index, voltage, (voltage - prevMinVoltage));

                // Enable discharging
                conf.dischargeState.value |= (1 << j);
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
        serial->printf("\n");
        if (!(i % 2)) {
          // replace 2.497 with zero'd value from startup?
          float current = 50.0 * (gpio_adc[i][2]/10000.0 - 2.497) / 0.625;
          std::cout << "Current calculated: " << current/5 << '\n';
        }
      }


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
      ThisThread::sleep_for(1000);
    }
  }
};

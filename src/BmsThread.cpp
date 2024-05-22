#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <bitset>
#include <string>
#include <sstream>
#include <iomanip> 

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "pinout.h"
#include "LTC6813.h"
#include "LTC681xBus.h"
#include "Data.h"
#include "BmsThread.h"


BMSThread::BMSThread(Mail<mail_t, MSG_QUEUE_SIZE>* outbox, Mail<mail_t, MSG_QUEUE_SIZE>* inbox, 
  LTC681xBus* bus, LTC6813Bus* bus_6813) : 
   m_inbox(inbox), m_outbox(outbox), m_bus(bus), m_6813bus(bus_6813) {
    //m_chip = new LTC6813(*bus);
    //m_6813bus = new LTC6813Bus(*bus);
    /*for (int i = 0; i < NUM_CHIPS; i++) {
      m_chips.push_back(LTC6813(*bus, i));
    }*/
    // m_batterydata = &datacomm->batterydata;
    // m_batterysummary = &datacomm->batterysummary;
    //m_bus->wakeupChainSpi();
    //m_6813bus->updateConfig();
}
  //Thread m_thread(osPriorityHigh, OS_STACK_SIZE*2);

//std::vector<LTC6813> m_chips;
bool m_discharging = true;
uint16_t voltages[NUM_CHIPS][18];
uint16_t gpio_adc[NUM_CHIPS][2];
//uint8_t dieTemps[NUM_CHIPS];

int32_t currentZero[NUM_STRINGS] = {0};
bool currentZeroed[NUM_STRINGS] = {false};
int8_t minTemps[2][NUM_STRINGS] = {BMS_TEMPERATURE_THRESHOLD};
uint8_t tempSelect = 0;
bool SoCinitialized = false;
uint8_t SoC;
uint8_t buttonCount = 0;
bool startedUp = false;
bool voltagecheckOK = true;
bool stringcheckOK = true;
bool faultThrown = false;
int millicoulombs;
char canPower[2];
char* const canPowerSend = canPower;


batterydata_t m_batterydata;
batterysummary_t m_batterysummary;


void BMSThread::throwBmsFault() {
  m_discharging = false;
  *DO_ChargeEnable = 0;
  faultThrown = true;
  *led2 = 0;
  *led4 = 1;
}
void BMSThread::threadWorker() {

#ifdef TESTBALANCE
  uint8_t balance_index = 0;
#endif
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
  //std::cout << '\n';
  Timer t;
  t.start();

  uint32_t prevTime = 0;
  m_batterysummary.joules = 0;

  uint8_t printCount = 0;
  uint16_t errCount = 0;


  //uint32_t curtime = t.read_us();
  //std::cout << "Data thread received init\n";

  // Print CSV header
  std::cout << "time_millis,packVoltage";
  for (uint16_t i = 0; i < NUM_STRINGS; i++) {
    std::cout << ",current" << i;
  }
  std::cout << ",kW,Whr,soc";
  //serial->printf(printbuff.str().c_str());
  /*std::cout << printbuff.str();
  printbuff.str(std::string());*/
  for (uint16_t i = 0; i < NUM_CHIPS/2; i++) {
    for (uint16_t j = 1; j <= NUM_CELLS_PER_CHIP*2; j++) {
      std::cout << ",V_" << (char)('A'+i) << j;
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
    std::cout << ",T_" << (char)('A'+(i/2)) << (i%2)+1;
  }
  for (uint16_t i = 0; i < NUM_CHIPS; i++) {
    std::cout << ",dieTemp_" << (char)('A'+(i/2)) << (i%2)+1;
  }
  std::cout << ",numBalancing,errCount\n";

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



  while (true) {
    //uint32_t startTime = t.read_ms();

    uint32_t packVoltage = 0;
    uint16_t minVoltage = 0xFFFF;
    uint8_t minVoltage_cell = 255;
    uint16_t maxVoltage = 0x0000;
    uint8_t maxVoltage_cell = 255;
    float minTemp = std::numeric_limits<float>::max();
    uint8_t minTemp_box = 255;
    float maxTemp = std::numeric_limits<float>::min();
    uint8_t maxTemp_box = 255;
    unsigned int totalVoltage[NUM_STRINGS] = {0};
    //stringCurrents[NUM_STRINGS] = 0;
    m_batterydata.numBalancing = 0;
    m_batterydata.totalCurrent = 0;

    uint16_t ioexp_bits = 0;

    uint32_t timestamp = 0;


    int m_frequency = *DI_ChargeSwitch? CELL_SENSE_FREQUENCY_CHARGE : CELL_SENSE_FREQUENCY;
    int m_delay =  1000/m_frequency;
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
    switch (balance_index) {
      case 16:
        balance_index = 0;
        break;
      case 5:
        balance_index = 6;
        break;
      case 11:
        balance_index = 12;
        break;
      default:
        break;
    }

#endif
    m_bus->WakeupBus();
    m_6813bus->muteDischarge();
    m_6813bus->updateConfig();
    uint8_t pecStatus = m_6813bus->getCombined(voltages, gpio_adc);
    timestamp = t.read_ms();
    m_6813bus->unmuteDischarge();
    if (*DI_ChargeSwitch) {
      m_6813bus->getDieTemps(m_batterydata.dieTemps);
    }



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
    //if (true) {
      for (uint8_t i = 0; i < NUM_STRINGS; i++) {
        minTemps[tempSelect][i] = BMS_TEMPERATURE_THRESHOLD;
      }
      tempSelect = (tempSelect + 1) % 2;

      for (uint8_t string = 0; string < NUM_STRINGS; string++) {
        for (unsigned int i = 0; i < NUM_CHIPS/NUM_STRINGS; i++) {
          uint8_t chip_loc = BMS_CHIP_MAP[string][i];

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
            uint16_t voltage = voltages[chip_loc][j] / 10;


            int index = BMS_CELL_MAP[j];
            if (index != -1) {
              if ((chip_loc == 6 || chip_loc == 7 || chip_loc == 8 || chip_loc == 9)) {
                // add 5mV for top of cellbox and bottom of cellbox
                if ((i%2 && index == 13) || (!i%2 && index == 0)) {
                  voltage += 5;
                }
                // add 7 mV for top of cellbox for current sense
                // only works if there are no additional current sensors on the string!
                if (chip_loc == BMS_ISENSE_MAP[string] && index == 13) {
                  voltage += 7;
                }
              }
              // adjust for 15mV offset due to current sensor
              if (((BMS_ISENSE_MAP[string]%2 && chip_loc == BMS_ISENSE_MAP[string]-1) ||
                (!BMS_ISENSE_MAP[string]%2 && chip_loc == BMS_ISENSE_MAP[string]+1)) && index == 13) { 
                //std::cout << "Subtracting 15mv for current sense on chip " << (int)chip_loc << " string: " << (int)string << " i: " << (int)i << "\n";
                voltage -= 15;
              }
              if (chip_loc == BMS_ISENSE_MAP[string] && index == 0) {
                //std::cout << "Adding 15mv for current sense on chip " << (int)chip_loc << " string: " << (int)string << " i: " << (int)i << "\n";
                voltage += 15;
              }
              m_batterydata.allVoltages[string][(NUM_CELLS_PER_CHIP * i) + index] = voltage;
              totalVoltage[string] += voltage;

              if (voltage < minVoltage && voltage != 0) {
                minVoltage = voltage;
                minVoltage_cell = index + NUM_CELLS_PER_CHIP*(i + (string * NUM_CHIPS / NUM_STRINGS));
              }
              if (voltage > maxVoltage) {
                maxVoltage = voltage;
                maxVoltage_cell = index + NUM_CELLS_PER_CHIP*(i + (string * NUM_CHIPS / NUM_STRINGS));
              }
              //totalVoltage += voltage;
              //serial->printf("%dmV ", voltage);

              if (voltage >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH) {
                // Set fault line
                //std::cout << "***** BMS LOW VOLTAGE FAULT *****\nVoltage at " << voltage << "\n\n";
                throwBmsFault();
                voltagecheckOK = false;
                ioexp_bits |= (1 << MCP_PIN_BMSERR);
              }
              if (voltage <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW) {
                // Set fault line
                //std::cout << "***** BMS HIGH VOLTAGE FAULT *****\nVoltage at " << voltage << "\n\n";
                throwBmsFault();
                voltagecheckOK = false;
                ioexp_bits |= (1 << MCP_PIN_BMSERR);
              }

              // Discharge cells if enabled

              LTC6813::Configuration& conf = m_6813bus->m_chips[chip_loc].getConfig();
              if(!faultThrown && m_discharging && BALANCE_EN) {                
                if ((minTemps[tempSelect][string] < BMS_LOW_TEMPERATURE_THRESHOLD) && *DI_ChargeSwitch) {
                  // Discharge

                  //printf("DISCHARGE CHIP: %d CELL: %d: %dmV (%dmV)\n", chip_loc, index, voltage, (voltage - prevMinVoltage));

                  // Enable discharging
                  conf.dischargeState.value |= (1 << j);
                  m_batterydata.numBalancing++;

                  // And turn on G light to show low temp
                  ioexp_bits |= (1 << MCP_PIN_G);
                } else if((*DI_ChargeSwitch && ((voltage > prevMinVoltage) && (voltage - prevMinVoltage > BMS_DISCHARGE_THRESHOLD)))) {
                  // else if normal balancing just turn on the balancing resistorz
                  //printf("DISCHARGE CHIP: %d CELL: %d: %dmV (%dmV)\n", chip_loc, index, voltage, (voltage - prevMinVoltage));
                  conf.dischargeState.value |= (1 << j);
                  m_batterydata.numBalancing++;
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
          /*if (*DI_ChargeSwitch) {
            LTC6813::Configuration& conf = m_6813bus->m_chips[i].getConfig();
            std::bitset<16> balancemap(conf.dischargeState.value);
            std::cout << "Chip " << (int)i << " balance: " << balancemap << '\n';
          }*/
          //serial->printf("\n");
          // Calculate current sensor
          if (chip_loc == BMS_ISENSE_MAP[string]) {
            //std::cout << "On current sense chip i: " << (int)i << " chiploc: " << (int)chip_loc << "\n";
            if (!currentZeroed[string]) {
              currentZero[string] = gpio_adc[chip_loc][0] - gpio_adc[chip_loc][1];
              // std::cout << "CurrentZero string: " << string << " " << currentZero[string] << '\n';
              currentZeroed[string] = true;
            }
            // replace 2.497 with zero'd value from startup? maybe use ref
            m_batterydata.stringCurrents[string] = BMS_ISENSE_DIR[string]*
              (BMS_ISENSE_RANGE[string] * (gpio_adc[chip_loc][0] - gpio_adc[chip_loc][1] - currentZero[string])/10 / 0.625); // unit mA
              // std::cout << "Raw voltages: " << gpio_adc[chip_loc][0] << " " << gpio_adc[chip_loc][1] << "\n";
            m_batterydata.totalCurrent += m_batterydata.stringCurrents[string];
            // std::cout << "Current: " << m_batterydata.stringCurrents[string] << '\n';
          }

          // Calculate thermistors: present on even chips (lower chip of each box)
          if (!(i % 2)) {
            for (uint8_t j = 0; j < 2; j++) {
              // calculate resistance from voltage
              float thermvolt = gpio_adc[chip_loc][j]/10000.0;
              float resistance;
              if (chip_loc == 6 || chip_loc == 7 || chip_loc == 8 || chip_loc == 9) {
                resistance = (4700.0 * thermvolt)/(5.0 - thermvolt);
              } else {
                resistance = (10000.0 * thermvolt)/(5.0 - thermvolt);
              }
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
                m_batterydata.allTemperatures[(string*NUM_CHIPS/NUM_STRINGS) + i+j] = steinhart;
                //std::cout << "NTC " << j+1 << " " << (string*NUM_CHIPS/NUM_STRINGS) + i+j << ": " << steinhart << " " << chip_loc << '\n';
                if (steinhart < minTemp && steinhart != 0){
                  minTemp = steinhart;
                  minTemp_box = (string * NUM_CHIPS / NUM_STRINGS) + j + i;
                }
                if (steinhart > maxTemp) {
                  maxTemp = steinhart;
                  maxTemp_box = (string * NUM_CHIPS / NUM_STRINGS) + j + i;
                }
                if (steinhart < minTemps[(tempSelect+1)%2][string]) {
                  minTemps[(tempSelect+1)%2][string] = steinhart;
                }
                // max temp check
                if (steinhart > BMS_TEMPERATURE_THRESHOLD && *DI_ChargeSwitch) {
                  throwBmsFault();
                }
              }

            }
          }
        }
      }

      if (!SoCinitialized) {
        if (minVoltage < SoC_lookup[0]) {
          SoC = 0;
          millicoulombs = 0;
          SoCinitialized = true;
        } else {
        //printf("Min voltage is  %u\n", minVoltage);
          for (uint8_t j = 0; j < 101; j++) {
            if (minVoltage >= SoC_lookup[j] && minVoltage < SoC_lookup[j+1]) {
              SoC = j;
              SoCinitialized = true;
              millicoulombs = mc_lookup[SoC];
              //printf("SoC initialized to %u with %d coulombs to 0\n", SoC, millicoulombs/1000);
              break;
            }
          }
        }
        fuelgauge->write(0.5 + (0.005*SoC));
        if (SoC < BMS_SOC_RESERVE_THRESHOLD) {
          ioexp_bits |= (1 << MCP_PIN_LOWFUEL);
        }
      } else {
        millicoulombs -= m_batterydata.totalCurrent/m_frequency/NUM_STRINGS;
        if (millicoulombs < 0) {
          SoC = 0;
        } else {
          for (uint8_t j = 0; j < 101; j++) {
            if (millicoulombs >= mc_lookup[j] && millicoulombs < mc_lookup[j+1]) {
              SoC = j;

              fuelgauge->write(0.5 + (0.005*SoC));
              if (SoC < BMS_SOC_RESERVE_THRESHOLD) {
                ioexp_bits |= (1 << MCP_PIN_LOWFUEL);
              }
              break;
            }
          }
        }
        // printf("Current of %dmA MC at %u SoC at %u\n", m_batterydata.totalCurrent, millicoulombs, SoC);
      }
      
      *led3 = 0;
      for (uint8_t i = 0; i < NUM_STRINGS; i++) {
        //std::cout << "Adding " << totalVoltage[i] << " " << totalVoltage[i]/28 << "\n";
        packVoltage += totalVoltage[i];
      }
      packVoltage /= NUM_STRINGS;



      // check difference between strings
      for (uint8_t i = 0; i < NUM_STRINGS; i++) {
        //std::cout << "Stringcheck: " << (int)totalVoltage[i] - (int)packVoltage << "\n";
        if (abs((int)totalVoltage[i] - (int)packVoltage) > BMS_STRING_DIFFERENCE_THRESHOLD) {
          //std::cout << "String check failed " << packVoltage << " " << totalVoltage[0] << "\n";
          stringcheckOK = false;
          *led3 = 1;      
          ioexp_bits |= (1 << MCP_PIN_EGR);
          break;
        }
      }
    

      float totalCurrent_scaled = ((float)m_batterydata.totalCurrent)/1000.0;
      

      // float totalVoltage_scaled = ((float)packVoltage)/1000.0;
      // std::cout << "Pack Voltage: " << ceil(totalVoltage_scaled * 10.0) / 10.0 << "V"  // round to 1 decimal place
      // << " Current: " << totalCurrent_scaled << "A"
      // << "\nPower: " << ceil(totalCurrent_scaled * (totalVoltage_scaled * 10.0) / 1000.0) / 10.0 << "kW"  // round to 1 decimal place, scale to kW
      // << "\nMax Cell: " << maxVoltage << " " << (char)('A'+(maxVoltage_cell/28)) << (maxVoltage_cell%28)+1
      // << " Min Cell: " << minVoltage << " " << (char)('A'+(minVoltage_cell/28)) << (minVoltage_cell%28)+1
      // << " Avg Cell: " << (totalVoltage_scaled/(NUM_CELLS_PER_CHIP*NUM_CHIPS/NUM_STRINGS))
      // << "\nMax Temp: " << maxTemp << " " << (char)('A'+(maxTemp_box/2)) << (maxTemp_box%2)+1
      // << " Min Temp: " << minTemp << " " << (char)('A'+(minTemp_box/2)) << (minTemp_box%2)+1;
      // std::cout << '\n';
      // std::cout << '\n';

      m_batterysummary.minVoltage = minVoltage;
      m_batterysummary.minVoltage_cell = minVoltage_cell;
      m_batterysummary.maxVoltage = maxVoltage;
      m_batterysummary.maxVoltage_cell = maxVoltage_cell;
      m_batterysummary.minTemp = minTemp;
      m_batterysummary.minTemp_box = minTemp_box;
      m_batterysummary.maxTemp = maxTemp;
      m_batterysummary.maxTemp_box = maxTemp_box;
      m_batterysummary.totalCurrent = m_batterydata.totalCurrent;
      m_batterysummary.totalVoltage = packVoltage;

      m_batterysummary.joules += ((m_batterydata.totalCurrent/1000) * ((int32_t)(packVoltage/1000))/m_frequency);
      m_batterydata.joules = m_batterysummary.joules;

      m_batterydata.soc = SoC;
      m_batterysummary.soc = SoC;
      m_batterysummary.numBalancing = m_batterydata.numBalancing;

      //m_batterydata.totalCurrent = totalCurrent;
      m_batterydata.packVoltage = packVoltage;

      prevMinVoltage = minVoltage;

      mail_t *msg = m_outbox->alloc();
      msg->msg_event = NEW_CELL_DATA;
      m_outbox->put(msg);

      int16_t canPowerScaled = (((int16_t)(packVoltage/1000)) * (m_batterydata.totalCurrent/1000))/100 + 400;

      canPower[0] = (255 & canPowerScaled);
      canPower[1] = canPowerScaled >> 8;

      //uint16_t interpretedcanpower = (canPower[1] << 8) + canPower[0];

      //printf("CanPower: %d %d %d %d %d %d\n", (packVoltage/1000), (m_batterydata.totalCurrent/1000), canPowerScaled, canPower[1], canPower[0], interpretedcanpower);

      canBus->write(CANMessage(2, canPowerSend, 2));
      /*if (!canBus->write(CANMessage(2, canPowerSend, 2))) {
        printf("CAN write failed\n");
      }*/



      if (!startedUp && voltagecheckOK && stringcheckOK) {
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



      // printbuff.str(std::string());

      if (++printCount == CELL_PRINT_MULTIPLE) {
        // Print line of CSV data
        std::cout << std::fixed << std::setprecision(1) << timestamp << ',' << m_batterydata.packVoltage/1000.0 ;
        for (uint16_t i = 0; i < NUM_STRINGS; i++) {
          std::cout << ',' << ((float)m_batterydata.stringCurrents[i])/1000.0;
        }
        std::cout << ',' << totalCurrent_scaled * m_batterydata.packVoltage / 1000000.0 << ',' << m_batterydata.joules/3600 << ',' << (int)m_batterydata.soc;
        for (uint8_t j = 0; j < NUM_STRINGS; j++) {
          for (uint16_t i = 0; i < NUM_CHIPS / NUM_STRINGS * NUM_CELLS_PER_CHIP; i++) {
            std::cout << ',' << m_batterydata.allVoltages[j][i];
          }
          //std::cout << printbuff.str();

          //ThisThread::sleep_for(30);
        }
        for (uint16_t i = 0; i < NUM_CHIPS; i++) { 
          std::cout << ',' << m_batterydata.allTemperatures[i];
        }
        if (*DI_ChargeSwitch) {
          for (uint16_t i = 0; i < NUM_CHIPS; i++) {
            std::cout << ',' << (int)m_batterydata.dieTemps[i]; 
            //std::cout << "chiptemp\n";
          }
        } else {
          for (uint16_t i = 0; i < NUM_CHIPS; i++) {
            std::cout << ',';
            //std::cout << "chiptemp\n";
          }
        }
        std::cout << ',' << (int)m_batterydata.numBalancing;
        std::cout << ',' << (int)errCount;
        std::cout << '\n';
        //uint32_t curtime = t.read_us();

        //std::cout << "Data Print time: " << (t.read_us() - curtime) << "us \n";     

        printCount = 0;
      }



              //uint32_t curtime = t.read_us();
      //std::cout << "Data thread received summary\n";

      std::stringstream printbuff;
      /*float totalVoltage_scaled = ((float)m_batterysummary.totalVoltage)/1000.0;
      float totalCurrent_scaled = ((float)m_batterysummary.totalCurrent)/1000.0;



      printbuff << std::fixed << std::setprecision(1) << "Pack Voltage: " << totalVoltage_scaled << "V"  // round to 1 decimal place
      << " Current: " << totalCurrent_scaled << "A"
      << "\nPower: " << totalCurrent_scaled * (totalVoltage_scaled) / 1000.0 << "kW"  // scale to kW
      << "\nMax Cell: " << m_batterysummary.maxVoltage << " " << (char)('A'+(m_batterysummary.maxVoltage_cell/28)) << (m_batterysummary.maxVoltage_cell%28)+1
      << " Min Cell: " << m_batterysummary.minVoltage << " " << (char)('A'+(m_batterysummary.minVoltage_cell/28)) << (m_batterysummary.minVoltage_cell%28)+1
      << " Avg Cell: " << m_batterysummary.totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS)
      << "\nMax Temp: " << m_batterysummary.maxTemp << " " << (char)('A'+(m_batterysummary.maxTemp_box/2)) << (m_batterysummary.maxTemp_box%2)+1
      << " Min Temp: " << m_batterysummary.minTemp << " " << (char)('A'+(m_batterysummary.minTemp_box/2)) << (m_batterysummary.minTemp_box%2)+1;
      printbuff << "\n\n";
      std::cout << printbuff.str();
      printbuff.str("");*/
      //uint32_t curtime = t.read_us();

      float kwh = ((float)m_batterysummary.joules)/3600000.0;

      //uint16_t avgcell = 

      printbuff.setf(ios::fixed,ios::floatfield);

      //printbuff.str(std::string());

      printbuff << setw(3) << m_batterysummary.totalCurrent/1000 << "A " << setw(3) << m_batterysummary.totalVoltage/1000 << "V" 
      << setw(7) << setprecision(2) << std::showpoint << std::right << kwh << "kWhr" // line is finished so no need for newline char
      << "-:" << setw(3) << m_batterysummary.minVoltage/10 << " +:" << setw(3) << m_batterysummary.maxVoltage/10
      << " A:" << setw(3) << m_batterysummary.totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS/NUM_STRINGS)/10

      << "\r+: " << setw(2) << (int)round(m_batterysummary.maxTemp) << " " << (char)('A'+(m_batterysummary.maxTemp_box/2)) << (m_batterysummary.maxTemp_box%2)+1
      << " -: " << setw(2) << (int)round(m_batterysummary.minTemp) << " " << (char)('A'+(m_batterysummary.minTemp_box/2)) << (m_batterysummary.minTemp_box%2)+1 << "   ";

      //std::cout.setf(ios::fixed,ios::floatfield);
      //std::cout << std::showpoint << setprecision(1) << setw(6) << kwh << "kWhr \n";
      //serial2->printf(printbuff.str().c_str());

      char dispprint[22] = {0x80, // Move to 0,0
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // Block for current bar
        0x94}; // Move to 1,0

      //displayserial->putc(0x80); // move to 0,0

      if (*DI_ChargeSwitch) {
        sprintf(&dispprint[1], "%d", m_batterysummary.numBalancing);
      } else {
        int64_t power = m_batterysummary.totalCurrent*((int64_t)m_batterysummary.totalVoltage)/1000000;
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
      }

      /*for (uint8_t i = 0; i < (19 - fullcount); i++) {
        displayserial->putc(0);
      }*/
      //uint32_t curtime = t.read_us();
      displayserial->write(dispprint, 22);
      //ThisThread::sleep_for(10);
      /*std::cout << "Display len: " << strlen(printbuff.str().c_str()) << "\n";
      std::cout << "Display: " << printbuff.str().c_str() << "\n";*/
      // https://stackoverflow.com/questions/1374468/stringstream-string-and-char-conversion-confusion
      const std::string& dispbuff = printbuff.str();
      const char* dispbuff_cstr = dispbuff.c_str();
      //std::cout << "Display len: " << strlen(dispbuff_cstr) << "\n";
      //std::cout << "Display: " << dispbuff_cstr << "\n";


      //std::cout << "Aout to print to display\n";

      //displayserial->write(printbuff.str().c_str(), strlen(printbuff.str().c_str()));

      displayserial->write(dispbuff_cstr, strlen(dispbuff_cstr));
      //serial->write(dispbuff_cstr, strlen(dispbuff_cstr));
      //printbuff.str(std::string());
      //ThisThread::sleep_for(20);

      //std::cout << "Printed to display\n";

      //std::cout << "Print time: " << (t.read_us() - curtime) << "us \n";
      //displayserial->printf(printbuff.str().c_str());


      //std::cout << "Print time: " << (t.read_us() - curtime) << "us \n";

      //std::cout << "Display Print time: " << (t.read_us() - curtime) << "us \n";
      //std::cout << m_batterysummary.totalCurrent << "A " << m_batterysummary.totalVoltage/1000 << "V " << "Calc: " << m_batterysummary.totalCurrent*(int32_t)m_batterysummary.totalVoltage/1000000 << " " << power/1000 << "kW fullcount: " << (int)fullcount << " final: " << (int)finalchar << " time: " <<  t.read_ms()-startTime << '\n';





    } else {
      std::bitset<16> pecprint(pecStatus);

      mail_t *msg = m_outbox->alloc();
      msg->msg_event = BATT_ERR;
      m_outbox->put(msg);
      // std::cout << "PEC error! " << pecprint << '\n';
      *led3 = 1;      
      ioexp_bits |= (1 << MCP_PIN_EGR);
      errCount++;
    }

    ioexp->write_mask(ioexp_bits, MCP_BMS_THREAD_MASK);
    Watchdog::get_instance().kick();

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

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <bitset>
#include <string>
#include <sstream>
#include <iomanip>
#include <string.h>

#include "mbed.h"
#include "rtos.h"

#include "config.h"
#include "pinout.h"
#include "CanRx.h"
#include "Slcan.h"
#include "LTC6813.h"
#include "LTC681xBus.h"
#include "Data.h"
#include "Telemetry.h"
#include "BmsThread.h"


#if SLCAN_MODE && SLCAN_VERIFY_PATTERN
/*
 * The link instrument: a deterministic function of a free-running frame counter, in place of
 * the real slow tier.
 *
 * The counter travels in the frame, so every frame is self-identifying and self-verifying: a
 * gap in the counters is a lost frame, and a payload that does not match its own counter is
 * corruption that got past the structural check. Those two numbers are what this exists to
 * measure and neither can be had from real telemetry, because there is nothing to compare a
 * plausible-looking cell voltage against.
 *
 * The channel id cycles so the stream exercises a spread of IDs the way real telemetry does,
 * rather than hammering one. Note that the ids it walks over DO overlap the real map -- this
 * is a bench mode, and a capture taken with it on is not telemetry.
 */
static void slcan_emit_synthetic(uint32_t counter)
{
    uint8_t payload[8];
    payload[0] = (uint8_t)(counter >> 8);
    payload[1] = (uint8_t)(counter & 0xFF);
    for (int i = 2; i < 8; i++) {
        payload[i] = (uint8_t)((counter * 7u + i) & 0xFF);
    }
    slcan_emit(SLCAN_SYNTH_BASE | (counter & 0x3F), payload, 8, true);
}
#endif // SLCAN_MODE && SLCAN_VERIFY_PATTERN

#if LINK_TEST
/*
 * One line of the link test pattern:
 *
 *     LT<8 hex digits of seq>:<LINK_TEST_PAYLOAD_LEN payload bytes>\n
 *
 * The payload is a walking sequence of printable ASCII, 33..126, phase-shifted by the line's
 * sequence number so no two consecutive lines are identical -- a repeating pattern would let
 * a duplicated or dropped byte hide inside its own repetition. Every byte is therefore a
 * function of (seq, offset) alone, which is what lets the host rebuild the expected stream
 * from any single intact header and pinpoint each bad byte.
 *
 * Emitted with cout.write() rather than the << chain, so this measures the link and not the
 * iostream formatting that Step 2 already measured separately.
 */
static void emit_link_test_line(uint32_t seq)
{
    static const char hexd[] = "0123456789ABCDEF";
    static char line[11 + LINK_TEST_PAYLOAD_LEN + 1];

    line[0] = 'L';
    line[1] = 'T';
    for (int i = 0; i < 8; i++) {
        line[2 + i] = hexd[(seq >> (28 - 4 * i)) & 0xF];
    }
    line[10] = ':';
    for (uint32_t i = 0; i < LINK_TEST_PAYLOAD_LEN; i++) {
        line[11 + i] = (char)(33 + ((seq * 7u + i) % 94u));
    }
    line[11 + LINK_TEST_PAYLOAD_LEN] = '\n';

#if LINK_TEST_CHUNK > 0
    for (uint32_t off = 0; off < sizeof(line); off += LINK_TEST_CHUNK) {
        uint32_t n = sizeof(line) - off;
        if (n > LINK_TEST_CHUNK) n = LINK_TEST_CHUNK;
        std::cout.write(line + off, n);
    }
#else
    std::cout.write(line, sizeof(line));
#endif
}
#endif

BMSThread::BMSThread(Mail<mail_t, MSG_QUEUE_SIZE>* inbox_main, Mail<chargerdata_t, MSG_QUEUE_SIZE>* inbox_charger, 
  Mail<inverterdata_t, MSG_QUEUE_SIZE>* inbox_inverter, LTC681xBus* bus, LTC6813Bus* bus_6813) : 
   m_inbox_main(inbox_main), m_inbox_charger(inbox_charger), m_inbox_inverter(inbox_inverter), m_bus(bus), m_6813bus(bus_6813) {
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

// Fault state as bit sets over BmsFaultBit. `Now` is rebuilt from scratch every scan; `Latched`
// only ever gains bits, and like faultThrown it clears on reset alone. Both are transmitted,
// because they diverge: a fault that appears for one scan and clears leaves the latched byte
// set for the rest of the drive, and the latched byte is what the throttle limit follows.
uint8_t faultBitsNow = 0;
uint8_t faultBitsLatched = 0;

// Record a fault without taking any action. Used where the existing code sets a flag or a dash
// light but deliberately does not call throwBmsFault() -- string imbalance and PEC failures --
// so that observing the fault does not change what the car does.
static inline void setFault(BmsFaultBit fault) {
  faultBitsNow |= (uint8_t)(1u << fault);
  faultBitsLatched |= (uint8_t)(1u << fault);
}

// Set the balancing bit for a flat cell index. Bit n of byte b is cell b*8+n, matching both the
// DBC and the ordering of allVoltages.
static inline void markBalancing(batterydata_t &d, uint8_t cellIndex) {
  d.balanceMask[cellIndex >> 3] |= (uint8_t)(1u << (cellIndex & 7));
}

// char canPower[2];
// char* const canPowerSend = canPower;

union bytes {
    uint8_t bytes[8];
    uint32_t words[2];
    uint64_t bits;
} inverterCAN;

uint8_t* const inverterCANSend = inverterCAN.bytes;


batterydata_t m_batterydata;
batterysummary_t m_batterysummary;

chargerdata_t m_chargerdata;
inverterdata_t m_inverterdata;


void BMSThread::throwBmsFault(BmsFaultBit fault) {
  setFault(fault);
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

  // uint32_t prevTime = 0;
  m_batterysummary.joules = 0;

  uint16_t printCount = 0;
  uint16_t errCount = 0;

  uint8_t canrun = 0;

  int totalCurrent_previous = 0;

  m_chargerdata.VAC = 0;
  m_chargerdata.IAC = 0;


  //uint32_t curtime = t.read_us();
  //std::cout << "Data thread received init\n";

#if STDIO_PARITY_EVEN
  // The write comes first on purpose. mbed builds the stdio serial object lazily on first
  // use, and its constructor sets 8N1 -- so reconfiguring the UART before that point would
  // simply be undone. `serial` is a separate object on the same pins, but set_format() acts
  // on the shared UART peripheral, so setting it here sticks for stdio too.
  std::cout << "\n";
  std::cout.flush();
  serial->format(8, SerialBase::Even, 1);
#endif

#if SLCAN_MODE
  slcan_init();
#endif

// No CSV header in SLCAN mode: a line of text in the stream is not a valid frame, and the host
// would have to special-case it to avoid counting it as corruption. Dual-emit already requires
// the host to skip non-frame lines, so the header comes back there. Deliberately not EMIT_CSV,
// which is false under LINK_TEST -- the link test has always printed this header first and the
// archived captures start with it.
#if !SLCAN_MODE || SLCAN_DUAL_EMIT
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
  // The four canRx* fields instrument the CAN receive path; all four should stay at 0. See
  // CanRx.h for what each one means and which half of the path it covers.
  std::cout << ",hsTemp,numBalancing,errCount,canDrop,canOvr,canQPeak,canLatUs\n";
#endif // !SLCAN_MODE || SLCAN_DUAL_EMIT

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
    // lowest(), not min(). For a floating-point type min() is the smallest positive normalised
    // value (~1.175e-38), not the most negative one -- so this used to start the running maximum
    // at approximately zero. Any reading above freezing still won, which is why the car never
    // showed it; but with the whole pack below 0 C nothing ever exceeded the initialiser and
    // maxTemp stayed at ~0, reporting TempMax as 0.0 C with maxTemp_box left at its invalid 255.
    // min() *is* the most negative value for integer types, which is what makes this misread.
    // Found by bench dual-emit, where every thermistor reads -273.1 C -- see notes/plans.
    float maxTemp = std::numeric_limits<float>::lowest();
    uint8_t maxTemp_box = 255;
    unsigned int totalVoltage[NUM_STRINGS] = {0};
    //stringCurrents[NUM_STRINGS] = 0;
    m_batterydata.numBalancing = 0;
    m_batterydata.totalCurrent = 0;
    // Cleared beside numBalancing so the mask and the count can only ever describe the same
    // scan; verification 1 in the ID allocation is that popcount(mask) == numBalancing.
    memset(m_batterydata.balanceMask, 0, sizeof(m_batterydata.balanceMask));

    // Rebuilt every scan. The latched copy is not touched here -- that is the point of it.
    faultBitsNow = 0;

    uint16_t ioexp_bits = 0;

    uint32_t timestamp = 0;

    // Whether this scan is the one that logs. Computed up front because the die-temperature
    // read is now gated on it, and that happens long before printCount is advanced.
    //
    // Deliberately mirrors `++printCount == CELL_PRINT_MULTIPLE` exactly, including the
    // documented CELL_PRINT_MULTIPLE == 0 case, where the comparison is against a 16-bit wrap.
    const bool loggingScan = ((uint16_t)(printCount + 1) == CELL_PRINT_MULTIPLE);

    
    while(!m_inbox_inverter->empty()) {
      osEvent evt = m_inbox_inverter->get();

      if (evt.status == osEventMail) {
        //std::cout << "Received some mail\n";
        inverterdata_t *msg = (inverterdata_t *)evt.value.p;
        m_inverterdata = *msg;
        m_inbox_inverter->free(msg);
      } else {
        // Was a bare line printed into the data stream, which lands in the middle of whatever
        // record is being written -- the mechanism behind 5,099 broken rows in the archive.
        // A coded frame carries the same information and cannot damage a data frame.
#if SLCAN_MODE
        telemetry_emit_diag(BMS_DIAG_INVALID_MESSAGE, 1, (uint16_t)evt.status, 0);
#else
        std::cout << "Invalid inverter data received\n";
#endif
      }
    }
    while(!m_inbox_charger->empty()) {
      osEvent evt = m_inbox_charger->get();

      if (evt.status == osEventMail) {
        //std::cout << "Received some mail\n";
        chargerdata_t *msg = (chargerdata_t *)evt.value.p;
        m_chargerdata = *msg;
        m_inbox_charger->free(msg);
      } else {
#if SLCAN_MODE
        telemetry_emit_diag(BMS_DIAG_INVALID_MESSAGE, 2, (uint16_t)evt.status, 0);
#else
        std::cout << "Invalid charger data received\n";
#endif
      }
    }


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
    // Read on the logging scan only, and regardless of the charge switch.
    //
    // It used to run every scan but was only ever logged on the print interval, so four reads
    // in five were thrown away -- getDieTemps() is self-contained (it starts its own ITMP
    // conversion, polls it and reads Status Group A), so it can be gated freely.
    //
    // Dropping the charge-switch gate is a behaviour change: die temperatures are now recorded
    // while driving, where the CSV previously left the columns empty. It assumes the aux
    // reading is valid outside charging, which has not been confirmed on the car -- that is
    // verification 11 in the ID allocation.
    if (loggingScan) {
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


    // BENCH_IGNORE_PEC treats a failed read as good so the numbers behind it -- whatever the
    // isoSPI returned with nothing on the far end -- still flow through the crunch, pack and
    // emit path. See config.h. Off on the car, where this would be dangerous.
    if (!pecStatus || BENCH_IGNORE_PEC) {
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
              // if ((chip_loc == 6 || chip_loc == 7 || chip_loc == 8 || chip_loc == 9)) {
              //   // add 5mV for top of cellbox and bottom of cellbox
              //   if ((i%2 && index == 13) || (!i%2 && index == 0)) {
              //     voltage += 5;
              //   }
              //   // add 7 mV for top of cellbox for current sense
              //   // only works if there are no additional current sensors on the string!
              //   if (chip_loc == BMS_ISENSE_MAP[string] && index == 13) {
              //     voltage += 7;
              //   }
              // }
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

              // The flat 0..167 cell index, named once and used for the min/max cells and the
              // balancing mask alike. Writing the expression out three times is how the mask's
              // bit ordering would drift from the voltage ordering, which is not a failure that
              // announces itself: the log would simply attribute balancing to the wrong cells.
              const uint8_t cellIndex =
                index + NUM_CELLS_PER_CHIP*(i + (string * NUM_CHIPS / NUM_STRINGS));

              if (voltage < minVoltage && voltage != 0) {
                minVoltage = voltage;
                minVoltage_cell = cellIndex;
              }
              if (voltage > maxVoltage) {
                maxVoltage = voltage;
                maxVoltage_cell = cellIndex;
              }
              //totalVoltage += voltage;
              //serial->printf("%dmV ", voltage);

              if (voltage >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH) {
                // Set fault line
                //std::cout << "***** BMS LOW VOLTAGE FAULT *****\nVoltage at " << voltage << "\n\n";
                throwBmsFault(BMS_FAULT_CELL_OVERVOLTAGE);
                voltagecheckOK = false;
                ioexp_bits |= (1 << MCP_PIN_BMSERR);
              }
              if (voltage <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW) {
                // Set fault line
                //std::cout << "***** BMS HIGH VOLTAGE FAULT *****\nVoltage at " << voltage << "\n\n";
                throwBmsFault(BMS_FAULT_CELL_UNDERVOLTAGE);
                voltagecheckOK = false;
                ioexp_bits |= (1 << MCP_PIN_BMSERR);
              }

              // Discharge cells if enabled

              LTC6813::Configuration& conf = m_6813bus->m_chips[chip_loc].getConfig();
              if(!faultThrown && m_discharging && BALANCE_EN) {
                if ((minTemps[tempSelect][string] < BMS_LOW_TEMPERATURE_THRESHOLD) && *DI_ChargeSwitch) {
                  // Discharge to heat

                  //printf("DISCHARGE CHIP: %d CELL: %d: %dmV (%dmV)\n", chip_loc, index, voltage, (voltage - prevMinVoltage));

                  // Enable discharging
                  conf.dischargeState.value |= (1 << j);
                  m_batterydata.numBalancing++;
                  markBalancing(m_batterydata, cellIndex);

                  // And turn on G light to show low temp
                  ioexp_bits |= (1 << MCP_PIN_G);
                } else if (((*DI_ChargeSwitch && (voltage > BMS_BALANCE_VOLTAGE_THRESHOLD)) || !stringcheckOK) && ((voltage > prevMinVoltage) && (voltage - prevMinVoltage > BMS_DISCHARGE_THRESHOLD)) && (totalCurrent_previous > BMS_BALANCE_CURRENT_LIMIT)) {
                  // else if normal balancing just turn on the balancing resistor
                  //printf("DISCHARGE CHIP: %d CELL: %d: %dmV (%dmV)\n", chip_loc, index, voltage, (voltage - prevMinVoltage));
                  conf.dischargeState.value |= (1 << j);
                  m_batterydata.numBalancing++;
                  markBalancing(m_batterydata, cellIndex);
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
            if (!currentZeroed[string] && t.read_ms() > INIT_DELAY) {
              currentZero[string] = gpio_adc[chip_loc][0] - gpio_adc[chip_loc][1];
              // std::cout << "CurrentZero string: " << (int)string << " " << currentZero[string] << '\n';
              currentZeroed[string] = true;
            }
            // replace 2.497 with zero'd value from startup? maybe use ref
            m_batterydata.stringCurrents[string] = BMS_ISENSE_DIR[string]*
              (BMS_ISENSE_RANGE[string] * (gpio_adc[chip_loc][0] - gpio_adc[chip_loc][1] - currentZero[string])/10 / 0.625); // unit mA
              // std::cout << "Raw voltages: " << gpio_adc[chip_loc][0] << " " << gpio_adc[chip_loc][1] << "\n";
            m_batterydata.totalCurrent += m_batterydata.stringCurrents[string];
            // std::cout << "Current string: " << (int)string << " " << m_batterydata.stringCurrents[string] << '\n';
          }

          // Calculate thermistors: present on even chips (lower chip of each box)
          if (!(i % 2)) {
            for (uint8_t j = 0; j < 2; j++) {
              // calculate resistance from voltage
              float thermvolt = gpio_adc[chip_loc][j]/10000.0;
              float resistance = (10000.0 * thermvolt)/(5.0 - thermvolt);
              // if (chip_loc == 6 || chip_loc == 7 || chip_loc == 8 || chip_loc == 9) {
              //   resistance = (4700.0 * thermvolt)/(5.0 - thermvolt);
              // } else {
              //   resistance = (10000.0 * thermvolt)/(5.0 - thermvolt);
              // }
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
                if (steinhart < minTemp && steinhart != 0 && (((string * NUM_CHIPS / NUM_STRINGS) + j + i) != 1)) {
                  minTemp = steinhart;
                  minTemp_box = (string * NUM_CHIPS / NUM_STRINGS) + j + i;
                }
                if (steinhart > maxTemp) {
                  maxTemp = steinhart;
                  maxTemp_box = (string * NUM_CHIPS / NUM_STRINGS) + j + i;
                }
                if ((steinhart < minTemps[(tempSelect+1)%2][string])  && (((string * NUM_CHIPS / NUM_STRINGS) + j + i) != 1)) {
                  minTemps[(tempSelect+1)%2][string] = steinhart;
                }
                // max temp check
                if (steinhart > BMS_TEMPERATURE_THRESHOLD && *DI_ChargeSwitch) {
                  // No dash indication, unlike the voltage and string faults: throwBmsFault()
                  // sets led4 but no ioexp bit. That is deliberate and not a missing-indicator
                  // bug -- the 42 C stop is conservative against a cell datasheet that only
                  // begins charge throttling at 45 C, so the fault fires with margin in hand
                  // and there is nothing for a driver to react to.
                  throwBmsFault(BMS_FAULT_OVERTEMP);
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
          // Records the fault without calling throwBmsFault(), matching what this branch has
          // always done: it lights EGR and clears stringcheckOK but does not stop discharge.
          setFault(BMS_FAULT_STRING_IMBALANCE);
          *led3 = 1;
          ioexp_bits |= (1 << MCP_PIN_EGR);
          break;
        }
      }
    

      float totalCurrent_scaled = ((float)m_batterydata.totalCurrent)/1000.0;

      totalCurrent_previous = m_batterydata.totalCurrent;
      

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
      // Mean cell voltage in mV, the same expression the display below uses -- note the
      // divisor is the cells in *series*, 84, because the two strings are in parallel.
      m_batterysummary.avgVoltage =
        (uint16_t)(packVoltage / (NUM_CELLS_PER_CHIP * NUM_CHIPS / NUM_STRINGS));

      m_batterysummary.joules += ((m_batterydata.totalCurrent/1000) * ((int32_t)(packVoltage/1000))/m_frequency);
      m_batterydata.joules = m_batterysummary.joules;

      m_batterydata.soc = SoC;
      m_batterysummary.soc = SoC;
      m_batterysummary.numBalancing = m_batterydata.numBalancing;

      //m_batterydata.totalCurrent = totalCurrent;
      m_batterydata.packVoltage = packVoltage;

      prevMinVoltage = minVoltage;

      // Fast-tier telemetry: one set per scan, 10 Hz driving and 2 Hz charging. Emitted inside
      // the !pecStatus branch because these three frames carry measurements, and a scan whose
      // PEC failed has none -- transmitting the previous scan's numbers with a fresh timestamp
      // would be indistinguishable from a real reading that happened not to change. The status
      // frame is different and goes out either way; see below.
      telemetry_emit_pack(m_batterydata);
      telemetry_emit_cell_summary(m_batterysummary);
      telemetry_emit_temp_summary(m_batterysummary, m_inverterdata.heatsinktemp);

      // mail_t *msg = m_outbox->alloc();
      // msg->msg_event = NEW_CELL_DATA;
      // m_outbox->put(msg);

      // int16_t canPowerScaled = (((int16_t)(packVoltage/1000)) * (m_batterydata.totalCurrent/1000))/100 + 400;

      // canPower[0] = (255 & canPowerScaled);
      // canPower[1] = canPowerScaled >> 8;

      // //uint16_t interpretedcanpower = (canPower[1] << 8) + canPower[0];

      // //printf("CanPower: %d %d %d %d %d %d\n", (packVoltage/1000), (m_batterydata.totalCurrent/1000), canPowerScaled, canPower[1], canPower[0], interpretedcanpower);

      // canBus->write(CANMessage(2, canPowerSend, 2));
      // /*if (!canBus->write(CANMessage(2, canPowerSend, 2))) {
      //   printf("CAN write failed\n");
      // }*/

      inverterCAN.bits = 0;

      uint8_t bms_bit = 0;
      if (*DI_ReverseSwitch || !voltagecheckOK) {
        bms_bit = 1;
      }

      inverterCAN.bits = (*DI_BrakeSwitch << 26) | (bms_bit << 29) | ((uint64_t)(canrun & 0b11) << 30) | ((uint64_t)(canrun & 0b11) << 46);
      inverterCAN.bytes[6] = 0xFF; // regen preset

      uint32_t crc = 0xffffffff;
      crc = crc32_word(crc, inverterCAN.words[0]);
      crc = crc32_word(crc, inverterCAN.words[1]);

      inverterCAN.bytes[7] = crc & 0xFF;

      canBus->write(CANMessage(0x3F, inverterCANSend, 8));

      // Echo it into the log. A CAN node does not receive its own frames, so without this the
      // proxy never sees 0x3F and the single most important signal for the throttle
      // investigation -- the BMS limit bit at 29 -- is silently absent from a log that looks
      // otherwise complete. Emitted at transmit request, so it is evidence of intent rather
      // than proof the frame won arbitration.
      telemetry_echo_frame(TLM_ID_VCU_CONTROL, inverterCANSend, 8);

      // std::cout << "CAN bytes: ";
      // for (int p = 0; p < 8; p++) {

      //   std::bitset<8> px(inverterCANSend[p]);

      //   std::cout << px << " ";
      // }

      // std::bitset<64> x(inverterCAN.bits);
      // // std::cout << x << '\n';

      // std::cout << "\nCAN bits: " << x << " " << (int)canrun << " " << (int)(canrun & 0b11) << "\n";

      canrun++;


      if (!startedUp && voltagecheckOK && stringcheckOK) {
        *DO_BattContactor = 1;
        startedUp = true;
        *led1 = 1;
        // std::cout << "Starting up\n";
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
        // Read-and-clear, once, here. canRxMaxLatencyUs is a maximum over the logging interval
        // and it has exactly one owner: if both the link-health frame and the CSV clear it,
        // each reports the maximum over only part of the interval and both understate it.
        const uint32_t maxLatencyUs = canRxMaxLatencyUs;
        canRxMaxLatencyUs = 0;
        (void)maxLatencyUs;  // LINK_TEST builds emit neither consumer

#if PRINT_TIMING
        // Wall time of the whole CSV emit. Under UnbufferedSerial this is formatting plus
        // transmission serialised, because the write spins on the TX-ready flag; under
        // BufferedSerial with a TX buffer larger than one record the write returns once
        // queued, so the same number is the formatting cost on its own. The difference
        // between the two builds is what the UART actually costs. See Step 2 of
        // notes/plans/vcu-logging-refactor.md in the 914 notes repo.
        uint32_t printStartUs = us_ticker_read();
#endif
#if SLCAN_MODE
#if SLCAN_VERIFY_PATTERN
        {
          // Link instrument, not telemetry: a self-verifying counter pattern in place of the
          // real slow tier. Block boundaries are handled inside slcan_emit() now, so that
          // forwarded bus traffic is counted into the blocks too.
          static uint32_t slcanCounter = 0;
          for (uint16_t k = 0; k < SLCAN_FRAMES_PER_CYCLE; k++) {
            slcan_emit_synthetic(slcanCounter++);
          }
        }
#else
        // Slow tier: cells, thermistors, die temperatures, link health, balancing mask.
        telemetry_emit_slow(m_batterydata, maxLatencyUs);
#endif
#elif LINK_TEST
        {
          static uint32_t linkTestSeq = 0;
          for (uint16_t k = 0; k < LINK_TEST_LINES_PER_PRINT; k++) {
            emit_link_test_line(linkTestSeq++);
          }
        }
#endif

#if EMIT_CSV
#if SLCAN_DUAL_EMIT
        // Dual-emit: the CSV shares the stream with the frames, so it has to share the frame
        // lock as well. Without it, a frame forwarded from the main loop lands in the middle of
        // a CSV record and produces exactly the splice this design exists to prevent.
        slcan_lock();
#endif
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
        // Always populated now. The read moved to the logging scan and lost its charge-switch
        // gate, so these columns are no longer empty while driving -- the field count is
        // unchanged, but a parser that treated an empty die-temp column as "not charging" will
        // need to look at the charge-switch status instead.
        for (uint16_t i = 0; i < NUM_CHIPS; i++) {
          std::cout << ',' << (int)m_batterydata.dieTemps[i];
        }
        std::cout << ',' << (int)m_inverterdata.heatsinktemp;
        std::cout << ',' << (int)m_batterydata.numBalancing;
        std::cout << ',' << (int)errCount;
        std::cout << ',' << (unsigned long)canRxSwDrops;
        std::cout << ',' << (unsigned long)canRxHwOverruns;
        std::cout << ',' << (unsigned long)canRxQueuePeak;
        // canLatUs is the max over this print interval; it was read and cleared once above.
        // See CanRx.h for why a lifetime max would be useless here.
        std::cout << ',' << (unsigned long)maxLatencyUs;
        std::cout << '\n';
#if SLCAN_DUAL_EMIT
        slcan_unlock();
#endif
#endif // EMIT_CSV

#if PRINT_TIMING
        // Timed before the report below is emitted, so the report is not in its own numbers.
        uint32_t printUs = us_ticker_read() - printStartUs;
        static uint32_t timingMin = 0xFFFFFFFF;
        static uint32_t timingMax = 0;
        static uint32_t timingSum = 0;
        static uint16_t timingCount = 0;

        if (printUs < timingMin) timingMin = printUs;
        if (printUs > timingMax) timingMax = printUs;
        timingSum += printUs;

        if (++timingCount >= PRINT_TIMING_SAMPLES) {
          printf("PRINTTIME: n=%u min=%luus avg=%luus max=%luus\n",
                 (unsigned)timingCount,
                 (unsigned long)timingMin,
                 (unsigned long)(timingSum / timingCount),
                 (unsigned long)timingMax);
          timingMin = 0xFFFFFFFF;
          timingMax = 0;
          timingSum = 0;
          timingCount = 0;
        }
#endif

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
      << "\nMax Temp: " << m_batterysummary.maxTemp << " " << (char)('A'+(m_batterysummary.maxTemp_box/2)) << (m_batterysummary.maxTemp_box%2)+1
      << " Min Temp: " << m_batterysummary.minTemp << " " << (char)('A'+(m_batterysummary.minTemp_box/2)) << (m_batterysummary.minTemp_box%2)+1;
      printbuff << "\n\n";
      std::cout << printbuff.str();
      printbuff.str("");*/
      //uint32_t curtime = t.read_us();

      float kwh = ((float)m_batterysummary.joules)/3600000.0;

      printbuff.setf(ios::fixed,ios::floatfield);

      //printbuff.str(std::string());

      printbuff << setw(3) << m_batterysummary.totalCurrent/1000 << "A " << setw(3) << m_batterysummary.totalVoltage/1000 << "V" 
      << setw(7) << setprecision(2) << std::showpoint << std::right << kwh << "kWhr" // line is finished so no need for newline char
      << "-:" << setw(3) << m_batterysummary.minVoltage/10 << " +:" << setw(3) << m_batterysummary.maxVoltage/10
      << " A:" << setw(3) << m_batterysummary.totalVoltage/(NUM_CELLS_PER_CHIP*NUM_CHIPS/NUM_STRINGS)/10

      << "\r+:" << setw(2) << (int)round(m_batterysummary.maxTemp) << " " << (char)('A'+(m_batterysummary.maxTemp_box/2)) << (m_batterysummary.maxTemp_box%2)+1
      << " -:" << setw(2) << (int)round(m_batterysummary.minTemp) << " " << (char)('A'+(m_batterysummary.minTemp_box/2)) << (m_batterysummary.minTemp_box%2)+1 << " I:" << (int)round(m_inverterdata.heatsinktemp);

      //std::cout.setf(ios::fixed,ios::floatfield);
      //std::cout << std::showpoint << setprecision(1) << setw(6) << kwh << "kWhr \n";
      //serial2->printf(printbuff.str().c_str());

      char dispprint[22] = {0x80, // Move to 0,0
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // Block for current bar
        0x94}; // Move to 1,0

      //displayserial->putc(0x80); // move to 0,0

      if (*DI_ChargeSwitch) {
        sprintf(&dispprint[1], "%3dV %2dA %3d", m_chargerdata.VAC, m_chargerdata.IAC, m_batterydata.numBalancing);
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

      mail_t *msg = m_inbox_main->alloc();
      msg->msg_event = BATT_ERR;
      m_inbox_main->put(msg);
      // std::cout << "PEC error! " << pecprint << '\n';
      *led3 = 1;
      ioexp_bits |= (1 << MCP_PIN_EGR);
      errCount++;
      setFault(BMS_FAULT_PEC);
      // Reported as its own frame carrying the failing chip mask. The 2021 logs printed this
      // sort of thing straight into the middle of a CSV record, which is why 5,099 archived
      // rows are structurally broken; a coded frame cannot corrupt a data frame, so the error
      // no longer has to choose between being reported and being safe to report.
      telemetry_emit_diag(BMS_DIAG_PEC_FAILURE, pecStatus, errCount, 0);
    }

    ioexp->write_mask(ioexp_bits, MCP_BMS_THREAD_MASK);

    // Status frame, emitted every scan whether or not the PEC read succeeded -- it carries the
    // fault and status bits and the ioexp mirror, all of which are meaningful (arguably most
    // meaningful) on a scan that failed. Placed after write_mask() so byte 3 mirrors the
    // literal value the dash was given rather than a reconstruction of it.
    {
      BmsStatusFields sf;
      sf.faultLatched = faultBitsLatched;
      sf.faultNow     = faultBitsNow;
      sf.status =
          (uint8_t)((m_discharging                            ? 1u : 0u) << BMS_STATUS_DISCHARGING)
        | (uint8_t)((*DI_ChargeSwitch                         ? 1u : 0u) << BMS_STATUS_CHARGE_SWITCH)
        | (uint8_t)((*DO_ChargeEnable                         ? 1u : 0u) << BMS_STATUS_CHARGE_EN)
        | (uint8_t)((*DO_BattContactor                        ? 1u : 0u) << BMS_STATUS_CONTACTORS)
        | (uint8_t)((m_batterydata.numBalancing > 0           ? 1u : 0u) << BMS_STATUS_BALANCING)
        | (uint8_t)((SoC < BMS_SOC_RESERVE_THRESHOLD          ? 1u : 0u) << BMS_STATUS_SOC_RESERVE)
        | (uint8_t)((voltagecheckOK                           ? 1u : 0u) << BMS_STATUS_VCHECK_OK)
        | (uint8_t)((stringcheckOK                            ? 1u : 0u) << BMS_STATUS_STRINGCHECK_OK);
      sf.ioexpOut = (uint8_t)(ioexp_bits & 0xFF);
#if TELEMETRY_EMIT_KNOBS
      // The knob build reads the whole configured input range, not just the two pins the
      // switches are believed to be on, and shares the one I2C read with the status frame
      // below rather than doing a second.
      const uint8_t gpiPortB =
          (uint8_t)((ioexp->read_mask(MCP_BMS_THREAD_READ_MASK_ALL) >> 8) & 0xFF);
      sf.ioexpIn = gpiPortB;
#elif TELEMETRY_READ_GPIO_INPUTS
      // MCP23017 port B, pin 8+n in bit n.
      sf.ioexpIn = (uint8_t)((ioexp->read_mask(MCP_BMS_THREAD_READ_MASK) >> 8) & 0xFF);
#else
      sf.ioexpIn = 0;
#endif
      sf.errCount = errCount;
      telemetry_emit_status(sf);

#if TELEMETRY_EMIT_KNOBS
      {
        BmsKnobFields kf;
        kf.knob1Raw = knob1->read_u16();
        kf.knob2Raw = knob2->read_u16();
        kf.knob3Raw = knob3->read_u16();
        kf.gpiPortB = gpiPortB;
        kf.gpiMask  = (uint8_t)((MCP_BMS_THREAD_READ_MASK_ALL >> 8) & 0xFF);
        telemetry_emit_knobs(kf);
      }
#endif
    }

    // Schema identity at 1 Hz in both rate modes, since m_frequency is the scan rate. A
    // consumer joining mid-stream therefore attributes at most one second of data to an unknown
    // schema, for about 0.4% of the link.
    {
      static uint16_t schemaCount = 0;
      if (++schemaCount >= (uint16_t)m_frequency) {
        schemaCount = 0;
        telemetry_emit_schema();
      }
    }

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

#if LINK_TEST && LINK_TEST_SATURATE
    // Saturated link test: skip the pacing sleep so the transmitter is never left without
    // work. This sleep is what puts an idle gap on the wire -- the TX buffer holds only 33 ms
    // of data at 460800, so any pause longer than that lets the line go quiet, and the
    // record-start corruption only ever appears on the first record after such a gap.
    // Enlarging the TX buffer instead was tried and is not viable: it pushes IRAM1 to 88% and
    // the firmware takes a BusFault running off the end of the heap.
    ThisThread::yield();
#else
    ThisThread::sleep_for(m_delay - (t.read_ms()%m_delay));
#endif
  }
}

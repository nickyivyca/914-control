#include "config.h"
#include "pinout.h"

#include <array>
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <iostream>

#include "mbed.h"
#include "rtos.h"

#include "LTC681xChainBus.h"
#include "LTC6813.h"
#include "BmsThread.h"
#include "Data.h"

#include "MCP23017.h"

#include "CanRx.h"

#include "Slcan.h"
#include "Telemetry.h"

#include "MovingAverage.h"

#define CAN_RX_INT_FLAG             (1UL << 0)


UnbufferedSerial* serial;
BufferedSerial* displayserial;
IsrSafeCAN* canBus;

MCP23017* ioexp;

PwmOut* fuelgauge;

DigitalOut* led1;
DigitalOut* led2;
DigitalOut* led3;
DigitalOut* led4;
DigitalOut* DO_Tach;
DigitalOut* DO_BattContactor;
DigitalOut* DO_BattContactor2;
DigitalOut* DO_DriveEnable;
DigitalOut* DO_ChargeEnable;

DigitalIn* DI_ChargeSwitch;
DigitalIn* DI_BrakeSwitch;
DigitalIn* DI_ReverseSwitch;

AnalogIn* knob1;

Mail<mail_t, MSG_QUEUE_SIZE> inbox_main;
Mail<mail_t, MSG_QUEUE_SIZE> inbox_bms;
Mail<chargerdata_t, MSG_QUEUE_SIZE> inbox_bms_charger;
Mail<inverterdata_t, MSG_QUEUE_SIZE> inbox_bms_inverter;

MovingAverage <int16_t, 16> hstempfilter;

Ticker tachometer;

void initIO();
void tach_update();
void print_cpu_stats();
void print_stack_stats();
void canRX();
void canOverrun();
void processCAN();
void sendChargerInfo();

uint64_t prev_idle_time = 0;
uint8_t tachcount = 0;

CANMessage canmsg;

// Definitions for the externs in CanRx.h; see that file for why this lives in AHB SRAM and
// what each counter means.
__attribute__((section("AHBSRAM")))
CircularBuffer<TimestampedCANMessage, CAN_RX_QUEUE_DEPTH> canqueue;

volatile uint32_t canRxSwDrops = 0;
volatile uint32_t canRxHwOverruns = 0;
volatile uint32_t canRxQueuePeak = 0;
volatile uint32_t canRxMaxLatencyUs = 0;

EventFlags eventFlags;
//EventQueue CANqueue(4 * EVENTS_EVENT_SIZE);

uint8_t canCount;

uint16_t c1uac;
uint16_t c2uac;
uint16_t c3uac;
uint8_t c1iac;
uint8_t c2iac;
uint8_t c3iac;

int main() {
  // Init all io pins
  initIO();

  //uint8_t blink = 0;

 /*float fueltest = 1;
  uint8_t soctest = 0;
  float tachdutytest = 1;*/

  uint32_t tachtest = 60000;

  SPI* spiDriver = new SPI(PIN_6820_SPI_MOSI,
                           PIN_6820_SPI_MISO,
                           PIN_6820_SPI_SCLK,
                           PIN_6820_SPI_SSEL,
                           use_gpio_ssel);
  spiDriver->format(8, 0);
  auto ltcBus = LTC681xChainBus<NUM_CHIPS>(spiDriver);
  LTC6813Bus ltc6813Bus = LTC6813Bus(ltcBus);

  Watchdog &watchdog = Watchdog::get_instance();
  watchdog.start(WATCHDOG_TIMEOUT);

  bool canInitialized = false;
  canCount = 0;

  // The BMS thread ran on the 1280-byte rtos.thread-stack-size default before the Mbed CE
  // port and overflowed it there, corrupting the RTOS memory holding the SPI mutex (fatal
  // "Mutex: Parameter error", every LTC6813 PEC read failing). Sized generously here so the
  // high-water mark can be measured; trim once print_stack_stats() reports a real figure.
  Thread bmsThreadThread(osPriorityNormal, BMS_THREAD_STACK_SIZE);
  BMSThread bmsThread(&inbox_main, &inbox_bms_charger, &inbox_bms_inverter, &ltcBus, &ltc6813Bus);
  osStatus bmsStartStatus = bmsThreadThread.start(callback(&BMSThread::startThread, &bmsThread));
  if (bmsStartStatus != osOK) {
#if SLCAN_MODE
    // Emitted before slcan_init() runs in the BMS thread -- which by definition never happens
    // if we are here. The frame is well formed regardless; the block counters simply start
    // from whatever they were.
    telemetry_emit_diag(BMS_DIAG_THREAD_START, (uint16_t)bmsStartStatus, 0, 0);
#else
    std::cout << "FATAL: BMS thread failed to start, status " << bmsStartStatus << std::endl;
#endif
  }
  Thread CANThread(osPriorityAboveNormal, 512);

  osThreadSetPriority(osThreadGetId(), osPriorityHigh7);
  Timer t;
  t.start();

  // Compiled out unless SLCAN_HOST_TX. Set up here rather than in initIO() because it needs the
  // stdio console to exist, and mbed builds that lazily on first use.
  slcan_input_init();

  c1uac = 0;
  c2uac = 0;
  c3uac = 0;
  c1iac = 0;
  c2iac = 0;
  c3iac = 0;


  while (1) {
    while(!inbox_main.empty()) {
      osEvent evt = inbox_main.get();

      if (evt.status == osEventMail) {
        //std::cout << "Received some mail\n";
        mail_t *msg = (mail_t *)evt.value.p;


        switch(msg->msg_event) {
          case INIT_ALL:
#if !SLCAN_MODE
            std::cout << "Main thread init\n";
#endif
            break;
          case NEW_CELL_DATA:
            break;
          case BATT_ERR:
            {
              /*mail_t *msg_out = inbox_data.alloc();
              msg_out->msg_event = DATA_ERR;
              inbox_data.put(msg_out);     */         
            }
            break;
          default:
#if SLCAN_MODE
            telemetry_emit_diag(BMS_DIAG_INVALID_MESSAGE, 3, (uint16_t)msg->msg_event, 0);
#else
            std::cout << "Invalid message received in Main thread!\n";
#endif
            break;
        }
        inbox_main.free(msg);

      }

    }



    //CANMessage msg;

    if (!canInitialized && t.read_ms() > 2000) {
        //std::cout << "Initializing CAN\n";

      //osStatus threadresult = CANThread.start(callback(processCAN));
      //printf("Can thread status: %lx\n", threadresult);
      //std::cout << "Trying to start CAN thread: " << threadresult << "\n";
      //canBus->attach(CANqueue.event(processCAN));
      //canBus->filter(1, 0, CANStandard, 0);
      canBus->attach(canRX);
      // Separate _irq[] slot per type, so this does not disturb the receive handler above.
      canBus->attach(canOverrun, CAN::DoIrq);

      //std::cout << "Started CAN stuff\n";
      //ThisThread::sleep_for(40);
      //eventFlags.set(CAN_RX_INT_FLAG);
      canInitialized = true;
    }

    while (!canqueue.empty()) {
        TimestampedCANMessage entry;
        canqueue.pop(entry);
        const CANMessage &msg = entry.msg;

        // How long this frame waited between the ISR and here. Unsigned subtraction, so the
        // ~71.6 minute wrap of us_ticker_read() is handled without a special case.
        uint32_t latencyUs = us_ticker_read() - entry.rxTimeUs;
        if (latencyUs > canRxMaxLatencyUs) {
          canRxMaxLatencyUs = latencyUs;
        }

#if SLCAN_MODE && SLCAN_FORWARD_CAN
        // Forward real bus traffic as a standard-ID frame. Emitted from the main loop while
        // telemetry comes from the BMS thread, which is exactly the two-writer situation that
        // put `ERR:` messages inside CSV rows in the 2021 logs -- slcan_emit() takes a mutex
        // and writes each frame in one call to keep frames whole.
        slcan_emit(msg.id, msg.data, msg.len, msg.format == CANExtended);
#endif

        switch(msg.id) {
          case 1:
            // Inverter data
            {
              //printf("ID: %X %d %d\n", msg.id, msg.data[4], msg.data[5]);
              static const uint32_t tachScaleBase = 29580000;
              uint16_t rpm = (msg.data[5] << 8) + msg.data[4];
              //printf("RPM: %d\n", (msg.data[5] << 8) + msg.data[4]);
              //printf("volt: %d\n", (msg.data[1] << 8) + msg.data[0]);
              //printf("Tach vaule: %d ")
              if (rpm > 0) {
                tachometer.detach();
                tachometer.attach(&tach_update, std::chrono::microseconds(tachScaleBase/rpm/2));
              } else {
                tachometer.detach();
              }

              inverterdata_t *msg_out = inbox_bms_inverter.alloc();
              hstempfilter.add((msg.data[3] << 8) + msg.data[2]);
              msg_out->heatsinktemp = hstempfilter.get()/10;
              inbox_bms_inverter.put(msg_out);
            }
            break;
          case 519: // Charger 1 AC data
            {
              c1uac = msg.data[1];
              // iac is at bit 41, 9 bits long, scale 0.066666.... (1/15)
              c1iac = (((uint16_t)(msg.data[5] >> 1)) + (((uint16_t)msg.data[6]) << 7))/15;
              sendChargerInfo();
              // printf("Received Charger 1 AC data %d %x %x\n", c1iac, msg.data[5], msg.data[6]);
              break;
            }
          case 521: // Charger 2 AC data
            {

              c2uac = msg.data[1];
              // iac is at bit 41, 9 bits long, scale 0.066666.... (1/15)
              c2iac = (((uint16_t)(msg.data[5] >> 1)) + (((uint16_t)msg.data[6]) << 7))/15;
              sendChargerInfo();
              // printf("Received Charger 2 AC data %d \n", c2iac);
              break;
            }
          case 523: // Charger 3 AC data
            {
              c3uac = msg.data[1];
              // iac is at bit 41, 9 bits long, scale 0.066666.... (1/15)
              c3iac = (((uint16_t)(msg.data[5] >> 1)) + (((uint16_t)msg.data[6]) << 7))/15;
              sendChargerInfo();
              break;
            }
          default:
            //printf("ID: %d\n", msg.id);
            break;
        }
    } 

    // Host -> bus. Polled once per MAIN_PERIOD, so a host command is acted on within 50 ms --
    // far inside the timeouts python-can and openinverter-can-tool use for SDO transfers.
    slcan_poll_input();

    //print_cpu_stats();

#if PRINT_STACK_STATS
    static uint16_t stackStatsCount = 0;
    if (++stackStatsCount >= (5000 / MAIN_PERIOD)) {
      stackStatsCount = 0;
      print_stack_stats();
    }
#endif

    ThisThread::sleep_for(MAIN_PERIOD - (t.read_ms()%MAIN_PERIOD));
  }
}

void initIO() {
  //serial = new MODSERIAL(USBTX, USBRX, 1024, 32);
  serial = new UnbufferedSerial(USBTX, USBRX, 460800);
  //serial->set_dma_usage_tx(DMA_USAGE_ALWAYS);

  //serial2 = new Serial(PIN_SERIAL2_TX, PIN_SERIAL2_RX, 230400);
  displayserial = new BufferedSerial(PIN_DISPLAY_TX, PIN_DISPLAY_RX, 19200);
  //displayserial->set_dma_usage_tx(DMA_USAGE_ALWAYS);

  //serial->printf("INIT\n");
  
  canBus = new IsrSafeCAN(PIN_CAN_RX, PIN_CAN_TX, CAN_FREQUENCY);
#if SLCAN_CAN_SELFTEST
  // Bench harness; see config.h. Set after construction, because can_frequency() writes MOD
  // wholesale to restore the pre-change mode -- doing this first would be undone there.
  canBus->mode(CAN::LocalTest);
#endif
  led1 = new DigitalOut(LED1);
  led2 = new DigitalOut(LED2);
  led3 = new DigitalOut(LED3);
  led4 =  new DigitalOut(LED4);
  DO_Tach = new DigitalOut(PIN_DO_TACH);
  DO_BattContactor = new DigitalOut(PIN_DO_BATTCONTACTOR);
  DO_BattContactor2 = new DigitalOut(PIN_DO_BATTCONTACTOR2);
  DO_DriveEnable = new DigitalOut(PIN_DO_DRIVEENABLE);
  DO_ChargeEnable = new DigitalOut(PIN_DO_CHARGEENABLE);

  DI_ChargeSwitch = new DigitalIn(PIN_DI_CHARGESWITCH);
  DI_BrakeSwitch = new DigitalIn(PIN_DI_BRAKESWITCH);
  DI_ReverseSwitch = new DigitalIn(PIN_DI_REVERSESWITCH);

  ioexp = new MCP23017(PIN_I2C_SDA, PIN_I2C_SCL, MCP_ADDRESS, 1000000);
  ioexp->config(0b1111100000000000, 0b0001100000000000, 0);

  ioexp->write_mask(0, 0xff);

  fuelgauge = new PwmOut(PIN_PWM_FUELGAUGE);
  //tach = new PwmOut(PIN_PWM_TACH); //config on tach TBD

  fuelgauge->period_ms(1);
  //tach->period_ms(2);
  fuelgauge->write(0.1);
  //tach->write(0.5);

  knob1 = new AnalogIn(PIN_ANALOG_KNOB1);

  *DO_Tach = 0;
  *DO_BattContactor = 0;
  *DO_BattContactor2 = 0;
  *DO_DriveEnable = 0;
  *DO_ChargeEnable = 0;
}

void tach_update() {
  // tachcount++;
  // if (tachcount == 10) {    
  //   *DO_Tach = 1;
  //   tachcount = 0;
  // } else {
  //   *DO_Tach = 0;
  //}
  if (*DO_Tach) {
    *DO_Tach = 0;
  } else {
    *DO_Tach = 1;
  }
}


void print_cpu_stats()
{
    mbed_stats_cpu_t stats;
    mbed_stats_cpu_get(&stats);

    // Calculate the percentage of CPU usage
    uint64_t diff_usec = (stats.idle_time - prev_idle_time);
    uint8_t idle = (diff_usec * 100) / (MAIN_PERIOD*1000);
    uint8_t usage = 100 - ((diff_usec * 100) / (MAIN_PERIOD*1000));
    prev_idle_time = stats.idle_time;
    
    printf("Time(us): Up: %lld", stats.uptime);
    printf("   Idle: %lld", stats.idle_time);
    printf("   Sleep: %lld", stats.sleep_time);
    printf("   DeepSleep: %lld\n", stats.deep_sleep_time);
    printf("Idle: %d%% Usage: %d%%\n\n", idle, usage);
}

// Reports each thread's reserved stack against its observed peak, so the stack sizes can be
// set from measurement. "max_size" is the high-water mark since boot. Gated by
// PRINT_STACK_STATS; requires platform.stack-stats-enabled in mbed_app.json5.
void print_stack_stats()
{
    mbed_stats_stack_t stats[8];
    size_t count = mbed_stats_stack_get_each(stats, 8);

    for (size_t i = 0; i < count; i++) {
        printf("STACK: thread 0x%08lX reserved %lu peak %lu headroom %lu\n",
               (unsigned long)stats[i].thread_id,
               (unsigned long)stats[i].reserved_size,
               (unsigned long)stats[i].max_size,
               (unsigned long)(stats[i].reserved_size - stats[i].max_size));
    }
}



void canRX() {
  //canCount++;
  //std::cout << "cc: " << (int)canCount << "\n";
  //CANqueue.call(canProcess);
  //canBus->read(canmsg);
  TimestampedCANMessage entry;

  // Taken before the read so it reflects arrival rather than ISR latency, and so a frame
  // read during a burst is not stamped with the time the burst finished.
  entry.rxTimeUs = us_ticker_read();

  // readNoLock: canRX() is an ISR; CAN::read() would take a mutex here. See IsrSafeCAN.h.
  if (canBus->readNoLock(entry.msg)) {
      // push() overwrites the oldest entry when full and reports nothing, so the only place
      // a drop can be seen is right here, before it happens.
      if (canqueue.full()) {
          canRxSwDrops++;
      }
      canqueue.push(entry);

      uint32_t depth = canqueue.size();
      if (depth > canRxQueuePeak) {
          canRxQueuePeak = depth;
      }
  }
  //eventFlags.set(CAN_RX_INT_FLAG);
}

// CAN::DoIrq handler: a frame was lost inside the peripheral because the receive buffer was
// still occupied. Clearing DOS is what keeps this counting past the first event; see
// IsrSafeCAN::clearDataOverrun().
void canOverrun() {
  canRxHwOverruns++;
  canBus->clearDataOverrun();
}

void sendChargerInfo() {
  chargerdata_t *msg_out = inbox_bms_charger.alloc();
  msg_out->VAC = 0;
  uint8_t activecount = 0;
  if (c1uac > 0) {
    activecount++;
    msg_out->VAC += c1uac;
  }
  if (c2uac > 0) {
    activecount++;
    msg_out->VAC += c2uac;
  }
  if (c3uac > 0) {
    activecount++;
    msg_out->VAC += c3uac;
  }
  msg_out->VAC /= activecount;
  msg_out->IAC = c1iac + c2iac + c3iac;
  inbox_bms_charger.put(msg_out);
}
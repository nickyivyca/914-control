#pragma once

#include "mbed.h"




// Number of 6813 chips on isospi bus
#ifndef NUM_CHIPS
#define NUM_CHIPS 12
#endif

// Number of strings of batteries
#ifndef NUM_STRINGS
#define NUM_STRINGS 2
#endif

// number of times the cells are read per second
#ifndef CELL_SENSE_FREQUENCY
#define CELL_SENSE_FREQUENCY 10
#endif

// number of times the cells are read per second while charging
#ifndef CELL_SENSE_FREQUENCY_CHARGE
#define CELL_SENSE_FREQUENCY_CHARGE 2
#endif

// print cell info every x samples, 0 to disable (until 16 bit overflow)
#ifndef CELL_PRINT_MULTIPLE
#define CELL_PRINT_MULTIPLE 5
#endif

#ifndef MAIN_PERIOD
#define MAIN_PERIOD 50
#endif

#ifndef BALANCE_EN
#define BALANCE_EN 1
#endif

#ifndef WATCHDOG_TIMEOUT
#define WATCHDOG_TIMEOUT 2000
#endif

// Stack for the BMS thread, which does the iostream-heavy CSV and display formatting.
// The rtos.thread-stack-size default of 1280 bytes is not enough for it: measured on the car,
// it peaks at 1304 bytes normally and 1336 in charge mode, and that overflow corrupted the RTOS
// memory holding the SPI mutex, failing every LTC6813 PEC read. 4096 is ~3x the measured peak,
// leaving room for the paths that capture did not exercise (fault handling, PEC-error reporting).
// Re-measure with print_stack_stats() before trimming this further.
#ifndef BMS_THREAD_STACK_SIZE
#define BMS_THREAD_STACK_SIZE 4096
#endif

// Periodic per-thread stack high-water reporting on the console. Useful during the Mbed CE
// bring-up; off by default because it interleaves non-CSV lines into the data stream. Turn
// it on only when measuring stacks.
//
// It is NOT the cause of the rare single-byte insertions seen in serial captures (a stray
// comma, an extra digit in a timestamp, roughly one per 100 KB). Turning this off did not
// stop them, and they appear in captures taken before the CAN RX work and before the Mbed CE
// port -- see notes/artifacts/csv_corruption_scan.py in the 914 notes repo. They come from
// the link, not from stdout contention between threads.
#ifndef PRINT_STACK_STATS
#define PRINT_STACK_STATS 0
#endif

// Link quality test mode. Replaces the CSV with a deterministic, self-describing pattern so
// the host can reconstruct exactly what should have arrived and classify every corrupted byte
// -- inserted, dropped or substituted -- rather than only noticing corruption that happens to
// break the CSV's structure. A flipped bit inside a cell voltage reads as a plausible 4005 mV
// and is invisible to a structural check, so a CSV-based figure is only ever a lower bound.
//
// Everything else -- BMS scanning, contactor logic, CAN -- keeps running; only the serial
// payload changes. Off by default; this is a bench instrument, not a mode to drive with.
#ifndef LINK_TEST
#define LINK_TEST 0
#endif

// Pattern lines emitted per BMS print interval. 1 matches the CSV's ~2 KB/s. The interval is
// 500 ms and a line is 963 bytes, so the ceiling is the baud rate: 23 lines at 460800, 5 at
// 115200. Raising this is how "is it worse at higher data rates" gets tested at a fixed baud,
// separately from the line rate itself -- they are different variables and may not behave the
// same way.
#ifndef LINK_TEST_LINES_PER_PRINT
#define LINK_TEST_LINES_PER_PRINT 1
#endif

// Chosen so a pattern line totals 963 bytes, matching a real CSV record: "LT" + 8 hex digits
// of sequence number + ":" is 11, plus payload, plus the newline.
#ifndef LINK_TEST_PAYLOAD_LEN
#define LINK_TEST_PAYLOAD_LEN 951
#endif

// Bytes per write() call when emitting a pattern line. 0 means one write for the whole line.
//
// This is the variable that separates "the wire loses bytes" from "the way we hand bytes to
// stdio loses bytes". A CSV record is ~206 << operations averaging ~4.7 bytes each, whereas a
// pattern line at chunk 0 is a single 964-byte write. If corruption tracks the chunk size
// rather than the data rate, the fault is above the UART rather than on it.
#ifndef LINK_TEST_CHUNK
#define LINK_TEST_CHUNK 0
#endif

// Drop the BMS thread's pacing sleep so the serial line never goes idle. Used to prove that
// the single-byte insertions at record start need an idle gap: with this on, 31.7 MB produced
// none at all, while the same build with the sleep intact produced 15 in 11.0 MB, 14 of them
// on the first record after the gap.
//
// The gap is that sleep, not the display writes -- the display is a BufferedSerial whose
// writes are far smaller than its buffer, so they never block. Bridging the gap with a bigger
// TX buffer instead does not work: drivers.uart-serial-txbuf-size is shared with the display,
// so 8192 buys two buffers, pushes IRAM1 to 88.2%, and the firmware takes a BusFault past the
// end of the heap.
//
// Test builds only; it makes the BMS thread run flat out.
#ifndef LINK_TEST_SATURATE
#define LINK_TEST_SATURATE 0
#endif

// Emit telemetry as SLCAN (LAWICEL ASCII) CAN frames instead of the CSV. See Slcan.h for the
// format, the 11-bit/29-bit split between real and synthetic frames, and why an integrity
// frame is needed on top.
#ifndef SLCAN_MODE
#define SLCAN_MODE 0
#endif

// Synthetic frames emitted per BMS cycle. Real telemetry is ~51 frames per scan -- 168 cell
// voltages at 4 per frame, plus temperatures, die temperatures and summary -- so 51 is the
// realistic figure. Raise it to gather link statistics faster than real time.
#ifndef SLCAN_FRAMES_PER_CYCLE
#define SLCAN_FRAMES_PER_CYCLE 51
#endif

// Frames between integrity frames. Smaller costs more overhead but discards less data when a
// block fails its CRC: at 16 frames a failure loses about 450 bytes, roughly a third of one
// scan, instead of the whole thing.
#ifndef SLCAN_SEQ_INTERVAL
#define SLCAN_SEQ_INTERVAL 16
#endif

// Forward real vehicle bus traffic as standard-ID frames, i.e. actually act as the CAN proxy.
#ifndef SLCAN_FORWARD_CAN
#define SLCAN_FORWARD_CAN 1
#endif

// Fill synthetic payloads with a deterministic function of a frame counter so the host can
// verify every byte. This is what separates three outcomes that otherwise look identical in a
// log: frames that never arrived, frames rejected as malformed, and frames that arrived
// well-formed carrying wrong data. The last of those -- corruption that passes the structural
// check -- is the number this prototype exists to measure, and real telemetry cannot yield it.
#ifndef SLCAN_VERIFY_PATTERN
#define SLCAN_VERIFY_PATTERN 1
#endif

// Even parity on the stdio UART. The host sets the same through the CDC line coding, which
// the interface MCU applies to its own UART side. This does not report errors by itself --
// the point is the comparison. If the corruption rate moves, the damage is happening on the
// wire between the LPC1768 and the interface MCU. If it does not move, the wire is innocent
// and the fault is in the bridge, the USB stack, or the host.
#ifndef STDIO_PARITY_EVEN
#define STDIO_PARITY_EVEN 0
#endif

// Time the CSV emit in BmsThread and report min/avg/max every PRINT_TIMING_SAMPLES prints.
// A measurement aid for the transport work, not something to leave on: the report is an
// extra non-CSV line in the data stream. Off by default.
#ifndef PRINT_TIMING
#define PRINT_TIMING 0
#endif

#ifndef PRINT_TIMING_SAMPLES
#define PRINT_TIMING_SAMPLES 20
#endif

// Depth of the CAN receive queue, in frames. The main loop drains it once per MAIN_PERIOD,
// so this sets the frame rate the car can absorb: depth / (MAIN_PERIOD/1000) frames per
// second. The old depth of 32 gave 640 frames/s, which is ample for the handful of IDs used
// today but nowhere near a fully forwarded 500 kbit bus (several thousand frames/s). 512
// gives 10,240 frames/s, which covers back-to-back minimum-length frames.
//
// Costs 512 * 20 = 10 KB, allocated out of the otherwise unused AHB SRAM bank. See CanRx.h.
#ifndef CAN_RX_QUEUE_DEPTH
#define CAN_RX_QUEUE_DEPTH 512
#endif

// Delay in ms before zeroing current sensor and closing contactors
#ifndef INIT_DELAY
#define INIT_DELAY 750
#endif



// #ifndef TESTBALANCE
// #define TESTBALANCE
// #endif

/*#ifndef DEBUGN
#define DEBUGN
#endif*/

// Number of cells per chip
#ifndef NUM_CELLS_PER_CHIP
#define NUM_CELLS_PER_CHIP 14
#endif

// Mapping of BMS chip channels to cells
const int8_t BMS_CELL_MAP[18] = {0, 1, 2, 3, 4, -1, 5, 6, 7, 8, 9, -1, 10, 11, 12, 13, -1, -1};

// Which chips constitute each string, in order
const uint8_t BMS_CHIP_MAP[NUM_STRINGS][NUM_CHIPS/NUM_STRINGS] = {
		{0, 1, 2, 3, 4, 5},
	    {6, 7, 8, 9, 10, 11}};

// Which chip (in the string, not in the mapping) has each string's current sensor
const uint8_t BMS_ISENSE_MAP[NUM_STRINGS] = {1, 11};

const int8_t BMS_ISENSE_DIR[NUM_STRINGS] = {-1, 1};

const float BMS_ISENSE_RANGE[NUM_STRINGS] = {50.0, 200.0};

enum thread_message {INIT_ALL, NEW_CELL_DATA, BATT_ERR, BATT_STARTUP, CHARGE_ENABLED, // to main
  BMS_INIT, BMS_READ, ENABLE_BALANCING, DISABLE_BALANCING, // to bms thread
  DATA_INIT, DATA_DATA, DATA_SUMMARY, DATA_ERR};    // to data thread

// Power value for 100% on the bar on the display
#ifndef DISP_FULL_SCALE
#define DISP_FULL_SCALE 80000
#endif

// Divide out for 20 characters width
#ifndef DISP_PER_BOX
#define DISP_PER_BOX (DISP_FULL_SCALE/20)
#endif

// Threshold of difference between average battery string voltage and each string to close contactors
#ifndef BMS_STRING_DIFFERENCE_THRESHOLD
#define BMS_STRING_DIFFERENCE_THRESHOLD 850
#endif


// Upper threshold when fault will be thrown for cell voltage
//
// Units: millivolts
#ifndef BMS_FAULT_VOLTAGE_THRESHOLD_HIGH
#define BMS_FAULT_VOLTAGE_THRESHOLD_HIGH 4190
#endif

// Cells will only balance above this threshold (unless low temp or strings out of balance)
//
// Units: millivolts
#ifndef BMS_BALANCE_VOLTAGE_THRESHOLD
#define BMS_BALANCE_VOLTAGE_THRESHOLD 3900
#endif

// Cells will only balance below this DC Current
//
// Units: millivolts
#ifndef BMS_BALANCE_CURRENT_LIMIT
#define BMS_BALANCE_CURRENT_LIMIT -2000
#endif

// Lower threshold when fault will be thrown for cell voltage
//
// Units: millivolts
#ifndef BMS_FAULT_VOLTAGE_THRESHOLD_LOW
#define BMS_FAULT_VOLTAGE_THRESHOLD_LOW 3000
#endif

#ifndef BMS_SOC_RESERVE_THRESHOLD
#define BMS_SOC_RESERVE_THRESHOLD 16
#endif

// Threshold when cells will be discharged when discharging is enabled.
//
// Units: millivolts
#ifndef BMS_DISCHARGE_THRESHOLD
#define BMS_DISCHARGE_THRESHOLD 10
#endif

// Overtemp threshold
#ifndef BMS_TEMPERATURE_THRESHOLD
#define BMS_TEMPERATURE_THRESHOLD 42 // cell datasheet gives charging range up to 45C
#endif

// 'Heater' threshold
#ifndef BMS_LOW_TEMPERATURE_THRESHOLD
#define BMS_LOW_TEMPERATURE_THRESHOLD 12
#endif

const uint16_t SoC_lookup[102] = {
3319,
3353,
3378,
3398,
3410,
3420,
3427,
3434,
3442,
3450,
3460,
3470,
3480,
3489,
3498,
3505,
3511,
3517,
3523,
3529,
3535,
3541,
3547,
3552,
3558,
3563,
3567,
3571,
3576,
3580,
3584,
3588,
3592,
3595,
3599,
3603,
3607,
3610,
3614,
3618,
3621,
3624,
3627,
3631,
3635,
3640,
3644,
3649,
3654,
3659,
3665,
3671,
3677,
3684,
3692,
3700,
3708,
3717,
3726,
3736,
3746,
3757,
3768,
3779,
3790,
3801,
3813,
3825,
3837,
3849,
3859,
3869,
3879,
3889,
3899,
3909,
3919,
3930,
3943,
3953,
3963,
3973,
3984,
3994,
4004,
4015,
4026,
4036,
4047,
4059,
4070,
4081,
4093,
4104,
4115,
4127,
4138,
4150,
4162,
4175,
4185,
4200
};

const uint32_t mc_lookup[102] = {
0,
1243517,
2481329,
3712537,
4938463,
6158897,
7377319,
8593453,
9806001,
11016476,
12223470,
13427033,
14627606,
15824045,
17017223,
18207166,
19394578,
20578444,
21759736,
22942104,
24121818,
25299028,
26475429,
27648603,
28743864,
29914041,
31081654,
32245439,
33405467,
34569268,
35732377,
36893949,
38054154,
39212412,
40370358,
41526460,
42681597,
43835566,
44987800,
46138976,
47288745,
48436817,
49581174,
50727970,
51873346,
53017681,
54160368,
55301102,
56440868,
57578295,
58714660,
59847556,
60979591,
62109982,
63237921,
64363492,
65486386,
66605928,
67729405,
68844026,
69955189,
71063119,
72168576,
73270195,
74368931,
75463426,
76555504,
77643348,
78728009,
79807851,
80883688,
81959041,
83031391,
84101578,
85168235,
86232006,
87294014,
88352177,
89406795,
90452405,
91501416,
92547934,
93591503,
94632165,
95670199,
96705654,
97738125,
98767950,
99795013,
100818967,
101839437,
102857606,
103872877,
104885349,
105894728,
106901678,
107905178,
108907061,
109904787,
110900026,
111892080,
111892081
};

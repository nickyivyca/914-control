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

// Delay in ms before zeroing current sensor and closing contactors
#ifndef INIT_DELAY
#define INIT_DELAY 500
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
#define BMS_FAULT_VOLTAGE_THRESHOLD_HIGH 4180
#endif

// Cells will only balance above this threshold (unless low temp or strings out of balance)
//
// Units: millivolts
#ifndef BMS_BALANCE_VOLTAGE_THRESHOLD
#define BMS_BALANCE_VOLTAGE_THRESHOLD 3950
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
3363,
3387,
3400,
3416,
3434,
3440,
3447,
3457,
3466,
3474,
3483,
3493,
3503,
3511,
3518,
3526,
3533,
3540,
3547,
3553,
3559,
3565,
3570,
3575,
3579,
3583,
3587,
3591,
3595,
3599,
3604,
3609,
3613,
3618,
3623,
3627,
3631,
3635,
3640,
3643,
3646,
3649,
3653,
3656,
3659,
3665,
3670,
3675,
3681,
3687,
3692,
3697,
3702,
3708,
3714,
3721,
3727,
3733,
3739,
3748,
3756,
3764,
3773,
3783,
3793,
3802,
3811,
3823,
3833,
3841,
3852,
3863,
3874,
3884,
3894,
3906,
3918,
3928,
3940,
3952,
3963,
3974,
3985,
3997,
4008,
4019,
4030,
4041,
4052,
4063,
4074,
4086,
4096,
4107,
4119,
4130,
4142,
4156,
4170,
4185,
4200,
4201
};

const uint32_t mc_lookup[102] = {
0,
1187876,
2371080,
3549128,
4721483,
5891749,
7059932,
8225088,
9387298,
10547096,
11703780,
12857368,
14007955,
15155956,
16301651,
17445066,
18586209,
19725107,
20862032,
21997131,
23130365,
24261739,
25391537,
26519856,
27646730,
28772408,
29896892,
31020216,
32142443,
33263271,
34382552,
35500291,
36616973,
37732211,
38845737,
39958095,
41069287,
42179228,
43287740,
44395184,
45501740,
46607407,
47711850,
48815497,
49918129,
51019088,
52118380,
53216271,
54312416,
55406705,
56499519,
57590864,
58680612,
59768726,
60855055,
61939418,
63021823,
64102588,
65181493,
66257926,
67332012,
68403760,
69472903,
70538906,
71602114,
72662704,
73720691,
74775305,
75827024,
76876368,
77922606,
78965757,
80005748,
81043001,
82077309,
83108292,
84135971,
85160772,
86182385,
87200552,
88215720,
89227907,
90237033,
91242918,
92245732,
93245563,
94242431,
95236257,
96227226,
97215283,
98200234,
99182100,
100161257,
101137572,
102110636,
103080689,
104047748,
105011180,
105970723,
106926409,
107878268,
108828000
};

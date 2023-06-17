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

#ifndef CELL_SENSE_FREQUENCY
#define CELL_SENSE_FREQUENCY 10
#endif

#ifndef CELL_SENSE_FREQUENCY_CHARGE
#define CELL_SENSE_FREQUENCY_CHARGE 2
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
3341,
3364,
3377,
3391,
3406,
3412,
3418,
3426,
3434,
3441,
3450,
3459,
3468,
3475,
3482,
3489,
3496,
3502,
3508,
3514,
3519,
3525,
3530,
3534,
3539,
3543,
3546,
3550,
3553,
3558,
3563,
3567,
3571,
3575,
3581,
3585,
3588,
3592,
3597,
3601,
3603,
3606,
3610,
3613,
3616,
3622,
3627,
3632,
3638,
3644,
3649,
3654,
3660,
3665,
3671,
3678,
3685,
3691,
3697,
3706,
3714,
3723,
3732,
3744,
3754,
3764,
3773,
3786,
3797,
3806,
3818,
3830,
3842,
3853,
3865,
3878,
3892,
3903,
3917,
3931,
3944,
3956,
3969,
3983,
3997,
4010,
4023,
4037,
4050,
4063,
4078,
4092,
4105,
4118,
4134,
4148,
4163,
4181,
4200,
4199,
4200,
4201
};

const uint32_t mc_lookup[102] = {
0,
1159214,
2313869,
3463492,
4607560,
5749589,
6889585,
8026628,
9160795,
10292608,
11421383,
12547136,
13669961,
14790262,
15908313,
17024139,
18137749,
19249166,
20358658,
21466369,
22572260,
23676335,
24778873,
25879967,
26979651,
28078167,
29175520,
30271739,
31366888,
32460672,
33552946,
34643716,
35733454,
36821782,
37908440,
38993958,
40078339,
41161498,
42243264,
43323987,
44403842,
45482832,
46560626,
47637643,
48713670,
49788065,
50860832,
51932232,
53001928,
54069814,
55136260,
56201272,
57264726,
58326586,
59386702,
60444901,
61501189,
62555877,
63608749,
64659209,
65707379,
66753267,
67796613,
68836895,
69874449,
70909449,
71941907,
72971075,
73997417,
75021442,
76042436,
77060417,
78075315,
79087540,
80096892,
81102998,
82105880,
83105955,
84102917,
85096517,
86087190,
87074955,
88059731,
89041347,
90019963,
90995670,
91968485,
92938331,
93905389,
94869606,
95830792,
96788966,
97744497,
98697255,
99646841,
100593487,
101537212,
102477398,
103413789,
104346415,
105275307,
108000000,
};

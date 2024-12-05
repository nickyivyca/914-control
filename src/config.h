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

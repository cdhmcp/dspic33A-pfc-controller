#ifndef PFC_PARAMS_H
#define PFC_PARAMS_H

#include <stdint.h>

// === System Clocks ===
#define FCY                         200000000UL
#define PWM_CLK                     400000000UL

// === ADC / DAC Scaling ===
#define ADC_BITS                    12U
#define ADC_MAX                     4095U
#define ADC_SHIFT                   12U         // For normalizing multiply products
#define DAC_MAX_VALUE               4095U

// === Multiplier Scaling (Q15) ===
#define VAVG_SQ_NUMERATOR           32767U
#define VAVG_SQ_MIN                 256U

// === Voltage Thresholds (13-bit ADC counts, tune to actual divider ratios) ===
#define VAVG_BROWN_IN_THRESHOLD     1500U
#define VAVG_BROWN_OUT_THRESHOLD    1200U
#define VOUT_OV_THRESHOLD           7500U

// === Timing (100us ticks) ===
#define BROWN_IN_TIME_TICKS         6000U       // 600ms
#define BROWN_OUT_TIME_TICKS        300U        // 30ms
#define RELAY_SETTLE_TICKS          500U        // 50ms
#define SOFT_START_TICKS            2000U       // 200ms
#define OV_DEBOUNCE_TICKS           10U         // 1ms
#define FAULT_COOLDOWN_TICKS        50000U      // 5s

// === Duty Cycle Limits ===
#define DUTY_MAX_PERCENT            95U
#define DUTY_MIN_DEFAULT            0U
#define DUTY_HEADROOM_COUNTS        200U

// === Soft-Start ===
#define SOFT_START_DUTY_INITIAL     100U        // Integer PWM counts (~2.5% at 100kHz)

// === Frequency Selection ===
// 12-bit ADC range (0-4095) divided into 20 positions
// Band spacing = 4096/20 = 204.8 ~ 205 counts
// Valid band: center +/- 75 counts (accommodates 1% R tolerance + ADC error)
// Guard band: 55 counts between valid regions
#define FREQ_SELECT_POSITIONS       20U
#define FREQ_BAND_HALF_WIDTH        75U
#define FREQ_BAND_SPACING           205U
#define FREQ_BAND_OFFSET            102U
#define FREQ_BASE_KHZ               25U
#define FREQ_STEP_KHZ               25U

// PWM period lookup table (integer counts at 400 MHz, before << 4 shift)
// PG1PER = (400_000_000 / freq_hz - 1) for each frequency step
// Index 0 = 25 kHz, Index 19 = 500 kHz
static const uint16_t pg1per_table[FREQ_SELECT_POSITIONS] = {
    15999U,     //  25 kHz: 400M/25k - 1
    7999U,      //  50 kHz: 400M/50k - 1
    5332U,      //  75 kHz: 400M/75k - 1 (75.006 kHz actual)
    3999U,      // 100 kHz: 400M/100k - 1
    3199U,      // 125 kHz: 400M/125k - 1
    2666U,      // 150 kHz: 400M/150k - 1 (150.02 kHz actual)
    2284U,      // 175 kHz: 400M/175k - 1 (175.04 kHz actual)
    1999U,      // 200 kHz: 400M/200k - 1
    1777U,      // 225 kHz: 400M/225k - 1 (225.03 kHz actual)
    1599U,      // 250 kHz: 400M/250k - 1
    1453U,      // 275 kHz: 400M/275k - 1 (275.29 kHz actual)
    1332U,      // 300 kHz: 400M/300k - 1 (300.08 kHz actual)
    1230U,      // 325 kHz: 400M/325k - 1 (325.02 kHz actual)
    1142U,      // 350 kHz: 400M/350k - 1 (350.09 kHz actual)
    1066U,      // 375 kHz: 400M/375k - 1 (375.12 kHz actual)
    999U,       // 400 kHz: 400M/400k - 1
    940U,       // 425 kHz: 400M/425k - 1 (425.17 kHz actual)
    888U,       // 450 kHz: 400M/450k - 1 (450.05 kHz actual)
    841U,       // 475 kHz: 400M/475k - 1 (475.30 kHz actual)
    799U        // 500 kHz: 400M/500k - 1
};

#endif // PFC_PARAMS_H

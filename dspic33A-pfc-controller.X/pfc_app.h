#ifndef PFC_APP_H
#define PFC_APP_H

#include <stdint.h>

typedef enum {
    PFC_STATE_INIT = 0,
    PFC_STATE_STANDBY,
    PFC_STATE_RELAY_WAIT,
    PFC_STATE_SOFT_START,
    PFC_STATE_RUNNING,
    PFC_STATE_FAULT
} pfc_state_t;

typedef enum {
    PFC_FAULT_NONE = 0,
    PFC_FAULT_OVERVOLTAGE,
    PFC_FAULT_UNDERVOLTAGE,
    PFC_FAULT_OVERCURRENT,
    PFC_FAULT_FREQ_SELECT,
    PFC_FAULT_BROWNOUT
} pfc_fault_code_t;

// Shared variables between ISRs and state machine
extern volatile uint16_t g_inv_vavg_sq;
extern volatile uint8_t  g_pfc_running;
extern volatile uint16_t g_pg1per_cached;
extern volatile uint16_t g_duty_max_int;
extern volatile uint16_t g_duty_min_int;
extern volatile uint16_t g_vavg_raw;

// State accessible for diagnostics
extern volatile pfc_state_t      g_pfc_state;
extern volatile pfc_fault_code_t g_fault_code;

// Application functions
void PFC_App_Init(void);
void PFC_App_Background(void);
void PFC_StateMachine_Tick(void);

#endif // PFC_APP_H

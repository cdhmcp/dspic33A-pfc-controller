#include <xc.h>
#include "pfc_app.h"
#include "pfc_params.h"
#include "mcc_generated_files/system/pins.h"
#include "mcc_generated_files/cmp/cmp3.h"

// === Shared Variables ===
volatile uint16_t g_inv_vavg_sq   = 0U;
volatile uint8_t  g_pfc_running   = 0U;
volatile uint16_t g_pg1per_cached = 0U;
volatile uint16_t g_duty_max_int  = 0U;
volatile uint16_t g_duty_min_int  = 0U;
volatile uint16_t g_vavg_raw      = 0U;

volatile pfc_state_t      g_pfc_state  = PFC_STATE_INIT;
volatile pfc_fault_code_t g_fault_code = PFC_FAULT_NONE;

// === Local State ===
static uint16_t state_counter     = 0U;
static uint16_t brown_in_count    = 0U;
static uint16_t brown_out_count   = 0U;
static uint16_t ov_count          = 0U;
static uint16_t soft_start_step   = 0U;
static uint16_t duty_max_target   = 0U;
static uint16_t heartbeat_counter = 0U;

// === Forward Declarations ===
static int8_t PFC_FreqSelect_Read(void);
static void   PFC_EnterFault(pfc_fault_code_t code);

// === Frequency Selection ===
static int8_t PFC_FreqSelect_Read(void)
{
    // If switch SW0 is held at startup, override to debug frequency
    if (SW0_RC3_GetValue() == 0U)
    {
        return (int8_t)FREQ_DEBUG_INDEX;
    }

    AD4SWTRGbits.CH1TRG = 1U;
    while (!AD4STATbits.CH1RDY) { }
    uint16_t adc_val = (uint16_t)AD4CH1DATA;

    for (uint8_t i = 0U; i < FREQ_SELECT_POSITIONS; i++)
    {
        uint16_t center = FREQ_BAND_OFFSET + (uint16_t)i * FREQ_BAND_SPACING;
        uint16_t low  = (center > FREQ_BAND_HALF_WIDTH) ? (center - FREQ_BAND_HALF_WIDTH) : 0U;
        uint16_t high = center + FREQ_BAND_HALF_WIDTH;

        if (adc_val >= low && adc_val <= high)
        {
            return (int8_t)i;
        }
    }

    return -1;
}

// === Initialization ===
void PFC_App_Init(void)
{
    PG1CONbits.ON = 0U;
    LED_RC8_SetLow();
    g_pfc_running = 0U;

    CMP3_Calibrate();

    int8_t freq_index = PFC_FreqSelect_Read();

    if (freq_index < 0)
    {
        g_pfc_state = PFC_STATE_FAULT;
        g_fault_code = PFC_FAULT_FREQ_SELECT;
        return;
    }

    uint16_t period_int = pg1per_table[freq_index];
    uint32_t period_reg = (uint32_t)period_int << 4;
    PG1PER = period_reg & 0x000FFFF0UL;
    g_pg1per_cached = period_int;

    // ADC trigger at ~50% of period for mid-inductor-current sampling
    uint32_t trig_point = (period_reg >> 1) & 0x000FFFF0UL;
    PG1TRIGA = trig_point;

    // Duty cycle limits
    duty_max_target = period_int - DUTY_HEADROOM_COUNTS;
    g_duty_min_int = DUTY_MIN_DEFAULT;
    g_duty_max_int = 0U;

    // Compute soft-start ramp step: (target - initial) / ticks
    if (duty_max_target > SOFT_START_DUTY_INITIAL)
    {
        soft_start_step = (duty_max_target - SOFT_START_DUTY_INITIAL) / SOFT_START_TICKS;
        if (soft_start_step == 0U) soft_start_step = 1U;
    }
    else
    {
        soft_start_step = 1U;
    }

    // Adjust interrupt priorities: ADC2 > ADC3 > Timer1
    IPC22bits.AD2CH0IP = 6U;
    IPC25bits.AD3CH0IP = 5U;

    // Prime ADC4 CH0 pipeline
    AD4SWTRGbits.CH0TRG = 1U;

    g_pfc_state = PFC_STATE_STANDBY;
    state_counter = 0U;
    brown_in_count = 0U;
}

// === Fault Entry ===
static void PFC_EnterFault(pfc_fault_code_t code)
{
    g_pfc_running = 0U;
    PG1CONbits.ON = 0U;
    PG1IOCON2bits.OVRDAT = 0U;
    PG1IOCON2bits.OVRENH = 1U;
    LED_RC8_SetLow();

    PG1F1PCI1bits.SWTERM = 1U;

    g_fault_code = code;
    g_pfc_state = PFC_STATE_FAULT;
    state_counter = 0U;
}

// === State Machine (called from Timer1 ISR every 100us) ===
void PFC_StateMachine_Tick(void)
{
    // Heartbeat LED
    heartbeat_counter++;
    if (heartbeat_counter >= HEARTBEAT_PERIOD_TICKS)
    {
        heartbeat_counter = 0U;
        LED_RD0_Toggle();
    }

    switch (g_pfc_state)
    {
        case PFC_STATE_INIT:
            break;

        case PFC_STATE_STANDBY:
            if (g_vavg_raw > VAVG_BROWN_IN_THRESHOLD)
            {
                brown_in_count++;
                if (brown_in_count >= BROWN_IN_TIME_TICKS)
                {
                    g_pfc_state = PFC_STATE_RELAY_WAIT;
                    state_counter = 0U;
                    LED_RC8_SetHigh();
                }
            }
            else
            {
                brown_in_count = 0U;
            }
            break;

        case PFC_STATE_RELAY_WAIT:
            state_counter++;
            if (state_counter >= RELAY_SETTLE_TICKS)
            {
                g_pfc_state = PFC_STATE_SOFT_START;
                state_counter = 0U;
                g_duty_max_int = SOFT_START_DUTY_INITIAL;
                PG1IOCON2bits.OVRDAT = 0U;
                PG1IOCON2bits.OVRENH = 0U;
                PG1CONbits.ON = 1U;
                g_pfc_running = 1U;
            }
            break;

        case PFC_STATE_SOFT_START:
            state_counter++;
            g_duty_max_int += soft_start_step;
            if (g_duty_max_int >= duty_max_target)
            {
                g_duty_max_int = duty_max_target;
                g_pfc_state = PFC_STATE_RUNNING;
                state_counter = 0U;
                brown_out_count = 0U;
                ov_count = 0U;
            }
            // Check for hardware overcurrent fault during ramp
            if (PG1STATbits.FLT1EVT)
            {
                PFC_EnterFault(PFC_FAULT_OVERCURRENT);
            }
            break;

        case PFC_STATE_RUNNING:
            // Overvoltage detection: ADC1 reading above threshold means Vout too high
            if ((uint16_t)AD1CH0DATA > VOUT_OV_THRESHOLD)
            {
                ov_count++;
                if (ov_count >= OV_DEBOUNCE_TICKS)
                {
                    PFC_EnterFault(PFC_FAULT_OVERVOLTAGE);
                    break;
                }
            }
            else
            {
                ov_count = 0U;
            }

            // Brownout detection
            if (g_vavg_raw < VAVG_BROWN_OUT_THRESHOLD)
            {
                brown_out_count++;
                if (brown_out_count >= BROWN_OUT_TIME_TICKS)
                {
                    PFC_EnterFault(PFC_FAULT_BROWNOUT);
                    break;
                }
            }
            else
            {
                brown_out_count = 0U;
            }

            // Hardware overcurrent fault latch check
            if (PG1STATbits.FLT1EVT)
            {
                PFC_EnterFault(PFC_FAULT_OVERCURRENT);
            }
            break;

        case PFC_STATE_FAULT:
            state_counter++;
            if (state_counter >= FAULT_COOLDOWN_TICKS)
            {
                g_fault_code = PFC_FAULT_NONE;
                g_pfc_state = PFC_STATE_STANDBY;
                brown_in_count = 0U;
                state_counter = 0U;
            }
            break;

        default:
            PFC_EnterFault(PFC_FAULT_FREQ_SELECT);
            break;
    }
}

// === Background Task (non-time-critical, called from main loop) ===
void PFC_App_Background(void)
{
    // Placeholder for future diagnostics, communication, or LED status
}

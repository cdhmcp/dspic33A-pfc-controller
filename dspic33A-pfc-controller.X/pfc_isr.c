#include <xc.h>
#include "pfc_app.h"
#include "pfc_params.h"

// ADC2 ISR: Master multiplier — computes current reference and writes DAC3
// Runs at switching frequency (up to 500 kHz). Budget: 400 cycles at 200 MHz.
void __attribute__((__interrupt__, no_auto_psv)) _AD2CH0Interrupt(void)
{
    uint16_t iac_raw  = (uint16_t)AD2CH0DATA;
    uint16_t verr_raw = (uint16_t)AD1CH0DATA;

    if (g_pfc_running)
    {
        uint32_t temp = __builtin_muluu(iac_raw, verr_raw) >> 13;
        uint32_t current_ref = __builtin_muluu((uint16_t)temp, g_inv_vavg_sq) >> 15;

        if (current_ref > DAC_MAX_VALUE)
        {
            current_ref = DAC_MAX_VALUE;
        }

        DAC3DATbits.DACDAT = (uint16_t)current_ref;
    }
    else
    {
        DAC3DATbits.DACDAT = 0U;
    }

    IFS5bits.AD2CH0IF = 0U;
}

// ADC3 ISR: Current compensator output to PWM duty cycle
// Runs at switching frequency. Maps OPA3 output linearly to PG1DC.
void __attribute__((__interrupt__, no_auto_psv)) _AD3CH0Interrupt(void)
{
    uint16_t comp_out = (uint16_t)AD3CH0DATA;

    if (g_pfc_running)
    {
        uint32_t duty = __builtin_muluu(comp_out, g_pg1per_cached) >> 13;

        uint32_t duty_min = (uint32_t)g_duty_min_int << 4;
        uint32_t duty_max = (uint32_t)g_duty_max_int << 4;

        if (duty < duty_min)
        {
            duty = duty_min;
        }
        else if (duty > duty_max)
        {
            duty = duty_max;
        }

        PG1DC = duty & 0x000FFFF0UL;
    }

    IFS6bits.AD3CH0IF = 0U;
}

// Timer1 callback: 100us scheduler
// Overrides the weak TMR1_TimeoutCallback defined in MCC tmr1.c.
// The MCC ISR (_T1Interrupt) calls this and clears the interrupt flag.
void TMR1_TimeoutCallback(void)
{
    if (AD4STATbits.CH0RDY)
    {
        g_vavg_raw = (uint16_t)AD4CH0DATA;
    }
    AD4SWTRGbits.CH0TRG = 1U;

    uint32_t vavg_sq = __builtin_muluu(g_vavg_raw, g_vavg_raw) >> 13;
    if (vavg_sq < VAVG_SQ_MIN)
    {
        vavg_sq = VAVG_SQ_MIN;
    }
    g_inv_vavg_sq = (uint16_t)(VAVG_SQ_NUMERATOR / (vavg_sq + 1U));

    PFC_StateMachine_Tick();
}

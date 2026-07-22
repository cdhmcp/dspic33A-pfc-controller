# PFC Controller Development Log

## 2026-07-10 — Initial firmware implementation

### Goal
Implement application firmware for dsPIC33AK512MPS506 PFC controller mimicking TI UCC2818-EP functionality. Analog compensation (OPA1 = voltage error amp, OPA3 = current comp), digital multiplier, ADC-to-PWM translation.

### Decisions made
- **Single-phase only** — draw.io page 1/3 (2-phase interleaved) is deprecated; page 2 is the reference
- **ADC2 ISR is the master** — performs the multiplier computation (IAC x Verr / VAVG^2) and writes DAC3
- **ADC3 ISR handles ADC-to-PWM** — reads OPA3 output, maps linearly to PG1DC duty cycle
- **VAVG^2 computed at 10 kHz** (Timer1 callback) — acceptable because RMS voltage changes slowly vs switching frequency
- **Frequency selection via ADC4 CH1** — 20 bands (25–500 kHz in 25 kHz steps), resistor divider, validated at startup with guard bands for 1% tolerance + noise
- **+1 offset in VAVG path** — prevents divide-by-zero in 1/VAVG^2 feedforward
- **Soft-start ramps duty cycle limit** — OPA1 Vref may be DAC-controlled in future (hook in code)

### Files created
- `pfc_params.h` — all constants, thresholds, frequency/period lookup table
- `pfc_app.h` — state enum, fault codes, shared variable externs, prototypes
- `pfc_isr.c` — ADC2 ISR (multiplier), ADC3 ISR (duty cycle), TMR1_TimeoutCallback (scheduler)
- `pfc_app.c` — init, freq select, state machine, fault handling, relay control
- `main.c` — modified to call PFC_App_Init() and PFC_App_Background()

### Issues encountered
- **Linker error: multiple definition of `_T1Interrupt`** — The Timer1 MCC driver does NOT declare its ISR as `__attribute__((weak))` unlike the ADC drivers. It uses a callback pattern instead. Fix: override `TMR1_TimeoutCallback()` (which IS weak) instead of `_T1Interrupt`.

### State machine
```
INIT → STANDBY → RELAY_WAIT → SOFT_START → RUNNING → FAULT (cooldown → STANDBY)
```

### ADC bit width clarification
- MCC comments say "ACCNUM 4 samples, 13 bits result" but this is misleading
- MODE=0b00 = "Single sample initiated by TRG1SRC trigger" — only one conversion per trigger
- ACCNUM field is only used when MODE selects multi-sample/accumulation modes
- ADC produces 12-bit results (0–4095) — no MCC changes needed
- Updated all code to use 12-bit math (ADC_SHIFT=12, ADC_MAX=4095, freq band scaling for 4096 range)

## 2026-07-22 - Debug mode and ADC4 trigger fix

### Issues found
- ADC4 CH0 (VAVG) has same trigger issue as CH1: TRG1SRC=0 means software trigger doesn't work
- State machine blocks DAC3 testing (needs 850ms of valid VAVG before multiplier runs)

### Changes
- Added `AD4CH0CON1bits.TRG1SRC = 1U` in PFC_App_Init() (alongside existing CH1 fix)
- Added `PFC_DEBUG_MODE` define in pfc_params.h (set to 1 for bench testing)
- Debug mode bypasses state machine: sets g_pfc_running=1, starts PWM timebase with output forced low, enables ADC triggering immediately

### Test procedure (debug mode)
1. Verify heartbeat LED (RD0) blinks - Timer1 running
2. Watch g_vavg_raw in debugger - ADC4 CH0 working
3. Scope DAC3 output (RA1) - should track RB2 input shape
4. All with PWM output safely held low

### Next steps
- Compile clean build
- Add source files to MPLAB X project if not auto-detected
- Hardware bring-up: start at low frequency (25 kHz), verify each subsystem
- Tune thresholds in pfc_params.h to match actual resistor divider ratios
- Determine if soft-start uses DAC-controlled Vref or external RC ramp

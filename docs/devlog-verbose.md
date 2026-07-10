# PFC Controller — Verbose Development Log

## 2026-07-10 Session

### Phase 1: Understanding the Project

**Exploration steps:**
- Scanned full project tree: `dspic33A-pfc-controller.X/` contains `main.c` (empty skeleton), `mcc_generated_files/` with ADC1-4, OPA1-3, PWM_HS, CMP3, Timer1, and system drivers.
- Read the draw.io XML (3 pages). Page 1 described a 2-phase interleaved design with PWM1+PWM2, digital 2P2Z compensators, and a complex ISR chain. Page 2 (PNG) showed a simpler single-phase design with analog compensation. Page 3 was continuation of page 1.
- User confirmed: **only page 2 is relevant**. Single-phase, analog comp.
- Attempted to fetch UCC2818-EP datasheet PDF from TI — WebFetch returned binary (couldn't parse PDF). Used existing knowledge of UCC2818 architecture instead.

**Block diagram interpretation (from PNG, page 2):**
```
AC Mains → R-divider → OPA2 → ADC2 (IAC, AN1/RB2)
AC Mains → RC network → ADC4 CH0 (VAVG/VFF, AN2/RB7)
Output bulk → R-divider → External Vref on OPA1(+), feedback on OPA1(-)
OPA1 output → ADC1 (VOUT/Verr, AN0/RA2)
Current sense → OPA3 (with RC comp network) → ADC3 (CA-OUT, AN0/RA5)
ADC4 CH1 (AN4/RB10) → frequency select resistor divider (software triggered)

Software path: ADC2 × ADC1 / (ADC4_CH0)² → DAC3 → OPA3(+) input
               ADC3 output → scale → PG1DC

Hardware OCP: current sense → COMP3 vs DAC3 threshold → PCI fault → cycle-by-cycle blanking
```

### Phase 2: Questions and Clarifications

**Q: Single-phase or 2-phase?**
A: Single-phase. Ignore pages 1 and 3.

**Q: What is "ADC-to-PWM translation"?**
A: ADC3 reads OPA3 output (analog current compensator) and translates to PG1DC duty cycle. Soft-start is handled by ramping the positive input of OPA1 — may be DAC-controlled or external, not yet decided.

**Q: What does the "+1" block mean?**
A: Offset to prevent divide-by-zero in the feedforward denominator.

**Q: What is ADC4 CH1 used for?**
A: Switching frequency selection. Resistor divider read at startup. 20 positions from 25–500 kHz in 25 kHz steps. Must validate against 1% tolerance + ADC error + noise. Fault if out of band.

**Q: VAVG update rate?**
A: My recommendation — chose 10 kHz (Timer1, 100us) since VAVG changes at 2× line frequency (~120 Hz). No need to burn ISR cycles at switching frequency.

**Q: Which ISR is master?**
A: ADC2 — since IAC is needed for multiplier and it's the primary computation path.

**Q: ADC4 CH1 / VPFC usage?**
A: NOT for voltage regulation or OV/UV detection (that's handled by OPA1 analog loop + ADC1). It sets switching frequency at startup only.

### Phase 3: Design Decisions

**ISR architecture:**
- ADC2 ISR (priority 6): Read IAC + Verr, compute multiplier, write DAC3. ~50 cycles.
- ADC3 ISR (priority 5): Read OPA3 output, linear map to PG1DC. ~30 cycles.
- Timer1 callback (priority 1): Read VAVG, compute 1/VAVG², run state machine. ~200 cycles.

Priority 6 > 5 ensures ADC2 (multiplier) completes before ADC3 (duty) if both pending simultaneously. This matters because DAC3 must be updated before OPA3 settles for the next ADC3 sample.

**Why Timer1 uses callback not ISR override:**
Read tmr1.c — the `_T1Interrupt` is NOT declared weak (unlike ADC ISRs). MCC uses `TMR1_TimeoutHandler` callback registered at init. The `TMR1_TimeoutCallback()` function IS weak. We override that.

**Fixed-point math strategy:**
- All ADC values are 13-bit unsigned (0–8191) from 4x oversampling
- inv_vavg_sq is Q15 (0–32767 representing 0.0–1.0)
- Two-stage multiply to stay in 32-bit arithmetic:
  - Stage 1: (IAC × Verr) >> 13 → 13-bit result
  - Stage 2: (result × inv_vavg_sq) >> 15 → 13-bit final
- Uses `__builtin_muluu()` for single-cycle hardware multiply

**Frequency selection band design:**
- 13-bit ADC range / 20 positions = 409.6 counts per position
- Band centers: 205, 615, 1025, 1435, ... (205 + n×410)
- Valid region: center ± 150 counts
- Guard band: 110 counts between valid regions (410 - 2×150 = 110)
- 1% resistor tolerance on a 3.3V range: worst-case 82 counts error
- ADC INL/DNL at 13-bit: ~±12 counts
- 82 + 12 = 94 counts error budget, 150 count band → 56 counts margin

**PWM period register format:**
- PG1PER uses 20-bit value: bits [19:4] are integer counts, bits [3:0] are fractional
- So for 100 kHz: integer counts = 400M/100k - 1 = 3999, register = 3999 << 4 = 0xF9F0
- Confirmed matches MCC default: `PG1PER = 0xF9F0UL`

**DAC3 write in ISR:**
- MCC `CMP3_DACDataWrite()` blocks with `while(DAC3CONbits.UPDATE == 1U)` — unacceptable in fast ISR
- Direct write to `DAC3DATbits.DACDAT` is safe: UPDATE clears within one PWM cycle, and we write once per cycle

**State machine timing:**
- STANDBY: wait for VAVG above threshold for 600ms continuously
- RELAY_WAIT: 50ms settling after closing relay (NTC bypass)
- SOFT_START: 200ms duty cycle ramp from ~2.5% to 95%
- RUNNING: monitor OV (1ms debounce), brownout (30ms debounce), OC (immediate from hardware latch)
- FAULT: 5s cooldown then auto-retry to STANDBY

### Phase 4: Implementation

**File structure rationale:**
- `pfc_isr.c` contains all ISRs in one translation unit — compiler can optimize with full visibility
- `pfc_params.h` centralizes all tuning constants — one file to edit during hardware bring-up
- `pfc_app.c` has the slower state machine, never runs in ISR context
- No MCC files modified — all overrides via weak symbol mechanism

**Atomicity analysis:**
- dsPIC33A is 16-bit architecture: 16-bit reads/writes are atomic
- All shared variables between ISRs are `volatile uint16_t` or `volatile uint8_t`
- `g_pg1per_cached` written once during init (before ISRs enabled) — safe
- `g_duty_max_int` written by Timer1 (priority 1), read by ADC3 (priority 5) — ADC3 preempts Timer1 mid-write impossible since higher priority ISR runs atomically from Timer1's perspective... wait, actually Timer1 is LOWER priority, so ADC3 CAN preempt Timer1 during a write. But since `g_duty_max_int` is uint16_t (single instruction read on 16-bit arch), it's inherently atomic.

**Linker error encountered:**
```
multiple definition of `__T1Interrupt'; ... first defined in tmr1.o
```
Root cause: tmr1.c defines `_T1Interrupt` WITHOUT weak attribute (line 135: `void __attribute__ ( ( interrupt ) ) _T1Interrupt(void)`). ADC drivers use `__attribute__ ( ( __interrupt__, weak ) )` but Timer1 does not.
Fix: Override `TMR1_TimeoutCallback()` instead, which IS declared weak.

### ADC Bit Width Investigation

User flagged references to "13-bit ADC" — they don't want oversampling and believe the ADC is 12-bit.

**Investigation:**
- MCC-generated comments say `ACCNUM 4 samples, 13 bits result` on all ADC channels
- Checked device header (p33AK512MPS506.h): ACCNUM is a 2-bit field in ADxCHyCON1 at bits [15:14]
- Decoded register value `0x01230004`:
  - TRG1SRC[5:0] = 0x04 = PWM1_TRIGGER1
  - MODE[7:6] = 0b00 = "Single sample initiated by TRG1SRC trigger"
  - ACCNUM[15:14] = 0b00 = (encoded as 4 samples in the bitfield description)

**User correction:** With MODE=0b00 (single sample mode), only one conversion per trigger regardless of ACCNUM. The ACCNUM setting only takes effect in accumulation modes (MODE=0b01 or similar). So the ADC IS producing 12-bit results (0–4095). No MCC change needed.

**Lesson:** The MCC comment is misleading — it reports the ACCNUM register field value ("4 samples") even though it's irrelevant in Single Sample mode.

**Code changes:**
- `ADC_13BIT_MAX` → `ADC_MAX = 4095`
- All `>> 13` shifts → `>> ADC_SHIFT` (defined as 12)
- Frequency selection bands recalculated for 4096 range (spacing 205, offset 102, half-width 75)

### Observations / Things to Revisit
- DAC3 resolution: initialized with DACDAT=205. Need to verify if this is 12-bit (0–4095) or 9-bit. The `DAC3DAT = 0xCD00CD` register packs both DACDAT and DACLOW fields. Code currently clamps to 4095 (12-bit assumption).
- OV detection logic: currently checks `AD1CH0DATA > VOUT_OV_THRESHOLD`. This assumes OPA1 output rises when Vout rises (non-inverting on the error side). Need to verify polarity during bring-up — if OPA1 is inverting (error amp), low output = high Vout, and the comparison should be `< threshold`.
- ADC trigger point: set to 50% of period. May need adjustment depending on inductor current ripple and optimal sampling point.
- The `__builtin_muluu()` intrinsic: confirmed available in XC-DSC for dsPIC33A. Returns uint32_t from two uint16_t inputs in a single cycle.

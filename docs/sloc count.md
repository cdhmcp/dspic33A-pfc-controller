# MCC SLOC Reduction Estimate — dsPIC33A PFC Controller

**Estimate only — no implementation planned.** All figures are SLOC (comments and blank
lines excluded). Measured by brace-matching actual function bodies, not extrapolated.

## Note on the earlier discrepancy

Two numbers were reported earlier; the first was right and the second was wrong.

| Method | MCC SLOC | Correct? |
|---|---|---|
| Strip comments, then count non-blank lines | **4,573** | ✅ |
| Raw total − comment lines − blank lines | 4,379 | ❌ undercounts by 194 |

The subtraction method double-subtracts any line holding *both* code and a comment — e.g.
`AD1CON1 = 0x8000; // enable` is counted as a comment line and removed even though it is
code. The giveaway: `adc_features.h` computed to **−7** SLOC. `adc_features.h` and
`pwm_features.h` are pure doxygen prose, so a correct count gives them ~11–16 SLOC of
include guards, not a negative.

**Correct baseline: MCC 4,573 SLOC + app 391 SLOC = 4,964 total.**

---

## Baseline

| | Raw | **SLOC** |
|---|---|---|
| MCC generated (53 files) | 11,214 | **4,573** |
| Hand-written app (5 files) | 552 | **391** |
| **Total** | **11,766** | **4,964** |

MCC SLOC by module:

| Module | SLOC | Concentration |
|---|---|---|
| adc | 2,204 | 4 near-identical instances |
| pwm_hs | 970 | `pwm.h` alone = 563 |
| system | 703 | |
| opa | 292 | 3 near-identical instances |
| cmp | 181 | |
| timer | 122 | |
| uref | 26 | |

Single largest files: `pwm_hs/pwm.h` 563 · `adc/src/adc4.c` 355 · `adc2.c`/`adc3.c` 317 each ·
`adc1.c` 312 · `pwm.c` 276 · `adc4.h` 200 · `adc1-3.h` 169 each.

### Why so much is removable

The app calls only **8 MCC functions**: `SYSTEM_Initialize`, `SW0_RC3_GetValue`,
`CMP3_Calibrate`, `OPA1_Disable`, `OPA3_UnityGainEnable`, `LED_RD0_Toggle`,
`LED_RC8_SetLow/SetHigh`, and it *defines* `TMR1_TimeoutCallback` as a weak override.
Everything time-critical is already raw SFR access (`PG1DC`, `AD2CH0DATA`, `DAC3DATbits`).
MCC serves as a one-shot register initializer; the entire generated API surface beyond
`*_Initialize` is unreferenced.

---

## Reduction by strategy (SLOC)

### S1 — Delete zero-code and dead files: **−123 SLOC**

| File | SLOC |
|---|---|
| `system/src/reset.c` | 72 |
| `system/watchdog.h` | 16 |
| `adc/adc_features.h` | 16 |
| `pwm_hs/pwm_features.h` | 11 |
| `system/reset.h` | 8 |

The two `*_features.h` files are pure documentation. `reset.*` and `watchdog.h` have zero
callers — not even from `system.c`. Note this deletes **614 raw lines** but only 123 SLOC:
these files are almost entirely comment.

### S2 — Delete interface/vtable indirection: **−186 SLOC** (headers) **+ ~60** (struct instances)

| Header | SLOC |
|---|---|
| `pwm_hs/pwm_hs_interface.h` | 64 |
| `adc/adc_interface.h` | 50 |
| `cmp/cmp_interface.h` | 35 |
| `timer/timer_interface.h` | 20 |
| `opa/opa_interface.h` | 17 |

Plus the 11 `const struct` definitions in the `.c` files (`ADC1`–`ADC4`, `PWM_HS`, `Timer1`,
`CMP_DAC3`, `dac3_dc_interface`, `OPA1`–`OPA3`) ≈ 60 SLOC. **Total ≈ −246.**

These vtables take the address of every member function, which is why `ADC1_Deinitialize`,
`ADC2_ChannelCallbackRegister`, and `PWM_GeneratorTasks` all appear in the current `.map`
despite never being called. Removing them is also the prerequisite for the linker stripping
the dead API — not a SLOC concern, but it makes S3 verifiable.

### S3 — Delete never-called API bodies: **−2,113 SLOC** ⭐ largest win

Measured per function, not estimated:

| Group | SLOC | Detail |
|---|---|---|
| ADC `.c` dead functions ×4 | **761** | adc1 183, adc2 186, adc3 186, adc4 206 |
| ADC `.h` inline accessors ×4 | **582** | 15 fns each: 138+138+138+168 |
| `pwm.h` inline statics | **493** | 44 functions, of the file's 563 SLOC |
| `pwm.c` beyond `PWM_Initialize` | **222** | 276 total − 54 for Initialize |
| `cmp3.c` beyond Init+Calibrate | **55** | 78 total − 15 − 8 |
| `opa1-3.c` beyond Initialize | **37** | 77 total − 20 |
| `tmr1.c` beyond Init + `_T1Interrupt` | **49** | 64 total − 10 − 5 |
| `uref.c` beyond Initialize | ~8 | |

Inside the ADC group, the single biggest item is **`ADCn_TriggerSourceValueGet()` at 114 SLOC
each — 456 SLOC across 4 files.** It is a switch over `ADC_PWM_GENERATOR_*` cases reachable
only from `ADCn_PWMTriggerSourceSet()`, which is never called. The app configures triggers by
writing `AD1CH0CON1` directly at init.

Also dead: the `*_types.h` enums that only the dead API consumed (`adc_types.h` 30,
`pwm_hs_types.h` 56, `opa_types.h` 51, `cmp_types.h` 13, `clock_types.h` 35,
`interrupt_types.h` 14, `system_types.h` 13, `reset_types.h` 10) ≈ **222 SLOC**, partially
overlapping the above — verify per-enum before counting.

### S4 — Collapse duplicated instances: **−1,177 SLOC**

ADC: 4 instances = 2,008 SLOC total. `adc1.h`/`adc2.h`/`adc3.h` are 169 SLOC each and
**structurally identical — the diff is purely the instance digit**; `adc2.c`/`adc3.c` are
317 each, likewise. ADC4 differs only by a second channel (CH1, the frequency-select ladder
read at `pfc_app.c:44`).

- ADC → one parameterized instance + per-instance register tables: 2,008 → **~1,050** (−958)
- OPA: 3 instances = 224 SLOC → **~90** (−134)
- Alias macro blocks (10 `Timer1_*` + 10 `CMP_DAC3_*`) → **−85**

**Note S3 and S4 overlap heavily.** Delete the dead API first and the ADC files shrink from
2,008 to ~665 SLOC, leaving far less to de-duplicate. Do not sum S3 and S4 independently —
the cumulative table below accounts for this.

### S5 — Comment stripping: **−0 SLOC**

Worth stating explicitly since it dominated the earlier raw-line analysis: 5,005 raw lines
(45% of the tree) are block comments, and removing them changes SLOC by **zero**. On a SLOC
metric this strategy is worthless. Skip it — and keep the `//CH0RDY disabled;` register-value
annotations inside the `*_Initialize` bodies, which are useful certification evidence.

---

## Cumulative

| Stage | MCC SLOC | Project SLOC | Reduction |
|---|---|---|---|
| Baseline | 4,573 | 4,964 | — |
| +S1 (dead files) | 4,450 | 4,841 | −2% |
| +S2 (vtables) | 4,204 | 4,595 | −7% |
| +S3 (dead API) | **2,091** | **2,482** | **−50%** |
| +S4 (de-duplicate) | **~1,290** | **~1,681** | **−66%** |
| Hand-written init instead | ~700 | ~1,091 | −78% |

## The irreducible floor

What must survive, measured:

| Item | SLOC |
|---|---|
| `system/src/traps.c` + `traps.h` | 192 |
| `system/src/config_bits.c` (`#pragma config`) | 105 |
| `system/src/clock.c` | 99 |
| ADC `Initialize` ×4 (19+21+21+25) | 86 |
| ADC `SharedCorePowerEnable`/`SharedCoreCalibration` ×4 | 56 |
| ADC `_ADnCH0Interrupt` ×4 (2 replaced by app) | 56 |
| `pwm.c` `PWM_Initialize` | 54 |
| `system/src/pins.c` + `pins.h` | 82 |
| `system/src/system.c` init chain | 36 |
| `cmp3.c` Initialize + Calibrate | 23 |
| `opa1-3.c` Initialize | 20 |
| `tmr1.c` Initialize + `_T1Interrupt` | 15 |
| `system/src/interrupt.c` | 15 |
| `dmt.c` (if DMT kept) | 68 |
| **Floor** | **≈ 900–970** |

The actual peripheral configuration is tiny — every ADC `Initialize` is 19–25 SLOC. Around
**80% of the MCC tree is API scaffolding around ~250 SLOC of real register writes.**

---

## Effort vs. return

| Strategy | SLOC | Nature | Risk |
|---|---|---|---|
| S1 | −123 | delete files | none |
| S2 | −246 | delete files + structs | low |
| S3 | **−2,113** | delete functions | low — compiler catches every mistake |
| S4 | −1,177 | **rewrite** working init | med-high |
| S5 | 0 | — | skip |

**S3 is the whole story: 50% of the project SLOC, by deletion only.** Nothing is rewritten,
so the compiler flags any symbol still needed. S1+S2+S3 together get to 2,482 project SLOC
with no logic changes at all.

S4 adds 16 points more but is the only strategy that rewrites working peripheral
initialization on a live converter — the worst risk-to-SLOC ratio here. If the certification
target lands above ~2,500 SLOC, S4 buys nothing worth its risk.

---

## Three constraints on any cut

1. **`pfc_isr.c:71` defines `TMR1_TimeoutCallback` as a weak override.** MCC's `_T1Interrupt`
   in `timer/src/tmr1.c` dispatches to it *and clears* `IFS0bits.T1IF`. `tmr1.c` cannot be
   deleted wholesale despite its whole public API being unused — losing `_T1Interrupt` means
   an unserviced 25 kHz interrupt.
2. **`pfc_isr.c:10,42` define `_AD2CH0Interrupt`/`_AD3CH0Interrupt`,** replacing the weak
   handlers in `adc2.c`/`adc3.c`. Those files' `Initialize` register setup is live.
3. **ADC4 and OPA2 are not interchangeable with their siblings.** `adc4.h` carries a second
   channel; `OPA2` is initialized but never touched by the app — confirm it is unused on the
   board before dropping its init.

## Context for interpreting these numbers

Flash is 23,104 bytes = 4%, RAM 156 bytes = <1% (`dist/default/production/*.map`), so nothing
here is driven by memory pressure. Also worth noting for a certification count: the project
is mid-bring-up with `PFC_DEBUG_MODE 1U` (`pfc_params.h:8`) still enabled, and the
normal-operation path — brown-in, relay-wait, soft-start — has likely not yet been exercised
on hardware. Whichever SLOC figure gets reported will move once that path is validated.

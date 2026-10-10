# 5 V rail sense: build, verify, interpret (T008 / T011)

The POWER check senses the 5 V supply rail through a resistive divider on
GPIO34. This page documents how to build the divider (T008, issue #27),
how to verify it, and what the POWER check result means (T011, TC-003).

## Circuit

Two 10 kΩ resistors in series between 5 V and GND. The firmware reads the
midpoint on GPIO34 (input-only pad, plain `INPUT`, ADC1_CH13) and doubles the
ADC volts to the rail-equivalent value (`src/PowerSense.{h,cpp}`). GPIO34-39
have **no internal pull-up/pull-down**, so the defined low level of an unwired
pin comes from the external R2, not from the MCU: requesting `INPUT_PULLDOWN`
here only makes the IDF driver log `gpio_pulldown_en(123): GPIO number error
(input-only pad has no internal PD)` and applies nothing.

    5 V ---[ 10 kΩ ]---+---[ 10 kΩ ]--- GND
                       |
                     GPIO34

The midpoint sits at about 2.5 V on a healthy 5 V rail (inside the 0-3.3 V
ADC range).

## Build (T008, #27)

1. Solder 10 kΩ R1 from the 5 V rail to the GPIO34 pad.
2. Solder 10 kΩ R2 from the GPIO34 pad to GND.
3. Verify with a multimeter (powered on): about 2.5 V at the GPIO34 pad
   against GND.

## Expected readings (rail-equivalent)

| Rail volts | Boot POWER result | Meaning |
|------------|-------------------|---------|
| < 1.0 | NOT_WIRED (info) | divider absent, floating pad near 0 V (D3: absence is not a fault) |
| 1.0-4.50 | FAIL `POWER_LOW 0x0201` | rail low, device enters Error state (FR-009) |
| 4.50-4.75 | WARN `POWER_LOW 0x0201` | rail low, device stays operable |
| >= 4.75 | OK | normal |

Caveat while the divider is absent (#27 open): GPIO34 floats, so the
NOT_WIRED classification is best effort. The bench board measured 0.28 V
rail-equivalent (2026-10-10), but nothing holds a floating pad below the 1.0 V
threshold - if it drifted above it, the runtime monitor would treat the garbage
as a present rail and enter Error after 5 samples. Only the external R2 (or at
least a 100 k pull-down to GND) makes NOT_WIRED deterministic.

Boot: single-shot read. Runtime: 1 Hz monitor (FR-005). A runtime FAIL
needs 5 consecutive samples below 4.50 V (debounce); the 4.50-4.55 V
hysteresis band neither extends nor clears the streak; recovery above
4.55 V clears it and demotes the state.

## Where results appear

- Serial at boot: `CHECK id=POWER result=<STATUS> [code=0xNNNN] meas=<V> elapsed_ms=<ms>`
  (the `code=` field is only present for non-None codes).
- Web: `GET /api/status` -> `selftest.checks[id="POWER"]` + `selftest.overall`.
- UI: "Selbsttest" card, POWER row.
- Device state: any FAIL -> `STATE old=<s> new=Error` + `FAULT code=0xNNNN
  <NAME>` lines (FR-009).

## TC-003(a): observed matrix, no wiring (2026-08-21)

Bench state: HX711 + load cell wired, power divider NOT built (T008 open),
firmware 0.4.0 (`01e9ef5`, code identical to `5bd8ef7`).

    [ 0.512] BOOT fw=0.4.0 git=01e9ef59 reset=POWERON
    [ 0.515] STATE old=Booting new=SelfTest
    [ 0.520] CHECK id=BOARD result=OK meas=303636.00 elapsed_ms=0
    [ 0.529] CHECK id=NVS result=OK meas=1.00 elapsed_ms=12
    [ 0.535] CHECK id=POWER result=NOT_WIRED meas=0.28 elapsed_ms=0
    [ 1.031] CHECK id=HX711 result=WARN code=0x301 meas=0.00 elapsed_ms=500
    [ 1.036] CHECK id=MOTOR result=OK code=0x401 meas=0.00 elapsed_ms=0
    [ 1.040] STATE old=SelfTest new=Idle

| id | Expected (plan §4) | Observed | Match |
|----|--------------------|----------|-------|
| BOARD | OK | OK (free heap 303636 B) | yes |
| NVS | OK | OK (meas 1.00) | yes |
| POWER | NOT_WIRED (info) | NOT_WIRED (meas 0.28 V) | yes |
| HX711 | WARN or NOT_CONNECTED | WARN 0x301 NO_VARIATION (load cell wired, no load change in the 500 ms window) | yes, see note |
| MOTOR | OK (NO_FEEDBACK note) | OK 0x401 NO_FEEDBACK | yes |
| overall | Idle within 5 s, warnings only | Idle at t=1.040 s | yes (SC-001) |

**SC-001 note:** the spec originally expected `HX711 NOT_CONNECTED`. The
actual bench has the load cell wired, so the observed status is
`WARN HX711_NO_VARIATION 0x301` (connected, no load change during the
500 ms variance window). `NOT_CONNECTED` still occurs when the load cell
is unplugged. Both are warnings, and the SC-001 requirement (Idle within
5 s, warnings only) holds. spec.md SC-001 and the plan §4 table were
corrected to match the real bench.

Remaining rows (need bench work):

- (b) HX711 `OK`: brief load change on the load cell within the 500 ms
  boot window (e.g. touch the platform or set the known weight down).
- (c) POWER `OK`: after T008 divider is built, expect `meas` near 5.0 V.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| Still NOT_WIRED after building | Measure 2.5 V at the GPIO34 pad; re-check solder joints |
| FAIL POWER_LOW with a known-good 5 V | Rail really < 4.50 V (measure at the source), wrong resistor values, wrong GND reference |
| WARN only | Rail between 4.50 and 4.75 V: check PSU, cable sag under motor load |
| Intermittent runtime FAIL | 5-sample debounce already applied; check for brownout under motor start (measure rail while ramping `rpm`) |

# Load Cell Calibration - Operator Guide

How to calibrate the force measurement, and what mass to use. The
interactive wizard is specified in `specs/M5-loadcell-calibration/spec.md`
(M5); the manual commands exist today (`cal_zero`, `cal_apply <grams>`).

## What mass should I use?

- **Calibrate near the loads you actually measure.** The calibration is a
  straight line through zero and your mass point; the relative error is
  smallest at the mass you calibrate with. A mass at roughly **80% of the
  highest force you expect in tests** is the practical optimum.
- **The mass's own accuracy dominates.** Whatever you hang on the cell
  becomes the truth the machine is tuned to. A kitchen scale-verified
  dumbbell plate is fine for bench work; do not expect certified
  (OIML R 60 / ISO 376 / ASTM E74) traceability from ordinary weights.
- **Do not exceed the load cell rating.** This machine's YZC-516C is rated
  500 kg (~4.9 kN). A suggestion of "calibrate at 400 kg" is textbook-correct
  for a 500 kg cell but only useful if you own such a mass - calibrating at
  your realistic working load is better than not calibrating at all.
- **Textbook alternative:** for a 500 kg cell, calibrating at ~80% of rated
  capacity (~400 kg) minimizes span error across the full range. Choose
  this only if your tests use the full range.

## Manual procedure (available today)

1. `cal_zero` with the carriage unloaded - this tares the zero point.
2. Hang the known mass (verified on a scale, ideally).
3. `cal_apply <grams>` with the mass in **grams** - e.g. a 50 kg mass is
   `cal_apply 50000`.
4. The machine computes the factor (raw counts per gram), rejects a zero
   or negative result, and saves it to NVS - it survives reboots.
5. `force` should now read the mass times 9.80665 N/kg.

## Notes

- A mis-entered mass shifts every force reading proportionally. Future
  firmware (M5) adds a physics-derived plausibility check that rejects
  gross typos (factor outside ~[0.3x, 3x] of what the load cell hardware
  can produce).
- Tare again (`cal_zero` / `tare`) if the zero point drifts with
  temperature or after rig changes.
- Recalibrate after mechanical changes to the force path (grips, adapters,
  lever geometry).

// Loadcell contract (M0-T010, FR-007).
//
// Abstracts the HX711 module for dependency inversion and host-side
// fakes. Implemented by the concrete LoadCell class. No dynamic
// allocation: consumers hold references to statically created objects.
//
// The interface mirrors LoadCell's public API (plan.md section 3,
// adapted to the API that emerged in M0-T004/T005). CalError lives on
// the contract: it names the failure modes of the calibration operation
// and is part of the behavior any implementation must promise.
#pragma once

#include "Settings.h"

struct ILoadCell {
  enum class CalError {
    None,
    NotReady,
    ZeroNotCaptured,
    InvalidMass,
    ReadFailed,
    ZeroFactor,
  };

  virtual ~ILoadCell() = default;

  // HX711 begin + NVS calibration load via settings.
  virtual void begin(Settings &settings) = 0;

  // 10 Hz sample + low-pass filter; call from the loop task.
  virtual void poll() = 0;
  virtual bool isReady() = 0;
  virtual float forceNewton() = 0;

  // Tare: zero the filtered output (no-op when not ready).
  virtual void tare() = 0;

  virtual bool calibrationValid() = 0;
  virtual float calibrationFactor() = 0;

  // Calibration step 1: zero-point capture. False when not ready.
  virtual bool captureZeroPoint() = 0;

  // Calibration step 2: known-mass apply + NVS save.
  virtual CalError calibrateWithMass(float grams) = 0;
};

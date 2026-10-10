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

#include <cstdint>

#include "Settings.h"

struct ILoadCell {
  enum class CalError {
    None,
    NotReady,
    ZeroNotCaptured,
    InvalidMass,
    ReadFailed,
    ZeroFactor,
    NegativeFactor,
  };

  virtual ~ILoadCell() = default;

  // HX711 begin + NVS calibration load via settings.
  virtual void begin(Settings &settings) = 0;

  // 10 Hz sample + low-pass filter; call from the loop task.
  virtual void poll() = 0;
  virtual bool isReady() = 0;
  virtual float forceNewton() = 0;

  // Pre-filter force snapshot of the last poll() sample (M3 FR-003:
  // BreakDetector input). NOT rawSample(): that issues a second blocking
  // read and desyncs the 10 Hz cadence - this is the same sample the
  // filter consumed, zero extra HX711 traffic.
  virtual float rawForceNewton() = 0;

  // Selftest accessors (M1 FR-006): explicit-timeout wait_ready and a
  // single raw 24-bit sample. Independent of the isReady() state, which
  // belongs to begin()/poll(); the selftest reports via CheckResult.
  virtual bool waitReady(unsigned long timeoutMs) = 0;
  virtual bool rawSample(int32_t &raw) = 0;

  // Tare: zero the filtered output (no-op when not ready).
  virtual void tare() = 0;

  virtual bool calibrationValid() = 0;
  virtual float calibrationFactor() = 0;

  // Calibration step 1: zero-point capture. False when not ready.
  virtual bool captureZeroPoint() = 0;

  // Calibration step 2: known-mass apply + NVS save.
  virtual CalError calibrateWithMass(float grams) = 0;
};

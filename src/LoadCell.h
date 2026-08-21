// HX711 loadcell module (M0-T004, NVS via Settings since M0-T005,
// implements ILoadCell since M0-T010).
//
// Owns the HX711 driver, the 10 Hz sample + low-pass force filter, the
// in-memory calibration state, and the two-step calibration (zero-point
// capture + known-mass apply). NVS persistence (namespace "testmachine",
// keys lc_valid / lc_factor) lives in Settings. Command handlers in
// main.cpp own the response strings; this class owns the state and the
// compute.
#pragma once

#include <Arduino.h>
#include <HX711.h>

#include "ILoadCell.h"
#include "Settings.h"

class LoadCell : public ILoadCell {
 public:
  using CalError = ILoadCell::CalError;

  // HX711 begin + wait_ready(2000), NVS calibration load via settings,
  // set_scale, tare(1). Prints the legacy boot diagnostics.
  void begin(Settings &settings) override;

  bool isReady() override;
  float forceNewton() override;
  bool calibrationValid() override;
  float calibrationFactor() override;
  bool waitReady(unsigned long timeoutMs) override;
  bool rawSample(int32_t &raw) override;

  // 10 Hz sample + low-pass filter; call from the loop task.
  void poll() override;

  // scale.tare(1) + zero the filtered output (no-op when not ready).
  void tare() override;

  // Calibration step 1: set_scale, tare(1), capture zero-point flag,
  // zero the filtered output. Returns false when not ready.
  bool captureZeroPoint() override;

  // Calibration step 2: settle delay, averaged read,
  // factor = (avg - offset) / grams, NVS save, set_scale, reset filter
  // and zero-point flag.
  CalError calibrateWithMass(float grams) override;

 private:
  bool readAverageRawValue(uint8_t sampleCount, long &averageRaw);

  static constexpr uint8_t kDoutPin = 32;
  static constexpr uint8_t kSckPin = 33;
  static constexpr unsigned long kSampleIntervalMs = 100;
  static constexpr float kGravity = 9.80665f;
  static constexpr float kFilterAlpha = 0.18f;
  static constexpr uint8_t kCalibrationCaptureSamples = 10;
  static constexpr unsigned long kCalibrationSettleMs = 1200;

  HX711 scale_;
  Settings *settings_ = nullptr;

  unsigned long lastSampleMs_ = 0;
  float forceNewton_ = 0.0f;
  bool ready_ = false;
  bool calibrationValid_ = false;
  float calibrationFactor_ = Settings::kDefaultCalibrationFactor;  // default until begin() loads NVS
  bool calibrationZeroCaptured_ = false;
};

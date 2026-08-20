// NVS settings facade (M0-T005).
//
// Owns the Preferences namespace "testmachine" and the calibration keys
// (lc_valid / lc_factor). LoadCell consumes it via reference; no other
// module should touch Preferences directly.
#pragma once

#include <Arduino.h>
#include <Preferences.h>

class Settings {
 public:
  // Single source of truth for the default calibration factor (1 N per
  // raw-count unit); LoadCell's pre-begin fallback uses it too.
  static constexpr float kDefaultCalibrationFactor = 1.0f;

  struct Calibration {
    bool valid = false;
    float factor = kDefaultCalibrationFactor;
  };

  // Read the calibration from NVS. Legacy fallback behavior preserved:
  // invalid flag or factor == 0 resets to the default factor 1.0.
  Calibration loadCalibration();

  // Write the calibration to NVS and mark it valid.
  void saveCalibration(float factor);

 private:
  static constexpr const char *kNamespace = "testmachine";
  static constexpr const char *kPrefCalibrationValid = "lc_valid";
  static constexpr const char *kPrefCalibrationFactor = "lc_factor";

  Preferences preferences_;
};

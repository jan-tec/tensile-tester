// NVS settings facade implementation (M0-T005).
//
// Behavior carried over unchanged from the legacy src/main.cpp /
// LoadCell (M0 FR-004): namespace, keys, default factor, and the
// invalid-or-zero fallback.
#include "Settings.h"

Settings::Calibration Settings::loadCalibration() {
  Calibration calibration;

  preferences_.begin(kNamespace, true);
  calibration.valid =
      preferences_.getBool(kPrefCalibrationValid, false);
  calibration.factor = preferences_.getFloat(
      kPrefCalibrationFactor, kDefaultCalibrationFactor);
  preferences_.end();

  if (!calibration.valid || calibration.factor == 0.0f) {
    calibration.valid = false;
    calibration.factor = kDefaultCalibrationFactor;
  }
  return calibration;
}

void Settings::saveCalibration(float factor) {
  preferences_.begin(kNamespace, false);
  preferences_.putFloat(kPrefCalibrationFactor, factor);
  preferences_.putBool(kPrefCalibrationValid, true);
  preferences_.end();
}

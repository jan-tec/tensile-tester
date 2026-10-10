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

Settings::Calibration Settings::readRawCalibration() {
  Calibration calibration;

  preferences_.begin(kNamespace, true);
  calibration.valid =
      preferences_.getBool(kPrefCalibrationValid, false);
  calibration.factor = preferences_.getFloat(
      kPrefCalibrationFactor, kDefaultCalibrationFactor);
  preferences_.end();
  return calibration;
}

void Settings::saveCalibration(float factor) {
  preferences_.begin(kNamespace, false);
  preferences_.putFloat(kPrefCalibrationFactor, factor);
  preferences_.putBool(kPrefCalibrationValid, true);
  preferences_.end();
}

Locale Settings::loadLocale() {
  preferences_.begin(kNamespace, true);
  const String stored = preferences_.getString(kPrefLocale, "");
  preferences_.end();
  // Missing key ("") and unknown values resolve to En (FR-002 default).
  return localeFromName(stored.c_str());
}

void Settings::saveLocale(Locale locale) {
  preferences_.begin(kNamespace, false);
  preferences_.putString(kPrefLocale, localeName(locale));
  preferences_.end();
}

TestConfig Settings::loadTestConfig() {
  const TestConfig d = defaultTestConfig();
  TestConfig c;

  preferences_.begin(kNamespace, true);
  c.testSpeedMmPerMin =
      preferences_.getFloat(kPrefTestSpeed, d.testSpeedMmPerMin);
  c.armForceN = preferences_.getFloat(kPrefTestArm, d.armForceN);
  c.dropPercent = preferences_.getFloat(kPrefTestDrop, d.dropPercent);
  c.homingClearanceMm =
      preferences_.getFloat(kPrefHomeClear, d.homingClearanceMm);
  c.homingSeekMmPerMin =
      preferences_.getFloat(kPrefHomeSeek, d.homingSeekMmPerMin);
  c.homingTimeoutMs =
      preferences_.getUInt(kPrefHomeTimeout, d.homingTimeoutMs);
  preferences_.end();

  // Single validation boundary (TestConfig.h): NVS can hold anything a
  // previous firmware wrote - non-finite falls back per field, values
  // clamp to the operator-safe windows.
  return clampTestConfig(c);
}

void Settings::saveTestConfig(const TestConfig &config) {
  preferences_.begin(kNamespace, false);
  preferences_.putFloat(kPrefTestSpeed, config.testSpeedMmPerMin);
  preferences_.putFloat(kPrefTestArm, config.armForceN);
  preferences_.putFloat(kPrefTestDrop, config.dropPercent);
  preferences_.putFloat(kPrefHomeClear, config.homingClearanceMm);
  preferences_.putFloat(kPrefHomeSeek, config.homingSeekMmPerMin);
  preferences_.putUInt(kPrefHomeTimeout, config.homingTimeoutMs);
  preferences_.end();
}

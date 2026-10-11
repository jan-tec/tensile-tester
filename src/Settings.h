// NVS settings facade (M0-T005).
//
// Owns the Preferences namespace "testmachine" and the calibration keys
// (lc_valid / lc_factor). LoadCell consumes it via reference; no other
// module should touch Preferences directly.
#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <FirmwareCore.h>  // Locale
#include <TestConfig.h>    // M3 FR-005: test cycle config

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

  // Raw stored values WITHOUT the legacy fallback (M1-T004 NVS sanity
  // check): lets Diagnostics distinguish "first boot, no keys" from a
  // corrupt config (valid flag set, non-positive factor).
  Calibration readRawCalibration();

  // Write the calibration to NVS and mark it valid.
  void saveCalibration(float factor);

  // Test cycle config (M3 FR-005, T012): six operator knobs. Load
  // applies clampTestConfig() once - the single validation boundary - so
  // everything downstream (runner, models, replies) may trust the values.
  // Save writes the fields verbatim; callers pass the already-clamped
  // runner config.
  TestConfig loadTestConfig();
  void saveTestConfig(const TestConfig &config);

  // UI locale (M2 FR-002, key "locale"). Missing/unknown stored value -> En
  // (localeFromName's factory-fresh default).
  Locale loadLocale();
  void saveLocale(Locale locale);

 private:
  static constexpr const char *kNamespace = "testmachine";
  static constexpr const char *kPrefCalibrationValid = "lc_valid";
  static constexpr const char *kPrefCalibrationFactor = "lc_factor";
  static constexpr const char *kPrefLocale = "locale";
  // Test config keys (Preferences limit: <= 15 chars).
  static constexpr const char *kPrefTestSpeed = "test_speed";
  static constexpr const char *kPrefTestArm = "test_arm";
  static constexpr const char *kPrefTestDrop = "test_drop";
  static constexpr const char *kPrefHomeClear = "home_clear";
  static constexpr const char *kPrefHomeSeek = "home_seek";
  static constexpr const char *kPrefHomeTimeout = "home_tout";
  static constexpr const char *kPrefHoldTol = "hold_tol";    // M4 FR-001
  static constexpr const char *kPrefHoldClear = "hold_clear";
  static constexpr const char *kPrefHoldMax = "hold_max";    // #220

  Preferences preferences_;
};

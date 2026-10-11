// Unit tests for the pure calibration compute (host side, env:native).
// Run: pio test -e native
//
// Semantics extracted from LoadCell::calibrateWithMass (M0 FR-004
// behavior-preserving). The legacy zero-only rejection policy was a
// quirk: negative factors were silently accepted (NQP #46); the sign
// guard fixed in the PR for that issue.
#include <unity.h>

#include <FirmwareCore.h>

#define TEST_ASSERT_VERDICT(expected, actual) \
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual))

void test_calibration_factor_formula(void) {
  // factor = (averageRaw - offset) / grams
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f,
                           calibrationFactorFromMass(10000, 5000, 500.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f,
                           calibrationFactorFromMass(10100, 10000, 400.0f));
}

void test_calibration_factor_negative_below_offset(void) {
  // Noisy cell / offset drift: the averaged reading falls below the
  // captured offset, so the factor is negative.
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -2.0f,
                           calibrationFactorFromMass(4000, 5000, 500.0f));
}

void test_verify_calibration_factor_rejects_zero(void) {
  // Zero factor keeps the legacy rejection (existing response message).
  TEST_ASSERT_VERDICT(CalibrationVerdict::Zero,
                      verifyCalibrationFactor(0.0f));
  TEST_ASSERT_VERDICT(CalibrationVerdict::Valid,
                      verifyCalibrationFactor(10.0f));
}

void test_verify_calibration_factor_rejects_negative(void) {
  // NQP #46 fix: a negative factor (reading below the offset:
  // noisy cell, temperature drift, part disturbed between cal_zero and
  // cal_apply) is rejected instead of being silently persisted to NVS.
  TEST_ASSERT_VERDICT(CalibrationVerdict::Negative,
                      verifyCalibrationFactor(-2.0f));
  TEST_ASSERT_VERDICT(CalibrationVerdict::Negative,
                      verifyCalibrationFactor(-0.0001f));
}

void runCalibrationTests() {
  RUN_TEST(test_calibration_factor_formula);
  RUN_TEST(test_calibration_factor_negative_below_offset);
  RUN_TEST(test_verify_calibration_factor_rejects_zero);
  RUN_TEST(test_verify_calibration_factor_rejects_negative);
}

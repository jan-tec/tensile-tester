// Unit tests for the pure device state machine (host side, env:native).
// Run: pio test -e native
//
// M1 FR-001: Booting -> SelfTest -> Idle | Error. Every fault path ends in
// Error with the triggering machine code; the caller (Diagnostics) disables
// the motor driver when the model reports a fault. TC-001: 100% of the
// 4 states x 5 events transition table is pinned here, RED-first.
//
// Transition table (pinned):
//   Booting:   SelfTestStart->SelfTest  CheckResult->Booting
//              SelfTestDone->Booting    PowerLow->Booting   Reset->Booting
//   SelfTest:  SelfTestStart->SelfTest  CheckResult(0)->SelfTest
//              CheckResult(code)->Error SelfTestDone->Idle  Reset->Booting
//   Idle:      SelfTestStart->Idle      CheckResult->Idle
//              SelfTestDone->Idle       PowerLow->Error     Reset->Booting
//   Error:     SelfTestStart->Error     CheckResult->Error
//              SelfTestDone->Error      PowerLow->Error (first fault wins)
//              Reset->Booting
#include <unity.h>

#include <FirmwareCore.h>

#define TEST_ASSERT_STATE(expected, model) \
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>((model).state()))

void test_device_model_initial_state(void) {
  DeviceModel model;
  TEST_ASSERT_STATE(DeviceState::Booting, model);
  TEST_ASSERT_EQUAL_UINT16(0, model.faultCode());
}

void test_device_model_boot_to_selftest(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  TEST_ASSERT_STATE(DeviceState::SelfTest, model);
}

void test_device_model_check_ok_stays_in_selftest(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::CheckResult, 0);  // OK / WARN carry no code
  TEST_ASSERT_STATE(DeviceState::SelfTest, model);
  TEST_ASSERT_EQUAL_UINT16(0, model.faultCode());
}

void test_device_model_check_fail_enters_error_with_code(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::CheckResult, 0x0301);  // HX711 fault code
  TEST_ASSERT_STATE(DeviceState::Error, model);
  TEST_ASSERT_EQUAL_UINT16(0x0301, model.faultCode());
}

void test_device_model_selftest_done_to_idle(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::CheckResult, 0);
  model.dispatch(DeviceEvent::CheckResult, 0);
  model.dispatch(DeviceEvent::SelfTestDone);
  TEST_ASSERT_STATE(DeviceState::Idle, model);
  TEST_ASSERT_EQUAL_UINT16(0, model.faultCode());
}

void test_device_model_power_low_in_idle_enters_error(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::SelfTestDone);
  model.dispatch(DeviceEvent::PowerLow, 0x0201);
  TEST_ASSERT_STATE(DeviceState::Error, model);
  TEST_ASSERT_EQUAL_UINT16(0x0201, model.faultCode());
}

void test_device_model_first_fault_wins(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::CheckResult, 0x0301);
  model.dispatch(DeviceEvent::PowerLow, 0x0201);  // ignored in Error
  TEST_ASSERT_STATE(DeviceState::Error, model);
  TEST_ASSERT_EQUAL_UINT16(0x0301, model.faultCode());
}

void test_device_model_reset_from_every_state(void) {
  DeviceModel model;
  model.dispatch(DeviceEvent::Reset);  // Booting
  TEST_ASSERT_STATE(DeviceState::Booting, model);

  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::Reset);  // SelfTest
  TEST_ASSERT_STATE(DeviceState::Booting, model);

  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::SelfTestDone);
  model.dispatch(DeviceEvent::Reset);  // Idle
  TEST_ASSERT_STATE(DeviceState::Booting, model);

  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::CheckResult, 0x0301);
  model.dispatch(DeviceEvent::Reset);  // Error
  TEST_ASSERT_STATE(DeviceState::Booting, model);
  TEST_ASSERT_EQUAL_UINT16(0, model.faultCode());  // fault cleared
}

void test_device_model_invalid_events_are_no_ops(void) {
  // SelfTestStart outside Booting
  DeviceModel a;
  a.dispatch(DeviceEvent::SelfTestStart);  // SelfTest
  a.dispatch(DeviceEvent::SelfTestStart);
  TEST_ASSERT_STATE(DeviceState::SelfTest, a);

  // SelfTestDone outside SelfTest
  DeviceModel b;
  b.dispatch(DeviceEvent::SelfTestDone);  // Booting
  TEST_ASSERT_STATE(DeviceState::Booting, b);

  // CheckResult outside SelfTest
  DeviceModel c;
  c.dispatch(DeviceEvent::SelfTestStart);
  c.dispatch(DeviceEvent::SelfTestDone);  // Idle
  c.dispatch(DeviceEvent::CheckResult, 0x0301);
  TEST_ASSERT_STATE(DeviceState::Idle, c);
  TEST_ASSERT_EQUAL_UINT16(0, c.faultCode());

  // PowerLow outside Idle
  DeviceModel d;
  d.dispatch(DeviceEvent::SelfTestStart);  // SelfTest
  d.dispatch(DeviceEvent::PowerLow, 0x0201);
  TEST_ASSERT_STATE(DeviceState::SelfTest, d);
  TEST_ASSERT_EQUAL_UINT16(0, d.faultCode());

  // SelfTestStart / SelfTestDone / CheckResult in Error
  DeviceModel e;
  e.dispatch(DeviceEvent::SelfTestStart);
  e.dispatch(DeviceEvent::CheckResult, 0x0401);
  e.dispatch(DeviceEvent::SelfTestStart);
  e.dispatch(DeviceEvent::SelfTestDone);
  e.dispatch(DeviceEvent::CheckResult, 0x0101);
  TEST_ASSERT_STATE(DeviceState::Error, e);
  TEST_ASSERT_EQUAL_UINT16(0x0401, e.faultCode());
}

void test_device_model_reboot_recovery_path(void) {
  // FR-009: Error -> reset/reboot -> SelfTest -> Idle again.
  DeviceModel model;
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::SelfTestDone);
  model.dispatch(DeviceEvent::PowerLow, 0x0201);
  TEST_ASSERT_STATE(DeviceState::Error, model);

  model.dispatch(DeviceEvent::Reset);
  model.dispatch(DeviceEvent::SelfTestStart);
  model.dispatch(DeviceEvent::SelfTestDone);
  TEST_ASSERT_STATE(DeviceState::Idle, model);
  TEST_ASSERT_EQUAL_UINT16(0, model.faultCode());
}

void test_device_state_name_strings(void) {
  TEST_ASSERT_EQUAL_STRING("Booting", deviceStateName(DeviceState::Booting));
  TEST_ASSERT_EQUAL_STRING("SelfTest", deviceStateName(DeviceState::SelfTest));
  TEST_ASSERT_EQUAL_STRING("Idle", deviceStateName(DeviceState::Idle));
  TEST_ASSERT_EQUAL_STRING("Error", deviceStateName(DeviceState::Error));
  // Unknown values must not crash (defensive name lookup).
  TEST_ASSERT_NOT_NULL(deviceStateName(static_cast<DeviceState>(99)));
}

void runDeviceModelTests(void) {
  RUN_TEST(test_device_model_initial_state);
  RUN_TEST(test_device_model_boot_to_selftest);
  RUN_TEST(test_device_model_check_ok_stays_in_selftest);
  RUN_TEST(test_device_model_check_fail_enters_error_with_code);
  RUN_TEST(test_device_model_selftest_done_to_idle);
  RUN_TEST(test_device_model_power_low_in_idle_enters_error);
  RUN_TEST(test_device_model_first_fault_wins);
  RUN_TEST(test_device_model_reset_from_every_state);
  RUN_TEST(test_device_model_invalid_events_are_no_ops);
  RUN_TEST(test_device_model_reboot_recovery_path);
  RUN_TEST(test_device_state_name_strings);
}

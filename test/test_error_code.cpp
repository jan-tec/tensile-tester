// Unit tests for the selftest vocabulary helpers (host side, env:native).
// Run: pio test -e native
//
// M1 FR-008 (T010, RED-first): the overall rating of the /api/status
// selftest object - any FAIL dominates, otherwise any WARN, and
// NOT_WIRED / NOT_CONNECTED are info only (design D3: absence is not a
// fault, never FAIL, never lowers the overall rating).
#include <unity.h>

#include <ErrorCode.h>

void test_overall_all_ok(void) {
  const CheckStatus statuses[3] = {
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 3)));
}

void test_overall_fail_dominates(void) {
  const CheckStatus statuses[4] = {
      CheckStatus::Warn, CheckStatus::Ok,
      CheckStatus::Fail, CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Fail),
                    static_cast<int>(overallStatus(statuses, 4)));
}

void test_overall_warn_when_no_fail(void) {
  const CheckStatus statuses[2] = {CheckStatus::Ok, CheckStatus::Warn};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Warn),
                    static_cast<int>(overallStatus(statuses, 2)));
}

void test_overall_info_statuses_do_not_lower(void) {
  const CheckStatus statuses[3] = {
      CheckStatus::Ok, CheckStatus::NotWired, CheckStatus::NotConnected};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 3)));
}

void test_overall_empty_is_ok(void) {
  const CheckStatus statuses[1] = {CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 0)));
}

void runErrorCodeTests() {
  RUN_TEST(test_overall_all_ok);
  RUN_TEST(test_overall_fail_dominates);
  RUN_TEST(test_overall_warn_when_no_fail);
  RUN_TEST(test_overall_info_statuses_do_not_lower);
  RUN_TEST(test_overall_empty_is_ok);
}

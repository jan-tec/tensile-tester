// Unit tests for the pure test-cycle state machine (M3 FR-002, TC-001).
// Run: pio test -e native
//
// TestCycleModel is the second, orthogonal FSM (plan D1): DeviceModel keeps
// the health contract, this model only tracks the cycle. TC-001 pins EVERY
// row of the plan section 5 transition table including the invalid
// combinations (dispatch false, state unchanged).
#include <unity.h>

#include <TestCycle.h>

namespace {
void expectState(const TestCycleModel &m, TestState s) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(s), static_cast<int>(m.state()));
}
}  // namespace

void test_idle_starts_homing(void) {
  TestCycleModel m;
  expectState(m, TestState::Idle);
  TEST_ASSERT_FALSE(m.active());
  TEST_ASSERT_TRUE(m.dispatch(TestEvent::Start));
  expectState(m, TestState::Homing);
  TEST_ASSERT_TRUE(m.active());
}

void test_homing_rows(void) {
  TestCycleModel m;
  m.dispatch(TestEvent::Start);
  TEST_ASSERT_TRUE(m.dispatch(TestEvent::HomeDone));
  expectState(m, TestState::Pulling);

  TestCycleModel f;
  f.dispatch(TestEvent::Start);
  TEST_ASSERT_TRUE(f.dispatch(TestEvent::HomeFault));
  expectState(f, TestState::Error);

  TestCycleModel p;
  p.dispatch(TestEvent::Start);
  TEST_ASSERT_TRUE(p.dispatch(TestEvent::PowerFail));
  expectState(p, TestState::Error);

  TestCycleModel s;
  s.dispatch(TestEvent::Start);
  TEST_ASSERT_TRUE(s.dispatch(TestEvent::StopRequested));
  expectState(s, TestState::Aborted);
}

void test_pulling_rows(void) {
  TestCycleModel m;
  m.dispatch(TestEvent::Start);
  m.dispatch(TestEvent::HomeDone);
  TEST_ASSERT_TRUE(m.dispatch(TestEvent::BreakDetected));
  expectState(m, TestState::Tail);

  TestCycleModel b;
  b.dispatch(TestEvent::Start);
  b.dispatch(TestEvent::HomeDone);
  TEST_ASSERT_TRUE(b.dispatch(TestEvent::BufferFull));
  expectState(b, TestState::Tail);  // truncated flag is the runner's (T004)

  TestCycleModel t;
  t.dispatch(TestEvent::Start);
  t.dispatch(TestEvent::HomeDone);
  TEST_ASSERT_TRUE(t.dispatch(TestEvent::TopLimitHit));
  expectState(t, TestState::Limit);

  TestCycleModel p;
  p.dispatch(TestEvent::Start);
  p.dispatch(TestEvent::HomeDone);
  TEST_ASSERT_TRUE(p.dispatch(TestEvent::PowerFail));
  expectState(p, TestState::Error);

  TestCycleModel s;
  s.dispatch(TestEvent::Start);
  s.dispatch(TestEvent::HomeDone);
  TEST_ASSERT_TRUE(s.dispatch(TestEvent::StopRequested));
  expectState(s, TestState::Aborted);
}

void test_tail_rows(void) {
  TestCycleModel m;
  m.dispatch(TestEvent::Start);
  m.dispatch(TestEvent::HomeDone);
  m.dispatch(TestEvent::BreakDetected);
  TEST_ASSERT_TRUE(m.dispatch(TestEvent::TailComplete));
  expectState(m, TestState::Done);
  TEST_ASSERT_FALSE(m.active());

  TestCycleModel p;
  p.dispatch(TestEvent::Start);
  p.dispatch(TestEvent::HomeDone);
  p.dispatch(TestEvent::BreakDetected);
  TEST_ASSERT_TRUE(p.dispatch(TestEvent::PowerFail));
  expectState(p, TestState::Error);

  TestCycleModel s;
  s.dispatch(TestEvent::Start);
  s.dispatch(TestEvent::HomeDone);
  s.dispatch(TestEvent::BreakDetected);
  TEST_ASSERT_TRUE(s.dispatch(TestEvent::StopRequested));
  expectState(s, TestState::Aborted);
}

void test_terminal_states_restart(void) {
  // Done | Limit | Aborted restart with Start -> Homing (one per terminal state)
  TestCycleModel d;
  d.dispatch(TestEvent::Start);
  d.dispatch(TestEvent::HomeDone);
  d.dispatch(TestEvent::BreakDetected);
  d.dispatch(TestEvent::TailComplete);
  TEST_ASSERT_TRUE(d.dispatch(TestEvent::Start));
  expectState(d, TestState::Homing);

  TestCycleModel l;
  l.dispatch(TestEvent::Start);
  l.dispatch(TestEvent::HomeDone);
  l.dispatch(TestEvent::TopLimitHit);
  TEST_ASSERT_TRUE(l.dispatch(TestEvent::Start));
  expectState(l, TestState::Homing);

  TestCycleModel a;
  a.dispatch(TestEvent::Start);
  a.dispatch(TestEvent::StopRequested);
  TEST_ASSERT_TRUE(a.dispatch(TestEvent::Start));
  expectState(a, TestState::Homing);
}

void test_error_reset_and_invalid(void) {
  TestCycleModel m;
  m.dispatch(TestEvent::Start);
  m.dispatch(TestEvent::HomeFault);
  expectState(m, TestState::Error);
  // only Reset leaves Error
  TEST_ASSERT_FALSE(m.dispatch(TestEvent::Start));
  expectState(m, TestState::Error);
  TEST_ASSERT_TRUE(m.dispatch(TestEvent::Reset));
  expectState(m, TestState::Idle);
}

void test_idle_rejects_everything_but_start(void) {
  const TestEvent all[] = {
      TestEvent::HomeDone,  TestEvent::HomeFault,  TestEvent::BreakDetected,
      TestEvent::TopLimitHit, TestEvent::TailComplete, TestEvent::BufferFull,
      TestEvent::StopRequested, TestEvent::PowerFail, TestEvent::Reset};
  for (TestEvent e : all) {
    TestCycleModel m;
    TEST_ASSERT_FALSE_MESSAGE(m.dispatch(e), TestCycleModel::name(m.state()));
    expectState(m, TestState::Idle);
  }
}

void test_invalid_transitions_are_noops(void) {
  // Homing: BreakDetected/TailComplete/BufferFull/TopLimitHit/Reset invalid
  {
    TestCycleModel m;
    m.dispatch(TestEvent::Start);
    const TestEvent bad[] = {TestEvent::BreakDetected, TestEvent::TailComplete,
                             TestEvent::BufferFull, TestEvent::TopLimitHit,
                             TestEvent::Reset, TestEvent::Start};
    for (TestEvent e : bad) {
      TEST_ASSERT_FALSE(m.dispatch(e));
      expectState(m, TestState::Homing);
    }
  }
  // Pulling: HomeDone/TailComplete/Reset invalid; Start invalid (busy)
  {
    TestCycleModel m;
    m.dispatch(TestEvent::Start);
    m.dispatch(TestEvent::HomeDone);
    const TestEvent bad[] = {TestEvent::HomeDone, TestEvent::TailComplete,
                             TestEvent::Reset, TestEvent::Start};
    for (TestEvent e : bad) {
      TEST_ASSERT_FALSE(m.dispatch(e));
      expectState(m, TestState::Pulling);
    }
  }
  // Tail: BreakDetected/HomeDone/BufferFull/Reset invalid
  {
    TestCycleModel m;
    m.dispatch(TestEvent::Start);
    m.dispatch(TestEvent::HomeDone);
    m.dispatch(TestEvent::BreakDetected);
    const TestEvent bad[] = {TestEvent::BreakDetected, TestEvent::HomeDone,
                             TestEvent::BufferFull, TestEvent::Reset,
                             TestEvent::Start};
    for (TestEvent e : bad) {
      TEST_ASSERT_FALSE(m.dispatch(e));
      expectState(m, TestState::Tail);
    }
  }
  // Done/Limit/Aborted: only Start valid
  {
    TestCycleModel m;
    m.dispatch(TestEvent::Start);
    m.dispatch(TestEvent::HomeDone);
    m.dispatch(TestEvent::BreakDetected);
    m.dispatch(TestEvent::TailComplete);
    const TestEvent bad[] = {TestEvent::HomeDone, TestEvent::PowerFail,
                             TestEvent::Reset, TestEvent::TailComplete};
    for (TestEvent e : bad) {
      TEST_ASSERT_FALSE(m.dispatch(e));
      expectState(m, TestState::Done);
    }
  }
}

void test_state_names_stable(void) {
  // /api/status + log grammar surface: these strings are a contract.
  TEST_ASSERT_EQUAL_STRING("Idle", TestCycleModel::name(TestState::Idle));
  TEST_ASSERT_EQUAL_STRING("Homing", TestCycleModel::name(TestState::Homing));
  TEST_ASSERT_EQUAL_STRING("Pulling", TestCycleModel::name(TestState::Pulling));
  TEST_ASSERT_EQUAL_STRING("Tail", TestCycleModel::name(TestState::Tail));
  TEST_ASSERT_EQUAL_STRING("Done", TestCycleModel::name(TestState::Done));
  TEST_ASSERT_EQUAL_STRING("Limit", TestCycleModel::name(TestState::Limit));
  TEST_ASSERT_EQUAL_STRING("Aborted", TestCycleModel::name(TestState::Aborted));
  TEST_ASSERT_EQUAL_STRING("Error", TestCycleModel::name(TestState::Error));
}

void runCycleModelTests(void) {
  RUN_TEST(test_idle_starts_homing);
  RUN_TEST(test_homing_rows);
  RUN_TEST(test_pulling_rows);
  RUN_TEST(test_tail_rows);
  RUN_TEST(test_terminal_states_restart);
  RUN_TEST(test_error_reset_and_invalid);
  RUN_TEST(test_idle_rejects_everything_but_start);
  RUN_TEST(test_invalid_transitions_are_noops);
  RUN_TEST(test_state_names_stable);
}

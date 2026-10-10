#ifndef TESTCYCLE_H
#define TESTCYCLE_H

// M3 FR-002, TC-001: the test-cycle state machine, an FSM orthogonal to
// DeviceModel (plan D1). DeviceModel stays the health state machine whose
// codes and /api/status `state` field are stable contracts; a running test
// is "busy", not a fault. This model only tracks the cycle and never touches
// hardware, time, or error codes: the runner (src/TestRunner, T011) feeds
// events and translates Exit states into DeviceModel events.
//
// Transition table (specs/M3-tensile-test-cycle/plan.md section 5), pinned
// row-by-row by test/test_cycle_model.cpp:
//
//   Idle       | Start                        -> Homing
//   Homing     | HomeDone                     -> Pulling
//   Homing     | HomeFault | PowerFail        -> Error
//   Homing     | StopRequested                -> Aborted
//   Pulling    | BreakDetected                -> Tail
//   Pulling    | BufferFull                   -> Tail (truncated = runner's flag)
//   Pulling    | TopLimitHit                  -> Limit
//   Pulling    | PowerFail                    -> Error
//   Pulling    | StopRequested                -> Aborted
//   Tail       | TailComplete                 -> Done
//   Tail       | PowerFail                    -> Error
//   Tail       | StopRequested                -> Aborted
//   Done|Limit|Aborted | Start               -> Homing (restart a new cycle)
//   Error      | Reset                        -> Idle
//   Idle       | anything but Start           -> no-op
//   every other (from, event)                -> no-op, dispatch() returns false
//
// Exit states (Done, Limit, Aborted, Error) are reported to the operator by
// the runner (FR-006); `Reset` only applies to Error, matching DeviceModel's
// FR-009 recovery-by-reset convention.

enum class TestState {
  Idle,     // nothing running
  Homing,   // staged home search (US1)
  Pulling,  // continuous upward pull until break or top limit (US1)
  Tail,     // post-break downward travel (US3)
  Done,     // break detected, tail complete (normal exit)
  Limit,    // top limit reached without break (no-code exit, no break)
  Aborted,  // operator stop (no-code exit)
  Error,    // fault path (homing fault, power fail)
};

enum class TestEvent {
  Start,
  HomeDone,
  HomeFault,
  BreakDetected,
  TopLimitHit,
  TailComplete,
  BufferFull,
  StopRequested,
  PowerFail,
  Reset,
};

class TestCycleModel {
 public:
  TestState state() const { return state_; }

  // True while a cycle is running (Homing|Pulling|Tail): the runner uses it
  // for command exclusivity and to gate tick() work.
  bool active() const {
    return state_ == TestState::Homing || state_ == TestState::Pulling ||
           state_ == TestState::Tail;
  }

  bool dispatch(TestEvent event) {
    switch (state_) {
      case TestState::Idle:
        if (event == TestEvent::Start) {
          state_ = TestState::Homing;
          return true;
        }
        return false;
      case TestState::Homing:
        switch (event) {
          case TestEvent::HomeDone:
            state_ = TestState::Pulling;
            return true;
          case TestEvent::HomeFault:
          case TestEvent::PowerFail:
            state_ = TestState::Error;
            return true;
          case TestEvent::StopRequested:
            state_ = TestState::Aborted;
            return true;
          default:
            return false;
        }
      case TestState::Pulling:
        switch (event) {
          case TestEvent::BreakDetected:
          case TestEvent::BufferFull:
            state_ = TestState::Tail;
            return true;
          case TestEvent::TopLimitHit:
            state_ = TestState::Limit;
            return true;
          case TestEvent::PowerFail:
            state_ = TestState::Error;
            return true;
          case TestEvent::StopRequested:
            state_ = TestState::Aborted;
            return true;
          default:
            return false;
        }
      case TestState::Tail:
        switch (event) {
          case TestEvent::TailComplete:
            state_ = TestState::Done;
            return true;
          case TestEvent::PowerFail:
            state_ = TestState::Error;
            return true;
          case TestEvent::StopRequested:
            state_ = TestState::Aborted;
            return true;
          default:
            return false;
        }
      case TestState::Done:
      case TestState::Limit:
      case TestState::Aborted:
        if (event == TestEvent::Start) {
          state_ = TestState::Homing;  // restart
          return true;
        }
        return false;
      case TestState::Error:
        if (event == TestEvent::Reset) {
          state_ = TestState::Idle;
          return true;
        }
        return false;
    }
    return false;  // unreachable: every state is handled
  }

  // Stable status/log names (/api/status `testState` field, FR-006 surface).
  static const char *name(TestState state) {
    switch (state) {
      case TestState::Idle:
        return "Idle";
      case TestState::Homing:
        return "Homing";
      case TestState::Pulling:
        return "Pulling";
      case TestState::Tail:
        return "Tail";
      case TestState::Done:
        return "Done";
      case TestState::Limit:
        return "Limit";
      case TestState::Aborted:
        return "Aborted";
      case TestState::Error:
        return "Error";
    }
    return "Idle";  // unreachable: every state is handled
  }

 private:
  TestState state_ = TestState::Idle;
};

#endif  // TESTCYCLE_H

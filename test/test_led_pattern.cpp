// Unit tests for the LED status language pattern engine (host, env:native).
// Run: pio test -e native
//
// M8 FR-003..FR-008 / TC-101 (specs/M8-led-status/spec.md, plan.md sec 5).
//
// Pinned semantics the contract leaves open:
//   - duty255 gamma values are literals (7/34/83/255), not recomputed
//   - pattern period = sum(steps onMs) + count*gap(350) + pauseMs:
//     SelfTest 2100, Idle 3000, Homing 2500, Error 3000 ms
//   - an event passed to update() starts the overlay AT that call
//     (the event timestamp is the update call's nowMs)
//   - overlay = 2 reps of (group + pause), total = 2*group + 1*pause
//   - after the overlay the base pattern resumes as a pure function of
//     absolute time (phases never drift)
//   - Error restarts its phase at the moment the base becomes Error and
//     drops any running overlay; events during Error are ignored
#include <cstdio>
#include <vector>

#include <unity.h>

#include <LedPattern.h>

namespace {

struct Sample {
  uint32_t t;
  bool on;
  led::Level level;
};

// Walks the FSM from t0 for durationMs, sampling every stepMs (default
// 50 ms); the first call forwards firstEvent, later calls None.
std::vector<Sample> walk(led::LedPatternFSM& fsm, led::LedBase base,
                         led::LedEvent firstEvent, uint32_t t0,
                         uint32_t durationMs, uint32_t stepMs = 50) {
  std::vector<Sample> out;
  for (uint32_t t = 0; t <= durationMs; t += stepMs) {
    led::LedOutput o =
        fsm.update(base, t == 0 ? firstEvent : led::LedEvent::None, t0 + t);
    out.push_back({t0 + t, o.on, o.level});
  }
  return out;
}

// Rising edges: off->on transitions, including a first observed ON.
std::vector<uint32_t> risingEdges(const std::vector<Sample>& s) {
  std::vector<uint32_t> edges;
  bool prevOn = false;
  for (const auto& x : s) {
    if (x.on && !prevOn) edges.push_back(x.t);
    prevOn = x.on;
  }
  return edges;
}

led::LedOutput at(led::LedPatternFSM& fsm, led::LedBase base, uint32_t t) {
  return fsm.update(base, led::LedEvent::None, t);
}

uint8_t pct(led::Level l) { return static_cast<uint8_t>(l); }

void assertFlashRateWithin3(const std::vector<uint32_t>& e, const char* what) {
  for (size_t i = 0; i < e.size(); ++i) {
    uint32_t inWindow = 0;
    for (size_t j = i; j < e.size() && e[j] - e[i] < 1000; ++j) ++inWindow;
    if (inWindow > 3) {
      char msg[96];
      snprintf(msg, sizeof(msg), "%s: window@%u has %u rising edges",
               what, e[i], static_cast<unsigned>(inWindow));
      TEST_FAIL_MESSAGE(msg);
    }
  }
}

}  // namespace

void test_led_duty255GammaTable() {
  TEST_ASSERT_EQUAL_UINT8(7, led::duty255(led::Level::Dim));
  TEST_ASSERT_EQUAL_UINT8(34, led::duty255(led::Level::Mid));
  TEST_ASSERT_EQUAL_UINT8(83, led::duty255(led::Level::High));
  TEST_ASSERT_EQUAL_UINT8(255, led::duty255(led::Level::Full));
}

void test_led_levelPercents() {
  TEST_ASSERT_EQUAL_UINT8(20, pct(led::Level::Dim));
  TEST_ASSERT_EQUAL_UINT8(40, pct(led::Level::Mid));
  TEST_ASSERT_EQUAL_UINT8(60, pct(led::Level::High));
  TEST_ASSERT_EQUAL_UINT8(100, pct(led::Level::Full));
}

void test_led_selfTestPattern() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  auto s = walk(fsm, led::LedBase::SelfTest, led::LedEvent::None, 0, 6300);
  auto e = risingEdges(s);
  TEST_ASSERT_EQUAL_UINT32(4, e.size());  // 0, 2100, 4200, 6300
  TEST_ASSERT_EQUAL_UINT32(0, e[0]);
  TEST_ASSERT_EQUAL_UINT32(2100, e[1]);
  TEST_ASSERT_EQUAL_UINT32(4200, e[2]);
  TEST_ASSERT_EQUAL_UINT32(6300, e[3]);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::SelfTest, 0).on);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::SelfTest, 599).on);
  TEST_ASSERT_FALSE(at(fsm, led::LedBase::SelfTest, 600).on);
  TEST_ASSERT_EQUAL_UINT8(100, pct(at(fsm, led::LedBase::SelfTest, 100).level));
}

void test_led_idlePattern() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::Idle, 0).on);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::Idle, 149).on);
  TEST_ASSERT_FALSE(at(fsm, led::LedBase::Idle, 150).on);
  TEST_ASSERT_FALSE(at(fsm, led::LedBase::Idle, 2999).on);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::Idle, 3000).on);
  TEST_ASSERT_TRUE(at(fsm, led::LedBase::Idle, 6000).on);
  TEST_ASSERT_EQUAL_UINT8(20, pct(at(fsm, led::LedBase::Idle, 0).level));
}

void test_led_homingPattern() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  auto s = walk(fsm, led::LedBase::Homing, led::LedEvent::None, 0, 5000);
  auto e = risingEdges(s);
  TEST_ASSERT_EQUAL_UINT32(5, e.size());  // 0, 500, 2500, 3000, 5000
  TEST_ASSERT_EQUAL_UINT32(0, e[0]);
  TEST_ASSERT_EQUAL_UINT32(500, e[1]);  // 150 on + 350 gap
  TEST_ASSERT_EQUAL_UINT32(2500, e[2]);
  TEST_ASSERT_EQUAL_UINT8(60, pct(at(fsm, led::LedBase::Homing, 0).level));
}

void test_led_runningSteady() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  for (uint32_t t = 0; t <= 5000; t += 250) {
    led::LedOutput o = at(fsm, led::LedBase::Running, t);
    TEST_ASSERT_TRUE(o.on);
    TEST_ASSERT_EQUAL_UINT8(40, pct(o.level));
  }
}

void test_led_errorPattern() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  auto s = walk(fsm, led::LedBase::Error, led::LedEvent::None, 0, 3000);
  auto e = risingEdges(s);
  TEST_ASSERT_EQUAL_UINT32(4, e.size());  // 0, 500, 1000, 3000
  TEST_ASSERT_EQUAL_UINT32(0, e[0]);
  TEST_ASSERT_EQUAL_UINT32(500, e[1]);
  TEST_ASSERT_EQUAL_UINT32(1000, e[2]);
  TEST_ASSERT_EQUAL_UINT32(3000, e[3]);
  TEST_ASSERT_EQUAL_UINT8(100, pct(at(fsm, led::LedBase::Error, 0).level));
}

void test_led_endstopOverlayTwoReps() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  led::LedOutput o0 = fsm.update(led::LedBase::Idle, led::LedEvent::EndstopEdge, 5000);
  TEST_ASSERT_TRUE(o0.on);  // overlay starts at the event call
  TEST_ASSERT_EQUAL_UINT8(100, pct(o0.level));
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 5600).on);
  // rep 2 group starts at 600 on + 350 gap + 1000 inter-rep pause = 6950
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 6900).on);
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 6950).on);
  // overlay total = 2 reps - trailing pause = 2900 ms -> over at 7900
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 7950).on);
  // base Idle is a pure function of t: next heartbeat at 9000
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 9000).on);
}

void test_led_latestEventWins() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  led::LedOutput o1 = fsm.update(led::LedBase::Idle, led::LedEvent::EndstopEdge, 5000);
  TEST_ASSERT_TRUE(o1.on);  // long flash of the endstop overlay
  led::LedOutput o2 = fsm.update(led::LedBase::Idle, led::LedEvent::Break, 5500);
  TEST_ASSERT_TRUE(o2.on);  // break pattern restarts now (short, Full)
  TEST_ASSERT_EQUAL_UINT8(100, pct(o2.level));
  // break: rep = 2*(150+350) + 1000 pause = 2000; rep 2 starts at 7500;
  // overlay total = 2*2000 - 1000 = 3000 -> over at 8500
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 5700).on);
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 6050).on);  // 2nd short of rep 1
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 7550).on);  // rep 2 short 1
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 8600).on);
}

void test_led_calOverlayShortThenLong() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  led::LedOutput o = fsm.update(led::LedBase::Idle, led::LedEvent::CalApplied, 1000);
  TEST_ASSERT_TRUE(o.on);  // short first
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 1200).on);  // gap
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 1500).on);   // long
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 2200).on);  // long over
}

void test_led_errorDropsOverlayAndIgnoresEvents() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  fsm.update(led::LedBase::Idle, led::LedEvent::EndstopEdge, 2000);
  led::LedOutput o = fsm.update(led::LedBase::Error, led::LedEvent::None, 2500);
  TEST_ASSERT_TRUE(o.on);  // Error phase restarts at the transition
  TEST_ASSERT_EQUAL_UINT8(100, pct(o.level));
  led::LedOutput o2 = fsm.update(led::LedBase::Error, led::LedEvent::Break, 2600);
  TEST_ASSERT_TRUE(o2.on);  // still Error pattern (2600 -> 2nd short [500,650))
  // phase 550 at 3050 -> still 2nd short; pause starts at 1150 -> off at 3700
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Error, led::LedEvent::None, 3050).on);
  TEST_ASSERT_FALSE(fsm.update(led::LedBase::Error, led::LedEvent::None, 3700).on);
}

void test_led_beginResets() {
  led::LedPatternFSM fsm;
  fsm.begin(0);
  fsm.update(led::LedBase::Idle, led::LedEvent::EndstopEdge, 5000);
  fsm.begin(9000);
  TEST_ASSERT_TRUE(fsm.update(led::LedBase::Idle, led::LedEvent::None, 9000).on);
}

void test_led_tc101FlashRateAllPatterns() {
  const led::LedBase bases[] = {led::LedBase::SelfTest, led::LedBase::Idle,
                                led::LedBase::Homing, led::LedBase::Running,
                                led::LedBase::Error};
  for (led::LedBase b : bases) {
    led::LedPatternFSM fsm;
    fsm.begin(0);
    auto e = risingEdges(walk(fsm, b, led::LedEvent::None, 0, 30000));
    char what[32];
    snprintf(what, sizeof(what), "base=%d", static_cast<int>(b));
    assertFlashRateWithin3(e, what);
  }
  const led::LedEvent evs[] = {led::LedEvent::EndstopEdge, led::LedEvent::Break,
                               led::LedEvent::CalApplied};
  for (led::LedEvent ev : evs) {
    led::LedPatternFSM fsm;
    fsm.begin(0);
    auto e = risingEdges(walk(fsm, led::LedBase::Idle, ev, 0, 30000));
    char what[32];
    snprintf(what, sizeof(what), "event=%d", static_cast<int>(ev));
    assertFlashRateWithin3(e, what);
  }
}

void runLedPatternTests() {
  RUN_TEST(test_led_duty255GammaTable);
  RUN_TEST(test_led_levelPercents);
  RUN_TEST(test_led_selfTestPattern);
  RUN_TEST(test_led_idlePattern);
  RUN_TEST(test_led_homingPattern);
  RUN_TEST(test_led_runningSteady);
  RUN_TEST(test_led_errorPattern);
  RUN_TEST(test_led_endstopOverlayTwoReps);
  RUN_TEST(test_led_latestEventWins);
  RUN_TEST(test_led_calOverlayShortThenLong);
  RUN_TEST(test_led_errorDropsOverlayAndIgnoresEvents);
  RUN_TEST(test_led_beginResets);
  RUN_TEST(test_led_tc101FlashRateAllPatterns);
}

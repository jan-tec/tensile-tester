// Unit tests for the pure break detector (host side, env:native).
// Run: pio test -e native
//
// M3 FR-003/TC-002: the detector arms when force rises ABOVE the arm
// threshold; it tracks the running peak only while armed; break = force
// falls BELOW (dropFraction x peak); Broken latches; NaN/inf samples are
// ignored. Contract: specs/M3-tensile-test-cycle/plan.md section 5.
//
// Pinned semantics the contract leaves open (documented in BreakDetect.h):
//   - arm is strict: force > armForceN arms, force == armForceN does not
//   - drop is strict: force < dropFraction * peak breaks, == does not
//   - sample index is 0-based over ACCEPTED samples; ignored (NaN/inf)
//     samples neither update state nor advance the index
//   - configure() re-parameterises and resets the detector
//   - after Broken, sample() returns Broken without touching peak/index
#include <cmath>

#include <unity.h>

#include <BreakDetect.h>

namespace {

BreakDetector armed(float arm = 50.0f, float drop = 0.50f) {
  BreakDetector d;
  d.configure(arm, drop);
  return d;
}

const float kNan = std::nanf("");

}  // namespace

void test_breakDetect_startsWaiting() {
  BreakDetector d = armed();
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Waiting),
                        static_cast<int>(d.sample(0.0f)));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());
}

void test_breakDetect_slackImmunityBelowThreshold() {
  // Wild oscillation that never crosses the arm threshold must never arm
  // and never break, however large the swings (FR-003 slack immunity).
  BreakDetector d = armed();
  const float samples[] = {0.0f, 50.0f, 12.5f, 49.99f, -3.0f, 30.0f, 1.0f};
  for (float f : samples) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Waiting),
                          static_cast<int>(d.sample(f)));
  }
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.peakForceN());
}

void test_breakDetect_armsStrictlyAboveThreshold() {
  BreakDetector d = armed(50.0f, 0.50f);
  // exactly at threshold: not armed (pin: strict >)
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Waiting),
                        static_cast<int>(d.sample(50.0f)));
  TEST_ASSERT_FALSE(d.armed());
  // above threshold: armed, the arming sample is the first peak
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(50.1f)));
  TEST_ASSERT_TRUE(d.armed());
  TEST_ASSERT_EQUAL_FLOAT(50.1f, d.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(1, d.peakIndex());  // 50.0 was index 0
}

void test_breakDetect_peakTrackingOnlyWhileArmed() {
  // Sub-threshold spikes happen BEFORE arming: they must not set a peak.
  BreakDetector e = armed();
  e.sample(10.0f);   // idx 0: waiting
  e.sample(49.9f);   // idx 1: waiting, not a peak
  TEST_ASSERT_EQUAL_FLOAT(0.0f, e.peakForceN());
  e.sample(80.0f);   // idx 2: armed, peak 80 @ 2
  TEST_ASSERT_EQUAL_FLOAT(80.0f, e.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(2, e.peakIndex());
  e.sample(60.0f);   // idx 3: armed, no new peak, no break (60 > 40)
  TEST_ASSERT_EQUAL_FLOAT(80.0f, e.peakForceN());
  e.sample(95.0f);   // idx 4: running max updates
  TEST_ASSERT_EQUAL_FLOAT(95.0f, e.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(4, e.peakIndex());
}

void test_breakDetect_noFalseBreakNearThreshold() {
  // Oscillation while armed but always above dropFraction x peak: stays Armed.
  BreakDetector d = armed(50.0f, 0.50f);
  d.sample(100.0f);  // peak 100, drop level 50
  const float above[] = {90.0f, 60.0f, 50.0f, 55.0f, 99.0f};
  for (float f : above) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                          static_cast<int>(d.sample(f)));
  }
  TEST_ASSERT_TRUE(d.armed());
  TEST_ASSERT_EQUAL_FLOAT(100.0f, d.peakForceN());
}

void test_breakDetect_dropVerdictStrictlyBelowDropLevel() {
  BreakDetector d = armed(50.0f, 0.50f);
  d.sample(100.0f);  // peak 100 -> drop level 50
  // exactly at the level: no break (pin: strict <)
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(50.0f)));
  // below it: broken
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(49.9f)));
}

void test_breakDetect_negativeForceBreaksAfterPeak() {
  // Tension cell also reports negative values; falling to/below zero after
  // a peak is a break, not an ignore.
  BreakDetector d = armed();
  d.sample(200.0f);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(-1.0f)));
}

void test_breakDetect_latchesBroken() {
  BreakDetector d = armed();
  d.sample(100.0f);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(10.0f)));
  TEST_ASSERT_FALSE(d.armed());  // armed() is true only in the Armed state
  // recovery after the break does not un-latch and does not move peak/index
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(500.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(1.0f)));
  TEST_ASSERT_EQUAL_FLOAT(100.0f, d.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());
}

void test_breakDetect_ignoresNanAndInf() {
  BreakDetector d = armed();
  d.sample(100.0f);  // idx 0: peak 100
  // -inf is numerically below the drop level but must be ignored entirely
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(kNan)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(-INFINITY)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(INFINITY)));
  // +inf must not poison the peak
  TEST_ASSERT_EQUAL_FLOAT(100.0f, d.peakForceN());
  // ignored samples do not advance the index: next accepted is idx 1
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(90.0f)));  // accepted idx 1
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());  // 90 does not exceed peak 100
  d.sample(105.0f);  // accepted idx 2, new peak
  TEST_ASSERT_EQUAL_FLOAT(105.0f, d.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(2, d.peakIndex());
  // a real drop after the ignored samples still breaks
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(10.0f)));
}

void test_breakDetect_ignoredSamplesBeforeArming() {
  // NaN while Waiting neither arms nor advances the index.
  BreakDetector d = armed();
  d.sample(kNan);
  d.sample(INFINITY);
  TEST_ASSERT_FALSE(d.armed());
  d.sample(60.0f);  // first accepted sample -> index 0
  TEST_ASSERT_TRUE(d.armed());
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());
}

void test_breakDetect_resetClearsEverything() {
  BreakDetector d = armed();
  d.sample(100.0f);
  d.sample(10.0f);  // Broken
  d.reset();
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.peakForceN());
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());
  // latch cleared: the same sequence arms again from index 0
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(80.0f)));
  TEST_ASSERT_EQUAL_UINT32(0, d.peakIndex());
}

void test_breakDetect_configureResetsState() {
  // testarm/testdrop are operator commands that can arrive mid-run;
  // re-configuring pins to a clean detector (documented choice).
  BreakDetector d = armed();
  d.sample(100.0f);
  d.configure(80.0f, 0.25f);
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.peakForceN());
  // new parameters are live: 70 N no longer arms (arm 80)
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Waiting),
                        static_cast<int>(d.sample(70.0f)));
  // 100 N arms; drop level is now 25 (0.25 chosen so the product is exact -
  // the strict-boundary rule itself is pinned in the 0.50 tests)
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(100.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(25.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(24.0f)));
}

void test_breakDetect_dropFractionFullScale() {
  // dropFraction 1.0: any drop below the running peak breaks.
  BreakDetector d = armed(10.0f, 1.0f);
  d.sample(100.0f);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Armed),
                        static_cast<int>(d.sample(100.0f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(BreakDetector::Verdict::Broken),
                        static_cast<int>(d.sample(99.9f)));
}

void runBreakDetectTests() {
  RUN_TEST(test_breakDetect_startsWaiting);
  RUN_TEST(test_breakDetect_slackImmunityBelowThreshold);
  RUN_TEST(test_breakDetect_armsStrictlyAboveThreshold);
  RUN_TEST(test_breakDetect_peakTrackingOnlyWhileArmed);
  RUN_TEST(test_breakDetect_noFalseBreakNearThreshold);
  RUN_TEST(test_breakDetect_dropVerdictStrictlyBelowDropLevel);
  RUN_TEST(test_breakDetect_negativeForceBreaksAfterPeak);
  RUN_TEST(test_breakDetect_latchesBroken);
  RUN_TEST(test_breakDetect_ignoresNanAndInf);
  RUN_TEST(test_breakDetect_ignoredSamplesBeforeArming);
  RUN_TEST(test_breakDetect_resetClearsEverything);
  RUN_TEST(test_breakDetect_configureResetsState);
  RUN_TEST(test_breakDetect_dropFractionFullScale);
}

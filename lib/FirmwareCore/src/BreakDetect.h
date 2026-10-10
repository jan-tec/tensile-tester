#ifndef BREAK_DETECT_H
#define BREAK_DETECT_H

#include <cmath>
#include <cstdint>

// M3 FR-003 / TC-002: pure break detector for the tensile test cycle
// (specs/M3-tensile-test-cycle/plan.md section 5). No Arduino, no heap.
//
// Rules (pinned by test/test_break_detect.cpp):
//   - arms when a sample rises strictly ABOVE armForceN (== does not arm)
//   - the running peak is tracked only while armed; ignored samples and
//     pre-arm samples never touch it
//   - break when a sample falls strictly BELOW dropFraction * peak
//   - Broken latches: sample() keeps returning Broken and state is frozen
//     until reset() (or configure(), which re-parameterises AND resets)
//   - NaN/inf samples are ignored completely: no verdict change, no peak
//     update, no index advance
//   - sample indices are 0-based over accepted samples
//   - armed() is true only in the Armed state (false in Waiting and Broken)
//
// Defaults are the spec A1 provisional values: arm 50 N (~1 % of the
// 4903 N YZC-516C full scale), drop fraction 0.50.
class BreakDetector {
 public:
  enum class Verdict { Waiting, Armed, Broken };

  BreakDetector() = default;

  void configure(float armForceN, float dropFraction) {
    armForceN_ = armForceN;
    dropFraction_ = dropFraction;
    reset();
  }

  void reset() {
    armed_ = false;
    broken_ = false;
    peakN_ = 0.0f;
    peakIndex_ = 0;
    sampleIndex_ = 0;
  }

  Verdict sample(float forceN) {
    if (!std::isfinite(forceN)) {
      return current();  // ignored sample: state untouched, index not advanced
    }
    Verdict verdict = Verdict::Waiting;
    if (broken_) {
      verdict = Verdict::Broken;  // latched: peak and indices frozen
    } else {
      if (!armed_ && forceN > armForceN_) {
        armed_ = true;
      }
      if (armed_) {
        if (forceN > peakN_) {
          peakN_ = forceN;
          peakIndex_ = sampleIndex_;
        }
        if (forceN < dropFraction_ * peakN_) {
          broken_ = true;
          verdict = Verdict::Broken;
        } else {
          verdict = Verdict::Armed;
        }
      }
    }
    ++sampleIndex_;
    return verdict;
  }

  bool armed() const { return armed_ && !broken_; }
  float peakForceN() const { return peakN_; }
  uint32_t peakIndex() const { return peakIndex_; }

 private:
  Verdict current() const {
    if (broken_) return Verdict::Broken;
    if (armed_) return Verdict::Armed;
    return Verdict::Waiting;
  }

  float armForceN_ = 50.0f;
  float dropFraction_ = 0.50f;
  bool armed_ = false;
  bool broken_ = false;
  float peakN_ = 0.0f;
  uint32_t peakIndex_ = 0;
  uint32_t sampleIndex_ = 0;
};

#endif  // BREAK_DETECT_H

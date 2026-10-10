#ifndef LED_PATTERN_H
#define LED_PATTERN_H

#include <stdint.h>

// M8 FR-003..FR-008 / TC-101: pure pattern engine of the LED status
// language (specs/M8-led-status/spec.md, plan.md section 5). No Arduino,
// no heap, no time calls - update() is driven with explicit timestamps.
//
// Pinned semantics (also pinned by test/test_led_pattern.cpp):
//   - timing grid FR-003: short 150 ms, long 600 ms, gap 350 ms;
//     period = sum(steps) + count*gap + pauseMs:
//     SelfTest 2100, Idle 3000, Homing 2500, Error 3000 ms
//   - Running is steady ON at Mid (no flashing)
//   - an event passed to update() starts the overlay at that call; the
//     overlay plays 2 reps of (group + pause); a new event restarts it
//   - after the overlay the base resumes as a pure function of absolute
//     time (phases never drift)
//   - Error (FR-008): restarts its phase at the moment the base becomes
//     Error, drops a running overlay, and ignores further events
//   - duty255() is the gamma-2.2 table (FR-006) pinned to literals:
//     20/40/60/100 % -> 7/34/83/255 (8-bit)
//   - flash safety (FR-003, TC-101): no pattern may produce more than 3
//     ON-rising edges within any 1000 ms window (2/s by construction)

namespace led {

enum class LedBase : uint8_t { SelfTest, Idle, Homing, Running, Error };
enum class LedEvent : uint8_t { None, EndstopEdge, Break, CalApplied };

// Level is the perceptual brightness step in percent (never a value).
enum class Level : uint8_t { Dim = 20, Mid = 40, High = 60, Full = 100 };

inline uint8_t duty255(Level level) {
  switch (level) {
    case Level::Dim: return 7;    // round(255 * 0.20^2.2)
    case Level::Mid: return 34;   // round(255 * 0.40^2.2)
    case Level::High: return 83;  // round(255 * 0.60^2.2)
    default: return 255;
  }
}

struct LedOutput {
  bool on;
  Level level;
};

struct LedStep {
  uint16_t onMs;
  Level level;
};

struct LedPattern {
  const LedStep* steps;
  uint8_t count;
  uint16_t pauseMs;  // off-time after the group (also the inter-rep gap)
  uint8_t reps;
  bool steady;       // true: ignore steps/pause, output is always ON
  Level steadyLevel;
};

// FR-003 timing grid.
constexpr uint16_t kGapMs = 350;
constexpr uint16_t kShortMs = 150;
constexpr uint16_t kLongMs = 600;

// FR-004 pattern table.
// Steps are static (const storage, no heap); levels per spec FR-004.
inline const LedStep kSelfTestSteps[] = {{kLongMs, Level::Full}};
inline const LedStep kIdleSteps[] = {{kShortMs, Level::Dim}};
inline const LedStep kHomingSteps[] = {{kShortMs, Level::High},
                                       {kShortMs, Level::High}};
inline const LedStep kErrorSteps[] = {{kShortMs, Level::Full},
                                      {kShortMs, Level::Full},
                                      {kShortMs, Level::Full}};
inline const LedStep kEndstopSteps[] = {{kLongMs, Level::Full}};
inline const LedStep kBreakSteps[] = {{kShortMs, Level::Full},
                                      {kShortMs, Level::Full}};
inline const LedStep kCalSteps[] = {{kShortMs, Level::Full},
                                    {kLongMs, Level::Full}};

// period(SelfTest) = 600 + 350 + 1150 = 2100 ms (1 long, 1.5 s rest)
inline const LedPattern kSelfTest{kSelfTestSteps, 1, 1150, 1, false, Level::Full};
// period(Idle) = 150 + 350 + 2500 = 3000 ms (heartbeat)
inline const LedPattern kIdle{kIdleSteps, 1, 2500, 1, false, Level::Dim};
// period(Homing) = 2*(150) + 2*350 + 1500 = 2500 ms
inline const LedPattern kHoming{kHomingSteps, 2, 1500, 1, false, Level::High};
inline const LedPattern kRunning{nullptr, 0, 0, 1, true, Level::Mid};
// period(Error) = 3*150 + 3*350 + 1500 = 3000 ms
inline const LedPattern kError{kErrorSteps, 3, 1500, 1, false, Level::Full};
inline const LedPattern kEndstop{kEndstopSteps, 1, 1000, 2, false, Level::Full};
inline const LedPattern kBreak{kBreakSteps, 2, 1000, 2, false, Level::Full};
inline const LedPattern kCal{kCalSteps, 2, 1000, 2, false, Level::Full};

inline const LedPattern& basePattern(LedBase base) {
  switch (base) {
    case LedBase::SelfTest: return kSelfTest;
    case LedBase::Idle: return kIdle;
    case LedBase::Homing: return kHoming;
    case LedBase::Running: return kRunning;
    case LedBase::Error: return kError;
  }
  return kIdle;  // unreachable, keeps compilers quiet
}

inline const LedPattern& overlayPattern(LedEvent event) {
  switch (event) {
    case LedEvent::EndstopEdge: return kEndstop;
    case LedEvent::Break: return kBreak;
    case LedEvent::CalApplied: return kCal;
    case LedEvent::None: break;
  }
  return kIdle;  // unreachable
}

// Evaluates one pattern as a pure function of (origin, now): phase =
// elapsed % period; walks steps, each ON for onMs then OFF for kGapMs;
// the remainder after the last step's gap is the pause.
inline LedOutput evalPattern(const LedPattern& p, uint32_t origin,
                             uint32_t now) {
  if (p.steady) return {true, p.steadyLevel};
  uint32_t groupMs = 0;
  for (uint8_t i = 0; i < p.count; ++i) groupMs += p.steps[i].onMs;
  uint32_t period = groupMs + static_cast<uint32_t>(p.count) * kGapMs + p.pauseMs;
  uint32_t phase = (now - origin) % period;
  uint32_t t = 0;
  for (uint8_t i = 0; i < p.count; ++i) {
    uint32_t onEnd = t + p.steps[i].onMs;
    if (phase >= t && phase < onEnd) return {true, p.steps[i].level};
    t = onEnd + kGapMs;
  }
  return {false, Level::Dim};  // in the pause (or a gap tail)
}

class LedPatternFSM {
 public:
  void begin(uint32_t nowMs) {
    baseOrigin_ = nowMs;
    overlayActive_ = false;
    overlayEvent_ = LedEvent::None;
  }

  LedOutput update(LedBase base, LedEvent event, uint32_t nowMs) {
    if (base == LedBase::Error) {
      // FR-008: Error owns the LED; overlays are dropped/ignored.
      if (!wasError_) {
        baseOrigin_ = nowMs;  // restart the Error phase at the transition
        wasError_ = true;
      }
      overlayActive_ = false;
    } else {
      if (wasError_) {
        baseOrigin_ = nowMs;  // leaving Error: restart the base phase
        wasError_ = false;
      }
      if (event != LedEvent::None) {
        overlayActive_ = true;
        overlayStart_ = nowMs;
        overlayEvent_ = event;
      }
    }
    if (overlayActive_) {
      const LedPattern& p = overlayPattern(overlayEvent_);
      uint32_t groupMs = 0;
      for (uint8_t i = 0; i < p.count; ++i) groupMs += p.steps[i].onMs;
      uint32_t repMs = groupMs + static_cast<uint32_t>(p.count) * kGapMs + p.pauseMs;
      uint32_t totalMs =
          repMs * p.reps - p.pauseMs;  // no trailing pause after the last rep
      if (nowMs - overlayStart_ < totalMs) return evalPattern(p, overlayStart_, nowMs);
      overlayActive_ = false;
    }
    return evalPattern(basePattern(base), baseOrigin_, nowMs);
  }

 private:
  uint32_t baseOrigin_ = 0;
  bool overlayActive_ = false;
  uint32_t overlayStart_ = 0;
  LedEvent overlayEvent_ = LedEvent::None;
  bool wasError_ = false;
};

}  // namespace led

#endif  // LED_PATTERN_H

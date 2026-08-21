// Pure motion math + command parsing, platform independent.
// Unit-tested via env:native.
#include "FirmwareCore.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

float outputRpmToStepsPerSecond(float rpm) {
  // jane: float constant (200 x 13.73f x 8), lroundf below keeps step counts
  // integral. Exact rational (1373/100) if ever needed for long runs.
  return (rpm * OUTPUT_STEPS_PER_REV) / 60.0f;
}

long outputTurnsToSteps(float turns) {
  return lroundf(turns * OUTPUT_STEPS_PER_REV);
}

long spindleMmToSteps(float mm) {
  return outputTurnsToSteps(mm / SPINDLE_PITCH_MM_PER_REV);
}

float rpmToFeedMmPerMin(float rpm) {
  return rpm * SPINDLE_PITCH_MM_PER_REV;
}

float feedMmPerMinToRpm(float mmPerMin) {
  return mmPerMin / SPINDLE_PITCH_MM_PER_REV;
}

float clampFeedMmPerMin(float mmPerMin) {
  if (mmPerMin < MIN_MOVE_MM_PER_MIN) {
    return MIN_MOVE_MM_PER_MIN;
  }
  if (mmPerMin > MAX_MOVE_MM_PER_MIN) {
    return MAX_MOVE_MM_PER_MIN;
  }
  return mmPerMin;
}

float calibrationFactorFromMass(long averageRaw, long offset, float grams) {
  return static_cast<float>(averageRaw - offset) / grams;
}

CalibrationVerdict verifyCalibrationFactor(float factor) {
  if (factor == 0.0f) {
    return CalibrationVerdict::Zero;
  }
  // NQP #46 (fix): legacy accepted negative factors; they are rejected
  // now (kept separate from Zero so the responses stay distinct).
  if (factor < 0.0f) {
    return CalibrationVerdict::Negative;
  }
  return CalibrationVerdict::Valid;
}

namespace {

constexpr char kCommandWhitespace[] = " \t\r\n";

// Trim both ends into `out` (NUL-terminated, at most outSize bytes).
void trimCopy(const char* in, char* out, size_t outSize) {
  const size_t len = strlen(in);
  size_t begin = strspn(in, kCommandWhitespace);
  size_t end = len;
  while (end > begin && strchr(kCommandWhitespace, in[end - 1]) != nullptr) {
    --end;
  }
  size_t copy = end - begin;
  // jane: tokens longer than the buffer are truncated (unreachable for
  // commands/values used by this machine).
  if (copy >= outSize) {
    copy = outSize - 1;
  }
  memcpy(out, in + begin, copy);
  out[copy] = '\0';
}

}  // namespace

Command commandFromName(const char* name) {
  char trimmed[48];
  trimCopy(name, trimmed, sizeof(trimmed));
  if (strcmp(trimmed, "en") == 0) {
    return Command::Enable;
  }
  if (strcmp(trimmed, "off") == 0) {
    return Command::Disable;
  }
  if (strcmp(trimmed, "zero") == 0) {
    return Command::Zero;
  }
  if (strcmp(trimmed, "tare") == 0) {
    return Command::Tare;
  }
  if (strcmp(trimmed, "force") == 0) {
    return Command::Force;
  }
  if (strcmp(trimmed, "stop") == 0) {
    return Command::Stop;
  }
  if (strcmp(trimmed, "pos") == 0) {
    return Command::Pos;
  }
  if (strcmp(trimmed, "cal_zero") == 0) {
    return Command::CalZero;
  }
  if (strcmp(trimmed, "jog up") == 0) {
    return Command::JogUp;
  }
  if (strcmp(trimmed, "jog down") == 0) {
    return Command::JogDown;
  }
  return Command::Unknown;
}

ParseResult parseCommandValue(const char* command, const char* prefix,
                              float* value) {
  const size_t prefixLen = strlen(prefix);
  if (strncmp(command, prefix, prefixLen) != 0) {
    return ParseResult::NotACommand;
  }
  // NQP #42 (fix): the prefix must end at a word boundary (whitespace or
  // end-of-string), so "rpmx 5" is not dispatched as "rpm".
  const char boundary = command[prefixLen];
  if (boundary != '\0' && strchr(kCommandWhitespace, boundary) == nullptr) {
    return ParseResult::NotACommand;
  }

  char number[48];
  trimCopy(command + prefixLen, number, sizeof(number));
  if (number[0] == '\0') {
    return ParseResult::NotACommand;
  }

  // Legacy German decimal comma (replace(',', '.')).
  for (char* p = number; *p != '\0'; ++p) {
    if (*p == ',') {
      *p = '.';
    }
  }

  // NQP #43 (fix): the ENTIRE token must be consumed by strtof. Legacy
  // truncated trailing junk ("5abc" == 5) and mapped non-numeric input to
  // 0.0, which callers misreported as a "> 0" range error.
  char* end = nullptr;
  const float parsed = strtof(number, &end);
  if (end == number || *end != '\0') {
    return ParseResult::InvalidValue;
  }
  // strtof also accepts "inf"/"nan" fully; they must not reach the step
  // math (lroundf(inf) is UB, an infinite setpoint is machine-unsafe).
  if (!std::isfinite(parsed)) {
    return ParseResult::InvalidValue;
  }
  *value = parsed;
  return ParseResult::Ok;
}

// ---------------------------------------------------------------------------
// Device state machine (M1 FR-001). Transition table pinned in
// test/test_device_model.cpp: invalid events are no-ops, a FAIL check or a
// runtime power fault moves the device to Error, and Reset is the only way
// out of Error (FR-009: recovery by reset/reboot).
// ---------------------------------------------------------------------------

void DeviceModel::dispatch(DeviceEvent event, uint16_t code) {
  switch (event) {
    case DeviceEvent::Reset:
      state_ = DeviceState::Booting;
      faultCode_ = 0;
      break;
    case DeviceEvent::SelfTestStart:
      if (state_ == DeviceState::Booting) {
        state_ = DeviceState::SelfTest;
      }
      break;
    case DeviceEvent::CheckResult:
      if (state_ == DeviceState::SelfTest && code != 0) {
        state_ = DeviceState::Error;
        faultCode_ = code;
      }
      break;
    case DeviceEvent::SelfTestDone:
      if (state_ == DeviceState::SelfTest) {
        state_ = DeviceState::Idle;
      }
      break;
    case DeviceEvent::PowerLow:
      if (state_ == DeviceState::Idle && code != 0) {
        state_ = DeviceState::Error;
        faultCode_ = code;
      }
      break;
  }
}

// ---------------------------------------------------------------------------
// Health evaluation (M1 FR-005 / FR-006). Pinned in
// test/test_health_eval.cpp.
// ---------------------------------------------------------------------------

PowerStatus powerStatusForVoltage(float volts) {
  if (volts < POWER_FAIL_VOLTS) {
    return PowerStatus::Fail;
  }
  if (volts < POWER_WARN_VOLTS) {
    return PowerStatus::Warn;
  }
  return PowerStatus::OK;
}

bool powerNotWired(float railVolts) {
  return railVolts < POWER_NOT_WIRED_VOLTS;
}

PowerStatus PowerMonitor::update(float volts) {
  if (volts < POWER_FAIL_VOLTS) {
    // Out of band: extend the debounce streak.
    if (failStreak_ < POWER_FAIL_DEBOUNCE) {
      failStreak_++;
    }
    if (failStreak_ >= POWER_FAIL_DEBOUNCE) {
      state_ = PowerStatus::Fail;
    }
  } else if (volts >= POWER_FAIL_VOLTS + POWER_FAIL_HYSTERESIS) {
    // Recovered past the hysteresis band: clear the streak, demote state.
    failStreak_ = 0;
    if (state_ == PowerStatus::Fail) {
      state_ = (volts < POWER_WARN_VOLTS + POWER_FAIL_HYSTERESIS)
                   ? PowerStatus::Warn
                   : PowerStatus::OK;
    } else {
      state_ = powerStatusForVoltage(volts);
    }
  }
  // else: inside the hysteresis band [FAIL, FAIL+HYST) - neither extend
  // nor clear the streak, state unchanged (FR-005 hysteresis).
  return state_;
}

void SampleWindow::push(float sample) {
  samples_[next_] = sample;
  next_ = (next_ + 1) % CAPACITY;
  if (size_ < CAPACITY) {
    size_++;
  }
}

float SampleWindow::spread() const {
  if (size_ < 2) {
    return 0.0f;
  }
  float lo = samples_[0];
  float hi = samples_[0];
  for (size_t i = 1; i < size_; i++) {
    if (samples_[i] < lo) {
      lo = samples_[i];
    }
    if (samples_[i] > hi) {
      hi = samples_[i];
    }
  }
  return hi - lo;
}

const char* deviceStateName(DeviceState state) {
  switch (state) {
    case DeviceState::Booting:
      return "Booting";
    case DeviceState::SelfTest:
      return "SelfTest";
    case DeviceState::Idle:
      return "Idle";
    case DeviceState::Error:
      return "Error";
    default:
      return "Unknown";
  }
  return "?";
}

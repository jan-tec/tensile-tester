// Selftest check vocabulary (M1 FR-002/FR-003, T003).
//
// Pure declarations - no Arduino.h, includable from FirmwareCore, the
// target modules and the host tests. Machine codes are STABLE: they are
// the machine-readable identity of a fault in serial logs (FR-007) and
// /api/status (FR-008). Never renumber or reuse a code; add new ones at
// the end of their 0xNNxx block. Human messages are presentation (M2);
// the string names below are English ops text, not localized UI text.
#pragma once

#include <cstddef>
#include <cstdint>

// Stable machine codes (FR-003).
enum class ErrorCode : uint16_t {
  None = 0x0000,
  // NVS (0x01xx)
  NvsOpenFailed = 0x0101,
  NvsCalibrationCorrupt = 0x0102,
  // Power (0x02xx)
  PowerLow = 0x0201,  // US2: "Versorgungsspannung zu niedrig"
  // HX711 (0x03xx)
  Hx711NoVariation = 0x0301,  // WARN diagnostic (FR-006), not a failure
  // Motor (0x04xx)
  MotorNoFeedback = 0x0401,  // info note (plan D5): no sense wiring exists
  // Endstops (0x05xx, M3): names must stay stable - machine-readable log
  // grammar (FR-007). Never renumber.
  EndstopSeekTimeout = 0x0501,   // homing seek never found the switch
  EndstopInstantTrigger = 0x0502, // switch already closed at seek start
  EndstopStuckSwitch = 0x0503,    // debounced chatter, no clean edge
  EndstopPressedOrBroken = 0x0504, // WARN only (boot check): pressed or broken
  // Serial API (0x06xx, M6): command-envelope failure codes (plan D8,
  // FR-004). Machines parse error.code only; message is display text.
  UnknownCommand = 0x0601,  // not in the accepted command set
  InvalidValue = 0x0602,    // unparsable or out-of-range argument
  StateConflict = 0x0603,   // busy/error-state/dump exclusivity
  NotHomed = 0x0604,        // mm-accurate motion without a datum
  NotCalibrated = 0x0605,   // force reading without a valid factor
  EmptyCommand = 0x0606,    // blank line dispatch
};

// Check outcome (FR-003). Absence is not a fault (D3): NOT_CONNECTED and
// NOT_WIRED are warning/info, never FAIL.
enum class CheckStatus { Ok, Warn, Fail, NotConnected, NotWired };

// Check set (FR-002; M3 T013: +ENDSTOP1/ENDSTOP2, appended - never
// renumber, the ids index /api/status selftest.checks rows).
enum class CheckId { Board, Nvs, Power, Hx711, Motor, Endstop1, Endstop2 };

// Result of one selftest check (FR-003 result shape).
struct CheckResult {
  CheckId id;
  CheckStatus status;
  ErrorCode errorCode;  // ErrorCode::None unless fault/diagnostic
  float measurement;    // e.g. rail volts, free heap bytes, raw counts
  uint32_t elapsedMs;
};

// Stable string names (log grammar FR-007 + /api/status FR-008).
inline const char* checkIdName(CheckId id) {
  switch (id) {
    case CheckId::Board:
      return "BOARD";
    case CheckId::Nvs:
      return "NVS";
    case CheckId::Power:
      return "POWER";
    case CheckId::Hx711:
      return "HX711";
    case CheckId::Motor:
      return "MOTOR";
    case CheckId::Endstop1:
      return "ENDSTOP1";
    case CheckId::Endstop2:
      return "ENDSTOP2";
  }
  return "?";
}

inline const char* checkStatusName(CheckStatus status) {
  switch (status) {
    case CheckStatus::Ok:
      return "OK";
    case CheckStatus::Warn:
      return "WARN";
    case CheckStatus::Fail:
      return "FAIL";
    case CheckStatus::NotConnected:
      return "NOT_CONNECTED";
    case CheckStatus::NotWired:
      return "NOT_WIRED";
  }
  return "?";
}

inline const char* errorCodeName(ErrorCode code) {
  switch (code) {
    case ErrorCode::NvsOpenFailed:
      return "NVS_OPEN_FAILED";
    case ErrorCode::NvsCalibrationCorrupt:
      return "NVS_CALIBRATION_CORRUPT";
    case ErrorCode::PowerLow:
      return "POWER_LOW";
    case ErrorCode::Hx711NoVariation:
      return "HX711_NO_VARIATION";
    case ErrorCode::MotorNoFeedback:
      return "MOTOR_NO_FEEDBACK";
    case ErrorCode::EndstopSeekTimeout:
      return "ENDSTOP_SEEK_TIMEOUT";
    case ErrorCode::EndstopInstantTrigger:
      return "ENDSTOP_INSTANT_TRIGGER";
    case ErrorCode::EndstopStuckSwitch:
      return "ENDSTOP_STUCK_SWITCH";
    case ErrorCode::EndstopPressedOrBroken:
      return "ENDSTOP_PRESSED_OR_BROKEN";
    case ErrorCode::UnknownCommand:
      return "UNKNOWN_COMMAND";
    case ErrorCode::InvalidValue:
      return "INVALID_VALUE";
    case ErrorCode::StateConflict:
      return "STATE_CONFLICT";
    case ErrorCode::NotHomed:
      return "NOT_HOMED";
    case ErrorCode::NotCalibrated:
      return "NOT_CALIBRATED";
    case ErrorCode::EmptyCommand:
      return "EMPTY_COMMAND";
    default:
      return "";
  }
}

// FR-007 log grammar: codes are always 4 padded upper-case hex digits with
// an 0x prefix (`code=0x0301`, never `0x301` - issue #97). Writes 6 chars
// + NUL into out (exactly 7 bytes) and returns out.
inline const char* errorCodeHex(uint16_t code, char (&out)[7]) {
  static const char kDigits[] = "0123456789ABCDEF";
  out[0] = '0';
  out[1] = 'x';
  for (unsigned i = 0; i < 4; i++) {
    out[2 + i] = kDigits[(code >> (12 - 4 * i)) & 0xF];
  }
  out[6] = '\0';
  return out;
}

// FR-008: overall rating of the /api/status selftest object. Any FAIL
// dominates, otherwise any WARN. NOT_WIRED / NOT_CONNECTED are info only
// (D3: absence is not a fault) and never lower the rating.
inline CheckStatus overallStatus(const CheckStatus* statuses, size_t count) {
  bool anyWarn = false;
  for (size_t i = 0; i < count; i++) {
    if (statuses[i] == CheckStatus::Fail) {
      return CheckStatus::Fail;
    }
    if (statuses[i] == CheckStatus::Warn) {
      anyWarn = true;
    }
  }
  return anyWarn ? CheckStatus::Warn : CheckStatus::Ok;
}

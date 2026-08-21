// Selftest check vocabulary (M1 FR-002/FR-003, T003).
//
// Pure declarations - no Arduino.h, includable from FirmwareCore, the
// target modules and the host tests. Machine codes are STABLE: they are
// the machine-readable identity of a fault in serial logs (FR-007) and
// /api/status (FR-008). Never renumber or reuse a code; add new ones at
// the end of their 0xNNxx block. Human messages are presentation (M2);
// the string names below are English ops text, not localized UI text.
#pragma once

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
};

// Check outcome (FR-003). Absence is not a fault (D3): NOT_CONNECTED and
// NOT_WIRED are warning/info, never FAIL.
enum class CheckStatus { Ok, Warn, Fail, NotConnected, NotWired };

// Check set (FR-002).
enum class CheckId { Board, Nvs, Power, Hx711, Motor };

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
    default:
      return "";
  }
}

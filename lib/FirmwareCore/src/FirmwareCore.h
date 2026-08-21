// FirmwareCore - pure C++ motion math, no Arduino/FreeRTOS dependencies.
// Compiled for both the ESP32 (linked into main.cpp) and the host
// (env:native unit tests). Keep this file free of Arduino.h, PROGMEM, F(),
// String, portMUX and hardware pins; hardware access stays in src/main.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

// Gear train: 200 full steps/rev motor x 13.73:1 gearbox x 8 microsteps
// = 21968 steps per spindle revolution. Keep in sync with the hardware.
constexpr long MOTOR_FULL_STEPS_PER_REV = 200;
constexpr float GEAR_RATIO = 13.73f;
constexpr uint8_t MICROSTEPS = 8;

constexpr float OUTPUT_STEPS_PER_REV =
    MOTOR_FULL_STEPS_PER_REV * GEAR_RATIO * MICROSTEPS;
constexpr float SPINDLE_PITCH_MM_PER_REV = 2.0f;

// Allowed feed range (mm/min), enforced by clampFeedMmPerMin.
constexpr float MIN_MOVE_MM_PER_MIN = 0.5f;
constexpr float MAX_MOVE_MM_PER_MIN = 60.0f;

// rpm/acceleration (per second) at the spindle -> steps/s (steps/s^2 uses
// the same helper: per-rpm scaling is identical).
float outputRpmToStepsPerSecond(float rpm);

// Spindle revolutions (or spindle mm) <-> driver steps, rounded.
long outputTurnsToSteps(float turns);
long spindleMmToSteps(float mm);

// Spindle rpm <-> feed speed in mm/min (2 mm per spindle revolution).
float rpmToFeedMmPerMin(float rpm);
float feedMmPerMinToRpm(float mmPerMin);

// Clamp a feed speed into [MIN_MOVE_MM_PER_MIN, MAX_MOVE_MM_PER_MIN].
float clampFeedMmPerMin(float mmPerMin);

// ---------------------------------------------------------------------------
// Calibration compute (pure). Extracted from LoadCell::calibrateWithMass
// for host-side unit tests (NQP #46).
// ---------------------------------------------------------------------------

// Calibration factor for a known mass: (averageRaw - offset) / grams.
// Caller guarantees grams > 0 (InvalidMass check in LoadCell).
// Named *FromMass* to avoid hiding LoadCell::calibrationFactor().
float calibrationFactorFromMass(long averageRaw, long offset, float grams);

// Verdict on a computed calibration factor. Zero keeps the legacy
// rejection; negative is rejected per NQP #46 (offset drift or a
// disturbed part between cal_zero and cal_apply would otherwise be
// persisted and invert every force reading).
enum class CalibrationVerdict {
  Valid,
  Zero,
  Negative,
};

CalibrationVerdict verifyCalibrationFactor(float factor);

// ---------------------------------------------------------------------------
// Command parsing (pure). Mirrors the legacy String-based dispatch in
// src/main.cpp, quirks included (case-sensitive names, prefix matching
// without word boundary, non-numeric value -> 0.0). See
// test/test_command_parser.cpp for the pinned behavior.
// ---------------------------------------------------------------------------

// Value-less command names (exact match after trimming, case-sensitive).
enum class Command {
  Unknown,
  Enable,   // "en"
  Disable,  // "off"
  Zero,     // "zero"
  Tare,     // "tare"
  Force,    // "force"
  Stop,     // "stop"
  Pos,      // "pos"
  CalZero,  // "cal_zero"
  JogUp,    // "jog up"
  JogDown,  // "jog down"
};

Command commandFromName(const char* name);

// FR-009: true if the command would drive the motion system (en/off/zero/
// stop/jog + the rpm/acc/feed/move/turn value prefixes). Used to reject
// motion commands in the Error state; readout (pos/force) and
// loadcell/calibration commands stay usable.
bool isMotionCommand(const char* command);

// Result of parsing a value command. NQP #43 (fix): the legacy two-state
// parser turned garbage into 0.0 (misleading "> 0" error) and truncated
// trailing junk ("5abc" == 5); both are now a distinct InvalidValue.
enum class ParseResult {
  NotACommand,  // prefix mismatch, no number, or not at a word boundary
  InvalidValue, // token present but not a fully-consumed finite number
  Ok,           // *value set
};

// Parse the number after `prefix` in `command` (caller must pass the already
// trimmed command, as the legacy dispatch does). The ENTIRE trimmed token
// must be consumed by strtof (German decimal comma accepted); empty or
// non-matching tokens yield NotACommand (legacy unknown-command fallthrough).
ParseResult parseCommandValue(const char* command, const char* prefix,
                              float* value);

// ---------------------------------------------------------------------------
// Device state machine (pure). M1 FR-001: Booting -> SelfTest -> Idle |
// Error. No hardware here; the caller (Diagnostics) disables the motor
// driver whenever faultCode() != 0. See test/test_device_model.cpp for the
// pinned transition table (TC-001, 100% coverage).
// ---------------------------------------------------------------------------

enum class DeviceState { Booting, SelfTest, Idle, Error };

// Events the host code feeds the model. CheckResult/PowerLow carry the
// stable machine code (0x01xx NVS, 0x02xx power, 0x03xx HX711, 0x04xx
// motor); 0 means OK/WARN (no fault code).
enum class DeviceEvent { SelfTestStart, CheckResult, SelfTestDone, PowerLow,
                         Reset };

// Stable English state names for the FR-007 STATE log lines.
const char* deviceStateName(DeviceState state);

// ---------------------------------------------------------------------------
// Health evaluation (pure). M1 FR-005 / FR-006, TC-002: power rail
// thresholds, FAIL-boundary hysteresis, >= 5-sample FAIL debounce and the
// HX711 500 ms variance window. Pinned in test/test_health_eval.cpp.
// ---------------------------------------------------------------------------

// 5 V rail thresholds (10 k/10 k divider on GPIO34).
constexpr float POWER_FAIL_VOLTS = 4.50f;
constexpr float POWER_WARN_VOLTS = 4.75f;
// Hysteresis at the FAIL boundary: samples in [FAIL, FAIL+HYST) neither
// extend nor clear the debounce streak; recovery needs FAIL + HYST.
constexpr float POWER_FAIL_HYSTERESIS = 0.05f;
// Consecutive out-of-band samples required before a runtime FAIL (FR-005).
constexpr int POWER_FAIL_DEBOUNCE = 5;

enum class PowerStatus { OK, Warn, Fail };

// Single-shot rail evaluation (boot POWER check): v < 4.50 -> Fail,
// v < 4.75 -> Warn, else OK.
PowerStatus powerStatusForVoltage(float volts);

// FR-005 (D4): NOT_WIRED detection for the 5 V rail sense input. The
// sense pin is INPUT_PULLDOWN, so an absent divider reads ~0 V. A live
// 5 V rail can never sit below this rail-equivalent threshold, so a
// reading under it means absent (NOT_WIRED, info per D3), not low.
constexpr float POWER_NOT_WIRED_VOLTS = 1.0f;
bool powerNotWired(float railVolts);

// Runtime rail monitor (1 Hz, FR-005): FAIL needs POWER_FAIL_DEBOUNCE
// consecutive samples below POWER_FAIL_VOLTS; the hysteresis band around
// the fail boundary neither extends nor clears the streak; recovery above
// POWER_FAIL_VOLTS + POWER_FAIL_HYSTERESIS clears the streak and demotes
// Fail to Warn/OK.
class PowerMonitor {
 public:
  // Ingest one sample, returns the current state.
  PowerStatus update(float volts);
  PowerStatus state() const { return state_; }

 private:
  PowerStatus state_ = PowerStatus::OK;
  int failStreak_ = 0;
};

// HX711 signal window: 500 ms at the 10 Hz sampling rate (FR-006).
// Spread (max - min) below HX711_NO_VARIATION_SPREAD means a dead/shorted
// cell (WARN NO_VARIATION diagnostic); a live cell exceeds it.
constexpr float HX711_NO_VARIATION_SPREAD = 1.0f;

class SampleWindow {
 public:
  static constexpr size_t CAPACITY = 50;

  void push(float sample);
  size_t size() const { return size_; }
  bool full() const { return size_ == CAPACITY; }
  // max - min over the stored samples; 0 when fewer than two samples.
  float spread() const;

 private:
  float samples_[CAPACITY] = {};
  size_t size_ = 0;
  size_t next_ = 0;
};

class DeviceModel {
 public:
  DeviceState state() const { return state_; }
  // 0 = no fault; otherwise the code that caused the Error transition
  // (first fault wins; cleared by Reset).
  uint16_t faultCode() const { return faultCode_; }

  // Invalid events (e.g. SelfTestDone in Booting) are no-ops.
  void dispatch(DeviceEvent event, uint16_t code = 0);

 private:
  DeviceState state_ = DeviceState::Booting;
  uint16_t faultCode_ = 0;
};

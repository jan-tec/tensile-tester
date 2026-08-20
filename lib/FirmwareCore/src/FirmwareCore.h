// FirmwareCore - pure C++ motion math, no Arduino/FreeRTOS dependencies.
// Compiled for both the ESP32 (linked into main.cpp) and the host
// (env:native unit tests). Keep this file free of Arduino.h, PROGMEM, F(),
// String, portMUX and hardware pins; hardware access stays in src/main.cpp.
#pragma once

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

// Parse the number after `prefix` in `command` (caller must pass the already
// trimmed command, as the legacy dispatch does). Returns false when the
// prefix does not match or no number remains; non-numeric input yields 0.0
// with a true return (the caller rejects it via its > 0 check).
bool parseCommandValue(const char* command, const char* prefix, float* value);

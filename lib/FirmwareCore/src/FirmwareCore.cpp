// Pure motion math, platform independent. Unit-tested via env:native.
#include "FirmwareCore.h"

#include <cmath>

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

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

bool parseCommandValue(const char* command, const char* prefix, float* value) {
  if (strncmp(command, prefix, strlen(prefix)) != 0) {
    return false;
  }

  char number[48];
  trimCopy(command + strlen(prefix), number, sizeof(number));
  if (number[0] == '\0') {
    return false;
  }

  // Legacy String::replace(',', '.') then toFloat(): non-numeric input
  // yields 0.0 and parsing stops at the first non-numeric character.
  for (char* p = number; *p != '\0'; ++p) {
    if (*p == ',') {
      *p = '.';
    }
  }
  *value = strtof(number, nullptr);
  return true;
}

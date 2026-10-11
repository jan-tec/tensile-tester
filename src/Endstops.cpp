#include "Endstops.h"

void Endstops::begin() {
  pinMode(kBottomPin, INPUT_PULLUP);
  pinMode(kTopPin, INPUT_PULLUP);
  bootBottom_ = rawBottom();
  bootTop_ = rawTop();
  bottomStable_ = bootBottom_;
  topStable_ = bootTop_;
}

void Endstops::poll() {
  unsigned long now = millis();
  if (now - lastSampleMs_ < kSampleIntervalMs) return;
  lastSampleMs_ = now;

  // D4 debounce: count consecutive raw samples that differ from the
  // accepted level; after kDebounceSamples equal ones, accept and raise
  // an edge on a LOW -> HIGH transition.
  const bool rb = rawBottom();
  if (rb != bottomStable_) {
    if (rb == bottomCandidate_) {
      if (++bottomRun_ >= kDebounceSamples) {
        bottomStable_ = rb;
        if (rb) bottomEdge_ = true;  // rising edge only
        bottomRun_ = 0;
      }
    } else {
      bottomCandidate_ = rb;
      bottomRun_ = 1;
    }
  } else {
    bottomRun_ = 0;
  }

  const bool rt = rawTop();
  if (rt != topStable_) {
    if (rt == topCandidate_) {
      if (++topRun_ >= kDebounceSamples) {
        topStable_ = rt;
        if (rt) topEdge_ = true;
        topRun_ = 0;
      }
    } else {
      topCandidate_ = rt;
      topRun_ = 1;
    }
  } else {
    topRun_ = 0;
  }
}

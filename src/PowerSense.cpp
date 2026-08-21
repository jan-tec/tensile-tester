// 5 V rail power sense implementation (M1 FR-005).
#include "PowerSense.h"

void PowerSense::begin() {
  pinMode(kPin, INPUT_PULLDOWN);
  sample();
}

float PowerSense::readRailVolts() {
  const int mv = analogReadMilliVolts(kPin);
  return (static_cast<float>(mv) / 1000.0f) * kRailMultiplier;
}

void PowerSense::poll() {
  const uint32_t now = millis();
  if (now - lastPollMs_ < kPollIntervalMs) {
    return;
  }
  lastPollMs_ = now;
  sample();
}

void PowerSense::sample() {
  lastVolts_ = readRailVolts();
  // D3/D4: an absent divider reads ~0 V (INPUT_PULLDOWN). Feeding that
  // into the monitor would debounce a phantom FAIL after ~5 s and drop
  // the model into Error on every boot. The monitor tracks a present
  // rail only; absence is reported as NOT_WIRED info by the boot check.
  if (!powerNotWired(lastVolts_)) {
    monitor_.update(lastVolts_);
  }
}

// 5 V rail power sense implementation (M1 FR-005).
#include "PowerSense.h"

void PowerSense::begin() {
  // GPIO34-39 are input-only pads without an internal pull-up/pull-down:
  // INPUT_PULLDOWN on this pin makes the IDF driver log
  // "gpio_pulldown_en(123): GPIO number error (input-only pad has no internal
  // PD)" on every boot and applies nothing. INPUT is the only legal mode; the
  // defined low level for an absent divider has to come from the external
  // R2 of the divider (docs/hardware-power-check.md, #27).
  pinMode(kPin, INPUT);
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
  // D3/D4: an absent divider reads ~0 V only because a floating input-only
  // pad leaks low; GPIO34 has no internal pull-down to guarantee it (see
  // begin()). Feeding that
  // into the monitor would debounce a phantom FAIL after ~5 s and drop
  // the model into Error on every boot. The monitor tracks a present
  // rail only; absence is reported as NOT_WIRED info by the boot check.
  if (!powerNotWired(lastVolts_)) {
    monitor_.update(lastVolts_);
  }
}

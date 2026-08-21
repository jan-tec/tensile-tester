// 5 V rail power sense (M1 FR-005, D4): 10 k/10 k divider,
// 5 V rail -> GPIO34 (INPUT_PULLDOWN). The ADC sees half the rail;
// rail-equivalent volts = 2 x ADC volts. Runtime monitoring at 1 Hz;
// hysteresis/debounce lives in FirmwareCore::PowerMonitor (pinned in
// test/test_health_eval.cpp). NOT_WIRED until the divider is built
// (hardware task T008).
#pragma once

#include <Arduino.h>

#include <FirmwareCore.h>

class PowerSense {
 public:
  static constexpr uint8_t kPin = 34;
  static constexpr float kRailMultiplier = 2.0f;  // 10 k / 10 k divider
  static constexpr unsigned long kPollIntervalMs = 1000;  // FR-005: 1 Hz

  void begin();

  // One-shot read: 5 V rail-equivalent volts (boot POWER check).
  float readRailVolts();

  // 1 Hz runtime tick feeding the PowerMonitor; call from the loop task.
  void poll();

  PowerStatus state() const { return monitor_.state(); }
  float lastVolts() const { return lastVolts_; }

 private:
  // One ADC read into lastVolts_; feeds the monitor only for a present
  // rail (see the powerNotWired guard - D3: absence must not drive the
  // runtime monitor to FAIL).
  void sample();

  PowerMonitor monitor_;
  float lastVolts_ = 0.0f;
  uint32_t lastPollMs_ = 0;
};

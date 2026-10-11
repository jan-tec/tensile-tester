// LED status channel (M8 FR-001/FR-009): the DevKit onboard LED on
// GPIO2 driven by LEDC PWM (ch 0, 1 kHz, 8-bit). Brightness uses the
// gamma-corrected duty table from FirmwareCore (led::duty255); pattern
// selection stays in the pure LedPatternFSM. tick() is called from
// App::loop() after endstops_.poll(); the whole call is two integer
// compares plus a cached ledcWrite on change (SC-004 budget < 1 ms).
//
// FR-009 strapping note: begin() is called only after the boot selftest
// has run, never before the bootloader decisions; GPIO2 idles high-Z
// through boot.
#pragma once

#include <Arduino.h>

#include <LedPattern.h>

class LedStatus {
 public:
  static constexpr uint8_t kLedPin = 2;
  static constexpr uint8_t kLedcChannel = 0;
  static constexpr uint32_t kFreqHz = 1000;
  static constexpr uint8_t kResolutionBits = 8;

  // Configure LEDC and start the pattern engine (post-selftest, FR-009).
  void begin(unsigned long nowMs);

  // One non-blocking step: evaluate the FSM, write duty on change only.
  void tick(led::LedBase base, led::LedEvent event, unsigned long nowMs);

 private:
  led::LedPatternFSM fsm_;
  bool lastOn_ = false;
  uint8_t lastDuty_ = 255;  // != any real first value -> forces one write
};

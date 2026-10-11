// Endstop inputs (M3 FR-001/FR-007, M3 plan T008/D4): Endstop 1 (bottom,
// zero datum) on GPIO 25, Endstop 2 (top travel limit) on GPIO 26, both
// INPUT_PULLUP. Wiring (operator-confirmed, M3 spec A2): COM+NC
// microswitch to GND -> idle LOW, pressed = contact opens = HIGH
// (rising edge = trigger; a broken wire reads like pressed, guarded by
// the homing instant-trigger rule and the M3-T013 boot WARN).
//
// Debounce (M3 plan D4): raw sample every kSampleIntervalMs, a level is
// accepted after kDebounceSamples consecutive equal samples (~15 ms
// settle; KW4-3Z-3 snap-action bounce is well under that). Rising-edge
// flags are cleared on read (read-and-clear).
//
// Consumers: M8-T002 LED overlay (poll from App::loop, edges -> LED);
// M3-T011 TestRunner (levels during homing); bootState() for the
// M3-T013 selftest rows (HIGH at boot = WARN 0x0504, never FAIL).
#pragma once

#include <Arduino.h>

class Endstops {
 public:
  static constexpr uint8_t kBottomPin = 25;
  static constexpr uint8_t kTopPin = 26;
  static constexpr unsigned long kSampleIntervalMs = 5;
  static constexpr uint8_t kDebounceSamples = 3;

  void begin();  // pinMode INPUT_PULLUP + boot snapshot of the raw levels
  void poll();   // call from the loop task; samples at the 5 ms cadence

  // Debounced levels; HIGH = pressed (or broken wire).
  bool bottom() const { return bottomStable_; }
  bool top() const { return topStable_; }

  // Rising edge (LOW -> HIGH) since the last read; clears on read.
  bool bottomEdge() {
    bool e = bottomEdge_;
    bottomEdge_ = false;
    return e;
  }
  bool topEdge() {
    bool e = topEdge_;
    topEdge_ = false;
    return e;
  }

  // Raw levels latched at begin() (selftest input, M3-T013).
  bool bootBottom() const { return bootBottom_; }
  bool bootTop() const { return bootTop_; }

 private:
  bool rawBottom() const { return digitalRead(kBottomPin) == HIGH; }
  bool rawTop() const { return digitalRead(kTopPin) == HIGH; }

  bool bottomStable_ = false;
  bool topStable_ = false;
  bool bottomEdge_ = false;
  bool topEdge_ = false;
  bool bootBottom_ = false;
  bool bootTop_ = false;
  uint8_t bottomRun_ = 0;  // consecutive raw samples differing from stable
  uint8_t topRun_ = 0;
  bool bottomCandidate_ = false;
  bool topCandidate_ = false;
  unsigned long lastSampleMs_ = 0;
};

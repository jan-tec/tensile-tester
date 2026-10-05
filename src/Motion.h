// Stepper motion module (M0-T003; implements IMotion since M0-T010).
//
// Owns the AccelStepper (DRIVER mode), the dedicated FreeRTOS motor task on
// core 0, and the portMUX that guards every stepper access. Two physical
// motors are driven synchronously from the same STEP/DIR signals; each
// driver has its own active-low EN line (EN1 GPIO5, EN2 GPIO23), always
// switched together.
//
// Gear/feed conversion (steps per mm, rpm to steps/s) stays in
// lib/FirmwareCore; command handlers compute step targets with it and hand
// them to startMoveToSteps().
#pragma once

#include <AccelStepper.h>
#include <Arduino.h>

#include "IMotion.h"

class Motion : public IMotion {
 public:
  // Pins, AccelStepper config, initial speed, motor task (core 0).
  void begin() override;

  void enable() override;
  void disable() override;  // stops motion, turns outputs off

  void stop() override;  // stops motion, driver stays enabled

  void zero() override;  // sets the axis position to 0 steps

  void setOutputRpm(float rpm) override;
  void setAccelRpmPerS(float accelRpmPerS) override;
  float outputRpm() override;
  float accelRpmPerS() override;

  // Moves to an absolute step target; enables the driver first (legacy
  // startMoveTo behavior).
  void startMoveToSteps(long targetSteps) override;

  bool isEnabled() override;
  bool isRunning() override;
  long steps() override;
  float positionMm() override;

 private:
  void applySettings();
  static void motorTaskEntry(void* parameter);
  void motorTaskLoop();

  // Runtime config (legacy main.cpp values). begin() derives the initial
  // rpm from kDefaultFeedMmPerMin, so outputRpm_ starts at 5.0 and becomes
  // 6.0 in begin().
  static constexpr float kDefaultOutputRpm = 5.0f;
  static constexpr float kDefaultAccelRpmPerS = 20.0f;
  static constexpr float kDefaultFeedMmPerMin = 12.0f;
  static constexpr uint16_t kMotorTaskDelayUs = 200;
  static constexpr uint8_t kMotorTaskYieldInterval = 20;

  static constexpr uint8_t kDirPin = 19;
  static constexpr uint8_t kStepPin = 18;
  static constexpr uint8_t kEnPin = 5;    // EN1, motor 1 (active low)
  static constexpr uint8_t kEn2Pin = 23;  // EN2, motor 2 (active low)

  AccelStepper stepper_ =
      AccelStepper(AccelStepper::DRIVER, kStepPin, kDirPin);
  portMUX_TYPE mux_;
  float outputRpm_ = kDefaultOutputRpm;
  float accelRpmPerS_ = kDefaultAccelRpmPerS;
  bool driverEnabled_ = false;
  TaskHandle_t taskHandle_ = nullptr;
};

// Stepper motion module implementation (M0-T003).
//
// All stepper access is wrapped in the motionMux critical section; the
// motor task never blocks inside it. Behavior is carried over unchanged
// from the legacy src/main.cpp (M0 FR-004).
#include "Motion.h"

#include <FirmwareCore.h>

void Motion::begin() {
  mux_ = portMUX_INITIALIZER_UNLOCKED;

  pinMode(kEnPin, OUTPUT);
  digitalWrite(kEnPin, HIGH);
  pinMode(kEn2Pin, OUTPUT);
  digitalWrite(kEn2Pin, HIGH);

  stepper_.setEnablePin(kEnPin);
  stepper_.setPinsInverted(false, false, true);
  stepper_.disableOutputs();

  outputRpm_ = feedMmPerMinToRpm(kDefaultFeedMmPerMin);
  applySettings();

  xTaskCreatePinnedToCore(&Motion::motorTaskEntry, "motorTask", 4096, this, 2,
                          &taskHandle_, 0);
}

void Motion::enable() {
  portENTER_CRITICAL(&mux_);
  digitalWrite(kEnPin, LOW);
  digitalWrite(kEn2Pin, LOW);
  stepper_.enableOutputs();
  driverEnabled_ = true;
  portEXIT_CRITICAL(&mux_);
}

void Motion::disable() {
  portENTER_CRITICAL(&mux_);
  stepper_.stop();
  stepper_.disableOutputs();
  digitalWrite(kEnPin, HIGH);
  digitalWrite(kEn2Pin, HIGH);
  driverEnabled_ = false;
  portEXIT_CRITICAL(&mux_);
}

void Motion::stop() {
  portENTER_CRITICAL(&mux_);
  stepper_.stop();
  portEXIT_CRITICAL(&mux_);
}

void Motion::zero() {
  portENTER_CRITICAL(&mux_);
  stepper_.setCurrentPosition(0);
  portEXIT_CRITICAL(&mux_);
}

void Motion::setOutputRpm(float rpm) {
  outputRpm_ = rpm;
  applySettings();
}

void Motion::setAccelRpmPerS(float accelRpmPerS) {
  accelRpmPerS_ = accelRpmPerS;
  applySettings();
}

float Motion::outputRpm() {
  return outputRpm_;
}

float Motion::accelRpmPerS() {
  return accelRpmPerS_;
}

void Motion::startMoveToSteps(long targetSteps) {
  enable();
  portENTER_CRITICAL(&mux_);
  stepper_.moveTo(targetSteps);
  portEXIT_CRITICAL(&mux_);
}

bool Motion::isEnabled() {
  return driverEnabled_;
}

bool Motion::isRunning() {
  portENTER_CRITICAL(&mux_);
  const bool running = stepper_.isRunning();
  portEXIT_CRITICAL(&mux_);
  return running;
}

long Motion::steps() {
  portENTER_CRITICAL(&mux_);
  const long position = stepper_.currentPosition();
  portEXIT_CRITICAL(&mux_);
  return position;
}

float Motion::positionMm() {
  const float turns = static_cast<float>(steps()) / OUTPUT_STEPS_PER_REV;
  return turns * SPINDLE_PITCH_MM_PER_REV;
}

void Motion::applySettings() {
  portENTER_CRITICAL(&mux_);
  stepper_.setMaxSpeed(outputRpmToStepsPerSecond(outputRpm_));
  stepper_.setAcceleration(outputRpmToStepsPerSecond(accelRpmPerS_));
  portEXIT_CRITICAL(&mux_);
}

void Motion::motorTaskEntry(void* parameter) {
  static_cast<Motion*>(parameter)->motorTaskLoop();
}

void Motion::motorTaskLoop() {
  uint8_t loopCounter = 0;

  for (;;) {
    portENTER_CRITICAL(&mux_);
    if (driverEnabled_) {
      stepper_.run();
    }
    portEXIT_CRITICAL(&mux_);

    delayMicroseconds(kMotorTaskDelayUs);

    if (++loopCounter >= kMotorTaskYieldInterval) {
      loopCounter = 0;
      vTaskDelay(1);
    }
  }
}

// Motion contract (M0-T010, FR-007).
//
// Abstracts the stepper module for dependency inversion and host-side
// fakes. Implemented by the concrete Motion class. No dynamic
// allocation: consumers hold references to statically created objects.
//
// The interface mirrors Motion's public API (plan.md section 3,
// adapted to the API that emerged in M0-T003).
#pragma once

struct IMotion {
  virtual ~IMotion() = default;

  virtual void begin() = 0;
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual void stop() = 0;
  virtual void zero() = 0;

  virtual void setOutputRpm(float rpm) = 0;
  virtual float outputRpm() = 0;
  virtual void setAccelRpmPerS(float accelRpmPerS) = 0;
  virtual float accelRpmPerS() = 0;

  virtual void startMoveToSteps(long targetSteps) = 0;

  // Continuous move at the current speed in the given direction until
  // stop()/forceStop()/disable() (homing seeks; M3 FR-001).
  virtual void startContinuous(int direction) = 0;
  // Immediate stop, no deceleration ramp (endstop hard stop; M3 plan D5:
  // FastAccelStepper::forceStop()). stop() keeps its ramp semantics.
  virtual void forceStop() = 0;

  virtual bool isEnabled() = 0;
  virtual bool isRunning() = 0;
  virtual long steps() = 0;
  virtual float positionMm() = 0;
};

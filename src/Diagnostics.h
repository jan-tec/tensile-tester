// Startup selftest & health diagnostics (M1, T004 subset: boot log
// grammar + BOARD + NVS checks).
//
// Owns the DeviceModel (M1 FR-001) and the FR-007 serial log grammar:
//   BOOT fw=<ver> git=<sha8> reset=<reason>
//   CHECK id=<id> result=<status> [code=0xNNNN] [meas=<v>] elapsed_ms=<n>
//   STATE old=<s> new=<s> [code=0xNNNN]
// Logs are always English (FR-007); user-facing messages localize in M2.
#pragma once

#include <Arduino.h>

#include "ErrorCode.h"
#include "FirmwareCore.h"
#include "Endstops.h"
#include "ILoadCell.h"
#include "IMotion.h"
#include "PowerSense.h"
#include "Settings.h"

// Firmware version reported in the BOOT line. Bump manually per release.
constexpr const char *kFwVersion = "0.4.0";

class Diagnostics {
 public:
  // Blocking boot selftest (all seven FR-002 checks: BOARD, NVS, POWER,
  // HX711, MOTOR, ENDSTOP1, ENDSTOP2 - M3 T013). Prints the BOOT line,
  // runs the checks, prints CHECK/STATE lines, dispatches the results
  // into the DeviceModel, and disables the motor driver on every fault
  // path (FR-001). Budget: <= 5 s (NFR).
  void runBootSelfTest(Settings &settings, ILoadCell &loadcell, IMotion &motion,
                       PowerSense &power, Endstops &endstops);

  // FR-005: feed the 1 Hz runtime power state into the DeviceModel
  // (Fail -> Error; FR-009: recovery only by reset/reboot). Call after
  // every PowerSense::poll().
  void reportRuntimePowerState(PowerStatus state);

  // M3 FR-007 (T011): a runtime fault with a stable machine code (0x05xx
  // homing faults from TestRunner) -> DeviceModel Error through the same
  // tracked path (STATE line, driver disabled by App's Error transition).
  void reportRuntimeFault(uint16_t code);

  DeviceModel &model() { return model_; }
  const DeviceModel &model() const { return model_; }

  // Latest results of the executed checks (for /api/status, T007).
  const CheckResult *checks(size_t *count) const;

  // FR-002 check set size. Public: presenters size their gather buffers
  // from it (#90) instead of duplicating the constant.
  static constexpr size_t kMaxChecks = 7;

 private:
  static constexpr unsigned long kHx711WaitMs = 1000;   // FR-006 wait_ready
  static constexpr unsigned long kHx711VarianceMs = 500;  // FR-006 window

  CheckResult checkBoard();
  CheckResult checkNvs(Settings &settings);
  CheckResult checkPower(PowerSense &power);
  CheckResult checkHx711(ILoadCell &loadcell);
  CheckResult checkMotor(IMotion &motion);
  CheckResult checkEndstop(CheckId id, bool pressedHigh);  // M3 T013
  void record(const CheckResult &result);
  void dispatchTracked(DeviceEvent event, uint16_t code = 0);
  void printCheck(const CheckResult &result);
  void printState(DeviceState oldState, DeviceState newState, uint16_t code);

  DeviceModel model_;
  CheckResult results_[kMaxChecks] = {};
  size_t resultCount_ = 0;
};

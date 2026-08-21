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
#include "IMotion.h"
#include "Settings.h"

// Firmware version reported in the BOOT line. Bump manually per release.
constexpr const char *kFwVersion = "0.4.0";

class Diagnostics {
 public:
  // Blocking boot selftest (T004: BOARD + NVS; T005/T006 add HX711/MOTOR).
  // Prints the BOOT line, runs the checks, prints CHECK/STATE lines,
  // dispatches the results into the DeviceModel, and disables the motor
  // driver on every fault path (FR-001). Budget: <= 5 s (NFR).
  void runBootSelfTest(Settings &settings, IMotion &motion);

  DeviceModel &model() { return model_; }
  const DeviceModel &model() const { return model_; }

  // Latest results of the executed checks (for /api/status, T007).
  const CheckResult *checks(size_t *count) const;

 private:
  static constexpr size_t kMaxChecks = 5;  // FR-002 check set size

  CheckResult checkBoard();
  CheckResult checkNvs(Settings &settings);
  void record(const CheckResult &result);
  void dispatchTracked(DeviceEvent event, uint16_t code = 0);
  void printCheck(const CheckResult &result);
  void printState(DeviceState oldState, DeviceState newState, uint16_t code);

  DeviceModel model_;
  CheckResult results_[kMaxChecks] = {};
  size_t resultCount_ = 0;
};

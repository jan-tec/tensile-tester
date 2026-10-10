#include "App.h"

#include "Text.h"

void App::begin() {
  Serial.begin(115200);

  motion_.begin();

  // M2 FR-002: the stored locale (NVS, default EN) decides all CLI text
  // before the first line is printed.
  setTextLocale(settings_.loadLocale());

  delay(500);
  Serial.println();
  Serial.println(msg(Msg::BootBanner));
  Serial.println(msg(Msg::MotorsSyncNote));

  loadcell_.begin(settings_);
  powerSense_.begin();

  // M1: blocking startup selftest (FR-007 boot log); must finish before
  // the Web UI answers so /api/status can already report the checks.
  diagnostics_.runBootSelfTest(settings_, loadcell_, motion_, powerSense_);

  webUi_.begin();
}

void App::loop() {
  webUi_.handleClient();
  loadcell_.poll();
  serialCli_.poll();

  // FR-005: 1 Hz runtime power monitoring; a debounced FAIL drives the
  // model to Error (STATE line), FR-009 recovery by reset/reboot.
  powerSense_.poll();
  diagnostics_.reportRuntimePowerState(powerSense_.state());

  // FR-009: on the transition into Error the driver must be off
  // (boot faults already disable it in Diagnostics; this covers the
  // runtime power path).
  const DeviceState modelState = diagnostics_.model().state();
  if (modelState == DeviceState::Error &&
      lastModelState_ != DeviceState::Error) {
    motion_.disable();
  }
  lastModelState_ = modelState;

  const unsigned long now = millis();
  if (now - lastStatusMs_ >= kStatusIntervalMs) {
    lastStatusMs_ = now;
    if (motion_.isEnabled() || loadcell_.isReady()) {
      commands_.printPosition();
    }
  }
}

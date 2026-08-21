#include "App.h"

void App::begin() {
  Serial.begin(115200);

  motion_.begin();

  delay(500);
  Serial.println();
  Serial.println("ESP32 Pruefmaschinen-Steuerung gestartet.");
  Serial.println("Zwei Motoren laufen synchron ueber dieselben STEP/DIR/EN-Signale.");

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

  const unsigned long now = millis();
  if (now - lastStatusMs_ >= kStatusIntervalMs) {
    lastStatusMs_ = now;
    if (motion_.isEnabled() || loadcell_.isReady()) {
      commands_.printPosition();
    }
  }
}

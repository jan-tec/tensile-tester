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
  endstops_.begin();  // M3-T008: endstop inputs (levels live from boot)

  // M1: blocking startup selftest (FR-007 boot log); must finish before
  // the Web UI answers so /api/status can already report the checks.
  diagnostics_.runBootSelfTest(settings_, loadcell_, motion_, powerSense_);

  // M8 FR-009: LED configured only after the boot decisions (GPIO2 is a
  // strapping pin); the pattern engine starts with the post-selftest
  // state, so Idle shows the heartbeat and a boot FAIL shows Error.
  led_.begin(millis());

  webUi_.begin();
}

void App::loop() {
  webUi_.handleClient();
  loadcell_.poll();
  endstops_.poll();  // M3-T008: 5 ms debounce cadence (edges for M8-T002)
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

  // M8: endstop edges -> LED overlay (FR-005 emission window: forwarded
  // from every non-Error state; Error keeps the LED per FR-008). Edges
  // are read-and-clear, so they never queue across states.
  led::LedEvent ledEvent = led::LedEvent::None;
  if (modelState != DeviceState::Error &&
      (endstops_.bottomEdge() || endstops_.topEdge())) {
    ledEvent = led::LedEvent::EndstopEdge;
  }
  led_.tick(ledBaseFor(modelState), ledEvent, millis());

  const unsigned long now = millis();
  if (now - lastStatusMs_ >= kStatusIntervalMs) {
    lastStatusMs_ = now;
    if (motion_.isEnabled() || loadcell_.isReady()) {
      commands_.printPosition();
    }
  }
}

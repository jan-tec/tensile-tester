#include <Arduino.h>

#include "Commands.h"
#include "LoadCell.h"
#include "Motion.h"
#include "SerialCli.h"
#include "Settings.h"
#include "WebUi.h"

namespace {
// Gear/feed math + clamp live in lib/FirmwareCore (host-unit-tested);
// this file keeps runtime config defaults only.
constexpr unsigned long STATUS_INTERVAL_MS = 500;

Settings settings;
Motion motion;
LoadCell loadcell;
Commands commands(motion, loadcell);
SerialCli serialCli(commands);
WebUi webUi(commands, motion, loadcell);

unsigned long lastStatusMs = 0;

// Stepper access lives in Motion (M0-T003); loadcell (HX711, sampling,
// filter, calibration) lives in LoadCell (M0-T004); command policy lives
// in Commands (M0-T006); the serial input loop lives in SerialCli
// (M0-T007); the Web UI (softAP, routes, INDEX_HTML) lives in WebUi
// (M0-T008); gear/feed conversion (outputRpmToStepsPerSecond,
// outputTurnsToSteps, spindleMmToSteps, rpm/feed, clampFeedMmPerMin)
// lives in lib/FirmwareCore, unit-tested on the host.
}  // namespace

void setup() {
  Serial.begin(115200);

  motion.begin();

  delay(500);
  Serial.println();
  Serial.println("ESP32 Pruefmaschinen-Steuerung gestartet.");
  Serial.println("Zwei Motoren laufen synchron ueber dieselben STEP/DIR/EN-Signale.");

  loadcell.begin(settings);
  webUi.begin();
}

void loop() {
  webUi.handleClient();
  loadcell.poll();
  serialCli.poll();

  const unsigned long now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    if (motion.isEnabled() || loadcell.isReady()) {
      commands.printPosition();
    }
  }
}

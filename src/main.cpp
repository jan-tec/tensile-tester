#include <Arduino.h>

#include "App.h"
#include "Commands.h"
#include "Diagnostics.h"
#include "LoadCell.h"
#include "Motion.h"
#include "SerialCli.h"
#include "Settings.h"
#include "WebUi.h"

namespace {
// Composition root: Settings (NVS calibration), Motion (steppers via
// TMC2209 STEP/DIR/EN), LoadCell (HX711, 10 Hz sampling, calibration),
// Commands (command policy, shared by serial and Web), SerialCli (serial
// input loop), WebUi (softAP + routes + UI), App (orchestration).
Settings settings;
Motion motion;
LoadCell loadcell;
Commands commands(motion, loadcell);
SerialCli serialCli(commands);
WebUi webUi(commands, motion, loadcell);
Diagnostics diagnostics;
App app(settings, motion, loadcell, commands, serialCli, webUi,
        diagnostics);
}  // namespace

void setup() {
  app.begin();
}

void loop() {
  app.loop();
}

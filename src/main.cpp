#include <Arduino.h>

#include "App.h"
#include "Commands.h"
#include "Diagnostics.h"
#include "Endstops.h"
#include "LoadCell.h"
#include "Motion.h"
#include "PowerSense.h"
#include "SerialCli.h"
#include "Settings.h"
#include "TestRunner.h"
#include "WebUi.h"

namespace {
// Composition root: Settings (NVS calibration), Motion (steppers via
// TMC2209 STEP/DIR/EN), LoadCell (HX711, 10 Hz sampling, calibration),
// Commands (command policy, shared by serial and Web), SerialCli (serial
// input loop), WebUi (softAP + routes + UI), TestRunner (M3 test cycle
// orchestration over Motion/LoadCell/Endstops), App (orchestration).
Settings settings;
Motion motion;
LoadCell loadcell;
PowerSense powerSense;
Endstops endstops;  // M3-T008: endstop inputs (GPIO 25/26)
Diagnostics diagnostics;
TestRunner testRunner(motion, loadcell, endstops, diagnostics);  // M3-T011
Commands commands(settings, motion, loadcell, diagnostics);
SerialCli serialCli(commands);
WebUi webUi(commands, motion, loadcell, diagnostics);
App app(settings, motion, loadcell, powerSense, endstops, testRunner,
        commands, serialCli, webUi, diagnostics);
}  // namespace

void setup() {
  app.begin();
}

void loop() {
  app.loop();
}

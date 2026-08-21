// Application orchestration (M0-T009; depends on the IMotion/
// ILoadCell contracts since M0-T010).
//
// Owns the boot sequence (Serial, Motion, banner, LoadCell calibration
// load, Web UI start) and the main loop (Web serving, loadcell sampling,
// serial input, periodic status print). main.cpp is the composition
// root: it constructs the modules and delegates setup()/loop() to App.
#pragma once

#include <Arduino.h>

#include "Commands.h"
#include "Diagnostics.h"
#include "ILoadCell.h"
#include "IMotion.h"
#include "PowerSense.h"
#include "SerialCli.h"
#include "Settings.h"
#include "WebUi.h"

class App {
 public:
  App(Settings &settings, IMotion &motion, ILoadCell &loadcell,
      PowerSense &powerSense, Commands &commands, SerialCli &serialCli,
      WebUi &webUi, Diagnostics &diagnostics)
      : settings_(settings),
        motion_(motion),
        loadcell_(loadcell),
        powerSense_(powerSense),
        commands_(commands),
        serialCli_(serialCli),
        webUi_(webUi),
        diagnostics_(diagnostics) {}

  // Boot sequence: Serial, Motion, banner, LoadCell, Web UI.
  void begin();

  // Main loop: Web serving, loadcell sampling, serial input, periodic
  // position/force print. Called from loop().
  void loop();

 private:
  static constexpr unsigned long kStatusIntervalMs = 500;

  Settings &settings_;
  IMotion &motion_;
  ILoadCell &loadcell_;
  PowerSense &powerSense_;
  Commands &commands_;
  SerialCli &serialCli_;
  WebUi &webUi_;
  Diagnostics &diagnostics_;
  unsigned long lastStatusMs_ = 0;
};

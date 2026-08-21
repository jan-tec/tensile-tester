// Application orchestration (M0-T009).
//
// Owns the boot sequence (Serial, Motion, banner, LoadCell calibration
// load, Web UI start) and the main loop (Web serving, loadcell sampling,
// serial input, periodic status print). main.cpp is the composition
// root: it constructs the modules and delegates setup()/loop() to App.
#pragma once

#include <Arduino.h>

#include "Commands.h"
#include "LoadCell.h"
#include "Motion.h"
#include "SerialCli.h"
#include "Settings.h"
#include "WebUi.h"

class App {
 public:
  App(Settings &settings, Motion &motion, LoadCell &loadcell,
      Commands &commands, SerialCli &serialCli, WebUi &webUi)
      : settings_(settings),
        motion_(motion),
        loadcell_(loadcell),
        commands_(commands),
        serialCli_(serialCli),
        webUi_(webUi) {}

  // Boot sequence: Serial, Motion, banner, LoadCell, Web UI.
  void begin();

  // Main loop: Web serving, loadcell sampling, serial input, periodic
  // position/force print. Called from loop().
  void loop();

 private:
  static constexpr unsigned long kStatusIntervalMs = 500;

  Settings &settings_;
  Motion &motion_;
  LoadCell &loadcell_;
  Commands &commands_;
  SerialCli &serialCli_;
  WebUi &webUi_;
  unsigned long lastStatusMs_ = 0;
};

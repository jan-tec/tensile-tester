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
#include "Endstops.h"
#include "ILoadCell.h"
#include "IMotion.h"
#include "LedStatus.h"
#include "PowerSense.h"
#include "SerialCli.h"
#include "Settings.h"
#include "TestCycle.h"
#include "TestRunner.h"
#include "WebUi.h"

class App {
 public:
  App(Settings &settings, IMotion &motion, ILoadCell &loadcell,
      PowerSense &powerSense, Endstops &endstops, TestRunner &testRunner,
      Commands &commands, SerialCli &serialCli, WebUi &webUi,
      Diagnostics &diagnostics)
      : settings_(settings),
        motion_(motion),
        loadcell_(loadcell),
        powerSense_(powerSense),
        endstops_(endstops),
        testRunner_(testRunner),
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

  // M3 T012: one-line completion reports on runner state transitions
  // (FR-002(6) summary, Limit/Aborted notes, homing results). Called
  // every loop; prints only on a transition.
  void reportRunnerEvents();

  Settings &settings_;
  IMotion &motion_;
  ILoadCell &loadcell_;
  PowerSense &powerSense_;
  Endstops &endstops_;
  TestRunner &testRunner_;
  Commands &commands_;
  SerialCli &serialCli_;
  WebUi &webUi_;
  Diagnostics &diagnostics_;
  unsigned long lastStatusMs_ = 0;
  DeviceState lastModelState_ = DeviceState::Booting;

  // M8: LED base-state mapping (plan D2) - Homing/Running become
  // reachable with M3-T011; the mapping is complete from day one.
  static led::LedBase ledBaseFor(DeviceState state) {
    switch (state) {
      case DeviceState::Booting:
      case DeviceState::SelfTest: return led::LedBase::SelfTest;
      case DeviceState::Error:    return led::LedBase::Error;
      case DeviceState::Idle:     break;
    }
    return led::LedBase::Idle;
  }

  LedStatus led_;

  // M3 T012: last-seen runner states for transition-edge reporting.
  TestState lastTestState_ = TestState::Idle;
  HomePhase lastHomePhase_ = HomePhase::Idle;
  bool started_ = false;  // suppress the boot report until the first tick
};

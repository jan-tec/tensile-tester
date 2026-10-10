// Command policy (M0-T006; M3-T012: test-cycle command surface).
//
// Owns the executeCommand() policy that previously lived inline in
// main.cpp: dispatch, validation, German response text, and the
// available-commands help list. Both entry points (serial CLI and Web
// API) call execute().
//
// Name matching uses the T002 parser (FirmwareCore::commandFromName);
// value parsing uses FirmwareCore::parseCommandValue via a String
// adapter. The quirk tracked in #41 (case-sensitive names) is preserved;
// #42 (word boundary) and #43 (garbage value -> 0.0 / truncated token)
// are fixed in their own PRs per the No-Quirk Policy.
//
// M3 (T012): home/home top/test route into TestRunner (D11 exclusivity,
// D15 Error rules); `stop` routes to the runner first; testspeed/
// testarm/testdrop persist through Settings; `data` streams the CSV in
// loop-tick chunks (D8) with the D14 status-line suppression riding on
// TestRunner::dumpInProgress().
#pragma once

#include <Arduino.h>
#include <FirmwareCore.h>

#include "LoadCell.h"
#include "Motion.h"

class Diagnostics;
class Settings;
class TestRunner;

class Commands {
 public:
  Commands(Settings &settings, Motion &motion, LoadCell &loadcell,
           Diagnostics &diagnostics, TestRunner &testRunner)
      : settings_(settings),
        motion_(motion),
        loadcell_(loadcell),
        diagnostics_(diagnostics),
        testRunner_(testRunner) {}

  // Runs one command (trimmed or untrimmed) and fills response with the
  // localized status text (M2: all replies resolve through Text's msg();
  // the locale is switched with `lang <en|de>`). Returns true on success.
  // The serial caller prints helpText() on false; the Web API maps false
  // to HTTP 400.
  bool execute(String command, String &response);

  // Serial status line, shared by the "pos" command and the periodic
  // status output in the loop task. Travel shows n/a while no datum
  // exists (M3 FR-006).
  void printPosition();

  // CSV dump pump (M3 D8): call from App::loop(); writes at most one
  // kCsvChunkBytes chunk per tick, only when the serial TX buffer can
  // take it whole. No-op unless a `data` command started a dump.
  void pollDump();

  // M8-T003: read-and-clear flag set by a successful cal_apply - App
  // turns it into the CalApplied LED overlay (Idle window, FR-005).
  bool consumeCalAppliedEvent() {
    const bool event = calAppliedEvent_;
    calAppliedEvent_ = false;
    return event;
  }

  // Help line printed by the serial path after an unknown command
  // (localized pair from Text, M2-T003).
  static String helpText();

  // Jog travel for "jog up" / "jog down" (full spindle stroke); the jog
  // commands target current position +/- this distance.
  static constexpr float kJogTravelMm = 1000.0f;

  // One CSV chunk per loop tick, max (D8; NFR: the dump never blocks the
  // motor task). 128 B fits the default UART TX buffer in one write.
  static constexpr size_t kCsvChunkBytes = 128;

 private:
  // String adapter around the T002 parser (FirmwareCore::parseCommandValue).
  ParseResult parseValueAfterCommand(const String &command, const char *prefix,
                                     float &value) const;

  // String adapter around parseCommandIndex (`data <from>`).
  ParseResult parseIndexAfterCommand(const String &command, const char *prefix,
                                     uint32_t &index) const;

  // Shared response for ParseResult::InvalidValue (NQP #43): the token was
  // present but not a number, distinct from the per-command "> 0" errors.
  bool reportInvalidValue(String &response) const;

  // Applies one test-config field change: rejects while a run is active
  // (applyConfig resets the homing FSM), clamps through the runner,
  // persists, replies with the EFFECTIVE (clamped) value + suffix.
  bool applyTestConfigValue(float value, String &response,
                            float TestConfig::*field, const char *suffix);

  // Shared gate for config changes: locked in fault state (applyConfig
  // resets the homing FSM - unlatching its Fault behind the device
  // Error gate would blur FR-009's recovery rule) and rejected while a
  // run is active (D11). True when the change may proceed.
  bool testConfigChangeAllowed(String &response) const;

  Settings &settings_;
  Motion &motion_;
  LoadCell &loadcell_;
  Diagnostics &diagnostics_;
  TestRunner &testRunner_;

  bool calAppliedEvent_ = false;  // M8-T003 (consumed by App's LED block)
};

// Command policy (M0-T006).
//
// Owns the executeCommand() policy that previously lived inline in
// main.cpp: dispatch, validation, German response text, and the
// available-commands help list. Both entry points (serial CLI and Web
// API) call execute().
//
// Name matching uses the T002 parser (FirmwareCore::commandFromName);
// value parsing uses FirmwareCore::parseCommandValue via a String
// adapter. Behavior is byte-identical to the legacy inline dispatch
// (M0 FR-004) - the quirks tracked in #41 (case-sensitive names), #42
// (prefix matching without word boundary) and #43 (garbage value ->
// 0.0) are preserved, not fixed.
#pragma once

#include <Arduino.h>
#include <FirmwareCore.h>

#include "LoadCell.h"
#include "Motion.h"

class Commands {
 public:
  Commands(Motion &motion, LoadCell &loadcell)
      : motion_(motion), loadcell_(loadcell) {}

  // Runs one command (trimmed or untrimmed) and fills response with the
  // German status text. Returns true on success. The serial caller
  // prints helpText() on false; the Web API maps false to HTTP 400.
  bool execute(String command, String &response);

  // Serial status line, shared by the "pos" command and the periodic
  // status output in the loop task.
  void printPosition();

  // Help line printed by the serial path after an unknown command.
  static const char *helpText();

  // Jog travel for "jog up" / "jog down" (full spindle stroke); the jog
  // commands target current position +/- this distance.
  static constexpr float kJogTravelMm = 1000.0f;

 private:
  // String adapter around the T002 parser (FirmwareCore::parseCommandValue).
  bool parseValueAfterCommand(const String &command, const char *prefix,
                              float &value) const;

  Motion &motion_;
  LoadCell &loadcell_;
};

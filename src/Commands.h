// Command policy (M0-T006).
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
#pragma once

#include <Arduino.h>
#include <FirmwareCore.h>

#include "LoadCell.h"
#include "Motion.h"

class Diagnostics;
class Settings;

class Commands {
 public:
  Commands(Settings &settings, Motion &motion, LoadCell &loadcell,
           Diagnostics &diagnostics)
      : settings_(settings), motion_(motion), loadcell_(loadcell),
        diagnostics_(diagnostics) {}

  // Runs one command (trimmed or untrimmed) and fills response with the
  // localized status text (M2: all replies resolve through Text's msg();
  // the locale is switched with `lang <en|de>`). Returns true on success.
  // The serial caller prints helpText() on false; the Web API maps false
  // to HTTP 400.
  bool execute(String command, String &response);

  // Serial status line, shared by the "pos" command and the periodic
  // status output in the loop task.
  void printPosition();

  // Help line printed by the serial path after an unknown command
  // (localized pair from Text, M2-T003).
  static String helpText();

  // Jog travel for "jog up" / "jog down" (full spindle stroke); the jog
  // commands target current position +/- this distance.
  static constexpr float kJogTravelMm = 1000.0f;

 private:
  // String adapter around the T002 parser (FirmwareCore::parseCommandValue).
  ParseResult parseValueAfterCommand(const String &command, const char *prefix,
                                     float &value) const;

  // Shared response for ParseResult::InvalidValue (NQP #43): the token was
  // present but not a number, distinct from the per-command "> 0" errors.
  bool reportInvalidValue(String &response) const;

  Settings &settings_;
  Motion &motion_;
  LoadCell &loadcell_;
  Diagnostics &diagnostics_;
};

// Serial CLI (M0-T007).
//
// Owns the serial input loop that lived inline in main.cpp: line
// accumulation, CR skipping, LF dispatch, and the unknown-command help
// printout. Command policy stays in Commands; poll() is driven by the
// loop task and must return quickly.
#pragma once

#include <Arduino.h>

#include "Commands.h"

class SerialCli {
 public:
  explicit SerialCli(Commands &commands) : commands_(commands) {}

  // Drains available serial bytes and dispatches each completed line.
  void poll();

 private:
  Commands &commands_;
  String buffer_;
};

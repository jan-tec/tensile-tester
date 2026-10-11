// Web UI (M0-T008; M7 FR-006/FR-007: /api/data CSV surface).
//
// Owns the WiFi AP, the WebServer and the four routes:
// "/" (INDEX_HTML), "/api/status" (statusJson), "/api/command"
// (delegated to Commands - shared with the serial CLI) and "/api/data"
// (the run's sample buffer as the M3 `data` CSV grammar). handleClient()
// is driven by the loop task and must return quickly.
#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "Commands.h"
#include "LoadCell.h"
#include "Motion.h"

class Diagnostics;
class TestRunner;

class WebUi {
 public:
  WebUi(Commands &commands, Motion &motion, LoadCell &loadcell,
        Diagnostics &diagnostics, TestRunner &testRunner)
      : server_(80),
        commands_(commands),
        motion_(motion),
        loadcell_(loadcell),
        diagnostics_(diagnostics),
        testRunner_(testRunner) {}

  // Starts the softAP, registers the routes and starts the server.
  void begin();

  // Serves pending HTTP requests. Called from the loop task.
  void handleClient();

 private:
  String statusJson();
  static String jsonMessage(bool ok, const String &message);
  void handleRoot();
  void handleStatus();
  void handleCommandApi();
  void handleDataApi();

  WebServer server_;
  Commands &commands_;
  Motion &motion_;
  LoadCell &loadcell_;
  Diagnostics &diagnostics_;
  TestRunner &testRunner_;
};

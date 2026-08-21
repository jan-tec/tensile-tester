// Web UI (M0-T008).
//
// Owns the WiFi AP, the WebServer and the three routes:
// "/" (INDEX_HTML), "/api/status" (statusJson) and "/api/command"
// (delegated to Commands - shared with the serial CLI). handleClient()
// is driven by the loop task and must return quickly.
#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "Commands.h"
#include "LoadCell.h"
#include "Motion.h"

class WebUi {
 public:
  WebUi(Commands &commands, Motion &motion, LoadCell &loadcell)
      : server_(80),
        commands_(commands),
        motion_(motion),
        loadcell_(loadcell) {}

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

  WebServer server_;
  Commands &commands_;
  Motion &motion_;
  LoadCell &loadcell_;
};

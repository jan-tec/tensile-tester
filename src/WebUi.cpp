#include <FirmwareCore.h>
#include <WiFi.h>

#include "UiHtml.h"

#include "WebUi.h"

namespace {
constexpr char WIFI_SSID[] = "ESP32-Testmaschine";
constexpr char WIFI_PASSWORD[] = "testmaschine";
}  // namespace

void WebUi::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("WLAN Access Point gestartet: ");
  Serial.println(WIFI_SSID);
  Serial.print("Passwort: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Weboberflaeche: http://");
  Serial.println(WiFi.softAPIP());

  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/command", HTTP_GET, [this] { handleCommandApi(); });
  server_.begin();
}

void WebUi::handleClient() {
  server_.handleClient();
}

String WebUi::statusJson() {
  String json = "{";
  json += "\"driverEnabled\":";
  json += motion_.isEnabled() ? "true" : "false";
  json += ",\"running\":";
  json += motion_.isRunning() ? "true" : "false";
  json += ",\"rpm\":";
  json += String(motion_.outputRpm(), 2);
  json += ",\"acc\":";
  json += String(motion_.accelRpmPerS(), 2);
  json += ",\"feedMmPerMin\":";
  json += String(rpmToFeedMmPerMin(motion_.outputRpm()), 2);
  json += ",\"positionMm\":";
  json += String(motion_.positionMm(), 3);
  json += ",\"steps\":";
  json += String(motion_.steps());
  json += ",\"forceNewton\":";
  json += String(loadcell_.forceNewton(), 2);
  json += ",\"calibrationValid\":";
  json += loadcell_.calibrationValid() ? "true" : "false";
  json += ",\"calibrationFactor\":";
  json += String(loadcell_.calibrationFactor(), 6);
  json += ",\"network\":\"SSID ";
  json += WIFI_SSID;
  json += " | IP ";
  json += WiFi.softAPIP().toString();
  json += "\"}";
  return json;
}

String WebUi::jsonMessage(bool ok, const String &message) {
  String json = "{\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"message\":\"";
  String escaped = message;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  json += escaped;
  json += "\"}";
  return json;
}

void WebUi::handleRoot() {
  server_.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebUi::handleStatus() {
  server_.send(200, "application/json", statusJson());
}

void WebUi::handleCommandApi() {
  if (!server_.hasArg("cmd")) {
    server_.send(400, "application/json",
                 jsonMessage(false, "Parameter 'cmd' fehlt."));
    return;
  }

  String response;
  const String command = server_.arg("cmd");
  const bool ok = commands_.execute(command, response);
  Serial.print("Web-Kommando: ");
  Serial.println(command);
  Serial.println(response);
  server_.send(ok ? 200 : 400, "application/json", jsonMessage(ok, response));
}

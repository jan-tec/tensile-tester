#include <FirmwareCore.h>
#include <WiFi.h>

#include "Diagnostics.h"
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
  if (loadcell_.isReady()) {
    json += String(loadcell_.forceNewton(), 2);
  } else {
    // HX711 not ready: force is "n/a" (M1 T005), not a zero reading.
    json += "\"n/a\"";
  }
  json += ",\"calibrationValid\":";
  json += loadcell_.calibrationValid() ? "true" : "false";
  json += ",\"calibrationFactor\":";
  json += String(loadcell_.calibrationFactor(), 6);
  json += ",\"network\":\"SSID ";
  json += WIFI_SSID;
  json += " | IP ";
  json += WiFi.softAPIP().toString();
  json += "\"";

  // FR-008: device state + the boot selftest object (overall + checks[]).
  const DeviceModel &model = diagnostics_.model();
  json += ",\"state\":\"";
  json += deviceStateName(model.state());
  json += "\"";
  json += ",\"faultCode\":";
  json += String(model.faultCode());

  size_t count = 0;
  const CheckResult *checks = diagnostics_.checks(&count);
  CheckStatus statuses[5] = {};
  for (size_t i = 0; i < count && i < 5; i++) {
    statuses[i] = checks[i].status;
  }
  json += ",\"selftest\":{\"overall\":\"";
  json += checkStatusName(overallStatus(statuses, count));
  json += "\",\"checks\":[";
  for (size_t i = 0; i < count; i++) {
    if (i != 0) {
      json += ",";
    }
    json += "{\"id\":\"";
    json += checkIdName(checks[i].id);
    json += "\",\"status\":\"";
    json += checkStatusName(checks[i].status);
    json += "\",\"errorCode\":";
    json += String(static_cast<unsigned>(checks[i].errorCode));
    json += ",\"measurement\":";
    json += String(checks[i].measurement, 2);
    json += ",\"elapsedMs\":";
    json += String(checks[i].elapsedMs);
    json += "}";
  }
  json += "]}}";
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

#include <FirmwareCore.h>
#include <WiFi.h>

#include "Diagnostics.h"
#include "TestRunner.h"
#include "Text.h"
#include "UiHtml.h"

#include "WebUi.h"

namespace {
constexpr char WIFI_SSID[] = "ESP32-Testmaschine";
constexpr char WIFI_PASSWORD[] = "testmaschine";
}  // namespace

void WebUi::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print(msg(Msg::ApStarted));
  Serial.println(WIFI_SSID);
  Serial.print(msg(Msg::ApPassword));
  Serial.println(WIFI_PASSWORD);
  Serial.print(msg(Msg::WebUiUrl));
  Serial.println(WiFi.softAPIP());

  server_.on("/", HTTP_GET, [this] { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
  server_.on("/api/command", HTTP_GET, [this] { handleCommandApi(); });
  server_.on("/api/data", HTTP_GET, [this] { handleDataApi(); });
  server_.begin();
}

void WebUi::handleClient() {
  server_.handleClient();
}

String WebUi::statusJson() {
  String json;
  json.reserve(1100);  // jane: 722 B at 5 checks (2026-10-10) + ~230 B M7 cycle/run block
  // One steps() read for both fields: the motor task on core 0 can advance
  // the position between two reads, which would make them disagree.
  const long currentSteps = motion_.steps();
  json += "{";
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
  json += String(spindleMmFromSteps(currentSteps), 3);
  json += ",\"steps\":";
  json += String(currentSteps);
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

  // M7 FR-005/FR-008: tear-workflow surface. Cycle state via the pinned
  // TestCycleModel::name() contract (same strings as the log grammar);
  // the run object is the `testdata` facts the run summary renders.
  // breakMm is meaningful only when break is true.
  json += ",\"homed\":";
  json += testRunner_.homed() ? "true" : "false";
  json += ",\"testState\":\"";
  json += TestCycleModel::name(testRunner_.testState());
  json += "\"";
  json += ",\"run\":{\"samples\":";
  json += String(testRunner_.samples().count());
  json += ",\"peakN\":";
  json += String(testRunner_.peakForceN(), 2);
  json += ",\"peakIndex\":";
  json += String(testRunner_.peakIndex());
  json += ",\"break\":";
  json += testRunner_.breakDetected() ? "true" : "false";
  json += ",\"breakMm\":";
  json += String(testRunner_.breakDisplacementMm(), 2);
  json += ",\"truncated\":";
  json += testRunner_.samples().truncated() ? "true" : "false";
  json += "}";
  // M2-T005 (FR-005): the JS dictionary follows the server locale, so the
  // serial `lang` command and the Web selector stay in sync (plan D3).
  json += ",\"locale\":\"";
  json += localeName(textLocale());
  json += "\"";

  // #95: monotonic boot clock - a client that sees it DECREASE between
  // polls knows the device rebooted under it (driver/position state reset
  // with no other trace in the UI). Wraps after 49.7 days - irrelevant
  // here, and the client only compares for "smaller than before".
  json += ",\"uptimeMs\":";
  json += String(millis());

  // FR-008: device state + the boot selftest object (overall + checks[]).
  const DeviceModel &model = diagnostics_.model();
  json += ",\"state\":\"";
  json += deviceStateName(model.state());
  json += "\"";
  json += ",\"faultCode\":";
  json += String(model.faultCode());
  if (model.faultCode() != 0) {
    // M2-T004 (FR-007): localized fault detail next to the stable code.
    // Text rows are ASCII-only (FR-006), so no JSON escaping is needed.
    json += ",\"faultMessage\":\"";
    json += msg(msgForFault(static_cast<ErrorCode>(model.faultCode())));
    json += "\"";
  }

  size_t count = 0;
  const CheckResult *checks = diagnostics_.checks(&count);
  // #90: gather is sized by the check-set contract, not a duplicated 5; the
  // overall rating reads exactly the gathered entries, so a check beyond the
  // buffer (impossible today, defensive by construction) can never expose
  // uninitialized values or be silently ignored.
  CheckStatus statuses[Diagnostics::kMaxChecks] = {};
  size_t gathered = 0;
  for (; gathered < count && gathered < Diagnostics::kMaxChecks; gathered++) {
    statuses[gathered] = checks[gathered].status;
  }
  json += ",\"selftest\":{\"overall\":\"";
  json += checkStatusName(overallStatus(statuses, gathered));
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
                 jsonMessage(false, msg(Msg::CmdParamMissing)));
    return;
  }

  String response;
  const String command = server_.arg("cmd");
  // M6-T005 (plan D1): the web path ignores the error code and the meta
  // - its contract is the HTTP body alone (FR-006 byte-identical).
  ErrorCode error;
  ApiReplyMeta meta;
  const bool ok = commands_.execute(command, response, error, meta);
  // M6-T006 (plan D3): the serial echo of a web command is one JSON
  // event line (was: three prose lines) - SC-001 keeps the serial line
  // JSON-only; the HTTP body below is unchanged.
  char echo[512];
  formatApiWebCommandEvent(command.c_str(), ok, response.c_str(),
                           commands_.stateSnapshot(), echo, sizeof(echo));
  Serial.println(echo);
  server_.send(ok ? 200 : 400, "application/json", jsonMessage(ok, response));
}

// M7 FR-006/FR-007: the run's samples as the exact M3 `data` CSV grammar
// (formatCsvChunk is the single implementation - header, lines, END marker),
// chunked so 8192 samples (~170 KB) never buffer in one String. The same
// endpoint serves the curve fetch (FR-006) and the file download (FR-007,
// Content-Disposition + the UI's download attribute).
//
// Exclusivity mirrors the `data` command policy: rejected while a cycle is
// active or a serial dump is in flight. jane: this handler blocks the loop
// task for the send duration (seconds on weak WiFi); acceptable because it
// is unreachable while a test runs - the load cell and motor are idle.
void WebUi::handleDataApi() {
  if (testRunner_.active()) {
    server_.send(400, "application/json",
                 jsonMessage(false, msg(Msg::DumpBlockedRunning)));
    return;
  }
  if (testRunner_.dumpInProgress()) {
    server_.send(400, "application/json",
                 jsonMessage(false, msg(Msg::DumpBusy)));
    return;
  }
  server_.sendHeader("Content-Disposition",
                     "attachment; filename=\"tensile-run.csv\"");
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "text/csv; charset=utf-8", "");
  char chunk[512];
  size_t next = 0;
  while (true) {
    const size_t written =
        formatCsvChunk(testRunner_.samples(), next, chunk, sizeof(chunk), &next);
    if (written == 0) {
      break;  // complete (or empty buffer): header + lines + END are out
    }
    server_.sendContent(String(chunk, written));
  }
  server_.sendContent("");  // terminate the chunked transfer
}

#include <Arduino.h>
#include <AccelStepper.h>
#include <HX711.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
constexpr uint8_t DIR_PIN = 19;
constexpr uint8_t STEP_PIN = 18;
constexpr uint8_t EN_PIN = 5;

constexpr uint8_t LOADCELL_DOUT_PIN = 32;
constexpr uint8_t LOADCELL_SCK_PIN = 33;

constexpr long MOTOR_FULL_STEPS_PER_REV = 200;
constexpr float GEAR_RATIO = 13.73f;
constexpr uint8_t MICROSTEPS = 8;

constexpr float OUTPUT_STEPS_PER_REV =
    MOTOR_FULL_STEPS_PER_REV * GEAR_RATIO * MICROSTEPS;
constexpr float DEFAULT_OUTPUT_RPM = 5.0f;
constexpr float DEFAULT_OUTPUT_ACCEL_RPM_PER_S = 20.0f;
constexpr float DEFAULT_MOVE_MM_PER_MIN = 10.0f;
constexpr float SPINDLE_PITCH_MM_PER_REV = 2.0f;
constexpr unsigned long STATUS_INTERVAL_MS = 500;
constexpr unsigned long HX711_SAMPLE_INTERVAL_MS = 100;
constexpr float GRAVITY = 9.80665f;
constexpr float FORCE_FILTER_ALPHA = 0.18f;
constexpr uint16_t MOTOR_TASK_DELAY_US = 200;
constexpr uint8_t MOTOR_TASK_YIELD_INTERVAL = 20;

constexpr char WIFI_SSID[] = "ESP32-Testmaschine";
constexpr char WIFI_PASSWORD[] = "testmaschine";

// Diesen Faktor spaeter mit bekanntem Gewicht kalibrieren.
constexpr float LOADCELL_CALIBRATION_FACTOR = 1.0f;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Testmaschine</title>
  <style>
    :root {
      --bg: #f2efe8;
      --panel: rgba(255, 251, 245, 0.92);
      --line: #d7ccbc;
      --text: #1f1c18;
      --muted: #6e6356;
      --accent: #b14d25;
      --accent-dark: #8f3816;
      --success: #2f6f4f;
      --danger: #9e2f2f;
      --plot: #d9622b;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      color: var(--text);
      font-family: "Avenir Next", "Segoe UI", sans-serif;
      background:
        radial-gradient(circle at top left, #f8e0c6 0, transparent 30%),
        radial-gradient(circle at right, #eed9cf 0, transparent 24%),
        linear-gradient(160deg, #f4eee5 0%, #e7ddd0 100%);
      min-height: 100vh;
    }
    .wrap {
      max-width: 1100px;
      margin: 0 auto;
      padding: 24px 16px 48px;
    }
    h1 {
      margin: 0 0 8px;
      font-size: clamp(2rem, 4vw, 3.2rem);
      letter-spacing: 0.02em;
    }
    .sub {
      margin: 0 0 20px;
      color: var(--muted);
      max-width: 760px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 14px;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 20px;
      padding: 18px;
      box-shadow: 0 14px 32px rgba(79, 49, 23, 0.08);
      backdrop-filter: blur(6px);
    }
    .plot-card {
      grid-column: 1 / -1;
    }
    h2 {
      margin: 0 0 14px;
      font-size: 0.95rem;
      text-transform: uppercase;
      letter-spacing: 0.08em;
      color: var(--muted);
    }
    .status {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }
    .pill {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 11px 12px;
      background: rgba(255,255,255,0.5);
    }
    .label {
      display: block;
      font-size: 0.77rem;
      margin-bottom: 4px;
      color: var(--muted);
    }
    .value {
      font-size: 1.1rem;
      font-weight: 700;
    }
    .btnrow {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    form {
      display: grid;
      gap: 10px;
      margin-bottom: 12px;
    }
    input, select {
      width: 100%;
      padding: 12px 13px;
      border-radius: 12px;
      border: 1px solid var(--line);
      background: rgba(255,255,255,0.8);
      font-size: 1rem;
      color: var(--text);
    }
    button {
      border: 0;
      border-radius: 12px;
      padding: 12px 14px;
      font-size: 0.98rem;
      font-weight: 700;
      cursor: pointer;
      color: white;
      background: var(--accent);
      transition: transform 120ms ease, background 120ms ease;
    }
    button:hover {
      transform: translateY(-1px);
      background: var(--accent-dark);
    }
    button.secondary { background: #74695f; }
    button.success { background: var(--success); }
    button.danger { background: var(--danger); }
    canvas {
      width: 100%;
      height: 260px;
      background: linear-gradient(180deg, rgba(255,255,255,0.7), rgba(248,240,230,0.9));
      border: 1px solid var(--line);
      border-radius: 16px;
    }
    .note {
      margin-top: 10px;
      color: var(--muted);
      font-size: 0.92rem;
    }
    #message {
      min-height: 24px;
      margin-top: 16px;
      font-weight: 700;
    }
    @media (max-width: 640px) {
      .status, .btnrow { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>Pruefmaschinen-Steuerung</h1>
    <p class="sub">Zwei synchron laufende Motoren auf gemeinsamen STEP/DIR/EN-Signalen, Spindelhub 2 mm/Umdrehung und Live-Kraftmessung ueber HX711.</p>

    <div class="grid">
      <section class="card plot-card">
        <h2>Kraftverlauf</h2>
        <canvas id="forceChart"></canvas>
        <p class="note">Live-Plot der gemessenen Kraft mit 10 Samples pro Sekunde. Nullpunkt mit "Tare Kraft" setzen, Kalibrierfaktor spaeter exakt einmessen.</p>
      </section>

      <section class="card">
        <h2>Status</h2>
        <div class="status">
          <div class="pill"><span class="label">Treiber</span><span class="value" id="driverState">-</span></div>
          <div class="pill"><span class="label">Bewegung</span><span class="value" id="motionState">-</span></div>
          <div class="pill"><span class="label">Modus</span><span class="value" id="testMode">-</span></div>
          <div class="pill"><span class="label">Kraft</span><span class="value" id="forceValue">-</span></div>
          <div class="pill"><span class="label">Position</span><span class="value" id="positionValue">-</span></div>
          <div class="pill"><span class="label">Vorschub</span><span class="value" id="feedValue">-</span></div>
        </div>
        <p class="note" id="wifiInfo">Verbinde dich mit dem ESP32</p>
      </section>

      <section class="card">
        <h2>Achse</h2>
        <div class="btnrow">
          <button class="success" data-cmd="en">Enable</button>
          <button class="secondary" data-cmd="off">Disable</button>
          <button data-cmd="zero">Zero Position</button>
          <button class="secondary" data-cmd="tare">Tare Kraft</button>
          <button class="danger" data-cmd="stop">Stop</button>
          <button class="secondary" data-cmd="force">Kraft loggen</button>
        </div>
      </section>

      <section class="card">
        <h2>Geschwindigkeit</h2>
        <form data-prefix="rpm">
          <input name="value" type="number" step="0.1" min="0.1" placeholder="RPM Ausgangswelle">
          <button type="submit">RPM setzen</button>
        </form>
        <form data-prefix="acc">
          <input name="value" type="number" step="0.1" min="0.1" placeholder="Beschleunigung in U/min/s">
          <button type="submit">ACC setzen</button>
        </form>
        <form data-prefix="feed">
          <input name="value" type="number" step="0.1" min="0.1" placeholder="Vorschub mm/min">
          <button type="submit">Vorschub setzen</button>
        </form>
      </section>

      <section class="card">
        <h2>Bewegung</h2>
        <form data-prefix="move">
          <input name="value" type="number" step="0.01" placeholder="Relativweg in mm">
          <button type="submit">mm fahren</button>
        </form>
        <form data-prefix="turn">
          <input name="value" type="number" step="0.01" placeholder="Umdrehungen relativ">
          <button type="submit">Turns fahren</button>
        </form>
      </section>

      <section class="card">
        <h2>Tests</h2>
        <select id="modeSelect" disabled>
          <option>Manuell</option>
          <option>Ziehen bis Bruch</option>
          <option>Kraft halten</option>
        </select>
        <p class="note">Automatische Testmodi folgen in der naechsten Iteration. Die Struktur dafuer ist jetzt vorbereitet.</p>
      </section>

    </div>

    <div id="message"></div>
  </div>

  <script>
    const sampleRateHz = 10;
    const maxSamples = 600;
    const fixedYAxisMin = 0;
    const fixedYAxisMax = 100;
    const messageEl = document.getElementById("message");
    const chart = document.getElementById("forceChart");
    const ctx = chart.getContext("2d");
    const forceHistory = [];

    function resizeCanvas() {
      const ratio = window.devicePixelRatio || 1;
      const cssWidth = chart.clientWidth || 1000;
      const cssHeight = 300;
      chart.width = Math.floor(cssWidth * ratio);
      chart.height = Math.floor(cssHeight * ratio);
      ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      drawPlot();
    }

    const statusFields = {
      driverState: document.getElementById("driverState"),
      motionState: document.getElementById("motionState"),
      testMode: document.getElementById("testMode"),
      forceValue: document.getElementById("forceValue"),
      positionValue: document.getElementById("positionValue"),
      feedValue: document.getElementById("feedValue"),
      wifiInfo: document.getElementById("wifiInfo")
    };

    function setMessage(text, isError = false) {
      messageEl.textContent = text;
      messageEl.style.color = isError ? "#9e2f2f" : "#2f6f4f";
    }

    function drawPlot() {
      const width = chart.clientWidth || 1000;
      const height = 300;
      ctx.clearRect(0, 0, width, height);

      ctx.strokeStyle = "#d7ccbc";
      ctx.lineWidth = 1;
      for (let i = 0; i <= 5; i++) {
        const y = 20 + ((height - 40) / 5) * i;
        ctx.beginPath();
        ctx.moveTo(40, y);
        ctx.lineTo(width - 12, y);
        ctx.stroke();
      }

      if (forceHistory.length < 2) {
        return;
      }

      const minForce = fixedYAxisMin;
      const maxForce = fixedYAxisMax;
      const span = Math.max(maxForce - minForce, 1);

      ctx.strokeStyle = "#d9622b";
      ctx.lineWidth = 3;
      ctx.beginPath();
      forceHistory.forEach((value, index) => {
        const denominator = Math.max(forceHistory.length - 1, 1);
        const x = 40 + ((width - 52) * index) / denominator;
        const y = height - 20 - ((value - minForce) / span) * (height - 40);
        if (index === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      });
      ctx.stroke();

      ctx.fillStyle = "#6e6356";
      ctx.font = "14px sans-serif";
      ctx.fillText(maxForce.toFixed(0) + " N", 8, 18);
      ctx.fillText(((maxForce + minForce) / 2).toFixed(0) + " N", 8, height / 2 + 4);
      ctx.fillText(minForce.toFixed(0) + " N", 8, height - 8);
      ctx.fillText(((forceHistory.length - 1) / sampleRateHz).toFixed(1) + " s", width - 58, height - 8);
    }

    async function sendCommand(command) {
      try {
        const response = await fetch("/api/command?cmd=" + encodeURIComponent(command));
        const data = await response.json();
        setMessage(data.message || command, !data.ok);
        await refreshStatus();
      } catch (error) {
        setMessage("Keine Verbindung zum ESP32.", true);
      }
    }

    async function refreshStatus() {
      try {
        const response = await fetch("/api/status");
        const data = await response.json();
        statusFields.driverState.textContent = data.driverEnabled ? "aktiv" : "aus";
        statusFields.motionState.textContent = data.running ? "laeuft" : "bereit";
        statusFields.testMode.textContent = data.testMode;
        statusFields.forceValue.textContent = Number(data.forceNewton).toFixed(2) + " N";
        statusFields.positionValue.textContent = Number(data.positionMm).toFixed(3) + " mm";
        statusFields.feedValue.textContent = Number(data.feedMmPerMin).toFixed(2) + " mm/min";
        statusFields.wifiInfo.textContent = data.network;

        forceHistory.push(Number(data.forceNewton));
        while (forceHistory.length > maxSamples) {
          forceHistory.shift();
        }
        drawPlot();
      } catch (error) {
        setMessage("Status kann nicht geladen werden.", true);
      }
    }

    document.querySelectorAll("[data-cmd]").forEach((button) => {
      button.addEventListener("click", () => sendCommand(button.dataset.cmd));
    });

    document.querySelectorAll("form[data-prefix]").forEach((form) => {
      form.addEventListener("submit", (event) => {
        event.preventDefault();
        const value = form.elements.value.value.trim();
        if (!value) {
          setMessage("Bitte einen Wert eingeben.", true);
          return;
        }
        sendCommand(form.dataset.prefix + " " + value);
      });
    });

    window.addEventListener("resize", resizeCanvas);
    resizeCanvas();
    refreshStatus();
    setInterval(refreshStatus, 100);
  </script>
</body>
</html>
)HTML";

enum class TestMode {
  Manual,
  PullUntilBreak,
  HoldForce,
};

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
HX711 scale;
WebServer server(80);
TaskHandle_t motorTaskHandle = nullptr;
portMUX_TYPE motionMux = portMUX_INITIALIZER_UNLOCKED;

float configuredOutputRpm = DEFAULT_OUTPUT_RPM;
float configuredOutputAccelRpmPerS = DEFAULT_OUTPUT_ACCEL_RPM_PER_S;
bool driverEnabled = false;
unsigned long lastStatusMs = 0;
unsigned long lastHx711SampleMs = 0;
float currentForceNewton = 0.0f;
bool loadcellReady = false;
TestMode activeTestMode = TestMode::Manual;
String serialBuffer;

bool executeCommand(String command, String &response);

float outputRpmToStepsPerSecond(float rpm) {
  return (rpm * OUTPUT_STEPS_PER_REV) / 60.0f;
}

float outputAccelToStepsPerSecondSq(float rpmPerSecond) {
  return (rpmPerSecond * OUTPUT_STEPS_PER_REV) / 60.0f;
}

long outputTurnsToSteps(float turns) {
  return lroundf(turns * OUTPUT_STEPS_PER_REV);
}

long spindleMmToSteps(float mm) {
  return outputTurnsToSteps(mm / SPINDLE_PITCH_MM_PER_REV);
}

long currentStepperPosition() {
  portENTER_CRITICAL(&motionMux);
  const long position = stepper.currentPosition();
  portEXIT_CRITICAL(&motionMux);
  return position;
}

bool motorIsRunning() {
  portENTER_CRITICAL(&motionMux);
  const bool running = stepper.isRunning();
  portEXIT_CRITICAL(&motionMux);
  return running;
}

float currentPositionMm() {
  const float turns =
      static_cast<float>(currentStepperPosition()) / OUTPUT_STEPS_PER_REV;
  return turns * SPINDLE_PITCH_MM_PER_REV;
}

float rpmToFeedMmPerMin(float rpm) {
  return rpm * SPINDLE_PITCH_MM_PER_REV;
}

float feedMmPerMinToRpm(float mmPerMin) {
  return mmPerMin / SPINDLE_PITCH_MM_PER_REV;
}

const __FlashStringHelper *testModeLabel() {
  switch (activeTestMode) {
    case TestMode::PullUntilBreak:
      return F("Ziehen bis Bruch");
    case TestMode::HoldForce:
      return F("Kraft halten");
    case TestMode::Manual:
    default:
      return F("Manuell");
  }
}

void applyMotionSettings() {
  portENTER_CRITICAL(&motionMux);
  stepper.setMaxSpeed(outputRpmToStepsPerSecond(configuredOutputRpm));
  stepper.setAcceleration(
      outputAccelToStepsPerSecondSq(configuredOutputAccelRpmPerS));
  portEXIT_CRITICAL(&motionMux);
}

void enableDriver() {
  portENTER_CRITICAL(&motionMux);
  digitalWrite(EN_PIN, LOW);
  stepper.enableOutputs();
  driverEnabled = true;
  portEXIT_CRITICAL(&motionMux);
}

void disableDriver() {
  portENTER_CRITICAL(&motionMux);
  stepper.stop();
  stepper.disableOutputs();
  digitalWrite(EN_PIN, HIGH);
  driverEnabled = false;
  portEXIT_CRITICAL(&motionMux);
}

void printPosition() {
  Serial.print("Position [Schritte]: ");
  Serial.print(currentStepperPosition());
  Serial.print(" | Weg [mm]: ");
  Serial.print(currentPositionMm(), 3);
  Serial.print(" | Kraft [N]: ");
  Serial.println(currentForceNewton, 2);
}

void updateForceMeasurement() {
  if (!loadcellReady) {
    return;
  }

  if (millis() - lastHx711SampleMs < HX711_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastHx711SampleMs = millis();

  if (!scale.is_ready()) {
    return;
  }

  const long rawReading = scale.read();
  const float massGrams = (static_cast<float>(rawReading) - scale.get_offset()) /
                          scale.get_scale();
  const float measuredForceNewton = (massGrams / 1000.0f) * GRAVITY;
  currentForceNewton +=
      (measuredForceNewton - currentForceNewton) * FORCE_FILTER_ALPHA;
}

void tareLoadCell() {
  if (!loadcellReady) {
    return;
  }

  scale.tare(1);
  currentForceNewton = 0.0f;
}

bool parseValueAfterCommand(const String &command, const char *prefix,
                            float &value) {
  if (!command.startsWith(prefix)) {
    return false;
  }

  String number = command.substring(strlen(prefix));
  number.trim();
  number.replace(',', '.');
  if (number.isEmpty()) {
    return false;
  }

  value = number.toFloat();
  return true;
}

void startMoveTo(long targetSteps) {
  activeTestMode = TestMode::Manual;
  enableDriver();
  portENTER_CRITICAL(&motionMux);
  stepper.moveTo(targetSteps);
  portEXIT_CRITICAL(&motionMux);
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      String response;
      const bool ok = executeCommand(serialBuffer, response);
      Serial.println(response);
      if (!ok) {
        Serial.println(
            "Verfuegbare Kommandos: en, off, zero, tare, force, rpm, acc, "
            "feed, move, turn, stop");
      }
      serialBuffer = "";
      continue;
    }

    serialBuffer += incoming;
  }
}

String statusJson() {
  String json = "{";
  json += "\"driverEnabled\":";
  json += driverEnabled ? "true" : "false";
  json += ",\"running\":";
  json += motorIsRunning() ? "true" : "false";
  json += ",\"rpm\":";
  json += String(configuredOutputRpm, 2);
  json += ",\"acc\":";
  json += String(configuredOutputAccelRpmPerS, 2);
  json += ",\"feedMmPerMin\":";
  json += String(rpmToFeedMmPerMin(configuredOutputRpm), 2);
  json += ",\"positionMm\":";
  json += String(currentPositionMm(), 3);
  json += ",\"steps\":";
  json += String(currentStepperPosition());
  json += ",\"forceNewton\":";
  json += String(currentForceNewton, 2);
  json += ",\"testMode\":\"";
  json += String(testModeLabel());
  json += "\"";
  json += ",\"network\":\"SSID ";
  json += WIFI_SSID;
  json += " | IP ";
  json += WiFi.softAPIP().toString();
  json += "\"}";
  return json;
}

String jsonMessage(bool ok, const String &message) {
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

bool executeCommand(String command, String &response) {
  command.trim();
  if (command.isEmpty()) {
    response = "Leeres Kommando.";
    return false;
  }

  if (command == "en") {
    enableDriver();
    response = "Beide Treiber aktiviert.";
    return true;
  }

  if (command == "off") {
    disableDriver();
    response = "Beide Treiber deaktiviert.";
    return true;
  }

  if (command == "zero") {
    portENTER_CRITICAL(&motionMux);
    stepper.setCurrentPosition(0);
    portEXIT_CRITICAL(&motionMux);
    response = "Achsposition auf 0 gesetzt.";
    return true;
  }

  if (command == "tare") {
    tareLoadCell();
    response = loadcellReady ? "Waegezelle tariert." : "HX711 nicht bereit.";
    return loadcellReady;
  }

  if (command == "force") {
    response = "Kraft aktuell: " + String(currentForceNewton, 2) + " N.";
    return true;
  }

  if (command == "stop") {
    activeTestMode = TestMode::Manual;
    portENTER_CRITICAL(&motionMux);
    stepper.stop();
    portEXIT_CRITICAL(&motionMux);
    response = "Stopp angefordert.";
    return true;
  }

  if (command == "pos") {
    printPosition();
    response = "Position im Serial Monitor ausgegeben.";
    return true;
  }

  float value = 0.0f;

  if (parseValueAfterCommand(command, "rpm", value)) {
    if (value <= 0.0f) {
      response = "RPM muss groesser als 0 sein.";
      return false;
    }

    configuredOutputRpm = value;
    applyMotionSettings();
    response = "Drehzahl gesetzt auf " + String(configuredOutputRpm, 2) +
               " U/min.";
    return true;
  }

  if (parseValueAfterCommand(command, "acc", value)) {
    if (value <= 0.0f) {
      response = "ACC muss groesser als 0 sein.";
      return false;
    }

    configuredOutputAccelRpmPerS = value;
    applyMotionSettings();
    response = "Beschleunigung gesetzt auf " +
               String(configuredOutputAccelRpmPerS, 2) + " U/min/s.";
    return true;
  }

  if (parseValueAfterCommand(command, "feed", value)) {
    if (value <= 0.0f) {
      response = "Vorschub muss groesser als 0 sein.";
      return false;
    }

    configuredOutputRpm = feedMmPerMinToRpm(value);
    applyMotionSettings();
    response = "Vorschub gesetzt auf " + String(value, 2) + " mm/min.";
    return true;
  }

  if (parseValueAfterCommand(command, "move", value)) {
    startMoveTo(currentStepperPosition() + spindleMmToSteps(value));
    response = "Relativbewegung gestartet: " + String(value, 3) + " mm.";
    return true;
  }

  if (parseValueAfterCommand(command, "turn", value)) {
    startMoveTo(currentStepperPosition() + outputTurnsToSteps(value));
    response = "Relativbewegung gestartet: " + String(value, 3) +
               " Umdrehungen.";
    return true;
  }

  response = "Unbekanntes Kommando.";
  return false;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleStatus() {
  server.send(200, "application/json", statusJson());
}

void handleCommandApi() {
  if (!server.hasArg("cmd")) {
    server.send(400, "application/json",
                jsonMessage(false, "Parameter 'cmd' fehlt."));
    return;
  }

  String response;
  const String command = server.arg("cmd");
  const bool ok = executeCommand(command, response);
  Serial.print("Web-Kommando: ");
  Serial.println(command);
  Serial.println(response);
  server.send(ok ? 200 : 400, "application/json", jsonMessage(ok, response));
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/command", HTTP_GET, handleCommandApi);
  server.begin();
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("WLAN Access Point gestartet: ");
  Serial.println(WIFI_SSID);
  Serial.print("Passwort: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Weboberflaeche: http://");
  Serial.println(WiFi.softAPIP());
}

void setupLoadCell() {
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  if (!scale.wait_ready_timeout(2000)) {
    Serial.println("HX711 nicht bereit. Kraftmessung bleibt deaktiviert.");
    loadcellReady = false;
    return;
  }

  scale.set_scale(LOADCELL_CALIBRATION_FACTOR);
  scale.tare(1);
  loadcellReady = true;
  Serial.println("HX711 bereit. Tara gesetzt.");
}

void motorTask(void *parameter) {
  uint8_t loopCounter = 0;

  for (;;) {
    portENTER_CRITICAL(&motionMux);
    if (driverEnabled) {
      stepper.run();
    }
    portEXIT_CRITICAL(&motionMux);

    delayMicroseconds(MOTOR_TASK_DELAY_US);

    loopCounter++;
    if (loopCounter >= MOTOR_TASK_YIELD_INTERVAL) {
      loopCounter = 0;
      vTaskDelay(1);
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  stepper.setEnablePin(EN_PIN);
  stepper.setPinsInverted(false, false, true);
  stepper.disableOutputs();
  configuredOutputRpm = feedMmPerMinToRpm(DEFAULT_MOVE_MM_PER_MIN);
  applyMotionSettings();

  delay(500);
  Serial.println();
  Serial.println("ESP32 Pruefmaschinen-Steuerung gestartet.");
  Serial.println("Zwei Motoren laufen synchron ueber dieselben STEP/DIR/EN-Signale.");

  setupLoadCell();
  setupWiFi();
  setupWebServer();

  xTaskCreatePinnedToCore(motorTask, "motorTask", 4096, nullptr, 2,
                          &motorTaskHandle, 0);
}

void loop() {
  server.handleClient();
  updateForceMeasurement();
  handleSerialInput();

  const unsigned long now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    if (driverEnabled || loadcellReady) {
      printPosition();
    }
  }
}

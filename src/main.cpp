#include <Arduino.h>
#include <AccelStepper.h>
#include <HX711.h>
#include <Preferences.h>
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
constexpr float DEFAULT_MOVE_MM_PER_MIN = 12.0f;
constexpr float MIN_MOVE_MM_PER_MIN = 0.5f;
constexpr float MAX_MOVE_MM_PER_MIN = 60.0f;
constexpr float SPINDLE_PITCH_MM_PER_REV = 2.0f;
constexpr unsigned long STATUS_INTERVAL_MS = 500;
constexpr unsigned long HX711_SAMPLE_INTERVAL_MS = 100;
constexpr float GRAVITY = 9.80665f;
constexpr float FORCE_FILTER_ALPHA = 0.18f;
constexpr uint16_t MOTOR_TASK_DELAY_US = 200;
constexpr uint8_t MOTOR_TASK_YIELD_INTERVAL = 20;
constexpr float JOG_TRAVEL_MM = 1000.0f;

constexpr char WIFI_SSID[] = "ESP32-Testmaschine";
constexpr char WIFI_PASSWORD[] = "testmaschine";

constexpr char PREFERENCES_NAMESPACE[] = "testmachine";
constexpr char PREF_CALIBRATION_VALID[] = "lc_valid";
constexpr char PREF_CALIBRATION_FACTOR[] = "lc_factor";
constexpr float DEFAULT_LOADCELL_CALIBRATION_FACTOR = 1.0f;
constexpr uint8_t CALIBRATION_CAPTURE_SAMPLES = 10;
constexpr unsigned long CALIBRATION_SETTLE_MS = 1200;

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
    .jog {
      display: grid;
      gap: 10px;
      justify-items: center;
      margin-bottom: 14px;
    }
    .jog-mid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 72px));
      gap: 10px;
      align-items: center;
    }
    .arrow {
      width: 72px;
      height: 72px;
      border-radius: 18px;
      font-size: 2rem;
      line-height: 1;
      padding: 0;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      user-select: none;
      touch-action: manipulation;
    }
    .speed-inline {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 10px;
      align-items: end;
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
    <h1>JanTec Zugversuch Steuerung</h1>

    <div class="grid">
      <section class="card plot-card">
        <h2>Kraftverlauf</h2>
        <canvas id="forceChart"></canvas>
        <p class="note">Live-Plot der gemessenen Kraft mit 10 Samples pro Sekunde. </p>
      </section>

      <section class="card">
        <h2>Steuerung</h2>
        <div class="jog">
          <button class="arrow success jog-btn" data-jog="up" type="button">&#9650;</button>
          <div class="jog-mid">
            <div></div>
            <button class="danger" data-cmd="stop" type="button">Stop</button>
            <div></div>
          </div>
          <button class="arrow secondary jog-btn" data-jog="down" type="button">&#9660;</button>
        </div>
        <form data-prefix="feed" class="speed-form">
          <div class="speed-inline">
            <input id="feedInput" name="value" type="number" step="0.1" min="0.5" max="60" placeholder="Vorschub mm/min">
            <button type="submit">Setzen</button>
          </div>
        </form>
        <p class="note">Gedrückt halten für kontinuierliche Bewegung. 1 Umdrehung der Spindel = 2 mm Weg.</p>
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
          <button class="success" data-cmd="en">Treiber EN</button>
          <button class="secondary" data-cmd="off">Treiber DIS</button>
          <button data-cmd="zero">Null Position</button>
          <button class="secondary" data-cmd="tare">Kraft Tarieren</button>
          <button class="danger" data-cmd="stop">Stop</button>
          <button class="secondary" data-cmd="force">Kraft loggen</button>
        </div>
      </section>

      <section class="card">
        <h2>Geschwindigkeit</h2>
        <form data-prefix="acc">
          <input name="value" type="number" step="0.1" min="0.1" placeholder="Beschleunigung in U/min/s">
          <button type="submit">ACC setzen</button>
        </form>
        <p class="note">Empfohlener Arbeitsbereich aktuell: ca. 0.5 bis 60 mm/min. Das entspricht bei der 2-mm-Spindel 0.25 bis 30 U/min an der Getriebe-Ausgangswelle.</p>
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

      <section class="card plot-card">
        <h2>Kalibrierung Waegezelle</h2>
        <div class="btnrow">
          <button id="calibrationZeroBtn" class="secondary" type="button">1. Unbelastet bestaetigen</button>
          <button id="calibrationMeasureBtn" class="success" type="button">2. Gewicht messen</button>
        </div>
        <form id="calibrationForm">
          <input id="calibrationMassInput" name="value" type="number" step="0.1" min="0.1" placeholder="Bekanntes Gewicht in Gramm">
        </form>
        <p class="note">Ablauf: Waegezelle entlasten und Schritt 1 ausfuehren. Danach bekanntes Gewicht einhaengen, Grammwert eintragen und Schritt 2 ausfuehren. Der Kalibrierfaktor wird im ESP gespeichert und beim naechsten Start automatisch geladen.</p>
        <p class="note" id="calibrationInfo">Kalibrierstatus wird geladen...</p>
      </section>

    </div>

    <div id="message"></div>
  </div>

  <script> <!-- Plotting (Kraft-Graph) -->
    const sampleRateHz = 10;
    const maxSamples = 600;
    const fixedYAxisMin = -50;
    const fixedYAxisMax = 400;
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
      wifiInfo: document.getElementById("wifiInfo"),
      feedInput: document.getElementById("feedInput"),
      calibrationInfo: document.getElementById("calibrationInfo"),
      calibrationMassInput: document.getElementById("calibrationMassInput")
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

      ctx.strokeStyle = "#2b8bd9";
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
        statusFields.calibrationInfo.textContent = data.calibrationValid
          ? "Kalibrierung aktiv. Faktor: " + Number(data.calibrationFactor).toFixed(6)
          : "Noch keine gueltige Kalibrierung gespeichert.";
        if (document.activeElement !== statusFields.feedInput) {
          statusFields.feedInput.value = Number(data.feedMmPerMin).toFixed(2);
        }

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

    function bindJogButton(button) {
      const direction = button.dataset.jog;
      const startCommand = direction === "up" ? "jog up" : "jog down";
      let active = false;

      const startJog = (event) => {
        event.preventDefault();
        if (active) {
          return;
        }
        active = true;
        sendCommand(startCommand);
      };

      const stopJog = (event) => {
        if (event) {
          event.preventDefault();
        }
        if (!active) {
          return;
        }
        active = false;
        sendCommand("stop");
      };

      button.addEventListener("pointerdown", startJog);
      button.addEventListener("pointerup", stopJog);
      button.addEventListener("pointerleave", stopJog);
      button.addEventListener("pointercancel", stopJog);
    }

    document.querySelectorAll(".jog-btn").forEach(bindJogButton);

    document.getElementById("calibrationZeroBtn").addEventListener("click", () => {
      sendCommand("cal_zero");
    });

    document.getElementById("calibrationMeasureBtn").addEventListener("click", () => {
      const grams = statusFields.calibrationMassInput.value.trim();
      if (!grams) {
        setMessage("Bitte zuerst das bekannte Gewicht in Gramm eingeben.", true);
        return;
      }
      sendCommand("cal_apply " + grams);
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
Preferences preferences;
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
bool calibrationValid = false;
float loadcellCalibrationFactor = DEFAULT_LOADCELL_CALIBRATION_FACTOR;
bool calibrationZeroCaptured = false;
TestMode activeTestMode = TestMode::Manual;
String serialBuffer;

bool executeCommand(String command, String &response);

void loadCalibrationFromPreferences() {
  preferences.begin(PREFERENCES_NAMESPACE, true);
  calibrationValid = preferences.getBool(PREF_CALIBRATION_VALID, false);
  loadcellCalibrationFactor = preferences.getFloat(
      PREF_CALIBRATION_FACTOR, DEFAULT_LOADCELL_CALIBRATION_FACTOR);
  preferences.end();

  if (!calibrationValid || loadcellCalibrationFactor == 0.0f) {
    calibrationValid = false;
    loadcellCalibrationFactor = DEFAULT_LOADCELL_CALIBRATION_FACTOR;
  }
}

void saveCalibrationToPreferences(float factor) {
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putFloat(PREF_CALIBRATION_FACTOR, factor);
  preferences.putBool(PREF_CALIBRATION_VALID, true);
  preferences.end();

  loadcellCalibrationFactor = factor;
  calibrationValid = true;
}

bool readAverageRawValue(uint8_t sampleCount, long &averageRaw) {
  if (!loadcellReady) {
    return false;
  }

  int64_t sum = 0;
  for (uint8_t i = 0; i < sampleCount; ++i) {
    if (!scale.wait_ready_timeout(1500)) {
      return false;
    }
    sum += scale.read();
  }

  averageRaw = static_cast<long>(sum / sampleCount);
  return true;
}

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

float clampFeedMmPerMin(float mmPerMin) {
  if (mmPerMin < MIN_MOVE_MM_PER_MIN) {
    return MIN_MOVE_MM_PER_MIN;
  }
  if (mmPerMin > MAX_MOVE_MM_PER_MIN) {
    return MAX_MOVE_MM_PER_MIN;
  }
  return mmPerMin;
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
  json += ",\"calibrationValid\":";
  json += calibrationValid ? "true" : "false";
  json += ",\"calibrationFactor\":";
  json += String(loadcellCalibrationFactor, 6);
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

  float value = 0.0f;

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

  if (command == "cal_zero") {
    if (!loadcellReady) {
      response = "HX711 nicht bereit.";
      return false;
    }

    scale.set_scale(loadcellCalibrationFactor);
    scale.tare(1);
    calibrationZeroCaptured = true;
    currentForceNewton = 0.0f;
    response = "Nullpunkt erfasst. Jetzt bekanntes Gewicht anhaengen, Grammwert eingeben und Schritt 2 ausfuehren.";
    return true;
  }

  if (parseValueAfterCommand(command, "cal_apply", value)) {
    if (!loadcellReady) {
      response = "HX711 nicht bereit.";
      return false;
    }
    if (!calibrationZeroCaptured) {
      response = "Bitte zuerst Schritt 1 mit unbelasteter Waegezelle ausfuehren.";
      return false;
    }
    if (value <= 0.0f) {
      response = "Kalibriergewicht muss groesser als 0 g sein.";
      return false;
    }

    delay(CALIBRATION_SETTLE_MS);

    long averageRaw = 0;
    if (!readAverageRawValue(CALIBRATION_CAPTURE_SAMPLES, averageRaw)) {
      response = "Kalibrierwert konnte nicht gelesen werden.";
      return false;
    }

    const long offset = scale.get_offset();
    const float newFactor =
        static_cast<float>(averageRaw - offset) / value;

    if (newFactor == 0.0f) {
      response = "Kalibrierung fehlgeschlagen: Faktor ist 0.";
      return false;
    }

    saveCalibrationToPreferences(newFactor);
    scale.set_scale(loadcellCalibrationFactor);
    currentForceNewton = 0.0f;
    calibrationZeroCaptured = false;
    response = "Kalibrierung gespeichert. Neuer Faktor: " +
               String(loadcellCalibrationFactor, 6);
    return true;
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

    const float clampedFeed = clampFeedMmPerMin(value);
    configuredOutputRpm = feedMmPerMinToRpm(clampedFeed);
    applyMotionSettings();
    response = "Vorschub gesetzt auf " + String(clampedFeed, 2) + " mm/min.";
    return true;
  }

  if (command == "jog up") {
    startMoveTo(currentStepperPosition() + spindleMmToSteps(JOG_TRAVEL_MM));
    response = "Jog nach oben gestartet.";
    return true;
  }

  if (command == "jog down") {
    startMoveTo(currentStepperPosition() - spindleMmToSteps(JOG_TRAVEL_MM));
    response = "Jog nach unten gestartet.";
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

  loadCalibrationFromPreferences();
  scale.set_scale(loadcellCalibrationFactor);
  scale.tare(1);
  loadcellReady = true;
  if (calibrationValid) {
    Serial.print("HX711 bereit. Kalibrierfaktor geladen: ");
    Serial.println(loadcellCalibrationFactor, 6);
  } else {
    Serial.println("HX711 bereit. Noch keine gueltige Kalibrierung gespeichert.");
  }
  Serial.println("Tara gesetzt.");
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

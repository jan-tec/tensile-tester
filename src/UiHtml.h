// Web UI template (M0-T009, SC-001).
//
// Pure data: the PROGMEM HTML asset served on "/", split out of
// WebUi.cpp so the module logic stays under the 350-line limit.
// Included by WebUi.cpp only; const gives it internal linkage.
#pragma once

// PROGMEM only exists inside the Arduino build; the host test (test/) includes
// this asset to assert on what the browser actually loads, same pure-header
// pattern as ErrorCode.h / Text.h.
#if defined(ARDUINO)
#include <Arduino.h>
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20viewBox='0%200%2032%2032'%3E%3Crect%20width='32'%20height='32'%20rx='6'%20fill='%232b8bd9'/%3E%3Cpath%20d='M9%2010h14L11%2022h10'%20fill='none'%20stroke='%23fff'%20stroke-width='3'%20stroke-linecap='round'/%3E%3C/svg%3E">
  <title data-i18n="title">Test machine</title>
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
    .checks {
      list-style: none;
      margin: 12px 0 0;
      padding: 0;
    }
    .checks li {
      display: flex;
      justify-content: space-between;
      gap: 8px;
      padding: 6px 0;
      border-top: 1px solid var(--line);
      font-size: 0.9rem;
    }
    .checks .cid {
      font-weight: 700;
    }
    .ok { color: var(--success); }
    .warn { color: var(--accent); }
    .fail { color: var(--danger); }
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
    .langs { display: inline-flex; gap: 6px; margin-bottom: 12px; }
    .langs button { padding: 6px 12px; font-size: 0.85rem; background: #74695f; opacity: 0.6; }
    .langs button[aria-pressed="true"] { background: var(--accent); opacity: 1; }
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
    <h1 data-i18n="h1">JanTec Tensile Test Control</h1>
    <div class="langs" role="group" aria-label="Language">
      <button type="button" data-lang="en" aria-pressed="true">EN</button>
      <button type="button" data-lang="de" aria-pressed="false">DE</button>
    </div>

    <div class="grid">
      <section class="card plot-card">
        <h2 data-i18n="headPlot">Force curve</h2>
        <canvas id="forceChart"></canvas>
        <p class="note" data-i18n="plotNote">Live plot of the measured force at 10 samples per second.</p>
      </section>

      <section class="card">
        <h2 data-i18n="headControls">Controls</h2>
        <div class="jog">
          <button class="arrow success jog-btn" data-jog="up" type="button" data-i18n-al="jogUpAl" aria-label="Jog up (hold button)">&#9650;</button>
          <div class="jog-mid">
            <div></div>
            <button class="danger" data-cmd="stop" type="button" data-i18n="btnStop">Stop</button>
            <div></div>
          </div>
          <button class="arrow secondary jog-btn" data-jog="down" type="button" data-i18n-al="jogDownAl" aria-label="Jog down (hold button)">&#9660;</button>
        </div>
        <form data-prefix="feed" class="speed-form">
          <div class="speed-inline">
            <input id="feedInput" name="value" type="number" step="0.1" min="0.5" max="60" data-i18n-ph="phFeed" placeholder="Feed mm/min">
            <button type="submit" data-i18n="btnSet">Set</button>
          </div>
        </form>
        <p class="note" data-i18n="jogNote">Hold to move continuously. 1 spindle revolution = 2 mm travel.</p>
      </section>

      <section class="card">
        <h2>Status</h2>
        <div class="status">
          <div class="pill"><span class="label" data-i18n="lblDriver">Drivers</span><span class="value" id="driverState">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblMotion">Motion</span><span class="value" id="motionState">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblForce">Force</span><span class="value" id="forceValue">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblPosition">Position</span><span class="value" id="positionValue">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblFeed">Feed</span><span class="value" id="feedValue">-</span></div>
        </div>
        <p class="note" id="wifiInfo" data-i18n="wifiInfo">Connect to the ESP32 network</p>
      </section>

      <section class="card">
        <h2 data-i18n="headSelftest">Selftest</h2>
        <div class="status">
          <div class="pill"><span class="label" data-i18n="lblOverall">Overall</span><span class="value" id="selftestOverall">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblState">State</span><span class="value" id="deviceState">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblFaultCode">Fault code</span><span class="value" id="faultCodeValue">-</span></div>
        </div>
        <ul class="checks" id="selftestChecks"></ul>
        <p class="note" id="selftestNote" data-i18n="selftestNote">Loading startup selftest...</p>
      </section>

      <section class="card">
        <h2 data-i18n="headAxis">Axis</h2>
        <div class="btnrow">
          <button class="success" data-cmd="en" data-i18n="btnEnable">Enable drivers</button>
          <button class="secondary" data-cmd="off" data-i18n="btnDisable">Disable drivers</button>
          <button data-cmd="zero" data-i18n="btnZero">Zero position</button>
          <button class="secondary" data-cmd="tare" data-i18n="btnTare">Tare force</button>
          <button class="danger" data-cmd="stop" data-i18n="btnStop">Stop</button>
          <button class="secondary" data-cmd="force" data-i18n="btnForce">Log force</button>
        </div>
      </section>

      <section class="card">
        <h2 data-i18n="headSpeed">Speed</h2>
        <form data-prefix="acc">
          <input name="value" type="number" step="0.1" min="0.1" data-i18n-ph="phAcc" placeholder="Acceleration in rpm/s">
          <button type="submit" data-i18n="btnAcc">Set ACC</button>
        </form>
        <p class="note" data-i18n="speedNote">Recommended working range: about 0.5 to 60 mm/min. With the 2 mm spindle that is 0.25 to 30 rpm at the gearbox output.</p>
      </section>

      <section class="card">
        <h2 data-i18n="headMove">Motion</h2>
        <form data-prefix="move">
          <input name="value" type="number" step="0.01" data-i18n-ph="phMove" placeholder="Relative travel in mm">
          <button type="submit" data-i18n="btnMove">Move mm</button>
        </form>
        <form data-prefix="turn">
          <input name="value" type="number" step="0.01" data-i18n-ph="phTurn" placeholder="Relative revolutions">
          <button type="submit" data-i18n="btnTurn">Move turns</button>
        </form>
      </section>

      <section class="card plot-card">
        <h2 data-i18n="headCal">Load cell calibration</h2>
        <div class="btnrow">
          <button id="calibrationZeroBtn" class="secondary" type="button" data-i18n="btnCal1">1. Confirm unloaded</button>
          <button id="calibrationMeasureBtn" class="success" type="button" data-i18n="btnCal2">2. Measure weight</button>
        </div>
        <form id="calibrationForm">
          <input id="calibrationMassInput" name="value" type="number" step="0.1" min="0.1" data-i18n-ph="phCal" placeholder="Known weight in grams">
        </form>
        <p class="note" data-i18n="calNote">Procedure: unload the load cell and run step 1. Then attach a known weight, enter its gram value and run step 2. The calibration factor is stored on the ESP and loaded automatically at the next start.</p>
        <p class="note" id="calibrationInfo" data-i18n="calInfo">Loading calibration status...</p>
      </section>

    </div>

    <div id="message" aria-live="polite"></div>
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

    // M2-T005 (FR-005, plan D3): client-side locale dictionary. The static
    // markup ships English (factory default, SC-001); /api/status carries the
    // server locale, and the EN/DE selector switches both surfaces through
    // the same `lang <en|de>` command the serial CLI uses. DE uses native
    // umlauts (UTF-8, D5) - unlike the ASCII-transcribed serial column.
    const I18N = {
      en: {
        "title": "Test machine",
        "h1": "JanTec Tensile Test Control",
        "headPlot": "Force curve",
        "plotNote": "Live plot of the measured force at 10 samples per second.",
        "headControls": "Controls",
        "jogUpAl": "Jog up (hold button)",
        "jogDownAl": "Jog down (hold button)",
        "btnStop": "Stop",
        "phFeed": "Feed mm/min",
        "btnSet": "Set",
        "jogNote": "Hold to move continuously. 1 spindle revolution = 2 mm travel.",
        "headStatus": "Status",
        "lblDriver": "Drivers", "lblMotion": "Motion", "lblForce": "Force",
        "lblPosition": "Position", "lblFeed": "Feed",
        "wifiInfo": "Connect to the ESP32 network",
        "headSelftest": "Selftest",
        "lblOverall": "Overall", "lblState": "State", "lblFaultCode": "Fault code",
        "selftestNote": "Loading startup selftest...",
        "headAxis": "Axis",
        "btnEnable": "Enable drivers", "btnDisable": "Disable drivers",
        "btnZero": "Zero position", "btnTare": "Tare force",
        "btnForce": "Log force",
        "headSpeed": "Speed", "phAcc": "Acceleration in rpm/s",
        "btnAcc": "Set ACC",
        "speedNote": "Recommended working range: about 0.5 to 60 mm/min. With the 2 mm spindle that is 0.25 to 30 rpm at the gearbox output.",
        "headMove": "Motion", "phMove": "Relative travel in mm",
        "btnMove": "Move mm", "phTurn": "Relative revolutions",
        "btnTurn": "Move turns",
        "headCal": "Load cell calibration",
        "btnCal1": "1. Confirm unloaded", "btnCal2": "2. Measure weight",
        "phCal": "Known weight in grams",
        "calNote": "Procedure: unload the load cell and run step 1. Then attach a known weight, enter its gram value and run step 2. The calibration factor is stored on the ESP and loaded automatically at the next start.",
        "calInfo": "Loading calibration status...",
        "driverOn": "active", "driverOff": "off",
        "motionRun": "running", "motionIdle": "idle",
        "stOk": "OK", "stWarn": "warning", "stFail": "fault",
        "stNoConn": "not connected", "stNoWired": "not wired",
        "stateIdle": "ready", "stateError": "error", "faultNone": "none",
        "noteError": "Error state: motion commands locked, reboot required.",
        "noteWarn": "Warnings from the startup selftest, machine operational.",
        "noteOk": "Startup selftest, nothing notable.",
        "calActive": "Calibration active. Factor: ",
        "calNone": "No valid calibration stored yet.",
        "errNoConn": "No connection to the ESP32.",
        "errNoStatus": "Status could not be loaded.",
        "errValue": "Please enter a value.",
        "errWeight": "Enter the known weight in grams first."
      },
      de: {
        "title": "Testmaschine",
        "h1": "JanTec Zugversuch Steuerung",
        "headPlot": "Kraftverlauf",
        "plotNote": "Live-Plot der gemessenen Kraft mit 10 Samples pro Sekunde.",
        "headControls": "Steuerung",
        "jogUpAl": "Aufwärts joggen (Taste halten)",
        "jogDownAl": "Abwärts joggen (Taste halten)",
        "btnStop": "Stopp",
        "phFeed": "Vorschub mm/min",
        "btnSet": "Setzen",
        "jogNote": "Gedrückt halten für kontinuierliche Bewegung. 1 Umdrehung der Spindel = 2 mm Weg.",
        "headStatus": "Status",
        "lblDriver": "Treiber", "lblMotion": "Bewegung", "lblForce": "Kraft",
        "lblPosition": "Position", "lblFeed": "Vorschub",
        "wifiInfo": "Verbinde dich mit dem ESP32",
        "headSelftest": "Selbsttest",
        "lblOverall": "Gesamt", "lblState": "Zustand", "lblFaultCode": "Fehlercode",
        "selftestNote": "Startprüfung wird geladen...",
        "headAxis": "Achse",
        "btnEnable": "Treiber EN", "btnDisable": "Treiber DIS",
        "btnZero": "Null Position", "btnTare": "Kraft Tarieren",
        "btnForce": "Kraft loggen",
        "headSpeed": "Geschwindigkeit", "phAcc": "Beschleunigung in U/min/s",
        "btnAcc": "ACC setzen",
        "speedNote": "Empfohlener Arbeitsbereich aktuell: ca. 0.5 bis 60 mm/min. Das entspricht bei der 2-mm-Spindel 0.25 bis 30 U/min an der Getriebe-Ausgangswelle.",
        "headMove": "Bewegung", "phMove": "Relativweg in mm",
        "btnMove": "mm fahren", "phTurn": "Umdrehungen relativ",
        "btnTurn": "Turns fahren",
        "headCal": "Kalibrierung Wägezelle",
        "btnCal1": "1. Unbelastet bestätigen", "btnCal2": "2. Gewicht messen",
        "phCal": "Bekanntes Gewicht in Gramm",
        "calNote": "Ablauf: Wägezelle entlasten und Schritt 1 ausführen. Danach bekanntes Gewicht einhängen, Grammwert eintragen und Schritt 2 ausführen. Der Kalibrierfaktor wird im ESP gespeichert und beim nächsten Start automatisch geladen.",
        "calInfo": "Kalibrierstatus wird geladen...",
        "driverOn": "aktiv", "driverOff": "aus",
        "motionRun": "läuft", "motionIdle": "bereit",
        "stOk": "in Ordnung", "stWarn": "Warnung", "stFail": "Fehler",
        "stNoConn": "nicht verbunden", "stNoWired": "nicht verdrahtet",
        "stateIdle": "bereit", "stateError": "Fehler", "faultNone": "keiner",
        "noteError": "Fehlerzustand: Motorkommandos gesperrt, Reboot nötig.",
        "noteWarn": "Warnungen aus der Startprüfung, Maschine betriebsbereit.",
        "noteOk": "Startprüfung ohne Auffällige.",
        "calActive": "Kalibrierung aktiv. Faktor: ",
        "calNone": "Noch keine gültige Kalibrierung gespeichert.",
        "errNoConn": "Keine Verbindung zum ESP32.",
        "errNoStatus": "Status kann nicht geladen werden.",
        "errValue": "Bitte einen Wert eingeben.",
        "errWeight": "Bitte zuerst das bekannte Gewicht in Gramm eingeben."
      }
    };
    let locale = "en";
    function t(key) {
      const dict = I18N[locale] || I18N.en;
      return dict[key] !== undefined ? dict[key] : I18N.en[key];
    }
    function applyLocale(next) {
      locale = next === "de" ? "de" : "en";  // unknown values fall back to EN
      document.documentElement.lang = locale;  // FR-005
      document.querySelectorAll("[data-i18n]").forEach((el) => {
        el.textContent = t(el.dataset.i18n);
      });
      document.querySelectorAll("[data-i18n-ph]").forEach((el) => {
        el.placeholder = t(el.dataset.i18nPh);
      });
      document.querySelectorAll("[data-i18n-al]").forEach((el) => {
        el.setAttribute("aria-label", t(el.dataset.i18nAl));
      });
      document.querySelectorAll("[data-lang]").forEach((btn) => {
        btn.setAttribute("aria-pressed", String(btn.dataset.lang === locale));
      });
    }
    // The last status poll failed. The pills keep showing the last received
    // values, so the message line is the only hint the operator gets - and it
    // has to disappear again once polling works: a single dropped request must
    // not leave "Status kann nicht geladen werden." on screen next to values
    // that keep updating.
    let statusUnavailable = false;

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
      forceValue: document.getElementById("forceValue"),
      positionValue: document.getElementById("positionValue"),
      feedValue: document.getElementById("feedValue"),
      wifiInfo: document.getElementById("wifiInfo"),
      feedInput: document.getElementById("feedInput"),
      calibrationInfo: document.getElementById("calibrationInfo"),
      calibrationMassInput: document.getElementById("calibrationMassInput"),
      selftestOverall: document.getElementById("selftestOverall"),
      deviceState: document.getElementById("deviceState"),
      faultCodeValue: document.getElementById("faultCodeValue"),
      selftestChecks: document.getElementById("selftestChecks"),
      selftestNote: document.getElementById("selftestNote")
    };

    let lastSelftestChecksJson = "";
    const selftestStatusLabel = {
      "OK": "stOk",
      "WARN": "stWarn",
      "FAIL": "stFail",
      "NOT_CONNECTED": "stNoConn",
      "NOT_WIRED": "stNoWired"
    };
    const selftestStatusClass = {
      "OK": "ok",
      "WARN": "warn",
      "FAIL": "fail",
      "NOT_CONNECTED": "warn",
      "NOT_WIRED": "warn"
    };

    function renderSelftest(data) {
      if (!data.selftest) {
        return;
      }
      const selftest = data.selftest;
      statusFields.selftestOverall.textContent =
        selftestStatusLabel[selftest.overall] ? t(selftestStatusLabel[selftest.overall]) : selftest.overall;
      statusFields.selftestOverall.className =
        "value " + (selftestStatusClass[selftest.overall] || "");

      const isError = data.state === "Error";
      statusFields.deviceState.textContent =
        isError ? t("stateError") : data.state === "Idle" ? t("stateIdle") : data.state;
      statusFields.deviceState.className = "value" + (isError ? " fail" : "");
      statusFields.faultCodeValue.textContent = data.faultCode
        ? "0x" + data.faultCode.toString(16).toUpperCase().padStart(4, "0")
        : t("faultNone");

      const checksJson = JSON.stringify(selftest.checks);
      if (checksJson !== lastSelftestChecksJson) {
        lastSelftestChecksJson = checksJson;
        statusFields.selftestChecks.innerHTML = "";
        selftest.checks.forEach((check) => {
          const row = document.createElement("li");
          const name = document.createElement("span");
          name.className = "cid";
          name.textContent = check.id;
          const detail = document.createElement("span");
          detail.textContent = selftestStatusLabel[check.status] ? t(selftestStatusLabel[check.status]) : check.status;
          detail.className = selftestStatusClass[check.status] || "";
          row.appendChild(name);
          row.appendChild(detail);
          statusFields.selftestChecks.appendChild(row);
        });
      }

      statusFields.selftestNote.textContent = isError
        ? (data.faultMessage || t("noteError"))  // localized detail from T004
        : selftest.overall === "WARN"
          ? t("noteWarn")
          : t("noteOk");
    }

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
        setMessage(t("errNoConn"), true);
      }
    }

    async function refreshStatus() {
      try {
        const response = await fetch("/api/status");
        const data = await response.json();
        if (data.locale && data.locale !== locale) {
          applyLocale(data.locale);  // serial `lang` flips the UI live
        }
        statusFields.driverState.textContent = data.driverEnabled ? t("driverOn") : t("driverOff");
        statusFields.motionState.textContent = data.running ? t("motionRun") : t("motionIdle");
        statusFields.forceValue.textContent = typeof data.forceNewton === "number"
          ? data.forceNewton.toFixed(2) + " N"
          : "n/a";
        statusFields.positionValue.textContent = Number(data.positionMm).toFixed(3) + " mm";
        statusFields.feedValue.textContent = Number(data.feedMmPerMin).toFixed(2) + " mm/min";
        statusFields.wifiInfo.textContent = data.network;
        statusFields.calibrationInfo.textContent = data.calibrationValid
          ? t("calActive") + Number(data.calibrationFactor).toFixed(6)
          : t("calNone");
        if (document.activeElement !== statusFields.feedInput) {
          statusFields.feedInput.value = Number(data.feedMmPerMin).toFixed(2);
        }
        renderSelftest(data);
        if (statusUnavailable) {
          statusUnavailable = false;
          messageEl.textContent = "";
        }

        if (typeof data.forceNewton === "number") {
          forceHistory.push(data.forceNewton);
        }
        while (forceHistory.length > maxSamples) {
          forceHistory.shift();
        }
        drawPlot();
      } catch (error) {
        statusUnavailable = true;
        setMessage(t("errNoStatus"), true);
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
          setMessage(t("errValue"), true);
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

      // Keyboard parity (WCAG 2.1.1): holding Enter/Space behaves like holding
      // the pointer down. Auto-repeat keydowns are ignored, so holding the key
      // sends one jog start instead of one per repeat.
      const isHoldKey = (key) => key === "Enter" || key === " " || key === "Spacebar";

      const keyDown = (event) => {
        if (!isHoldKey(event.key) || event.repeat) {
          return;
        }
        event.preventDefault();  // Space would scroll the page instead
        startJog(event);
      };

      const keyUp = (event) => {
        if (!isHoldKey(event.key)) {
          return;
        }
        stopJog(event);
      };

      // blur is the keyboard equivalent of pointerleave: if focus moves while a
      // key is still down the keyup never reaches the button, and the machine
      // would keep running unattended.
      button.addEventListener("keydown", keyDown);
      button.addEventListener("keyup", keyUp);
      button.addEventListener("blur", () => stopJog(null));
    }

    document.querySelectorAll(".jog-btn").forEach(bindJogButton);

    // Language selector (FR-003): instant client-side swap, then the server
    // persists via the shared `lang` command and echoes it in /api/status.
    document.querySelectorAll("[data-lang]").forEach((btn) => {
      btn.addEventListener("click", () => {
        applyLocale(btn.dataset.lang);
        sendCommand("lang " + btn.dataset.lang);
      });
    });

    document.getElementById("calibrationZeroBtn").addEventListener("click", () => {
      sendCommand("cal_zero");
    });

    document.getElementById("calibrationMeasureBtn").addEventListener("click", () => {
      const grams = statusFields.calibrationMassInput.value.trim();
      if (!grams) {
        setMessage(t("errWeight"), true);
        return;
      }
      sendCommand("cal_apply " + grams);
    });

    window.addEventListener("resize", resizeCanvas);
    resizeCanvas();
    applyLocale(locale);
    refreshStatus();
    setInterval(refreshStatus, 100);
  </script>
</body>
</html>
)HTML";

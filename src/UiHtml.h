// Web UI template (M0-T009, SC-001; M7 redesign: mono + single red accent,
// tear workflow US3 per specs/M7-web-ui-redesign/spec.md).
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

// M7 visual language (US2, owner decision 2026-10-11 #169): monochrome
// warm-free greys + exactly ONE accent - red, reserved for Stop/danger.
// Radius scale: 8 px everywhere, 999 px pill for the language toggles
// (taste N9). One global Stop in the sticky topbar (taste C1-analog).
// prefers-reduced-motion honoured (taste gate 4). Favicon is monochrome
// (taste gate 5). EN status words case-unified (taste T6).
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'%20viewBox='0%200%2032%2032'%3E%3Crect%20width='32'%20height='32'%20rx='6'%20fill='%231a1a1a'/%3E%3Cpath%20d='M7%2024c6-1%209-16%2018-16'%20fill='none'%20stroke='%23ffffff'%20stroke-width='3'%20stroke-linecap='round'/%3E%3C/svg%3E">
  <title data-i18n="title">Test machine</title>
  <style>
    :root {
      --bg: #f4f4f2;
      --panel: #ffffff;
      --line: #d9d9d6;
      --grid: #e5e5e2;
      --text: #1a1a1a;
      --muted: #6b6b68;
      --danger: #b3261e;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      color: var(--text);
      font-family: "Avenir Next", "Segoe UI", sans-serif;
      background: var(--bg);
      min-height: 100vh;
    }
    .wrap {
      max-width: 1100px;
      margin: 0 auto;
      padding: 0 16px 48px;
    }
    .topbar {
      position: sticky;
      top: 0;
      z-index: 1;
      background: var(--bg);
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 10px;
      padding: 12px 0;
      border-bottom: 1px solid var(--line);
    }
    h1 {
      margin: 0;
      font-size: 1.3rem;
      letter-spacing: 0.02em;
      flex: 1 1 auto;
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
      margin-top: 16px;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 18px;
    }
    .plot-card {
      grid-column: 1 / -1;
    }
    /* Operator request 2026-10-11: "Tensile test" and "Run curve" share
       one 50/50 row on desktop (they were stacked: the test card sat in
       a narrow auto-fit column looking shifted, the plot full-width
       below). Below 640 px they stack like everything else. */
    .row2 {
      grid-column: 1 / -1;
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
    }
    .row2 .plot-card {
      grid-column: auto;
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
      border-radius: 8px;
      padding: 11px 12px;
      background: var(--panel);
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
    /* M7: one accent. ok/warn read as weight in mono; only fail is red. */
    .ok { font-weight: 700; }
    .warn { color: var(--muted); font-style: italic; }
    .fail { color: var(--danger); }
    .btnrow {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
      margin-top: 14px;  /* clear spacing to the pills/summary above */
    }
    .btnrow3 {
      grid-template-columns: repeat(3, minmax(0, 1fr));
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
      border-radius: 8px;
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
      border-radius: 8px;
      border: 1px solid var(--line);
      background: var(--panel);
      font-size: 1rem;
      color: var(--text);
    }
    button {
      border: 0;
      border-radius: 8px;
      padding: 12px 14px;
      font-size: 0.98rem;
      font-weight: 700;
      cursor: pointer;
      color: #ffffff;
      background: var(--text);
      transition: transform 120ms ease, background 120ms ease;
    }
    button:hover {
      transform: translateY(-1px);
      background: #333333;
    }
    button.secondary {
      background: var(--panel);
      color: var(--text);
      border: 1px solid var(--line);
    }
    button.secondary:hover {
      background: var(--bg);
    }
    button.danger {
      background: var(--danger);
    }
    button.danger:hover {
      background: #8f1d17;
    }
    a.btn {
      display: inline-block;
      border-radius: 8px;
      padding: 12px 14px;
      font-size: 0.98rem;
      font-weight: 700;
      cursor: pointer;
      text-decoration: none;
      text-align: center;
      color: var(--text);
      background: var(--panel);
      border: 1px solid var(--line);
    }
    .langs { display: inline-flex; gap: 6px; }
    .langs button {
      padding: 6px 12px;
      font-size: 0.85rem;
      border-radius: 999px;
      background: var(--panel);
      color: var(--muted);
      border: 1px solid var(--line);
    }
    .langs button:hover { background: var(--bg); transform: none; }
    .langs button[aria-pressed="true"] {
      background: var(--text);
      color: #ffffff;
      border-color: var(--text);
    }
    canvas {
      width: 100%;
      height: 260px;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
    }
    .note {
      margin-top: 10px;
      color: var(--muted);
      font-size: 0.92rem;
    }
    label {
      display: block;
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text);
    }
    #message {
      min-height: 24px;
      margin-top: 16px;
      font-weight: 700;
    }
    /* #95: stale-contact banner - shown only while body.stale is set;
       pill values dim (never blanked - the numeric text stays readable). */
    #staleBanner {
      display: none;
      border: 1px dashed var(--muted);
      padding: 6px 10px;
      margin: 8px 0 0 0;
    }
    body.stale #staleBanner {
      display: block;
    }
    body.stale .pill .value {
      opacity: 0.35;
    }
    /* #95: reboot announce - latched (survives poll cycles), cleared by
       the next operator action (sendCommand). */
    #rebootBanner[hidden] {
      display: none;
    }
    #rebootBanner {
      border: 1px solid var(--muted);
      padding: 6px 10px;
      margin: 8px 0 0 0;
      font-weight: 700;
    }
    @media (max-width: 640px) {
      .status, .btnrow { grid-template-columns: 1fr; }
      .row2 { grid-template-columns: 1fr; }
    }
    @media (prefers-reduced-motion: reduce) {
      button, a.btn { transition: none; }
      button:hover, a.btn:hover { transform: none; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="topbar">
      <h1 data-i18n="h1">JanTec ATLAS</h1>
      <div class="langs" role="group" aria-label="Language">
        <button type="button" data-lang="en" aria-pressed="true">EN</button>
        <button type="button" data-lang="de" aria-pressed="false">DE</button>
      </div>
      <button class="danger" data-cmd="stop" type="button" data-i18n="btnStop">Stop</button>
    </div>

    <div class="grid">
      <div class="row2">
      <section class="card">
        <h2 data-i18n="headTest">Tensile test</h2>
        <div class="status">
          <div class="pill"><span class="label" data-i18n="lblCycle">Cycle</span><span class="value" id="cycleState">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblDatum">Datum</span><span class="value" id="datumState">-</span></div>
        </div>
        <p class="note" id="samplePrompt" hidden data-i18n="samplePrompt">Datum reached. Clamp the sample, then start the test.</p>
        <div class="status" id="runSummary">
          <div class="pill"><span class="label" data-i18n="lblPeak">Peak force</span><span class="value" id="runPeak">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblBreak">Break displacement</span><span class="value" id="runBreak">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblSamples">Samples</span><span class="value" id="runSamples">-</span></div>
          <div class="pill"><span class="label" data-i18n="lblTrunc">Truncated</span><span class="value" id="runTrunc">-</span></div>
        </div>
        <div class="btnrow btnrow3">
          <button data-cmd="home" type="button" data-i18n="btnHome">Home</button>
          <button type="button" data-cmd="test" data-i18n="btnStartTest">Start test</button>
          <a class="btn" id="csvDownload" href="/api/data" download="tensile-run.csv" data-i18n="csvDownload">Download CSV</a>
        </div>
      </section>

      <section class="card plot-card">
        <h2 data-i18n="headRunCurve">Run curve (force-displacement)</h2>
        <canvas id="runChart"></canvas>
        <p class="note" id="curveNote" data-i18n="curveEmpty">No run data yet.</p>
        <p class="note"><button class="secondary" id="curveRefresh" type="button" data-i18n="btnCurve">Reload curve</button></p>
      </section>
      </div>

      <section class="card plot-card">
        <h2 data-i18n="headPlot">Force curve</h2>
        <canvas id="forceChart"></canvas>
        <p class="note" data-i18n="plotNote">Live plot of the measured force at 10 samples per second.</p>
      </section>

      <section class="card">
        <h2 data-i18n="headControls">Movement</h2>
        <div class="jog">
          <button class="arrow secondary jog-btn" data-jog="up" type="button" data-i18n-al="jogUpAl" aria-label="Jog up (hold button)">&#9650;</button>
          <div class="jog-mid">
            <div></div>
            <div></div>
            <div></div>
          </div>
          <button class="arrow secondary jog-btn" data-jog="down" type="button" data-i18n-al="jogDownAl" aria-label="Jog down (hold button)">&#9660;</button>
        </div>
        <form data-prefix="feed" class="speed-form">
          <label for="feedInput" data-i18n="inFeed">Feed (mm/min)</label>
          <div class="speed-inline">
            <input id="feedInput" name="value" type="number" step="0.1" min="0.5" max="30" data-i18n-ph="phFeed" placeholder="Feed mm/min">
            <button type="submit" data-i18n="btnSet">Set</button>
          </div>
        </form>
        <p class="note" data-i18n="jogNote">Hold to move continuously. 1 spindle revolution = 2 mm travel.</p>
      </section>

      <section class="card">
        <h2 data-i18n="headAxis">Axis</h2>
        <div class="btnrow">
          <button data-cmd="en" data-i18n="btnEnable">Enable drivers</button>
          <button class="secondary" data-cmd="off" data-i18n="btnDisable">Disable drivers</button>
          <button data-cmd="zero" data-i18n="btnZero">Zero position</button>
          <button class="secondary" data-cmd="tare" data-i18n="btnTare">Tare force</button>
          <button class="secondary" data-cmd="force" data-i18n="btnForce">Log force</button>
        </div>
      </section>

      <section class="card">
        <h2 data-i18n="headTestCfg">Test settings</h2>
        <form data-prefix="testspeed">
          <label for="testspeedInput" data-i18n="inTestSpeed">Test speed (mm/min)</label>
          <input id="testspeedInput" name="value" type="number" step="0.1" min="0.5" max="30" data-i18n-ph="phTestSpeed" placeholder="Test speed in mm/min">
          <button type="submit" data-i18n="btnSet">Set</button>
        </form>
        <form data-prefix="testarm">
          <label for="testarmInput" data-i18n="inTestArm">Arming force (N)</label>
          <input id="testarmInput" name="value" type="number" step="0.1" min="0.1" data-i18n-ph="phTestArm" placeholder="Arming force in N">
          <button type="submit" data-i18n="btnSet">Set</button>
        </form>
        <form data-prefix="testdrop">
          <label for="testdropInput" data-i18n="inTestDrop">Break drop (%)</label>
          <input id="testdropInput" name="value" type="number" step="1" min="10" max="90" data-i18n-ph="phTestDrop" placeholder="Break drop in %">
          <button type="submit" data-i18n="btnSet">Set</button>
        </form>
        <p class="note" data-i18n="testCfgNote">Stored on the ESP and applied from the next test run.</p>
      </section>

      <section class="card">
        <h2 data-i18n="headMove">Motion</h2>
        <form data-prefix="move">
          <label for="moveInput" data-i18n="inMove">Relative travel (mm)</label>
          <input id="moveInput" name="value" type="number" step="0.01" data-i18n-ph="phMove" placeholder="Relative travel in mm">
          <button type="submit" data-i18n="btnMove">Move mm</button>
        </form>
        <form data-prefix="turn">
          <label for="turnInput" data-i18n="inTurn">Relative revolutions</label>
          <input id="turnInput" name="value" type="number" step="0.01" data-i18n-ph="phTurn" placeholder="Relative revolutions">
          <button type="submit" data-i18n="btnTurn">Move turns</button>
        </form>
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

      <section class="card plot-card">
        <h2 data-i18n="headCal">Load cell calibration</h2>
        <div class="btnrow">
          <button id="calibrationZeroBtn" class="secondary" type="button" data-i18n="btnCal1">1. Confirm unloaded</button>
          <button id="calibrationMeasureBtn" type="button" data-i18n="btnCal2">2. Measure weight</button>
        </div>
        <form id="calibrationForm">
          <label for="calibrationMassInput" data-i18n="inCal">Known weight (grams)</label>
          <input id="calibrationMassInput" name="value" type="number" step="0.1" min="0.1" data-i18n-ph="phCal" placeholder="Known weight in grams">
        </form>
        <p class="note" data-i18n="calNote">Procedure: unload the load cell and run step 1. Then attach a known weight, enter its gram value and run step 2. The calibration factor is stored on the ESP and loaded automatically at the next start.</p>
        <p class="note" id="calibrationInfo" data-i18n="calInfo">Loading calibration status...</p>
      </section>

    </div>

    <div id="staleBanner" role="status"></div>
    <div id="rebootBanner" role="alert" hidden></div>
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
    const runChartEl = document.getElementById("runChart");
    const runCtx = runChartEl.getContext("2d");
    const forceHistory = [];
    // M7 FR-006: the finished run as force-displacement points, fetched
    // from /api/data (the exact `data` CSV grammar) and drawn auto-scaled.
    let runPoints = [];
    let runPeakIndex = -1;

    // M2-T005 (FR-005, plan D3): client-side locale dictionary. The static
    // markup ships English (factory default, SC-001); /api/status carries the
    // server locale, and the EN/DE selector switches both surfaces through
    // the same `lang <en|de>` command the serial CLI uses. DE uses native
    // umlauts (UTF-8, D5) - unlike the ASCII-transcribed serial column.
    const I18N = {
      en: {
        "title": "Test machine",
        "h1": "JanTec ATLAS",
        "headTest": "Tensile test",
        "lblCycle": "Cycle", "lblDatum": "Datum",
        "samplePrompt": "Datum reached. Clamp the sample, then start the test.",
        "btnHome": "Home", "btnStartTest": "Start test",
        "lblPeak": "Peak force", "lblBreak": "Break displacement",
        "lblSamples": "Samples", "lblTrunc": "Truncated",
        "csvDownload": "Download CSV",
        "headRunCurve": "Run curve (force-displacement)",
        "curveEmpty": "No run data yet.",
        "runNote": "Curve loaded from the machine sample buffer.",
        "btnCurve": "Reload curve",
        "headPlot": "Force curve",
        "plotNote": "Live plot of the measured force at 10 samples per second.",
        "headControls": "Movement",
        "jogUpAl": "Jog up (hold button)",
        "jogDownAl": "Jog down (hold button)",
        "btnStop": "Stop",
        "inFeed": "Feed (mm/min)", "phFeed": "Feed mm/min",
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
        "headTestCfg": "Test settings",
        "inTestSpeed": "Test speed (mm/min)", "phTestSpeed": "Test speed in mm/min",
        "inTestArm": "Arming force (N)", "phTestArm": "Arming force in N",
        "inTestDrop": "Break drop (%)", "phTestDrop": "Break drop in %",
        "testCfgNote": "Stored on the ESP and applied from the next test run.",
        "headMove": "Motion", "inMove": "Relative travel (mm)", "phMove": "Relative travel in mm",
        "btnMove": "Move mm", "inTurn": "Relative revolutions", "phTurn": "Relative revolutions",
        "btnTurn": "Move turns",
        "headCal": "Load cell calibration",
        "btnCal1": "1. Confirm unloaded", "btnCal2": "2. Measure weight",
        "inCal": "Known weight (grams)", "phCal": "Known weight in grams",
        "calNote": "Procedure: unload the load cell and run step 1. Then attach a known weight, enter its gram value and run step 2. The calibration factor is stored on the ESP and loaded automatically at the next start.",
        "calInfo": "Loading calibration status...",
        "driverOn": "Active", "driverOff": "Off",
        "motionRun": "Running", "motionIdle": "Idle",
        "stOk": "OK", "stWarn": "Warning", "stFail": "Fault",
        "stNoConn": "Not connected", "stNoWired": "Not wired",
        "stateIdle": "Ready", "stateError": "Error", "faultNone": "None",
        "tsIdle": "Idle", "tsHoming": "Homing", "tsPulling": "Pulling",
        "tsTail": "Tail", "tsDone": "Done",
        "tsLimit": "Limit (no break)", "tsAborted": "Aborted", "tsError": "Error",
        "datumYes": "Valid (this power cycle)", "datumNo": "None - home first",
        "breakNone": "No break", "truncYes": "Yes (buffer full)", "truncNo": "No",
        "noteError": "Error state: motion commands locked, reboot required.",
        "noteWarn": "Warnings from the startup selftest, machine operational.",
        "noteOk": "Startup selftest, nothing notable.",
        "calActive": "Calibration active. Factor: ",
        "calNone": "No valid calibration stored yet.",
        "errNoConn": "No connection to the ESP32.",
        "errNoStatus": "Status could not be loaded.",
        "staleContact": "No contact for {n} s - shown values may be outdated.",
        "deviceRebooted": "Device was restarted - check position and driver state.",
        "errValue": "Please enter a value.",
        "errWeight": "Enter the known weight in grams first.",
        "errCycleActive": "A test is running - download after it finishes."
      },
      de: {
        "title": "Testmaschine",
        "h1": "JanTec ATLAS",
        "headTest": "Zugversuch",
        "lblCycle": "Zyklus", "lblDatum": "Nullpunkt",
        "samplePrompt": "Nullpunkt erreicht. Probe einspannen, dann Test starten.",
        "btnHome": "Referenzfahrt", "btnStartTest": "Test starten",
        "lblPeak": "Max. Kraft", "lblBreak": "Bruchweg",
        "lblSamples": "Messwerte", "lblTrunc": "Abgeschnitten",
        "csvDownload": "CSV herunterladen",
        "headRunCurve": "Kurvenzug (Kraft-Weg)",
        "curveEmpty": "Noch keine Messdaten.",
        "runNote": "Kurve aus dem Messpuffer der Maschine geladen.",
        "btnCurve": "Kurve neu laden",
        "headPlot": "Kraftverlauf",
        "plotNote": "Live-Plot der gemessenen Kraft mit 10 Samples pro Sekunde.",
        "headControls": "Bewegung",
        "jogUpAl": "Aufwärts joggen (Taste halten)",
        "jogDownAl": "Abwärts joggen (Taste halten)",
        "btnStop": "Stopp",
        "inFeed": "Vorschub (mm/min)", "phFeed": "Vorschub mm/min",
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
        "headTestCfg": "Testeinstellungen",
        "inTestSpeed": "Prüfgeschwindigkeit (mm/min)", "phTestSpeed": "Prüfgeschwindigkeit in mm/min",
        "inTestArm": "Kraftschwellwert (N)", "phTestArm": "Kraftschwellwert in N",
        "inTestDrop": "Abfall (%)", "phTestDrop": "Abfall in %",
        "testCfgNote": "Wird im ESP gespeichert und gilt ab dem nächsten Testlauf.",
        "headMove": "Bewegung", "inMove": "Relativweg (mm)", "phMove": "Relativweg in mm",
        "btnMove": "mm fahren", "inTurn": "Umdrehungen relativ", "phTurn": "Umdrehungen relativ",
        "btnTurn": "Turns fahren",
        "headCal": "Kalibrierung Wägezelle",
        "btnCal1": "1. Unbelastet bestätigen", "btnCal2": "2. Gewicht messen",
        "inCal": "Bekanntes Gewicht in Gramm", "phCal": "Bekanntes Gewicht in Gramm",
        "calNote": "Ablauf: Wägezelle entlasten und Schritt 1 ausführen. Danach bekanntes Gewicht einhängen, Grammwert eintragen und Schritt 2 ausführen. Der Kalibrierfaktor wird im ESP gespeichert und beim nächsten Start automatisch geladen.",
        "calInfo": "Kalibrierstatus wird geladen...",
        "driverOn": "Aktiv", "driverOff": "Aus",
        "motionRun": "Läuft", "motionIdle": "Bereit",
        "stOk": "In Ordnung", "stWarn": "Warnung", "stFail": "Fehler",
        "stNoConn": "Nicht verbunden", "stNoWired": "Nicht verdrahtet",
        "stateIdle": "Bereit", "stateError": "Fehler", "faultNone": "Keiner",
        "tsIdle": "Leerlauf", "tsHoming": "Referenzfahrt", "tsPulling": "Zug",
        "tsTail": "Nachlauf", "tsDone": "Fertig",
        "tsLimit": "Begrenzung (kein Bruch)", "tsAborted": "Abgebrochen", "tsError": "Fehler",
        "datumYes": "Gültig (diese Einschaltdauer)", "datumNo": "Keiner - erst Nullpunkt fahren",
        "breakNone": "Kein Bruch", "truncYes": "Ja (Puffer voll)", "truncNo": "Nein",
        "noteError": "Fehlerzustand: Motorkommandos gesperrt, Reboot nötig.",
        "noteWarn": "Warnungen aus der Startprüfung, Maschine betriebsbereit.",
        "noteOk": "Startprüfung ohne Auffällige.",
        "calActive": "Kalibrierung aktiv. Faktor: ",
        "calNone": "Noch keine gültige Kalibrierung gespeichert.",
        "errNoConn": "Keine Verbindung zum ESP32.",
        "errNoStatus": "Status kann nicht geladen werden.",
        "staleContact": "Kein Kontakt seit {n} s - angezeigte Werte können veraltet sein.",
        "deviceRebooted": "Gerät wurde neu gestartet - Position und Treiberzustand prüfen.",
        "errValue": "Bitte einen Wert eingeben.",
        "errWeight": "Bitte zuerst das bekannte Gewicht in Gramm eingeben.",
        "errCycleActive": "Ein Testlauf ist aktiv - Download danach."
      }
    };
    // M2-T006: the last locale survives page reload (localStorage) until the
    // first /api/status poll confirms the server value (NVS) - no EN flash
    // after `lang de` + reload, and a cleared cache falls back to EN, the
    // factory default (FR-002).
    let locale = (function () {
      try {
        return localStorage.getItem("locale") === "de" ? "de" : "en";
      } catch (e) {
        return "en";
      }
    })();
    function t(key) {
      const dict = I18N[locale] || I18N.en;
      return dict[key] !== undefined ? dict[key] : I18N.en[key];
    }
    function applyLocale(next) {
      locale = next === "de" ? "de" : "en";  // unknown values fall back to EN
      try {
        localStorage.setItem("locale", locale);
      } catch (e) {
        // private-mode browsers: in-session switching keeps working
      }
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
    // #95: failure backoff ladder (ms) and stale-contact tracking. The
    // fixed 100 ms interval became a self-rescheduling timeout: while
    // unreachable the request rate drops to <= 1/s (the WebServer serves
    // one client at a time - hammering it during degradation is the
    // worst load pattern). Reset to the first rung on every success.
    const POLL_STEPS_MS = [100, 250, 500, 1000];
    let pollStep = 0;
    let lastContactMs = Date.now();
    let lastUptimeMs = -1;
    const staleBanner = document.getElementById("staleBanner");
    const rebootBanner = document.getElementById("rebootBanner");

    function markStale() {
      const ageSeconds = Math.round((Date.now() - lastContactMs) / 1000);
      if (ageSeconds >= 1) {
        document.body.classList.add("stale");
        staleBanner.textContent = t("staleContact").replace("{n}", ageSeconds);
      }
    }

    function clearStale() {
      document.body.classList.remove("stale");
    }

    function resizeCanvas() {
      const ratio = window.devicePixelRatio || 1;
      for (const pair of [[chart, ctx], [runChartEl, runCtx]]) {
        const cssWidth = pair[0].clientWidth || 1000;
        pair[0].width = Math.floor(cssWidth * ratio);
        pair[0].height = Math.floor(300 * ratio);
        pair[1].setTransform(ratio, 0, 0, ratio, 0, 0);
      }
      drawPlot();
      drawRunCurve();
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
      selftestNote: document.getElementById("selftestNote"),
      cycleState: document.getElementById("cycleState"),
      datumState: document.getElementById("datumState"),
      samplePrompt: document.getElementById("samplePrompt"),
      runPeak: document.getElementById("runPeak"),
      runBreak: document.getElementById("runBreak"),
      runSamples: document.getElementById("runSamples"),
      runTrunc: document.getElementById("runTrunc"),
      curveNote: document.getElementById("curveNote")
    };

    let lastSelftestChecksKey = "";
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
      // #95: the cache key includes the LOCALE - the checks JSON itself
      // is locale-neutral, so without this a lang switch left the rows
      // in the old language (bench-found via the vision+DOM cross-check).
      const checksKey = locale + ":" + checksJson;
      if (checksKey !== lastSelftestChecksKey) {
        lastSelftestChecksKey = checksKey;
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

    // M7 US3: tear-workflow state. Cycle names arrive as the pinned
    // TestCycleModel::name() strings; the tsIdle..tsError dictionary
    // family is fed through the tsKey variable below.
    const activeStates = ["Homing", "Pulling", "Tail"];
    const terminalStates = ["Done", "Limit", "Aborted"];
    let cycleActive = false;
    let lastCurveKey = "";

    function renderTear(data) {
      const ts = data.testState || "Idle";
      cycleActive = activeStates.includes(ts);
      // t() is fed through `key` (not a literal): the dictionary
      // completeness test scans literals, the ts* family is pinned as
      // indirect keys there instead.
      const tsKey = "ts" + ts;
      statusFields.cycleState.textContent = t(tsKey);
      statusFields.datumState.textContent = data.homed ? t("datumYes") : t("datumNo");
      // FR-005: the sample prompt is the operator-paced pause between
      // datum reached and cycle start (never an auto-start).
      statusFields.samplePrompt.hidden =
        !(data.homed && !cycleActive && ts === "Idle");

      const run = data.run || { samples: 0 };
      const hasRun = run.samples > 0;
      statusFields.runPeak.textContent =
        hasRun ? Number(run.peakN).toFixed(2) + " N" : "-";
      statusFields.runBreak.textContent =
        hasRun ? (run.break ? Number(run.breakMm).toFixed(2) + " mm" : t("breakNone")) : "-";
      statusFields.runSamples.textContent = hasRun ? String(run.samples) : "-";
      statusFields.runTrunc.textContent =
        hasRun ? (run.truncated ? t("truncYes") : t("truncNo")) : "-";
      runPeakIndex = hasRun ? run.peakIndex : -1;

      // FR-006: fetch the finished run's samples once per completed run.
      const key = ts + "|" + run.samples + "|" + run.peakN;
      if (!cycleActive && terminalStates.includes(ts) && hasRun &&
          key !== lastCurveKey) {
        lastCurveKey = key;
        loadRunCurve();
      }
    }

    async function loadRunCurve() {
      try {
        const response = await fetch("/api/data");
        if (!response.ok) {
          setMessage(t("errNoStatus"), true);
          return;
        }
        const text = await response.text();
        runPoints = [];
        for (const line of text.split("\n")) {
          if (!line || line.charAt(0) === "#") {
            continue;  // header / END marker
          }
          const fields = line.split(",");
          if (fields.length < 3) {
            continue;
          }
          const mm = parseFloat(fields[1]);
          const n = parseFloat(fields[2]);
          if (isFinite(mm) && isFinite(n)) {
            runPoints.push([mm, n]);
          }
        }
        statusFields.curveNote.textContent =
          runPoints.length ? t("runNote") : t("curveEmpty");
        drawRunCurve();
      } catch (error) {
        setMessage(t("errNoConn"), true);
      }
    }

    function setMessage(text, isError = false) {
      messageEl.textContent = text;
      messageEl.style.color = isError ? "#b3261e" : "";
    }

    function drawPlot() {
      const width = chart.clientWidth || 1000;
      const height = 300;
      ctx.clearRect(0, 0, width, height);

      ctx.strokeStyle = "#e5e5e2";
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

      ctx.strokeStyle = "#1a1a1a";
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

      ctx.fillStyle = "#6b6b68";
      ctx.font = "14px sans-serif";
      ctx.fillText(maxForce.toFixed(0) + " N", 8, 18);
      ctx.fillText(((maxForce + minForce) / 2).toFixed(0) + " N", 8, height / 2 + 4);
      ctx.fillText(minForce.toFixed(0) + " N", 8, height - 8);
      ctx.fillText(((forceHistory.length - 1) / sampleRateHz).toFixed(1) + " s", width - 58, height - 8);
    }

    function drawRunCurve() {
      const width = runChartEl.clientWidth || 1000;
      const height = 300;
      runCtx.clearRect(0, 0, width, height);

      runCtx.strokeStyle = "#e5e5e2";
      runCtx.lineWidth = 1;
      for (let i = 0; i <= 5; i++) {
        const y = 20 + ((height - 40) / 5) * i;
        runCtx.beginPath();
        runCtx.moveTo(52, y);
        runCtx.lineTo(width - 12, y);
        runCtx.stroke();
      }

      if (runPoints.length < 2) {
        return;
      }

      // Auto-scale both axes (no fixed travel/force bounds: the run
      // defines them; the live chart keeps its fixed force axis).
      let xMin = Infinity, xMax = -Infinity, yMin = Infinity, yMax = -Infinity;
      for (const p of runPoints) {
        xMin = Math.min(xMin, p[0]); xMax = Math.max(xMax, p[0]);
        yMin = Math.min(yMin, p[1]); yMax = Math.max(yMax, p[1]);
      }
      if (xMax - xMin < 1e-6) { xMax = xMin + 1; }
      if (yMax - yMin < 1e-6) { yMax = yMin + 1; }
      const xSpan = xMax - xMin;
      const ySpan = yMax - yMin;

      const px = (mm) => 52 + ((width - 64) * (mm - xMin)) / xSpan;
      const py = (n) => height - 20 - ((n - yMin) / ySpan) * (height - 40);

      runCtx.strokeStyle = "#1a1a1a";
      runCtx.lineWidth = 3;
      runCtx.beginPath();
      runPoints.forEach((p, index) => {
        if (index === 0) {
          runCtx.moveTo(px(p[0]), py(p[1]));
        } else {
          runCtx.lineTo(px(p[0]), py(p[1]));
        }
      });
      runCtx.stroke();

      // Peak marker (FR-008 visual echo): a ring on the peak sample.
      if (runPeakIndex >= 0 && runPeakIndex < runPoints.length) {
        const p = runPoints[runPeakIndex];
        runCtx.strokeStyle = "#b3261e";
        runCtx.lineWidth = 2;
        runCtx.beginPath();
        runCtx.arc(px(p[0]), py(p[1]), 5, 0, Math.PI * 2);
        runCtx.stroke();
      }

      runCtx.fillStyle = "#6b6b68";
      runCtx.font = "14px sans-serif";
      runCtx.fillText(yMax.toFixed(1) + " N", 8, 18);
      runCtx.fillText(yMin.toFixed(1) + " N", 8, height - 8);
      runCtx.fillText(xMin.toFixed(1) + " mm", 52, height - 2);
      const xLabel = xMax.toFixed(1) + " mm";
      runCtx.fillText(xLabel, width - 12 - runCtx.measureText(xLabel).width, height - 2);
    }

    async function sendCommand(command) {
      // #95: an operator action supersedes the reboot notice.
      rebootBanner.hidden = true;
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
        // #95: bounded fetch - an ESP reboot kills established TCP
        // silently (no FIN/RST); without a timeout the poll hangs on
        // the dead socket forever and the page never goes stale.
        const response = await fetch("/api/status",
                                     { signal: AbortSignal.timeout(2000) });
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
        renderTear(data);
        if (statusUnavailable) {
          statusUnavailable = false;
          messageEl.textContent = "";
        }
        // #95: contact age + reboot watch.
        pollStep = 0;
        lastContactMs = Date.now();
        clearStale();
        if (typeof data.uptimeMs === "number") {
          if (lastUptimeMs >= 0 && data.uptimeMs < lastUptimeMs) {
            // The boot clock went backwards - the device rebooted under
            // the page (driver/position state reset with it). Latched:
            // poll noise must not eat it; the next operator action
            // (sendCommand) clears it.
            rebootBanner.hidden = false;
            rebootBanner.textContent = t("deviceRebooted");
          }
          lastUptimeMs = data.uptimeMs;
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
        pollStep = Math.min(pollStep + 1, POLL_STEPS_MS.length - 1);
        markStale();
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

    // FR-007: the CSV link targets /api/data, which the firmware rejects
    // (400) while a run is active - catch that in the browser instead of
    // downloading a JSON error as a file.
    document.getElementById("csvDownload").addEventListener("click", (event) => {
      if (cycleActive) {
        event.preventDefault();
        setMessage(t("errCycleActive"), true);
      }
    });

    document.getElementById("curveRefresh").addEventListener("click", () => {
      loadRunCurve();
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
    // #95: self-rescheduling poll with failure backoff (100 ms -> 1 s cap,
    // reset on success) instead of a fixed 100 ms interval.
    (function pollLoop() {
      refreshStatus().finally(() => {
        setTimeout(pollLoop, POLL_STEPS_MS[pollStep]);
      });
    })();
  </script>
</body>
</html>
)HTML";

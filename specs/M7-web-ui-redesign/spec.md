# Feature Specification: M7 - Web UI Redesign

Status: Draft | Rigor: Spec-First | Created: 2026-10-07 | Updated:
2026-10-11 | Priority: Low (owner re-affirmed 2026-10-11: tear workflow
first-class, #167)
(owner) | Depends on: M3, M4, M5, M6 (cycles + command grammar; M3 command
surface bench-proven 2026-10-11, #156)

**Goal:** Redesign the on-device web UI into a clean, modern, colour-free
control surface that exposes every machine function as a button or control:
calibration wizard, tensile test, constant-force hold, stepped and jog
movement, stop - with live status. Owner request #83, explicitly low
priority.

## User Scenarios

### US1 (P1) - Drive the machine from the browser

As an operator with a phone or laptop on the machine's WiFi, I run every
workflow from the web UI without a serial terminal.

- **Given** the machine booted, **When** I open the softAP page, Then I see
  controls grouped by function (movement, calibration, tensile test,
  constant-force hold) and a live status area (position, force, cycle
  state, fault).
- **Given** a tensile test is running, **When** I watch the status area,
  Then position, force, and cycle state update without a page reload.

### US2 (P2) - Visual language

As the owner, the UI reads as a clean instrument panel.

- **Given** the rendered page, **Then** it is monochrome with exactly ONE
  functional accent - red for Stop/danger states, used nowhere else - in
  the style of the Qitech Automation reference (issue #83 screenshot -
  asset not retrievable at spec time, blocked on operator re-attachment).
  Owner decision 2026-10-11 (#167 taste probe, rule N8): the emergency
  action stays unmissable; strict zero-colour was rejected.

### US3 (P1) - Tear test end-to-end from the browser (#167)

As an operator, I run the complete tear test on a phone/laptop over the
softAP without a serial terminal.

- **Given** the machine is Idle, **When** I tap Home, Then the homing runs
  and the UI reports datum reached, then prompts me to insert the sample.
- **Given** a sample is mounted, **When** I tap Start test, Then the cycle
  runs with live state, and on break the UI shows the run summary (peak
  force, break displacement, samples/truncated).
- **Given** a finished run, **When** I open the curve view, Then the
  recorded force-displacement curve is rendered from the sample buffer, and
  I can download the run as a CSV file.

## Requirements

### Functional

- **FR-001:** command buttons map onto the existing command grammar
  (M3/M4/M5 cycles + movement + stop) through the established command
  policy; no second firmware command surface. Buttons consume the legacy
  `/api/command` responses this milestone; envelope unification on the web
  stays deferred (M6 A2).
- **FR-002:** live status area updates without full page reload
  (transport - polling vs SSE - decided in plan under the ESP32 WebServer
  constraint).
- **FR-003:** the calibration wizard is web-driven: mass entry field,
  hang/remove prompts mirrored from the M5 wizard states.
- **FR-004:** locale support survives (M2 web i18n: EN/DE labels) - the
  redesign restructures layout, not the locale mechanism.
- **FR-005 (US3):** tear workflow controls exist in the UI: Home (datum),
  Start test, Stop - wired through FR-001's command policy; between datum
  reached and cycle start the UI prompts sample insertion (operator-paced,
  no auto-start).
- **FR-006 (US3):** after a run the UI renders the recorded
  force-displacement curve from the machine's sample buffer (source: `data`
  dump or its M6 JSON equivalent - plan decides transport; the live
  force-vs-time chart is separate and stays).
- **FR-007 (US3):** the last run's samples are downloadable as a CSV file
  from the browser (filename convention is plan-level; content = the `data`
  CSV grammar, `# END` trailer included or excluded per plan).
- **FR-008 (US3):** the run summary shows peak force, break displacement,
  sample count, truncated flag (from the `testdata` surface).

### Constraints & TDD Targets

- **TC-001:** PROGMEM budget: the redesigned page (HTML+CSS+JS, classless-
  CSS style, ~2-8 KB semantic styling per classless-framework research)
  measured in the build output and documented in the milestone-close PR.
- **TC-002:** every button issues a command that exists in the M6 command
  table; the UI has no private command strings.
- **TC-003:** browser smoke test (browser-automation) covers: load, one
  movement action, one cycle start/stop, status refresh.

### Non-Functional

- Monochrome design with exactly one accent - red, reserved for
  Stop/danger states (US2 owner decision 2026-10-11); no second hue
  anywhere, including the favicon. No external CDN assets (offline softAP).
- Page must remain a single PROGMEM asset served by `WebUi` (architecture
  unchanged).

## Assumptions & Handoffs

- **A1:** the Qitech reference screenshot in #83 was not retrievable at
  spec time (404) - the owner re-attaches or confirms the style before
  `/spec.plan` (blocked).
- **A2:** layout/details (grouping order, panel density) are plan-level;
  this spec pins the functional surface and the visual constraints.
- **Next Step:** unblock A1, then `/spec.plan` after M3-M6 ship.

## Success Criteria

- **SC-001:** every workflow (calibration, tensile, hold, movement, stop)
  is operable end-to-end from the browser.
- **SC-002:** the page matches the monochrome clean style and passes the
  TC-001 budget.
- **SC-003:** the US3 tear workflow (home -> sample prompt -> test ->
  summary -> curve -> CSV download) completes on a phone over the softAP
  with no serial terminal involved (#167).

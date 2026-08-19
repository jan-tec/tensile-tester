# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

No release has been tagged yet. The first release is planned as `v0.1.0`
(`0.x` = initial development: hardware firmware with a changing command set
and API).

## [Unreleased]

### Added

- Initial PlatformIO project for ESP32 (`esp32dev`, Arduino framework)
  (2026-04-12, `283123b`, `5ac554a`)
- Debug sketches in `standalone_tests/`: LED blink, HX711 raw measurements
  (2026-04-12, `34d006e`)
- Load cell (HX711, GPIO 32/33) with serial menu for calibration and force
  measurement (2026-04-12, `5c1db96`)
- Stepper control over serial CLI (`h`, `en`, `off`, `zero`, `pos`, `rpm`,
  `acc`, `move`, `turn`, `sweep`, `stop`; relative units in degrees at this
  point) with gear math: 200 steps/rev, 13.73:1 gearbox, 8 microsteps; two
  NEMA 17 motors driven synchronously via STEP/DIR/EN (2026-04-13, `7e0e344`)
- WiFi AP mode (`ESP32-Testmaschine`) with web UI: live force plot, status
  panel, feed/acceleration forms, and `GET /api/status` / `GET /api/command`
  endpoints; load cell now sampled at 10 Hz with low-pass filtering;
  mm-based `move`/`turn` and `feed` commands with 2 mm/rev spindle pitch
  (2026-04-13, `75c07e7`)
- Load cell calibration from the web UI (`cal_zero` + `cal_apply <grams>`)
  with factor persisted in NVS (Preferences namespace `testmachine`) and
  reloaded at boot; jog buttons and `jog up` / `jog down` commands (relative
  1000 mm travel) (2026-04-14, `f7ce4b4`)

### Known limitations

- Automatic test modes ("Ziehen bis Bruch", "Kraft halten") are placeholders:
  the `TestMode` enum and the UI select exist, but the test logic is not
  implemented.

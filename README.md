# Tensile Tester

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/jan-tec/tensile-tester)

ESP32 firmware for a tensile test machine (Zugversuchsmaschine): a
gear-stepper spindle with load-cell force measurement, controlled over a
WiFi web UI or the serial CLI. The UI is English-first with a German locale
switch (`lang de` on the serial CLI or the Web UI selector; the choice is
persisted in NVS and survives reboot).

## Hardware

| Component | Model / value |
|-----------|---------------|
| MCU | ESP32 (PlatformIO board `esp32dev`) |
| Stepper driver | TMC2209, plain STEP/DIR/EN (shared STEP/DIR, EN1/EN2 per driver; no UART configuration wired) |
| Stepper motor | NEMA 17, 200 full steps/rev, x2 (run synchronously on the same signals) |
| Gearbox | 13.73:1 |
| Spindle pitch | 2 mm/rev |
| Load cell | HX711 ADC |

### Wiring

| Signal | GPIO | Notes |
|--------|------|-------|
| STEP | 18 | |
| DIR | 19 | |
| EN1 | 5 | motor 1 driver, active low (LOW = driver enabled) |
| EN2 | 23 | motor 2 driver, active low, switched together with EN1 |
| HX711 DOUT | 32 | |
| HX711 SCK | 33 | |

## Requirements

- PlatformIO CLI: `pipx install platformio` (first build downloads the
  `espressif32` toolchain, ~200 MB)
- ESP32 dev board on USB for flashing

## Build and flash

```bash
pio run -e esp32dev            # build (only env)
pio run -t upload -e esp32dev  # flash
pio device monitor -p <port> -b 115200
```

## Serial CLI (115200 baud)

Same command set as the web API. On an unknown command the firmware prints
the available commands.

| Command | Effect |
|---------|--------|
| `en` | enable both driver outputs |
| `off` | disable both driver outputs |
| `zero` | set axis position to 0 |
| `tare` | tare the load cell |
| `pos` | print position (steps + mm) and force to serial |
| `force` | print current force in N |
| `stop` | stop motion |
| `rpm <U/min>` | set gear output speed |
| `acc <U/min/s>` | set acceleration |
| `feed <mm/min>` | set spindle feed rate (clamped 0.5 - 60) |
| `move <mm>` | relative spindle move |
| `turn <revs>` | relative gear output rotation |
| `jog up` / `jog down` | continuous jog for a relative 1000 mm travel; stop with `stop` |
| `cal_zero` | calibration step 1 (load cell unloaded) |
| `cal_apply <grams>` | calibration step 2 with a known mass in grams |

## Web UI

The ESP32 starts in WiFi **AP mode** (no client network needed):

- SSID: `ESP32-Testmaschine`, password: `testmaschine`
- UI at the softAP IP (default `192.168.4.1`; the actual IP is printed to
  serial and reported in `/api/status`)

API (all endpoints are GET):

| Endpoint | Payload |
|----------|---------|
| `/api/status` | JSON: `driverEnabled`, `running`, `rpm`, `acc`, `feedMmPerMin`, `positionMm`, `steps`, `forceNewton`, `calibrationValid`, `calibrationFactor`, `network` |
| `/api/command?cmd=<command>` | executes a CLI command; returns `{"ok":bool,"message":"..."}` |

The UI shows a live force plot (10 samples/s, fixed axis -50..400 N),
position, driver/motion state, jog buttons, feed/acceleration forms and the
calibration panel.

## Load cell calibration

1. Remove the load, run step 1 (`cal_zero` / "1. Unbelastet bestaetigen").
2. Hang a known mass, enter the mass in grams, run step 2
   (`cal_apply <grams>` / "2. Gewicht messen").

The calibration factor persists in NVS (Preferences namespace `testmachine`)
and is loaded automatically at the next boot. Force is displayed in newtons
(converted from grams with g = 9.80665 m/s^2).

## Motion math

| Parameter | Value |
|-----------|-------|
| Motor full steps/rev | 200 |
| Gear ratio | 13.73 |
| Microsteps | 8 |
| Steps per spindle revolution | 21968 |
| Spindle pitch | 2 mm/rev |
| Feed clamp | 0.5 - 60 mm/min |

The constants live at the top of `src/main.cpp` - keep them in sync if the
gearbox, motor or spindle changes.

## Repository layout

| Path | Purpose |
|------|---------|
| `src/main.cpp` | The only compiled file: firmware + web UI (inlined `INDEX_HTML`) |
| `standalone_tests/` | Debug sketches (LED blink, raw load cell, motor menu) - not built automatically |
| `test/`, `include/`, `lib/` | Empty PlatformIO scaffolding |
| `AGENTS.md` | Agent-specific build/architecture notes |

### Running a standalone test

PlatformIO compiles everything in `src/`; `src/main.cpp` is the only file
there, so a debug sketch is not built automatically. Swap it in temporarily
and never commit the swapped state:

```bash
mv src/main.cpp /tmp/main.cpp.bak
cp standalone_tests/test_02_loadcell_measure_raw.cpp src/main.cpp
pio run -e esp32dev -t upload
mv /tmp/main.cpp.bak src/main.cpp
```

## Status

| Feature | State |
|---------|-------|
| Manual control (web UI + serial CLI) | done |
| Load cell measurement + persisted calibration | done |
| Automatic test modes ("Ziehen bis Bruch", "Kraft halten") | placeholder: `TestMode` enum and UI select exist, logic not implemented |

## History and versioning

See [CHANGELOG.md](CHANGELOG.md). The project follows Semantic Versioning;
no release has been tagged yet (see `[Unreleased]`).

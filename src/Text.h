// M2 message-string tables (T002 skeleton): one id per user-facing string the
// firmware produces, plus the EN and DE columns. Host-testable (TC-002) and
// used by the target through the same header.
//
// Design (specs/M2-i18n-de-en/plan.md D2/D8):
// - `Msg` is the single source of user-facing text; call sites select an id,
//   they do not carry literals (grep gate SC-003, enforced in T008).
// - Two parallel tables, EN first (FR-001 default locale). No printf format
//   strings with embedded prose: values and units are concatenated by the
//   caller, so a locale can reorder or rename them (see the Prefix/Suffix and
//   Unit ids below).
// - Tables are header-inline (C++17) so the host test links them directly:
//   src/*.cpp is not part of the native test build, same pattern as
//   ErrorCode.h. On ESP32 const data lands in .flash.rodata, so the flash-only
//   / RAM-safe NFR holds without a copy.
// - Source stays ASCII-only (AGENTS.md convention). The DE column is the
//   ae/oe/ue/ss transcription FR-006 requires for serial. Web DE with native
//   umlauts (FR-005) is owned by the JS dictionary in UiHtml.h (D5, T005),
//   not by this table - keeping one DE column ASCII is what lets both
//   surfaces share it.
// - PROGMEM: no-op on ESP32 (unified address space, see
//   arduino-esp32 pgmspace.h). Defined empty here so the header stays free of
//   Arduino.h and compiles on the host; keep it if the project ever moves to
//   an AVR target, where it would need a real pgmspace read.
#pragma once

#include <cstddef>
#include <cstdint>

#ifndef PROGMEM
#define PROGMEM
#endif

// Order is presentation-neutral; append new ids at the end of their block and
// keep both tables in sync (the static_asserts below are the compile-time gate,
// test/test_text_tables.cpp is the runtime gate).
enum class Msg : uint8_t {
  // Boot and hardware bring-up
  BootBanner = 0,
  MotorsSyncNote,
  StepperInitFailed,
  Hx711ReadyCalLoaded,  // prefix, factor appended
  Hx711ReadyNoCal,
  Hx711NotReadyDisabled,
  TaraSet,
  ApStarted,        // prefix, SSID appended
  ApIpSeparator,    // between SSID and IP
  ApPassword,       // prefix, password appended
  WebUiUrl,         // prefix, "http://" + IP appended
  WebCommand,       // prefix, the received command appended
  CmdParamMissing,

  // Driver and motion replies
  DriversEnabled,
  DriversDisabled,
  RpmSet,           // prefix, value + unit appended
  AccelSet,         // prefix, value + unit appended
  FeedSet,          // prefix, value + unit appended
  RpmMustBePositive,
  AccelMustBePositive,
  FeedMustBePositive,
  RelativeMoveStarted,  // prefix, value + unit appended
  RevolutionsSuffix,
  RpmSuffix,
  AccelSuffix,
  MillimetreSuffix,
  MillimetrePerMinuteSuffix,
  NewtonSuffix,
  JogUpStarted,
  JogDownStarted,
  StopRequested,

  // Load cell and position replies
  OriginZeroed,
  LoadCellTared,
  PositionPrinted,
  PositionSteps,  // prefix, step count appended
  ForceCurrent,   // prefix, newtons appended
  StatusForceLabel,
  StatusTravelLabel,
  ForceUnavailable,
  Hx711NotReady,
  NotAvailable,  // the "n/a" token

  // Calibration (two-step, M5 keeps this flow)
  CalZeroCaptured,
  CalStep1First,
  CalWeightMustBePositive,
  CalSaved,  // prefix, factor appended
  CalFailedZeroFactor,
  CalFailedNegativeFactor,
  CalValueUnreadable,

  // Fault presentation (M1 FR-009, US2). The hex code is formatted by the
  // caller between the two halves, so no locale ever carries a format string.
  FaultStatePrefix,
  FaultStateSuffix,
  FaultPowerLow,

  // CLI generic
  EmptyCommand,
  UnknownCommand,
  InvalidValue,
  HelpCommandsPrefix,
  HelpCommandsSuffix,  // command names are locale-invariant

  Count
};

// EN is the default locale (FR-001/FR-002).
inline const char *const kMsgEn[] PROGMEM = {
    /* BootBanner */ "ESP32 test machine controller started.",
    /* MotorsSyncNote */
    "Two motors run synchronously on the same STEP/DIR signals (EN1/EN2 "
    "switched together).",
    /* StepperInitFailed */ "Stepper driver could not be created.",
    /* Hx711ReadyCalLoaded */ "HX711 ready. Calibration factor loaded: ",
    /* Hx711ReadyNoCal */ "HX711 ready. No valid calibration stored yet.",
    /* Hx711NotReadyDisabled */
    "HX711 not ready. Force measurement stays disabled.",
    /* TaraSet */ "Tare set.",
    /* ApStarted */ "WiFi access point started: ",
    /* ApIpSeparator */ " | IP ",
    /* ApPassword */ "Password: ",
    /* WebUiUrl */ "Web UI: http://",
    /* WebCommand */ "Web command: ",
    /* CmdParamMissing */ "Parameter 'cmd' missing.",

    /* DriversEnabled */ "Both drivers enabled.",
    /* DriversDisabled */ "Both drivers disabled.",
    /* RpmSet */ "Speed set to ",
    /* AccelSet */ "Acceleration set to ",
    /* FeedSet */ "Feed set to ",
    /* RpmMustBePositive */ "RPM must be greater than 0.",
    /* AccelMustBePositive */ "ACC must be greater than 0.",
    /* FeedMustBePositive */ "Feed must be greater than 0.",
    /* RelativeMoveStarted */ "Relative move started: ",
    /* RevolutionsSuffix */ " revolutions.",
    /* RpmSuffix */ " rpm.",
    /* AccelSuffix */ " rpm/s.",
    /* MillimetreSuffix */ " mm.",
    /* MillimetrePerMinuteSuffix */ " mm/min.",
    /* NewtonSuffix */ " N.",
    /* JogUpStarted */ "Jog up started.",
    /* JogDownStarted */ "Jog down started.",
    /* StopRequested */ "Stop requested.",

    /* OriginZeroed */ "Axis position set to 0.",
    /* LoadCellTared */ "Load cell tared.",
    /* PositionPrinted */ "Position printed to the serial monitor.",
    /* PositionSteps */ "Position [steps]: ",
    /* ForceCurrent */ "Current force: ",
    /* StatusForceLabel */ " | Force [N]: ",
    /* StatusTravelLabel */ " | Travel [mm]: ",
    /* ForceUnavailable */ "Force: n/a (HX711 not ready).",
    /* Hx711NotReady */ "HX711 not ready.",
    /* NotAvailable */ "n/a",

    /* CalZeroCaptured */
    "Zero point captured. Now attach a known weight, enter its gram value and "
    "run step 2.",
    /* CalStep1First */
    "Run step 1 first with the load cell unloaded.",
    /* CalWeightMustBePositive */
    "Calibration weight must be greater than 0 g.",
    /* CalSaved */ "Calibration saved. New factor: ",
    /* CalFailedZeroFactor */ "Calibration failed: factor is 0.",
    /* CalFailedNegativeFactor */
    "Calibration failed: factor is negative (offset drift?).",
    /* CalValueUnreadable */ "Calibration value could not be read.",

    /* FaultStatePrefix */ "Fault state (code 0x",
    /* FaultStateSuffix */ "): motion commands locked, reboot required.",
    /* FaultPowerLow */ "Supply voltage too low",

    /* EmptyCommand */ "Empty command.",
    /* UnknownCommand */ "Unknown command.",
    /* InvalidValue */ "Invalid value.",
    /* HelpCommandsPrefix */
    "Available commands: en, off, zero, tare, force, pos, stop, ",
    /* HelpCommandsSuffix */
    "rpm, acc, feed, move, turn, jog up, jog down, cal_zero, cal_apply",
};

// DE is the ASCII transcription (FR-006) of the strings currently in use.
inline const char *const kMsgDe[] PROGMEM = {
    /* BootBanner */ "ESP32 Pruefmaschinen-Steuerung gestartet.",
    /* MotorsSyncNote */
    "Zwei Motoren laufen synchron ueber dieselben STEP/DIR-Signale (EN1/EN2 "
    "gemeinsam geschaltet).",
    /* StepperInitFailed */ "Stepper-Treiber konnte nicht angelegt werden.",
    /* Hx711ReadyCalLoaded */ "HX711 bereit. Kalibrierfaktor geladen: ",
    /* Hx711ReadyNoCal */ "HX711 bereit. Noch keine gueltige Kalibrierung "
                          "gespeichert.",
    /* Hx711NotReadyDisabled */
    "HX711 nicht bereit. Kraftmessung bleibt deaktiviert.",
    /* TaraSet */ "Tara gesetzt.",
    /* ApStarted */ "WLAN Access Point gestartet: ",
    /* ApIpSeparator */ " | IP ",
    /* ApPassword */ "Passwort: ",
    /* WebUiUrl */ "Weboberflaeche: http://",
    /* WebCommand */ "Web-Kommando: ",
    /* CmdParamMissing */ "Parameter 'cmd' fehlt.",

    /* DriversEnabled */ "Beide Treiber aktiviert.",
    /* DriversDisabled */ "Beide Treiber deaktiviert.",
    /* RpmSet */ "Drehzahl gesetzt auf ",
    /* AccelSet */ "Beschleunigung gesetzt auf ",
    /* FeedSet */ "Vorschub gesetzt auf ",
    /* RpmMustBePositive */ "RPM muss groesser als 0 sein.",
    /* AccelMustBePositive */ "ACC muss groesser als 0 sein.",
    /* FeedMustBePositive */ "Vorschub muss groesser als 0 sein.",
    /* RelativeMoveStarted */ "Relativbewegung gestartet: ",
    /* RevolutionsSuffix */ " Umdrehungen.",
    /* RpmSuffix */ " U/min.",
    /* AccelSuffix */ " U/min/s.",
    /* MillimetreSuffix */ " mm.",
    /* MillimetrePerMinuteSuffix */ " mm/min.",
    /* NewtonSuffix */ " N.",
    /* JogUpStarted */ "Jog nach oben gestartet.",
    /* JogDownStarted */ "Jog nach unten gestartet.",
    /* StopRequested */ "Stopp angefordert.",

    /* OriginZeroed */ "Achsposition auf 0 gesetzt.",
    /* LoadCellTared */ "Waegezelle tariert.",
    /* PositionPrinted */ "Position im Serial Monitor ausgegeben.",
    /* PositionSteps */ "Position [Schritte]: ",
    /* ForceCurrent */ "Kraft aktuell: ",
    /* StatusForceLabel */ " | Kraft [N]: ",
    /* StatusTravelLabel */ " | Weg [mm]: ",
    /* ForceUnavailable */ "Kraft: n/a (HX711 nicht bereit).",
    /* Hx711NotReady */ "HX711 nicht bereit.",
    /* NotAvailable */ "n/a",

    /* CalZeroCaptured */
    "Nullpunkt erfasst. Jetzt bekanntes Gewicht anhaengen, Grammwert eingeben "
    "und Schritt 2 ausfuehren.",
    /* CalStep1First */
    "Bitte zuerst Schritt 1 mit unbelasteter Waegezelle ausfuehren.",
    /* CalWeightMustBePositive */
    "Kalibriergewicht muss groesser als 0 g sein.",
    /* CalSaved */ "Kalibrierung gespeichert. Neuer Faktor: ",
    /* CalFailedZeroFactor */ "Kalibrierung fehlgeschlagen: Faktor ist 0.",
    /* CalFailedNegativeFactor */
    "Kalibrierung fehlgeschlagen: Faktor negativ (Offset drift?).",
    /* CalValueUnreadable */ "Kalibrierwert konnte nicht gelesen werden.",

    /* FaultStatePrefix */ "Fehlerzustand (Code 0x",
    /* FaultStateSuffix */ "): Motorkommandos gesperrt, Reboot noetig.",
    /* FaultPowerLow */ "Versorgungsspannung zu niedrig",

    /* EmptyCommand */ "Leeres Kommando.",
    /* UnknownCommand */ "Unbekanntes Kommando.",
    /* InvalidValue */ "Ungueltiger Wert.",
    /* HelpCommandsPrefix */
    "Verfuegbare Kommandos: en, off, zero, tare, force, pos, stop, ",
    /* HelpCommandsSuffix */
    "rpm, acc, feed, move, turn, jog up, jog down, cal_zero, cal_apply",
};

// Compile-time completeness gate: both tables must stay parallel with Msg.
static_assert(sizeof(kMsgEn) / sizeof(kMsgEn[0]) ==
                  static_cast<size_t>(Msg::Count),
              "kMsgEn is out of sync with Msg (TC-002)");
static_assert(sizeof(kMsgDe) / sizeof(kMsgDe[0]) ==
                  static_cast<size_t>(Msg::Count),
              "kMsgDe is out of sync with Msg (TC-002)");
static_assert(static_cast<size_t>(Msg::Count) <= 255,
              "Msg::Count exceeds the uint8_t id range");

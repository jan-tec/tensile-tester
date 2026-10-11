// M2 message-string tables (T002 skeleton): one id per user-facing string the
// firmware produces, plus the EN and DE columns. Host-testable (TC-002) and
// used by the target through the same header.
//
// Design (specs/M2-i18n-de-en/plan.md D2/D8):
// - `Msg` is the single source of user-facing text; call sites select an id,
//   they do not carry literals (grep gate SC-003, enforced in T008).
// - One {en, de} row per message (generated from the shared list below), EN
//   first (FR-001 default locale). No printf format strings with embedded
//   prose: values and units are concatenated by the caller, so a locale can
//   reorder or rename them (see the Prefix/Suffix and Unit ids below).
// - The table is header-inline (C++17) so the host test links it directly:
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

#include <FirmwareCore.h>  // Locale, resolveMsgTable (pure)

#include "ErrorCode.h"  // ErrorCode (pure) - fault presentation (M2-T004)

#ifndef PROGMEM
#define PROGMEM
#endif

// Single source of truth for user-facing text (#88): ONE X-macro list drives
// BOTH the `Msg` enum and the message table, one MSG(id, en, de) row per
// message. The id and its two strings live in the same row and in the same
// expansion, so the pairing is enforced by construction: inserting, moving or
// forgetting rows can never attach a message to a foreign id. (The old shape
// kept the enum and two position-aligned EN/DE arrays in sync by comment
// anchors alone - a reorder kept every count green while shifting text by
// one id. That drift class is what #88 closes.)
// Table budget: 8 KB EN+DE total (raised from M2's 4 KB when M3/T012 added
// the cycle-command rows; the real constraint is the 3 MB huge_app partition,
// which the full image does not approach - flash use ~28 %).
// C++20 designated initializers cannot express this on GCC: the standard
// excludes them for arrays and GCC - host g++ 16 and xtensa gcc 14, both
// verified live on this project - parses them as lambdas; clang alone ships
// the extension, so the X-macro is the portable enforcement.
// Comments inside the list must be block comments - a line comment would
// swallow the macro continuation backslash.
#define MSG_X_LIST(MSG) \
  /* Boot and hardware bring-up */ \
  MSG(BootBanner, \
      "ESP32 test machine controller started.", \
      "ESP32 Pruefmaschinen-Steuerung gestartet.") \
  MSG(MotorsSyncNote, \
      "Two motors run synchronously on the same STEP/DIR signals (EN1/EN2 " "switched together).", \
      "Zwei Motoren laufen synchron ueber dieselben STEP/DIR-Signale (EN1/EN2 " "gemeinsam geschaltet).") \
  MSG(StepperInitFailed, \
      "Stepper driver could not be created.", \
      "Stepper-Treiber konnte nicht angelegt werden.") \
  MSG(Hx711ReadyCalLoaded, \
      "HX711 ready. Calibration factor loaded: ", \
      "HX711 bereit. Kalibrierfaktor geladen: ")  /* prefix, factor appended */ \
  MSG(Hx711ReadyNoCal, \
      "HX711 ready. No valid calibration stored yet.", \
      "HX711 bereit. Noch keine gueltige Kalibrierung " "gespeichert.") \
  MSG(Hx711NotReadyDisabled, \
      "HX711 not ready. Force measurement stays disabled.", \
      "HX711 nicht bereit. Kraftmessung bleibt deaktiviert.") \
  MSG(TaraSet, "Tare set.", "Tara gesetzt.") \
  MSG(ApStarted, \
      "WiFi access point started: ", \
      "WLAN Access Point gestartet: ")  /* prefix, SSID appended */ \
  MSG(ApPassword, "Password: ", "Passwort: ")  /* prefix, password appended */ \
  MSG(WebUiUrl, \
      "Web UI: http://", \
      "Weboberflaeche: http://")  /* prefix, "http://" + IP appended */ \
  MSG(CmdParamMissing, "Parameter 'cmd' missing.", "Parameter 'cmd' fehlt.") \
  /* Driver and motion replies */ \
  MSG(DriversEnabled, "Both drivers enabled.", "Beide Treiber aktiviert.") \
  MSG(DriversDisabled, "Both drivers disabled.", "Beide Treiber deaktiviert.") \
  MSG(RpmSet, "Speed set to ", "Drehzahl gesetzt auf ")  /* prefix, value + unit appended */ \
  MSG(AccelSet, \
      "Acceleration set to ", \
      "Beschleunigung gesetzt auf ")  /* prefix, value + unit appended */ \
  MSG(FeedSet, "Feed set to ", "Vorschub gesetzt auf ")  /* prefix, value + unit appended */ \
  MSG(RpmMustBePositive, "RPM must be greater than 0.", "RPM muss groesser als 0 sein.") \
  MSG(AccelMustBePositive, "ACC must be greater than 0.", "ACC muss groesser als 0 sein.") \
  MSG(FeedMustBePositive, "Feed must be greater than 0.", "Vorschub muss groesser als 0 sein.") \
  MSG(RelativeMoveStarted, \
      "Relative move started: ", \
      "Relativbewegung gestartet: ")  /* prefix, value + unit appended */ \
  MSG(RevolutionsSuffix, " revolutions.", " Umdrehungen.") \
  MSG(RpmSuffix, " rpm.", " U/min.") \
  MSG(AccelSuffix, " rpm/s.", " U/min/s.") \
  MSG(MillimetreSuffix, " mm.", " mm.") \
  MSG(MillimetrePerMinuteSuffix, " mm/min.", " mm/min.") \
  MSG(NewtonSuffix, " N.", " N.") \
  MSG(JogUpStarted, "Jog up started.", "Jog nach oben gestartet.") \
  MSG(JogDownStarted, "Jog down started.", "Jog nach unten gestartet.") \
  MSG(StopRequested, "Stop requested.", "Stopp angefordert.") \
  /* Load cell and position replies */ \
  MSG(OriginZeroed, "Axis position set to 0.", "Achsposition auf 0 gesetzt.") \
  MSG(LoadCellTared, "Load cell tared.", "Waegezelle tariert.") \
  MSG(PositionPrinted, \
      "Position printed to the serial monitor.", \
      "Position im Serial Monitor ausgegeben.") \
  MSG(ForceCurrent, "Current force: ", "Kraft aktuell: ")  /* prefix, newtons appended */ \
  MSG(ForceUnavailable, "Force: n/a (HX711 not ready).", "Kraft: n/a (HX711 nicht bereit).") \
  MSG(Hx711NotReady, "HX711 not ready.", "HX711 nicht bereit.") \
  /* Calibration (two-step, M5 keeps this flow) */ \
  MSG(CalZeroCaptured, \
      "Zero point captured. Now attach a known weight, enter its gram value and " "run step 2.", \
      "Nullpunkt erfasst. Jetzt bekanntes Gewicht anhaengen, Grammwert eingeben " "und Schritt 2 ausfuehren.") \
  MSG(CalStep1First, \
      "Run step 1 first with the load cell unloaded.", \
      "Bitte zuerst Schritt 1 mit unbelasteter Waegezelle ausfuehren.") \
  MSG(CalWeightMustBePositive, \
      "Calibration weight must be greater than 0 g.", \
      "Kalibriergewicht muss groesser als 0 g sein.") \
  MSG(CalSaved, \
      "Calibration saved. New factor: ", \
      "Kalibrierung gespeichert. Neuer Faktor: ")  /* prefix, factor appended */ \
  MSG(CalFailedZeroFactor, \
      "Calibration failed: factor is 0.", \
      "Kalibrierung fehlgeschlagen: Faktor ist 0.") \
  MSG(CalFailedNegativeFactor, \
      "Calibration failed: factor is negative (offset drift?).", \
      "Kalibrierung fehlgeschlagen: Faktor negativ (Offset drift?).") \
  MSG(CalValueUnreadable, \
      "Calibration value could not be read.", \
      "Kalibrierwert konnte nicht gelesen werden.") \
  /* Fault presentation (M1 FR-009, US2). The hex code is formatted by the caller between the two halves, so no locale ever carries a format string */ \
  MSG(FaultStatePrefix, "Fault state (code 0x", "Fehlerzustand (Code 0x") \
  MSG(FaultStateSuffix, \
      "): motion commands locked, reboot required.", \
      "): Motorkommandos gesperrt, Reboot noetig.") \
  MSG(FaultPowerLow, "Supply voltage too low", "Versorgungsspannung zu niedrig") \
  MSG(FaultNvsOpen, "Settings storage (NVS) could not be opened.", \
      "Einstellungen-Speicher (NVS) konnte nicht geoeffnet werden.") \
  MSG(FaultNvsCorrupt, "Stored calibration values are not valid.", \
      "Gespeicherte Kalibrierungswerte sind ungueltig.") \
  MSG(FaultHx711NoVariation, \
      "HX711 showed no signal variation during selftest.", \
      "HX711 zeigte beim Selbsttest keine Aenderung.") \
  MSG(FaultMotorNoFeedback, \
      "Motor feedback cannot be verified (no sense wiring).", \
      "Motor-Rueckmeldung nicht pruefbar (keine Fuehlerleitung).") \
  MSG(FaultEndstopSeekTimeout, \
      "Homing seek timed out - endstop not reached.", \
      "Referenzfahrt-Timeout - Endschalter nicht erreicht.") \
  MSG(FaultEndstopInstantTrigger, \
      "Endstop already triggered at seek start (pressed or broken wire?).", \
      "Endschalter bei Suchstart bereits ausgeloest (betaetigt oder Drahtbruch?).") \
  MSG(FaultEndstopStuckSwitch, \
      "Endstop re-triggered too early (stuck switch?).", \
      "Endschalter loeste zu frueh erneut aus (Schalter klemmt?).") \
  MSG(FaultEndstopPressedOrBroken, \
      "Endstop pressed or wire broken - jog off the switch and re-run.", \
      "Endschalter betaetigt oder Drahtbruch - vom Schalter wegfahren und neu starten.") \
  MSG(FaultUnknown, "Unknown fault.", "Unbekannter Fehler.") \
  /* CLI generic */ \
  MSG(EmptyCommand, "Empty command.", "Leeres Kommando.") \
  MSG(UnknownCommand, "Unknown command.", "Unbekanntes Kommando.") \
  MSG(InvalidValue, "Invalid value.", "Ungueltiger Wert.") \
  MSG(LangSet, "Language set to ", "Sprache eingestellt auf ") \
  MSG(HelpCommandsPrefix, \
      "Available commands: en, off, zero, tare, force, pos, stop, ", \
      "Verfuegbare Kommandos: en, off, zero, tare, force, pos, stop, ") \
  MSG(HelpCommandsSuffix, \
      "rpm, acc, feed, move, turn, jog up, jog down, cal_zero, cal_apply, " \
      "help, home, home top, test, testspeed, testarm, testdrop, data, " \
      "testdata, homeseek, hometimeout, ends, lang", \
      "rpm, acc, feed, move, turn, jog up, jog down, cal_zero, cal_apply, " \
      "help, home, home top, test, testspeed, testarm, testdrop, data, " \
      "testdata, homeseek, hometimeout, ends, lang")  /* command names are locale-invariant */ \
  /* Test cycle (M3 FR-005/FR-006, T012). Composed prefix + value + the \
     existing unit suffix rows - no format strings. */ \
  MSG(HomeStarted, \
      "Homing to the bottom endstop started.", \
      "Referenzfahrt zum unteren Endschalter gestartet.") \
  MSG(HomeTopStarted, \
      "Moving to the top endstop started (position datum unchanged).", \
      "Fahrt zum oberen Endschalter gestartet (Nullpunkt unveraendert).") \
  MSG(CycleBusy, \
      "Rejected: a homing or test cycle is active.", \
      "Abgelehnt: eine Referenzfahrt oder ein Testlauf ist aktiv.") \
  MSG(TestStarted, "Test cycle started.", "Testlauf gestartet.") \
  MSG(TestAbortedNote, \
      "Test/homing stopped - data kept.", \
      "Test/Referenzfahrt gestoppt - Daten erhalten.") \
  MSG(DumpBusy, \
      "A data dump is already running.", \
      "Ein Datenausgabe laeuft bereits.") \
  MSG(ConfigLockedInFault, \
      "Locked while in fault state - reboot required.", \
      "Im Fehlerzustand gesperrt - Reboot erforderlich.") \
  MSG(DumpBlockedRunning, \
      "Not while a homing or test cycle is active.", \
      "Nicht waehrend Referenzfahrt oder Testlauf aktiv ist.") \
  MSG(TestdataPrefix, "Test data: ", "Testdaten: ") \
  MSG(DumpStarted, "Data dump started.", "Datenausgabe gestartet.") \
  MSG(PercentSuffix, " %", " %") \
  MSG(SecondsSuffix, " s.", " s.") \
  MSG(HomeSeekFollows, \
      "Homing seek follows the test speed.", \
      "Referenzfahrt folgt der Testgeschwindigkeit.") \
  MSG(ValueMustBePositive, \
      "Value must be greater than 0.", \
      "Wert muss groesser als 0 sein.")


#define MSG_ENUM(id, en, de) id,
enum class Msg : uint8_t {
  MSG_X_LIST(MSG_ENUM)
  // Number of message ids, always last.
  Count
};
#undef MSG_ENUM

// One row per message, EN first (FR-001 default locale) - same list, same
// order as the enum, so index N IS Msg id N by construction.
struct MsgPair {
  const char *en;
  const char *de;
};

#define MSG_ROW(id, en, de) {en, de},
inline const MsgPair kMsgRows[] PROGMEM = {MSG_X_LIST(MSG_ROW)};
#undef MSG_ROW
#undef MSG_X_LIST

// Completeness gate: structurally true with the shared list; kept as cheap
// protection against a future change to the generators above.
static_assert(sizeof(kMsgRows) / sizeof(kMsgRows[0]) ==
                  static_cast<size_t>(Msg::Count),
              "kMsgRows is out of sync with Msg (TC-002)");

// Accessors (plain indexing works everywhere: PROGMEM is a no-op on the host
// and on ESP32, unified address space). Header-inline like ErrorCode.h - the
// host test links them directly, src/*.cpp is not in the native build. The
// const data lands in .flash.rodata on ESP32: flash-only, RAM-safe, no
// relocation, no extra RAM (NFR).
inline const char *msgEn(Msg id) { return kMsgRows[static_cast<int>(id)].en; }
inline const char *msgDe(Msg id) { return kMsgRows[static_cast<int>(id)].de; }

// Current UI locale (M2 FR-002/FR-003). Static storage, factory default EN;
// App seeds it from Settings at boot, the `lang` command (serial + Web)
// switches it at runtime. All command handling runs on the Arduino loop
// task, so no synchronization is needed for this single writer.
namespace text_detail {
inline Locale &currentTextLocale() {
  static Locale locale = Locale::En;
  return locale;
}
}  // namespace text_detail

inline void setTextLocale(Locale locale) { text_detail::currentTextLocale() = locale; }
inline Locale textLocale() { return text_detail::currentTextLocale(); }

// Locale-aware user-facing text (plan D2/FR-004): call sites select an id,
// they never carry literals (SC-003). An empty DE entry falls back to EN
// (resolveMsgTable, TC-001); TC-002 pins every row non-empty, so the
// fallback only fires on deliberate half-translation during development.
inline const char *msg(Msg id) {
  const MsgPair &row = kMsgRows[static_cast<int>(id)];
  const bool deEntry = row.de != nullptr && row.de[0] != '\0';
  return resolveMsgTable(textLocale(), deEntry) == 1 ? row.de : row.en;
}

// M2-T004 (FR-007): stable ErrorCode -> presentable Msg. Pure mapping so the
// serial FR-009 line and /api/status localize through the same table; the
// codes themselves stay machine-readable and are never renumbered.
// Unknown/None codes map to FaultUnknown (presentation must never be empty
// when a fault is on display).
inline Msg msgForFault(ErrorCode code) {
  switch (code) {
    case ErrorCode::NvsOpenFailed:
      return Msg::FaultNvsOpen;
    case ErrorCode::NvsCalibrationCorrupt:
      return Msg::FaultNvsCorrupt;
    case ErrorCode::PowerLow:
      return Msg::FaultPowerLow;
    case ErrorCode::Hx711NoVariation:
      return Msg::FaultHx711NoVariation;
    case ErrorCode::MotorNoFeedback:
      return Msg::FaultMotorNoFeedback;
    case ErrorCode::EndstopSeekTimeout:
      return Msg::FaultEndstopSeekTimeout;
    case ErrorCode::EndstopInstantTrigger:
      return Msg::FaultEndstopInstantTrigger;
    case ErrorCode::EndstopStuckSwitch:
      return Msg::FaultEndstopStuckSwitch;
    case ErrorCode::EndstopPressedOrBroken:
      return Msg::FaultEndstopPressedOrBroken;
    default:
      return Msg::FaultUnknown;
  }
}

// DE rows are the ASCII transcription (FR-006: ae/oe/ue/ss) of the German
// strings in use. Web DE with native umlauts (FR-005) is owned by the JS
// dictionary in UiHtml.h (D5, T005), not by this table - keeping the DE
// column ASCII is what lets both surfaces share it.

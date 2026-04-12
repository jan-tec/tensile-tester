#include <Arduino.h>
#include <Preferences.h>
#include "HX711.h"

const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const float GRAVITY = 9.80665f;
const uint8_t AVERAGE_SAMPLES = 10;
const unsigned long MAX_TEST_DURATION_MS = 5000;

// Fallback-Wert fuer den Fall, dass noch nichts im Flash gespeichert wurde.
const float DEFAULT_CALIBRATION_FACTOR = 1.0f;

HX711 scale;
Preferences preferences;

float calibrationFactor = DEFAULT_CALIBRATION_FACTOR;
bool calibrationValid = false;
bool continuousPlotMode = false;

bool readLineFromSerial(String &input) {
  if (Serial.available() == 0) {
    return false;
  }

  input = Serial.readStringUntil('\n');
  input.trim();
  return input.length() > 0;
}

float waitForFloatFromSerial() {
  String input;

  while (true) {
    if (readLineFromSerial(input)) {
      input.replace(',', '.');
      return input.toFloat();
    }
    delay(10);
  }
}

void waitForEnterFromSerial() {
  String input;

  while (!readLineFromSerial(input)) {
    delay(10);
  }
}

bool waitForHx711() {
  if (scale.wait_ready_timeout(2000)) {
    Serial.println("HX711 gefunden und bereit.");
    return true;
  }

  Serial.println("Fehler: HX711 nicht gefunden. Verkabelung pruefen!");
  return false;
}

void loadCalibrationFactor() {
  preferences.begin("loadcell", true);
  calibrationFactor = preferences.getFloat("factor", DEFAULT_CALIBRATION_FACTOR);
  calibrationValid = preferences.getBool("valid", false);
  preferences.end();

  if (calibrationValid) {
    scale.set_scale(calibrationFactor);
    Serial.print("Gespeicherter Kalibrierfaktor geladen: ");
    Serial.println(calibrationFactor, 6);
  } else {
    scale.set_scale(DEFAULT_CALIBRATION_FACTOR);
    Serial.println("Noch kein gueltiger Kalibrierfaktor gespeichert.");
  }
}

void saveCalibrationFactor(float factor) {
  preferences.begin("loadcell", false);
  preferences.putFloat("factor", factor);
  preferences.putBool("valid", true);
  preferences.end();

  calibrationFactor = factor;
  calibrationValid = true;
  scale.set_scale(calibrationFactor);
}

void printMenu() {
  Serial.println();
  Serial.println("=== Testmenue ===");
  Serial.println("1 - Waegezelle kalibrieren");
  Serial.println("2 - 5 Sekunden messen und maximale Kraft ausgeben");
  Serial.println("3 - Kraft dauerhaft plotten");
  Serial.println("4 - Tara neu setzen");
  Serial.println("m - Menue erneut anzeigen");
  Serial.println();
}

void printCalibrationSnippet(float factor) {
  Serial.println("Zum festen Hinterlegen im Code kannst du diese Zeile uebernehmen:");
  Serial.print("const float DEFAULT_CALIBRATION_FACTOR = ");
  Serial.print(factor, 6);
  Serial.println("f;");
}

bool ensureCalibrationAvailable() {
  if (calibrationValid) {
    return true;
  }

  Serial.println("Es ist noch kein gueltiger Kalibrierfaktor vorhanden.");
  Serial.println("Bitte zuerst Test 1 zur Kalibrierung ausfuehren.");
  return false;
}

void tareScale() {
  Serial.println("Waegezelle entlasten und ENTER senden.");
  waitForEnterFromSerial();
  scale.tare(AVERAGE_SAMPLES);
  Serial.println("Tara gesetzt.");
}

void runCalibration() {
  continuousPlotMode = false;

  Serial.println();
  Serial.println("--- Kalibrierung ---");
  tareScale();

  Serial.println("Bekanntes Gewicht auflegen.");
  Serial.println("Gewicht in Gramm eingeben und mit ENTER bestaetigen.");

  const float knownMassGrams = waitForFloatFromSerial();
  if (knownMassGrams <= 0.0f) {
    Serial.println("Ungueltige Eingabe. Kalibrierung abgebrochen.");
    return;
  }

  delay(1000);
  const float rawDelta = scale.get_value(AVERAGE_SAMPLES);
  const float factor = rawDelta / knownMassGrams;

  if (factor == 0.0f) {
    Serial.println("Kalibrierung fehlgeschlagen: Faktor ist 0.");
    return;
  }

  saveCalibrationFactor(factor);

  Serial.println("Kalibrierung abgeschlossen.");
  Serial.print("Neuer Kalibrierfaktor [counts/g]: ");
  Serial.println(calibrationFactor, 6);
  Serial.println("Faktor wurde im ESP32 gespeichert.");
  printCalibrationSnippet(calibrationFactor);
}

float readForceNewton() {
  const float massGrams = scale.get_units(AVERAGE_SAMPLES);
  return (massGrams / 1000.0f) * GRAVITY;
}

void runMaxForceTest() {
  continuousPlotMode = false;

  if (!ensureCalibrationAvailable()) {
    return;
  }

  Serial.println();
  Serial.println("--- 5-Sekunden-Test ---");
  Serial.println("Messung startet jetzt.");

  const unsigned long startMs = millis();
  float maxForceNewton = 0.0f;

  while (millis() - startMs < MAX_TEST_DURATION_MS) {
    if (!scale.is_ready()) {
      Serial.println("HX711 nicht bereit...");
      delay(100);
      continue;
    }

    const float forceNewton = readForceNewton();
    if (forceNewton > maxForceNewton) {
      maxForceNewton = forceNewton;
    }

    Serial.print("Kraft_N:");
    Serial.println(forceNewton, 4);
    delay(100);
  }

  Serial.print("Maximale Kraft in 5 Sekunden [N]: ");
  Serial.println(maxForceNewton, 4);
}

void startContinuousPlot() {
  if (!ensureCalibrationAvailable()) {
    return;
  }

  continuousPlotMode = true;
  Serial.println();
  Serial.println("--- Plotmodus aktiv ---");
  Serial.println("Es wird fortlaufend Kraft_N ausgegeben.");
  Serial.println("Mit 'm' und ENTER blendest du das Menue wieder ein.");
}

void handleMenuCommand(const String &command) {
  if (command == "1") {
    runCalibration();
    printMenu();
    return;
  }

  if (command == "2") {
    runMaxForceTest();
    printMenu();
    return;
  }

  if (command == "3") {
    startContinuousPlot();
    return;
  }

  if (command == "4") {
    continuousPlotMode = false;
    tareScale();
    printMenu();
    return;
  }

  if (command == "m" || command == "M") {
    continuousPlotMode = false;
    printMenu();
    return;
  }

  if (command.length() > 0) {
    Serial.println("Unbekannter Befehl.");
    printMenu();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- HX711 Kalibrierung und Testmenue ---");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (!waitForHx711()) {
    return;
  }

  loadCalibrationFactor();
  printMenu();
}

void loop() {
  String command;
  if (readLineFromSerial(command)) {
    handleMenuCommand(command);
  }

  if (continuousPlotMode) {
    if (scale.is_ready()) {
      Serial.print("Kraft_N:");
      Serial.println(readForceNewton(), 4);
    } else {
      Serial.println("HX711 nicht bereit...");
    }

    delay(100);
  } else {
    delay(10);
  }
}

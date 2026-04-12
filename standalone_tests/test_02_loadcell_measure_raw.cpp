#include <Arduino.h>
#include "HX711.h"

// Pins (wie besprochen)
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;

HX711 scale;

void setup() {
  Serial.begin(115200);
  Serial.println("--- HX711 Rohwert-Test ---");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // Kurzer Check, ob der Chip antwortet
  if (scale.wait_ready_timeout(2000)) {
    Serial.println("HX711 gefunden und bereit.");
  } else {
    Serial.println("Fehler: HX711 nicht gefunden. Verkabelung prüfen!");
  }
}

void loop() {
  if (scale.is_ready()) {
    // read() liefert den direkten 24-Bit Wert vom ADC
    long reading = scale.read();
    Serial.print("Rohwert: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711 nicht bereit...");
  }
  
  delay(250); // 4 Messungen pro Sekunde zum Testen
}
#include <Arduino.h>

// Die meisten ESP32 Dev Boards haben die LED an Pin 2
#define LED_PIN 2

void setup() {
    // Initialisiere die serielle Kommunikation
    Serial.begin(115200);
    
    // Setze den LED-Pin als Ausgang
    pinMode(LED_PIN, OUTPUT);
    
    Serial.println("--- ESP32 Testlauf gestartet ---");
}

void loop() {
    // LED einschalten
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ist AN");
    delay(1000); // 1 Sekunde warten
    
    // LED ausschalten
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED ist AUS");
    delay(1000); // 1 Sekunde warten
}
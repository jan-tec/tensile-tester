#include "App.h"

void App::begin() {
  Serial.begin(115200);

  motion_.begin();

  delay(500);
  Serial.println();
  Serial.println("ESP32 Pruefmaschinen-Steuerung gestartet.");
  Serial.println("Zwei Motoren laufen synchron ueber dieselben STEP/DIR/EN-Signale.");

  loadcell_.begin(settings_);
  webUi_.begin();
}

void App::loop() {
  webUi_.handleClient();
  loadcell_.poll();
  serialCli_.poll();

  const unsigned long now = millis();
  if (now - lastStatusMs_ >= kStatusIntervalMs) {
    lastStatusMs_ = now;
    if (motion_.isEnabled() || loadcell_.isReady()) {
      commands_.printPosition();
    }
  }
}

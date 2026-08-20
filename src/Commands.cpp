#include "Commands.h"

const char *Commands::helpText() {
  return "Verfuegbare Kommandos: en, off, zero, tare, force, pos, stop, "
         "rpm, acc, feed, move, turn, jog up, jog down, cal_zero, cal_apply";
}

void Commands::printPosition() {
  Serial.print("Position [Schritte]: ");
  Serial.print(motion_.steps());
  Serial.print(" | Weg [mm]: ");
  Serial.print(motion_.positionMm(), 3);
  Serial.print(" | Kraft [N]: ");
  Serial.println(loadcell_.forceNewton(), 2);
}

bool Commands::parseValueAfterCommand(const String &command,
                                      const char *prefix, float &value) const {
  // jane: parseCommandValue buffers 47-char numbers; the legacy String
  // version had no limit, but no realistic command (float has ~7
  // significant digits) can observe the difference.
  return parseCommandValue(command.c_str(), prefix, &value);
}

bool Commands::execute(String command, String &response) {
  command.trim();
  if (command.isEmpty()) {
    response = "Leeres Kommando.";
    return false;
  }

  const Command named = commandFromName(command.c_str());

  if (named == Command::Enable) {
    motion_.enable();
    response = "Beide Treiber aktiviert.";
    return true;
  }

  if (named == Command::Disable) {
    motion_.disable();
    response = "Beide Treiber deaktiviert.";
    return true;
  }

  if (named == Command::Zero) {
    motion_.zero();
    response = "Achsposition auf 0 gesetzt.";
    return true;
  }

  if (named == Command::Tare) {
    loadcell_.tare();
    response =
        loadcell_.isReady() ? "Waegezelle tariert." : "HX711 nicht bereit.";
    return loadcell_.isReady();
  }

  if (named == Command::CalZero) {
    if (!loadcell_.captureZeroPoint()) {
      response = "HX711 nicht bereit.";
      return false;
    }
    response = "Nullpunkt erfasst. Jetzt bekanntes Gewicht anhaengen, Grammwert eingeben und Schritt 2 ausfuehren.";
    return true;
  }

  float value = 0.0f;

  if (parseValueAfterCommand(command, "cal_apply", value)) {
    switch (loadcell_.calibrateWithMass(value)) {
      case LoadCell::CalError::NotReady:
        response = "HX711 nicht bereit.";
        return false;
      case LoadCell::CalError::ZeroNotCaptured:
        response =
            "Bitte zuerst Schritt 1 mit unbelasteter Waegezelle ausfuehren.";
        return false;
      case LoadCell::CalError::InvalidMass:
        response = "Kalibriergewicht muss groesser als 0 g sein.";
        return false;
      case LoadCell::CalError::ReadFailed:
        response = "Kalibrierwert konnte nicht gelesen werden.";
        return false;
      case LoadCell::CalError::ZeroFactor:
        response = "Kalibrierung fehlgeschlagen: Faktor ist 0.";
        return false;
      case LoadCell::CalError::None:
        break;
    }
    response = "Kalibrierung gespeichert. Neuer Faktor: " +
               String(loadcell_.calibrationFactor(), 6);
    return true;
  }

  if (named == Command::Force) {
    response = "Kraft aktuell: " + String(loadcell_.forceNewton(), 2) + " N.";
    return true;
  }

  if (named == Command::Stop) {
    motion_.stop();
    response = "Stopp angefordert.";
    return true;
  }

  if (named == Command::Pos) {
    printPosition();
    response = "Position im Serial Monitor ausgegeben.";
    return true;
  }

  if (parseValueAfterCommand(command, "rpm", value)) {
    if (value <= 0.0f) {
      response = "RPM muss groesser als 0 sein.";
      return false;
    }

    motion_.setOutputRpm(value);
    response = "Drehzahl gesetzt auf " + String(motion_.outputRpm(), 2) +
               " U/min.";
    return true;
  }

  if (parseValueAfterCommand(command, "acc", value)) {
    if (value <= 0.0f) {
      response = "ACC muss groesser als 0 sein.";
      return false;
    }

    motion_.setAccelRpmPerS(value);
    response = "Beschleunigung gesetzt auf " +
               String(motion_.accelRpmPerS(), 2) + " U/min/s.";
    return true;
  }

  if (parseValueAfterCommand(command, "feed", value)) {
    if (value <= 0.0f) {
      response = "Vorschub muss groesser als 0 sein.";
      return false;
    }

    const float clampedFeed = clampFeedMmPerMin(value);
    motion_.setOutputRpm(feedMmPerMinToRpm(clampedFeed));
    response = "Vorschub gesetzt auf " + String(clampedFeed, 2) + " mm/min.";
    return true;
  }

  if (named == Command::JogUp) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + spindleMmToSteps(kJogTravelMm)));
    response = "Jog nach oben gestartet.";
    return true;
  }

  if (named == Command::JogDown) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() - spindleMmToSteps(kJogTravelMm)));
    response = "Jog nach unten gestartet.";
    return true;
  }

  if (parseValueAfterCommand(command, "move", value)) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + spindleMmToSteps(value)));
    response = "Relativbewegung gestartet: " + String(value, 3) + " mm.";
    return true;
  }

  if (parseValueAfterCommand(command, "turn", value)) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + outputTurnsToSteps(value)));
    response = "Relativbewegung gestartet: " + String(value, 3) +
               " Umdrehungen.";
    return true;
  }

  response = "Unbekanntes Kommando.";
  return false;
}

#include "SerialCli.h"

void SerialCli::poll() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      String response;
      const bool ok = commands_.execute(buffer_, response);
      Serial.println(response);
      if (!ok) {
        Serial.println(Commands::helpText());
      }
      buffer_ = "";
      continue;
    }

    buffer_ += incoming;
  }
}

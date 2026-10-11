#include "SerialCli.h"

void SerialCli::poll() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      String response;
      ErrorCode error;  // M6-T005: consumed by the T006 envelope
      PosData pos;
      const bool ok = commands_.execute(buffer_, response, error, pos);
      if (pos.valid) {
        // M6-T005 (plan D2): the surface renders the position block the
        // policy computed (byte-identical to the old policy-side print).
        Serial.print(Commands::positionText(pos));
      }
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

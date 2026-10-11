#include "SerialCli.h"

void SerialCli::poll() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      String response;
      ErrorCode error;
      ApiReplyMeta meta;
      const bool ok = commands_.execute(buffer_, response, error, meta);
      // M6-T006 (#182, plan D2/FR-001): the surface renders - one
      // envelope line per command, one state snapshot taken AFTER the
      // policy ran, single write + flush (TC-005).
      const ApiStateSnapshot snap = commands_.stateSnapshot();
      char data[640];
      char line[768];  // help is the documented >256 B exception (D6)
      if (!ok) {
        formatApiError(snap, static_cast<uint16_t>(error),
                       response.c_str(), line, sizeof(line));
      } else {
        switch (meta.kind) {
          case ApiDataKind::Help:
            formatApiHelpData(data, sizeof(data));
            break;
          case ApiDataKind::Pos:
            snprintf(data, sizeof(data), "{\"steps\":%ld}", meta.steps);
            break;
          default:
            formatApiReplyData(response.c_str(), data, sizeof(data));
            break;
        }
        formatApiSuccess(snap, data, line, sizeof(line));
      }
      Serial.println(line);
      Serial.flush();  // TC-005: the line completes before the next one
      buffer_ = "";
      continue;
    }

    buffer_ += incoming;
  }
}

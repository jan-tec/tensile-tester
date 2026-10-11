#ifndef API_RESPONSE_H
#define API_RESPONSE_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

// M6 FR-001/FR-002 (TC-001/TC-002, #178): the JSON-lines response
// envelope for the serial API. Pure C++ - no Arduino, no String, no
// PROGMEM - host-tested with exact-string pins so the wire format is a
// contract, not an accident.
//
// Grammar (stable; newline is added by the transport, not here):
//   reply:   {"ok":true,"data":<fragment>,"state":{...}}
//            {"ok":false,"error":{"code":<dec>,"message":"..."},"state":{...}}
//   event:   {"event":"<name>","state":{...}}
//   state:   {"posMm":<dec3|null>,"forceN":<dec2|null>,"calValid":<bool>,
//             "cycleState":"<name>","faultCode":<dec>,"homed":<bool>}
//
// Conventions:
// - key order is fixed (FR-001); codes are decimal ints (the padded-hex
//   grammar stays a LOG grammar, ErrorCode.h FR-007)
// - numbers are locale-neutral printf decimals (C locale on host and
//   target; pinned by tests)
// - posMm is null while unhomed, forceN null while the cell is not
//   ready (the old "n/a" prose, FR-002)
// - message text is escaped (quote, backslash, control chars); data
//   fragments and cycleState are embedded verbatim - caller supplies
//   valid compact JSON / identifier text
// - buffer contract is snprintf's: returns the would-be length, >= cap
//   signals truncation (never a silent partial line)

struct ApiStateSnapshot {
  float posMm;             // meaningful only when homed
  float forceN;            // meaningful only when cellReady
  bool cellReady;          // HX711 ready
  bool calValid;           // stored calibration factor valid
  bool homed;              // datum established this power cycle
  uint16_t faultCode;      // 0 = none (stable vocabulary, ErrorCode.h)
  const char* cycleState;  // TestCycleModel::name() - static lifetime
};

namespace api_detail {

// Cursor builder with snprintf truncation semantics: keeps measuring the
// would-be total after the buffer is full, writes only what fits.
class LineBuilder {
 public:
  LineBuilder(char* buf, size_t cap) : buf_(buf), cap_(cap), len_(0), err_(false) {}

  void put(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const size_t pos = len_ < cap_ ? len_ : (cap_ ? cap_ - 1 : 0);
    char* dst = (cap_ > 0) ? buf_ + pos : nullptr;
    const int n = vsnprintf(dst, cap_ > 0 ? cap_ - pos : 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
      err_ = true;
      return;
    }
    len_ += static_cast<size_t>(n);
  }

  void putEscaped(const char* s) {
    if (s == nullptr) {
      return;
    }
    for (const char* p = s; *p != '\0'; ++p) {
      const unsigned char c = static_cast<unsigned char>(*p);
      switch (c) {
        case '"':
          put("\\\"");
          break;
        case '\\':
          put("\\\\");
          break;
        case '\n':
          put("\\n");
          break;
        case '\r':
          put("\\r");
          break;
        case '\t':
          put("\\t");
          break;
        default:
          if (c < 0x20u) {
            put("\\u%04X", c);
          } else {
            put("%c", static_cast<char>(c));
          }
      }
    }
  }

  int finish() const { return err_ ? -1 : static_cast<int>(len_); }

 private:
  char* buf_;
  size_t cap_;
  size_t len_;
  bool err_;
};

inline void putState(LineBuilder& b, const ApiStateSnapshot& s) {
  b.put("{\"posMm\":");
  if (s.homed) {
    b.put("%.3f", static_cast<double>(s.posMm));
  } else {
    b.put("null");
  }
  b.put(",\"forceN\":");
  if (s.cellReady) {
    b.put("%.2f", static_cast<double>(s.forceN));
  } else {
    b.put("null");
  }
  b.put(",\"calValid\":%s,\"cycleState\":\"%s\",\"faultCode\":%u,"
        "\"homed\":%s}",
        s.calValid ? "true" : "false",
        s.cycleState != nullptr ? s.cycleState : "?",
        static_cast<unsigned>(s.faultCode), s.homed ? "true" : "false");
}

}  // namespace api_detail

// Success reply. dataJson: compact JSON fragment ("{}", an object, an
// array for help, or "null" when the command carries no data payload).
inline int formatApiSuccess(const ApiStateSnapshot& s, const char* dataJson,
                            char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("{\"ok\":true,\"data\":%s,\"state\":",
        dataJson != nullptr ? dataJson : "null");
  api_detail::putState(b, s);
  b.put("}");
  return b.finish();
}

// Error reply. code: stable vocabulary as a decimal int (0x06xx API
// block or a fault code surfaced through a rejection).
inline int formatApiError(const ApiStateSnapshot& s, uint16_t code,
                          const char* message, char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("{\"ok\":false,\"error\":{\"code\":%u,\"message\":\"",
        static_cast<unsigned>(code));
  b.putEscaped(message != nullptr ? message : "");
  b.put("\"},\"state\":");
  api_detail::putState(b, s);
  b.put("}");
  return b.finish();
}

// Async event line (D3): periodic status, web-command echo. Parsers
// distinguish events from replies by the leading "event" key.
inline int formatApiEvent(const char* event, const ApiStateSnapshot& s,
                          char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("{\"event\":\"%s\",\"state\":", event != nullptr ? event : "?");
  api_detail::putState(b, s);
  b.put("}");
  return b.finish();
}

// The state object alone (FR-002 single snapshot; also the periodic
// {"event":"status"} payload).
inline int formatApiStateJson(const ApiStateSnapshot& s, char* out,
                              size_t cap) {
  api_detail::LineBuilder b(out, cap);
  api_detail::putState(b, s);
  return b.finish();
}

// M6 FR-005 (TC-004, #179): the `data` bulk-dump framing. The M3 CSV
// grammar ( TestData.h formatCsvChunk) stays byte-identical and is the
// BODY; these lines wrap it - header first (reply to the `data`
// command), footer after the END marker. Newlines belong to the
// transport. `truncated` renders 0|1 for symmetry with the CSV END
// marker; `format` tags the body grammar so parsers can dispatch.
inline int formatApiDumpHeader(const ApiStateSnapshot& s, size_t count,
                               size_t fromIndex, char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("{\"ok\":true,\"data\":{\"dump\":\"begin\",\"count\":%u,\"from\":%u,"
        "\"format\":\"csv-m3\"},\"state\":",
        static_cast<unsigned>(count), static_cast<unsigned>(fromIndex));
  api_detail::putState(b, s);
  b.put("}");
  return b.finish();
}

inline int formatApiDumpFooter(const ApiStateSnapshot& s, size_t sent,
                               bool truncated, char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("{\"ok\":true,\"data\":{\"dump\":\"end\",\"sent\":%u,"
        "\"truncated\":%d},\"state\":",
        static_cast<unsigned>(sent), truncated ? 1 : 0);
  api_detail::putState(b, s);
  b.put("}");
  return b.finish();
}

// M6 FR-003 (TC-003, #180): the API command surface. Order is the
// `help` order. MUST stay in sync with the dispatch truth
// (FirmwareCore commandFromName + the prefix value parsers + the
// Commands.cpp specials) - the T008 bench sweep is the drift catch.
// Phasing (plan D5): calibrate/cal_next (M5) and chold/choltol/
// choldclear (M4) join this table in their milestones, envelope-native.
struct ApiCommandInfo {
  const char* name;  // wire name, exact match
  const char* args;  // argument hint for help; "" when value-less
};

inline constexpr ApiCommandInfo kApiCommands[] = {
    {"en", ""},            {"off", ""},
    {"zero", ""},          {"tare", ""},
    {"pos", ""},           {"force", ""},
    {"stop", ""},          {"rpm", "<U/min>"},
    {"acc", "<U/min/s>"},  {"feed", "<mm/min>"},
    {"move", "<mm>"},      {"turn", "<rev>"},
    {"jog up", ""},        {"jog down", ""},
    {"help", ""},          {"cal_zero", ""},
    {"cal_apply", "<grams>"}, {"home", ""},
    {"home top", ""},      {"test", ""},
    {"testspeed", "<mm/min>"}, {"testarm", "<N>"},
    {"testdrop", "<%>"},   {"data", "[from]"},
    {"testdata", ""},      {"homeseek", "<mm/min>"},
    {"hometimeout", "<s>"}, {"ends", ""},
    {"lang", "<en|de>"},
};

inline constexpr size_t kApiCommandCount =
    sizeof(kApiCommands) / sizeof(kApiCommands[0]);

inline size_t apiCommandCount() { return kApiCommandCount; }

inline const ApiCommandInfo& apiCommandAt(size_t i) {
  return kApiCommands[i < kApiCommandCount ? i : kApiCommandCount - 1];
}

// D6: the help data payload - a JSON array of "name args" strings. This
// is the documented >256 B exception once wrapped in the envelope
// (FR-003/NFR); callers size the buffer accordingly.
inline int formatApiHelpData(char* out, size_t cap) {
  api_detail::LineBuilder b(out, cap);
  b.put("[");
  for (size_t i = 0; i < kApiCommandCount; ++i) {
    b.put("%s\"%s%s%s\"", i > 0 ? "," : "", kApiCommands[i].name,
          kApiCommands[i].args[0] != '\0' ? " " : "",
          kApiCommands[i].args);
  }
  b.put("]");
  return b.finish();
}

#endif  // API_RESPONSE_H

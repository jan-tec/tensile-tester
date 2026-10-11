// M2 (T002): compile-site for Text.h in the firmware build.
//
// The header carries the Msg enum and the inline EN/DE tables; nothing routes
// call sites through them until T003. This TU makes the ESP32 toolchain
// compile the header (and its TC-002 static_asserts) now, so the C++17
// inline-variable tables and the enum/table sync are proven by the build gate
// instead of surfacing in T003.
#include "Text.h"

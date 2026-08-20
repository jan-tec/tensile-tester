// Single Unity entry point for the env:native test binary.
// PlatformIO links every file in test/ into ONE program, so exactly one
// main()/setUp()/tearDown() must exist; each test file exposes a
// run*Tests() function that this file invokes.
// Run: pio test -e native
#include <unity.h>

extern void runGearMathTests();
extern void runCommandParserTests();

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  runGearMathTests();
  runCommandParserTests();
  return UNITY_END();
}

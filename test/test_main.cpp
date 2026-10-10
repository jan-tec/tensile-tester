// Single Unity entry point for the env:native test binary.
// PlatformIO links every file in test/ into ONE program, so exactly one
// main()/setUp()/tearDown() must exist; each test file exposes a
// run*Tests() function that this file invokes.
// Run: pio test -e native
#include <unity.h>

extern void runGearMathTests();
extern void runBreakDetectTests();
extern void runHomingModelTests();
extern void runCommandParserTests();
extern void runCalibrationTests();
extern void runDeviceModelTests();
extern void runHealthEvalTests();
extern void runErrorCodeTests();
extern void runLocaleTests();
extern void runTextTableTests();
extern void runConfigTests();
extern void runUiHtmlTests();

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  runGearMathTests();
  runBreakDetectTests();
  runHomingModelTests();
  runCommandParserTests();
  runCalibrationTests();
  runDeviceModelTests();
  runHealthEvalTests();
  runErrorCodeTests();
  runLocaleTests();
  runTextTableTests();
  runConfigTests();
  runUiHtmlTests();
  return UNITY_END();
}

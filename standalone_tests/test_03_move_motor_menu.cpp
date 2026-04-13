#include <Arduino.h>
#include <AccelStepper.h>

namespace {
constexpr uint8_t DIR_PIN = 19;
constexpr uint8_t STEP_PIN = 18;
constexpr uint8_t EN_PIN = 5;

constexpr long MOTOR_FULL_STEPS_PER_REV = 200;
constexpr float GEAR_RATIO = 13.73f;
// Wenn "turn 1" etwa doppelt so weit faehrt, ist der Treiber oft auf 1/8 statt
// 1/16 Microstepping eingestellt.
constexpr uint8_t MICROSTEPS = 8;

constexpr float OUTPUT_STEPS_PER_REV =
    MOTOR_FULL_STEPS_PER_REV * GEAR_RATIO * MICROSTEPS;
constexpr float DEFAULT_OUTPUT_RPM = 5.0f;
constexpr float DEFAULT_OUTPUT_ACCEL_RPM_PER_S = 20.0f;
constexpr long DEFAULT_SWEEP_DEGREES = 90;
constexpr unsigned long STATUS_INTERVAL_MS = 500;

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

float configuredOutputRpm = DEFAULT_OUTPUT_RPM;
float configuredOutputAccelRpmPerS = DEFAULT_OUTPUT_ACCEL_RPM_PER_S;
bool driverEnabled = false;
bool autoSweepEnabled = false;
long sweepDegrees = DEFAULT_SWEEP_DEGREES;
long sweepTargetSteps = 0;
int sweepDirection = 1;
unsigned long lastStatusMs = 0;

float outputRpmToStepsPerSecond(float rpm) {
  return (rpm * OUTPUT_STEPS_PER_REV) / 60.0f;
}

float outputAccelToStepsPerSecondSq(float rpmPerSecond) {
  return (rpmPerSecond * OUTPUT_STEPS_PER_REV) / 60.0f;
}

long outputDegreesToSteps(float degrees) {
  return lroundf((degrees / 360.0f) * OUTPUT_STEPS_PER_REV);
}

void applyMotionSettings() {
  stepper.setMaxSpeed(outputRpmToStepsPerSecond(configuredOutputRpm));
  stepper.setAcceleration(
      outputAccelToStepsPerSecondSq(configuredOutputAccelRpmPerS));
}

void enableDriver() {
  digitalWrite(EN_PIN, LOW);
  driverEnabled = true;
}

void disableDriver() {
  stepper.stop();
  digitalWrite(EN_PIN, HIGH);
  driverEnabled = false;
}

void printHelp() {
  Serial.println();
  Serial.println("=== NEMA17 / TMC2209 Steuerung ===");
  Serial.println("Pins: DIR=D19 STEP=D18 EN=D5");
  Serial.println("Kommandos:");
  Serial.println("  h                - Hilfe anzeigen");
  Serial.println("  en               - Treiber aktivieren");
  Serial.println("  off              - Treiber deaktivieren");
  Serial.println("  rpm <wert>       - Drehzahl der Getriebe-Ausgangswelle setzen");
  Serial.println("  acc <wert>       - Beschleunigung in U/min pro Sekunde setzen");
  Serial.println("  move <grad>      - Relativbewegung der Ausgangswelle in Grad");
  Serial.println("  turn <umdrehung> - Relativbewegung der Ausgangswelle in Umdrehungen");
  Serial.println("  zero             - Aktuelle Position auf 0 setzen");
  Serial.println("  pos              - Position ausgeben");
  Serial.println("  sweep <grad>     - Dauerhaft zwischen +/- Grad pendeln");
  Serial.println("  stop             - Bewegung sanft stoppen");
  Serial.println();
  Serial.print("Berechnete Schritte/Umdrehung Ausgangswelle: ");
  Serial.println(OUTPUT_STEPS_PER_REV, 1);
  Serial.println();
}

void printPosition() {
  const long currentSteps = stepper.currentPosition();
  const float outputDegrees =
      (static_cast<float>(currentSteps) / OUTPUT_STEPS_PER_REV) * 360.0f;
  Serial.print("Position [Schritte]: ");
  Serial.print(currentSteps);
  Serial.print(" | Ausgangswelle [Grad]: ");
  Serial.println(outputDegrees, 2);
}

void startMoveTo(long targetSteps) {
  enableDriver();
  autoSweepEnabled = false;
  stepper.moveTo(targetSteps);

  Serial.print("Neues Ziel [Schritte]: ");
  Serial.println(targetSteps);
}

void startRelativeMoveDegrees(float degrees) {
  const long relativeSteps = outputDegreesToSteps(degrees);
  startMoveTo(stepper.currentPosition() + relativeSteps);

  Serial.print("Relativbewegung [Grad]: ");
  Serial.println(degrees, 2);
}

void startRelativeMoveTurns(float turns) {
  startRelativeMoveDegrees(turns * 360.0f);
}

void startSweep(long degrees) {
  sweepDegrees = max(1L, labs(degrees));
  sweepTargetSteps = outputDegreesToSteps(static_cast<float>(sweepDegrees));
  sweepDirection = 1;
  autoSweepEnabled = true;
  enableDriver();
  stepper.moveTo(sweepTargetSteps);

  Serial.print("Pendelmodus aktiv +/- ");
  Serial.print(sweepDegrees);
  Serial.println(" Grad.");
}

void stopMotion() {
  autoSweepEnabled = false;
  stepper.stop();
  Serial.println("Stopp angefordert.");
}

bool parseValueAfterCommand(const String &command, const char *prefix,
                            float &value) {
  if (!command.startsWith(prefix)) {
    return false;
  }

  String number = command.substring(strlen(prefix));
  number.trim();
  number.replace(',', '.');

  if (number.isEmpty()) {
    return false;
  }

  value = number.toFloat();
  return true;
}

void handleCommand(String command) {
  command.trim();

  if (command.isEmpty()) {
    return;
  }

  if (command == "h") {
    printHelp();
    return;
  }

  if (command == "en") {
    enableDriver();
    Serial.println("Treiber aktiviert.");
    return;
  }

  if (command == "off") {
    disableDriver();
    autoSweepEnabled = false;
    Serial.println("Treiber deaktiviert.");
    return;
  }

  if (command == "zero") {
    stepper.setCurrentPosition(0);
    Serial.println("Position auf 0 gesetzt.");
    return;
  }

  if (command == "pos") {
    printPosition();
    return;
  }

  if (command == "stop") {
    stopMotion();
    return;
  }

  float value = 0.0f;

  if (parseValueAfterCommand(command, "rpm", value)) {
    if (value <= 0.0f) {
      Serial.println("Drehzahl muss groesser als 0 sein.");
      return;
    }

    configuredOutputRpm = value;
    applyMotionSettings();
    Serial.print("Drehzahl gesetzt auf ");
    Serial.print(configuredOutputRpm, 2);
    Serial.println(" U/min.");
    return;
  }

  if (parseValueAfterCommand(command, "acc", value)) {
    if (value <= 0.0f) {
      Serial.println("Beschleunigung muss groesser als 0 sein.");
      return;
    }

    configuredOutputAccelRpmPerS = value;
    applyMotionSettings();
    Serial.print("Beschleunigung gesetzt auf ");
    Serial.print(configuredOutputAccelRpmPerS, 2);
    Serial.println(" U/min/s.");
    return;
  }

  if (parseValueAfterCommand(command, "move", value)) {
    startRelativeMoveDegrees(value);
    return;
  }

  if (parseValueAfterCommand(command, "turn", value)) {
    startRelativeMoveTurns(value);
    return;
  }

  if (parseValueAfterCommand(command, "sweep", value)) {
    startSweep(lroundf(value));
    return;
  }

  Serial.println("Unbekanntes Kommando. Mit 'h' Hilfe anzeigen.");
}
}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(EN_PIN, OUTPUT);
  disableDriver();

  stepper.setEnablePin(EN_PIN);
  stepper.setPinsInverted(false, false, true);
  stepper.disableOutputs();
  applyMotionSettings();

  delay(500);
  Serial.println();
  Serial.println("ESP32 Schrittmotorsteuerung gestartet.");
  printHelp();
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  if (driverEnabled) {
    stepper.run();
  }

  if (autoSweepEnabled && stepper.distanceToGo() == 0) {
    sweepDirection *= -1;
    stepper.moveTo(sweepDirection * sweepTargetSteps);
  }

  const unsigned long now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    if (driverEnabled && (stepper.isRunning() || autoSweepEnabled)) {
      printPosition();
    }
  }
}

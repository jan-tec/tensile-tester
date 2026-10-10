#include "Commands.h"

#include "Diagnostics.h"
#include "Settings.h"
#include "TestRunner.h"
#include "Text.h"

String Commands::helpText() {
  return String(msg(Msg::HelpCommandsPrefix)) + msg(Msg::HelpCommandsSuffix);
}

void Commands::printPosition() {
  Serial.print(msg(Msg::PositionSteps));
  Serial.print(motion_.steps());
  Serial.print(msg(Msg::StatusTravelLabel));
  if (testRunner_.homed()) {
    // M3 FR-006: mm are only meaningful upward from the bottom datum.
    Serial.print(motion_.positionMm(), 3);
  } else {
    Serial.print(msg(Msg::NotAvailable));
  }
  Serial.print(msg(Msg::StatusForceLabel));
  if (loadcell_.isReady()) {
    Serial.println(loadcell_.forceNewton(), 2);
  } else {
    // HX711 not ready: no force reading (M1 T005)
    Serial.println(msg(Msg::NotAvailable));
  }
  if (!testRunner_.homed()) {
    Serial.println(msg(Msg::PositionDatumUnknown));
  }
}

ParseResult Commands::parseValueAfterCommand(const String &command,
                                             const char *prefix,
                                             float &value) const {
  // jane: parseCommandValue buffers 47-char numbers; the legacy String
  // version had no limit, but no realistic command (float has ~7
  // significant digits) can observe the difference.
  return parseCommandValue(command.c_str(), prefix, &value);
}

bool Commands::reportInvalidValue(String &response) const {
  response = msg(Msg::InvalidValue);
  return false;
}

ParseResult Commands::parseIndexAfterCommand(const String &command,
                                             const char *prefix,
                                             uint32_t &index) const {
  return parseCommandIndex(command.c_str(), prefix, &index);
}

bool Commands::applyTestConfigValue(float value, String &response,
                                    float TestConfig::*field,
                                    const char *suffix) {
  if (value <= 0.0f) {
    response = msg(Msg::ValueMustBePositive);
    return false;
  }
  if (diagnostics_.model().state() == DeviceState::Error) {
    // applyConfig() resets the homing FSM - unlatching its Fault behind
    // the device Error gate would blur FR-009's recovery-by-reboot rule.
    response = msg(Msg::ConfigLockedInFault);
    return false;
  }
  if (testRunner_.active()) {
    // applyConfig() re-parameterises the models (and resets the homing
    // FSM) - never while a run is in flight (D11).
    response = msg(Msg::CycleBusy);
    return false;
  }
  TestConfig config = testRunner_.config();
  config.*field = value;
  testRunner_.applyConfig(config);
  settings_.saveTestConfig(testRunner_.config());
  // Reply with the EFFECTIVE value: clampTestConfig may have adjusted it
  // (FR-005) and the operator needs to see what is now in force.
  response = String(testRunner_.config().*field, 2) + suffix;
  return true;
}

void Commands::pollDump() {
  if (!testRunner_.dumpInProgress()) {
    return;
  }
  // D8/NFR: write only what the TX buffer can take in one go; the dump
  // resumes on the next tick, never blocking the loop task.
  if (static_cast<size_t>(Serial.availableForWrite()) < kCsvChunkBytes) {
    return;
  }
  char chunk[kCsvChunkBytes];
  size_t next = testRunner_.nextDumpIndex();
  const size_t from = next;
  const size_t written = formatCsvChunk(testRunner_.samples(), next, chunk,
                                        sizeof(chunk), &next);
  testRunner_.advanceDump(next);
  if (written > 0) {
    Serial.write(reinterpret_cast<const uint8_t *>(chunk), written);
  }
  // Documented caller stop rule (TestData.h): 0 bytes = complete (or a
  // buffer too small for one line - impossible at 128 B), and a chunk
  // that wrote bytes WITHOUT advancing is the END-marker-only final
  // chunk (the empty-buffer document) - both finish the dump.
  if (written == 0 || next == from) {
    testRunner_.endDump(next);
  }
}

bool Commands::execute(String command, String &response) {
  command.trim();
  if (command.isEmpty()) {
    response = msg(Msg::EmptyCommand);
    return false;
  }

  // FR-009: in the Error state the driver stays off; motion commands
  // are rejected with the fault code + message. Recovery by reset/reboot.
  const DeviceModel &model = diagnostics_.model();
  if (model.state() == DeviceState::Error &&
      isMotionCommand(command.c_str())) {
    // M2-T004 (FR-007): the fault code carries its localized detail
    // message (e.g. POWER_LOW -> "Supply voltage too low"). The prefix
    // rows already end in "0x" - bench-found double prefix fixed here:
    // only the padded hex digits are appended (code 0x0501, #97 grammar).
    char buffer[160];
    snprintf(buffer, sizeof(buffer), "%s%04X%s %s", msg(Msg::FaultStatePrefix),
             model.faultCode(), msg(Msg::FaultStateSuffix),
             msg(msgForFault(static_cast<ErrorCode>(model.faultCode()))));
    response = String(buffer);
    return false;
  }

  // M2-T003 (FR-003): locale switch, usable in every state (not motion).
  Locale locale = textLocale();
  const ParseResult langResult =
      parseLocaleAfterCommand(command.c_str(), "lang", &locale);
  if (langResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (langResult == ParseResult::Ok) {
    setTextLocale(locale);
    settings_.saveLocale(locale);
    response = String(msg(Msg::LangSet)) + localeName(locale);
    return true;
  }

  const Command named = commandFromName(command.c_str());

  if (named == Command::Enable) {
    motion_.enable();
    response = msg(Msg::DriversEnabled);
    return true;
  }

  if (named == Command::Disable) {
    motion_.disable();
    response = msg(Msg::DriversDisabled);
    return true;
  }

  if (named == Command::Zero) {
    motion_.zero();
    response = msg(Msg::OriginZeroed);
    return true;
  }

  if (named == Command::Tare) {
    loadcell_.tare();
    response = loadcell_.isReady() ? msg(Msg::LoadCellTared)
                                   : msg(Msg::Hx711NotReady);
    return loadcell_.isReady();
  }

  if (named == Command::CalZero) {
    if (!loadcell_.captureZeroPoint()) {
      response = msg(Msg::Hx711NotReady);
      return false;
    }
    response = msg(Msg::CalZeroCaptured);
    return true;
  }

  float value = 0.0f;

  const ParseResult calApplyResult =
      parseValueAfterCommand(command, "cal_apply", value);
  if (calApplyResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (calApplyResult == ParseResult::Ok) {
    switch (loadcell_.calibrateWithMass(value)) {
      case LoadCell::CalError::NotReady:
        response = msg(Msg::Hx711NotReady);
        return false;
      case LoadCell::CalError::ZeroNotCaptured:
        response = msg(Msg::CalStep1First);
        return false;
      case LoadCell::CalError::InvalidMass:
        response = msg(Msg::CalWeightMustBePositive);
        return false;
      case LoadCell::CalError::ReadFailed:
        response = msg(Msg::CalValueUnreadable);
        return false;
      case LoadCell::CalError::ZeroFactor:
        response = msg(Msg::CalFailedZeroFactor);
        return false;
      case LoadCell::CalError::NegativeFactor:
        response = msg(Msg::CalFailedNegativeFactor);
        return false;
      case LoadCell::CalError::None:
        break;
    }
    calAppliedEvent_ = true;  // M8-T003: CalApplied LED overlay
    response = String(msg(Msg::CalSaved)) +
               String(loadcell_.calibrationFactor(), 6);
    return true;
  }

  if (named == Command::Force) {
    if (loadcell_.isReady()) {
      response = String(msg(Msg::ForceCurrent)) +
                 String(loadcell_.forceNewton(), 2) + msg(Msg::NewtonSuffix);
    } else {
      response = msg(Msg::ForceUnavailable);
    }
    return true;
  }

  if (named == Command::Stop) {
    // M3 T012: a running cycle/homing aborts through the runner first
    // (forceStop semantics); a plain move keeps its decel `stop`.
    if (testRunner_.abort()) {
      response = msg(Msg::TestAbortedNote);
    } else {
      motion_.stop();
      response = msg(Msg::StopRequested);
    }
    return true;
  }

  if (named == Command::Home || named == Command::HomeTop) {
    // D11: exclusive; the D15 Error-state rejection already ran above.
    if (testRunner_.active()) {
      response = msg(Msg::CycleBusy);
      return false;
    }
    if (testRunner_.dumpInProgress()) {
      // A started test would clear the sample buffer mid-dump (and a
      // homing report line would land inside the CSV stream).
      response = msg(Msg::DumpBusy);
      return false;
    }
    const bool topOnly = named == Command::HomeTop;
    if (!testRunner_.startHome(topOnly)) {
      // Homing FSM latched Fault (device Error) - unreachable through the
      // Error gate above, kept as a defensive no-crash path.
      response = msg(Msg::CycleBusy);
      return false;
    }
    response = msg(topOnly ? Msg::HomeTopStarted : Msg::HomeStarted);
    return true;
  }

  if (named == Command::Test) {
    if (testRunner_.active()) {
      response = msg(Msg::CycleBusy);
      return false;
    }
    if (testRunner_.dumpInProgress()) {
      // startTest() clears the buffer - mid-dump that destroys the very
      // data being streamed (US2).
      response = msg(Msg::DumpBusy);
      return false;
    }
    if (!testRunner_.startTest()) {
      response = msg(Msg::CycleBusy);
      return false;
    }
    response = msg(Msg::TestStarted);
    return true;
  }

  if (named == Command::Pos) {
    printPosition();
    response = msg(Msg::PositionPrinted);
    return true;
  }

  const ParseResult rpmResult = parseValueAfterCommand(command, "rpm", value);
  if (rpmResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (rpmResult == ParseResult::Ok) {
    if (value <= 0.0f) {
      response = msg(Msg::RpmMustBePositive);
      return false;
    }

    motion_.setOutputRpm(value);
    response = String(msg(Msg::RpmSet)) + String(motion_.outputRpm(), 2) +
               msg(Msg::RpmSuffix);
    return true;
  }

  const ParseResult accResult = parseValueAfterCommand(command, "acc", value);
  if (accResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (accResult == ParseResult::Ok) {
    if (value <= 0.0f) {
      response = msg(Msg::AccelMustBePositive);
      return false;
    }

    motion_.setAccelRpmPerS(value);
    response = String(msg(Msg::AccelSet)) + String(motion_.accelRpmPerS(), 2) +
               msg(Msg::AccelSuffix);
    return true;
  }

  const ParseResult feedResult =
      parseValueAfterCommand(command, "feed", value);
  if (feedResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (feedResult == ParseResult::Ok) {
    if (value <= 0.0f) {
      response = msg(Msg::FeedMustBePositive);
      return false;
    }

    const float clampedFeed = clampFeedMmPerMin(value);
    motion_.setOutputRpm(feedMmPerMinToRpm(clampedFeed));
    response = String(msg(Msg::FeedSet)) + String(clampedFeed, 2) +
               msg(Msg::MillimetrePerMinuteSuffix);
    return true;
  }

  if (named == Command::JogUp) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + spindleMmToSteps(kJogTravelMm)));
    response = msg(Msg::JogUpStarted);
    return true;
  }

  if (named == Command::JogDown) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() - spindleMmToSteps(kJogTravelMm)));
    response = msg(Msg::JogDownStarted);
    return true;
  }

  const ParseResult moveResult = parseValueAfterCommand(command, "move", value);
  if (moveResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (moveResult == ParseResult::Ok) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + spindleMmToSteps(value)));
    response = String(msg(Msg::RelativeMoveStarted)) + String(value, 3) +
               msg(Msg::MillimetreSuffix);
    return true;
  }

  const ParseResult turnResult = parseValueAfterCommand(command, "turn", value);
  if (turnResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (turnResult == ParseResult::Ok) {
    motion_.startMoveToSteps(
        static_cast<long>(motion_.steps() + outputTurnsToSteps(value)));
    response = String(msg(Msg::RelativeMoveStarted)) + String(value, 3) +
               msg(Msg::RevolutionsSuffix);
    return true;
  }

  // M3 T012: test config knobs (persisted, clamped; FR-005).
  const ParseResult testSpeedResult =
      parseValueAfterCommand(command, "testspeed", value);
  if (testSpeedResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (testSpeedResult == ParseResult::Ok) {
    return applyTestConfigValue(value, response,
                                &TestConfig::testSpeedMmPerMin,
                                msg(Msg::MillimetrePerMinuteSuffix));
  }

  const ParseResult testArmResult =
      parseValueAfterCommand(command, "testarm", value);
  if (testArmResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (testArmResult == ParseResult::Ok) {
    return applyTestConfigValue(value, response, &TestConfig::armForceN,
                                msg(Msg::NewtonSuffix));
  }

  const ParseResult testDropResult =
      parseValueAfterCommand(command, "testdrop", value);
  if (testDropResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (testDropResult == ParseResult::Ok) {
    return applyTestConfigValue(value, response, &TestConfig::dropPercent,
                                msg(Msg::PercentSuffix));
  }

  // `data [from-index]`: CSV dump (TC-005 resume grammar). Bare `data`
  // dumps from 0 (NotACommand from the index parser means no token).
  uint32_t dataIndex = 0;
  const ParseResult dataIndexResult =
      parseIndexAfterCommand(command, "data", dataIndex);
  if (dataIndexResult == ParseResult::InvalidValue) {
    return reportInvalidValue(response);
  }
  if (dataIndexResult == ParseResult::Ok) {
    if (testRunner_.active()) {
      response = msg(Msg::DumpBlockedRunning);
      return false;
    }
    if (testRunner_.dumpInProgress()) {
      response = msg(Msg::DumpBusy);
      return false;
    }
    testRunner_.beginDump(dataIndex);
    response = msg(Msg::DumpStarted);  // the CSV itself streams from
                                       // pollDump() in the loop task
    return true;
  }
  if (command == "data") {
    if (testRunner_.active()) {
      response = msg(Msg::DumpBlockedRunning);
      return false;
    }
    if (testRunner_.dumpInProgress()) {
      response = msg(Msg::DumpBusy);
      return false;
    }
    testRunner_.beginDump(static_cast<size_t>(0));
    response = msg(Msg::DumpStarted);
    return true;
  }

  if (command == "testdata") {
    // FR-005: machine-readable status via the pure formatter (stable
    // key=value grammar, locale-neutral numbers).
    char status[96];
    formatTestDataStatus(testRunner_.samples(), testRunner_.peakForceN(),
                         testRunner_.peakIndex(), status, sizeof(status));
    response = String(msg(Msg::TestdataPrefix)) + status;
    return true;
  }

  response = msg(Msg::UnknownCommand);
  return false;
}

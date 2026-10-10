#include "Commands.h"

#include "Diagnostics.h"
#include "Settings.h"
#include "Text.h"

String Commands::helpText() {
  return String(msg(Msg::HelpCommandsPrefix)) + msg(Msg::HelpCommandsSuffix);
}

void Commands::printPosition() {
  Serial.print(msg(Msg::PositionSteps));
  Serial.print(motion_.steps());
  Serial.print(msg(Msg::StatusTravelLabel));
  Serial.print(motion_.positionMm(), 3);
  Serial.print(msg(Msg::StatusForceLabel));
  if (loadcell_.isReady()) {
    Serial.println(loadcell_.forceNewton(), 2);
  } else {
    // HX711 not ready: no force reading (M1 T005)
    Serial.println(msg(Msg::NotAvailable));
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
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "%s0x%04X%s", msg(Msg::FaultStatePrefix),
             model.faultCode(), msg(Msg::FaultStateSuffix));
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
    motion_.stop();
    response = msg(Msg::StopRequested);
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

  response = msg(Msg::UnknownCommand);
  return false;
}

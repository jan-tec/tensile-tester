// HX711 loadcell module implementation (M0-T004).
//
// Behavior carried over unchanged from the legacy src/main.cpp (M0
// FR-004): sample interval, filter alpha, boot diagnostics, calibration
// flow, NVS keys.
#include "LoadCell.h"

#include "Text.h"

#include <FirmwareCore.h>

void LoadCell::begin(Settings &settings) {
  settings_ = &settings;
  scale_.begin(kDoutPin, kSckPin);

  if (!scale_.wait_ready_timeout(2000)) {
    Serial.println(msg(Msg::Hx711NotReadyDisabled));
    ready_ = false;
    return;
  }

  const Settings::Calibration calibration = settings_->loadCalibration();
  calibrationValid_ = calibration.valid;
  calibrationFactor_ = calibration.factor;
  scale_.set_scale(calibrationFactor_);
  scale_.tare(1);
  ready_ = true;
  if (calibrationValid_) {
    Serial.print(msg(Msg::Hx711ReadyCalLoaded));
    Serial.println(calibrationFactor_, 6);
  } else {
    Serial.println(msg(Msg::Hx711ReadyNoCal));
  }
  Serial.println(msg(Msg::TaraSet));
}

bool LoadCell::isReady() {
  return ready_;
}

float LoadCell::forceNewton() {
  return forceNewton_;
}

float LoadCell::rawForceNewton() {
  return rawForceNewton_;
}

bool LoadCell::waitReady(unsigned long timeoutMs) {
  return scale_.wait_ready_timeout(static_cast<unsigned int>(timeoutMs));
}

bool LoadCell::rawSample(int32_t &raw) {
  if (!scale_.is_ready()) {
    return false;
  }
  raw = static_cast<int32_t>(scale_.read());
  return true;
}

bool LoadCell::calibrationValid() {
  return calibrationValid_;
}

float LoadCell::calibrationFactor() {
  return calibrationFactor_;
}

void LoadCell::poll() {
  if (!ready_) {
    return;
  }

  if (millis() - lastSampleMs_ < kSampleIntervalMs) {
    return;
  }
  lastSampleMs_ = millis();

  if (!scale_.is_ready()) {
    return;
  }

  const long rawReading = scale_.read();
  const float massGrams =
      (static_cast<float>(rawReading) - scale_.get_offset()) /
      scale_.get_scale();
  const float measuredForceNewton = (massGrams / 1000.0f) * kGravity;
  rawForceNewton_ = measuredForceNewton;  // pre-filter snapshot (M3 FR-003)
  forceNewton_ += (measuredForceNewton - forceNewton_) * kFilterAlpha;
}

void LoadCell::tare() {
  if (!ready_) {
    return;
  }

  scale_.tare(1);
  forceNewton_ = 0.0f;
}

bool LoadCell::captureZeroPoint() {
  if (!ready_) {
    return false;
  }

  scale_.set_scale(calibrationFactor_);
  scale_.tare(1);
  calibrationZeroCaptured_ = true;
  forceNewton_ = 0.0f;
  return true;
}

LoadCell::CalError LoadCell::calibrateWithMass(float grams) {
  if (!ready_) {
    return CalError::NotReady;
  }
  if (!calibrationZeroCaptured_) {
    return CalError::ZeroNotCaptured;
  }
  if (grams <= 0.0f) {
    return CalError::InvalidMass;
  }

  delay(kCalibrationSettleMs);

  long averageRaw = 0;
  if (!readAverageRawValue(kCalibrationCaptureSamples, averageRaw)) {
    return CalError::ReadFailed;
  }

  const long offset = scale_.get_offset();
  const float newFactor = calibrationFactorFromMass(averageRaw, offset, grams);

  const CalibrationVerdict verdict = verifyCalibrationFactor(newFactor);
  if (verdict == CalibrationVerdict::Zero) {
    return CalError::ZeroFactor;
  }
  if (verdict == CalibrationVerdict::Negative) {
    return CalError::NegativeFactor;
  }

  settings_->saveCalibration(newFactor);
  calibrationFactor_ = newFactor;
  calibrationValid_ = true;
  scale_.set_scale(calibrationFactor_);
  forceNewton_ = 0.0f;
  calibrationZeroCaptured_ = false;
  return CalError::None;
}

bool LoadCell::readAverageRawValue(uint8_t sampleCount, long &averageRaw) {
  if (!ready_) {
    return false;
  }

  int64_t sum = 0;
  for (uint8_t i = 0; i < sampleCount; ++i) {
    if (!scale_.wait_ready_timeout(1500)) {
      return false;
    }
    sum += scale_.read();
  }

  averageRaw = static_cast<long>(sum / sampleCount);
  return true;
}

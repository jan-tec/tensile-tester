// HX711 loadcell module implementation (M0-T004).
//
// Behavior carried over unchanged from the legacy src/main.cpp (M0
// FR-004): sample interval, filter alpha, boot diagnostics, calibration
// flow, NVS keys.
#include "LoadCell.h"

void LoadCell::begin() {
  scale_.begin(kDoutPin, kSckPin);

  if (!scale_.wait_ready_timeout(2000)) {
    Serial.println("HX711 nicht bereit. Kraftmessung bleibt deaktiviert.");
    ready_ = false;
    return;
  }

  loadCalibrationFromPreferences();
  scale_.set_scale(calibrationFactor_);
  scale_.tare(1);
  ready_ = true;
  if (calibrationValid_) {
    Serial.print("HX711 bereit. Kalibrierfaktor geladen: ");
    Serial.println(calibrationFactor_, 6);
  } else {
    Serial.println("HX711 bereit. Noch keine gueltige Kalibrierung gespeichert.");
  }
  Serial.println("Tara gesetzt.");
}

bool LoadCell::isReady() {
  return ready_;
}

float LoadCell::forceNewton() {
  return forceNewton_;
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
  const float newFactor = static_cast<float>(averageRaw - offset) / grams;

  if (newFactor == 0.0f) {
    return CalError::ZeroFactor;
  }

  saveCalibrationToPreferences(newFactor);
  scale_.set_scale(calibrationFactor_);
  forceNewton_ = 0.0f;
  calibrationZeroCaptured_ = false;
  return CalError::None;
}

void LoadCell::loadCalibrationFromPreferences() {
  preferences_.begin(kPreferencesNamespace, true);
  calibrationValid_ =
      preferences_.getBool(kPrefCalibrationValid, false);
  calibrationFactor_ = preferences_.getFloat(
      kPrefCalibrationFactor, kDefaultCalibrationFactor);
  preferences_.end();

  if (!calibrationValid_ || calibrationFactor_ == 0.0f) {
    calibrationValid_ = false;
    calibrationFactor_ = kDefaultCalibrationFactor;
  }
}

void LoadCell::saveCalibrationToPreferences(float factor) {
  preferences_.begin(kPreferencesNamespace, false);
  preferences_.putFloat(kPrefCalibrationFactor, factor);
  preferences_.putBool(kPrefCalibrationValid, true);
  preferences_.end();

  calibrationFactor_ = factor;
  calibrationValid_ = true;
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

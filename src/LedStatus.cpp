#include "LedStatus.h"

void LedStatus::begin(unsigned long nowMs) {
  ledcSetup(kLedcChannel, kFreqHz, kResolutionBits);
  ledcAttachPin(kLedPin, kLedcChannel);
  ledcWrite(kLedcChannel, 0);
  lastOn_ = false;
  lastDuty_ = 0;
  fsm_.begin(nowMs);
}

void LedStatus::tick(led::LedBase base, led::LedEvent event,
                     unsigned long nowMs) {
  const led::LedOutput out = fsm_.update(base, event, nowMs);
  const uint8_t duty = out.on ? led::duty255(out.level) : 0;
  if (out.on != lastOn_ || duty != lastDuty_) {
    ledcWrite(kLedcChannel, duty);
    lastOn_ = out.on;
    lastDuty_ = duty;
  }
}

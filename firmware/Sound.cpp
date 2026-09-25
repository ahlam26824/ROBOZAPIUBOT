#include <Arduino.h>
#include "Sound.h"
#include "SoundData.h"

namespace {
  const int AMP_PIN = 7;
}

namespace Sound {

  void begin() {
    pinMode(AMP_PIN, OUTPUT);
    digitalWrite(AMP_PIN, LOW);
  }

  void playHappySound()  { playSample(HAPPY_SOUND,  HAPPY_SOUND_LEN); }
  void playSadSound()    { playSample(SAD_SOUND,    SAD_SOUND_LEN); }
  void playScaredSound() { playSample(SCARED_SOUND, SCARED_SOUND_LEN); }
  void playAngrySound()  { playSample(ANGRY_SOUND,  ANGRY_SOUND_LEN); }
  void playAlarmSound()  { playSample(ALARM_SOUND,  ALARM_SOUND_LEN); }

  void playSample(const uint8_t* samples, size_t length, uint16_t sampleRateHz) {
    if (samples == nullptr || length == 0 || sampleRateHz == 0) return;

    // A PWM "carrier" well above audible range, amplitude-modulated per
    // audio sample. The PAM8403's input coupling capacitor and the
    // speaker's own inertia smooth this into an audible tone -- the same
    // trick used by many hobbyist "PWM audio" projects that don't have a
    // dedicated DAC available (the ESP32-C3 has none).
    const unsigned long carrierPeriodUs = 25; // ~40kHz carrier
    unsigned long sampleIntervalUs = 1000000UL / sampleRateHz;

    for (size_t i = 0; i < length; i++) {
      uint8_t level = pgm_read_byte(&samples[i]);
      unsigned long highUs = (unsigned long)level * carrierPeriodUs / 255;
      unsigned long lowUs = carrierPeriodUs - highUs;

      unsigned long elapsed = 0;
      while (elapsed < sampleIntervalUs) {
        if (highUs > 0) {
          digitalWrite(AMP_PIN, HIGH);
          delayMicroseconds(highUs);
        }
        if (lowUs > 0) {
          digitalWrite(AMP_PIN, LOW);
          delayMicroseconds(lowUs);
        }
        elapsed += carrierPeriodUs;
      }
    }

    digitalWrite(AMP_PIN, LOW);
  }

}

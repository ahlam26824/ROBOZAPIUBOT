#pragma once
#include <stdint.h>
#include <stddef.h>

// Drives a PAM8403 mini amplifier + small speaker from a single GPIO
// (GPIO7) with bit-banged output. This deliberately avoids the ESP32
// tone()/ledc APIs, whose exact function signatures have changed between
// Arduino-ESP32 core versions -- digitalWrite/delayMicroseconds work
// identically on every version, so this stays portable and easy to
// debug.
//
// Each function below plays a real user-provided sound clip (see
// SoundData.h -- 8-bit unsigned mono PCM at 8000Hz, converted from the
// original WAV files with ffmpeg) via playSample(). To replace any of
// these with a new recording later, regenerate that one array in
// SoundData.h (see the comment there) -- nothing else needs to change.
namespace Sound {

  void begin();

  void playHappySound();  // touch / pat reaction
  void playSadSound();    // idle-too-long reaction
  void playScaredSound(); // startle reaction (shake / pickup, once wired up)
  void playAngrySound();  // "being bothered too much" reaction
  void playAlarmSound();  // a phone notification came in over Bluetooth
  // playSleepySound() was removed to reclaim flash space -- SLEEPY was
  // never wired up to anything (see types.h), so its 16,000-byte clip
  // was pure dead weight. Re-add it the same way (see SoundData.h's
  // header comment for the ffmpeg command) if/when Sleepy gets a real
  // trigger; the original WAV is needed since the converted bytes were
  // deleted, not just unlinked.

  // Plays a raw 8-bit unsigned PCM sample (values 0-255, 128 = silence)
  // through the amp via a bit-banged PWM carrier. This is the engine to
  // use once you have a real recorded/licensed/generated sound: convert
  // it to 8-bit mono PCM at 8000Hz, store the bytes in a PROGMEM array,
  // and call this with a pointer to it. Nothing else needs to change.
  void playSample(const uint8_t* samples, size_t length, uint16_t sampleRateHz = 8000);

}

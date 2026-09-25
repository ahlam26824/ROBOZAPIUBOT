#pragma once

// ADXL345 accelerometer, sharing the same I2C bus as the OLED display
// (SDA=GPIO8, SCL=GPIO9).
//
// Distinguishes two different physical events from the same sensor:
//  - a SHAKE: a rapid, brief jolt in overall acceleration.
//  - a PICKUP: the bot being lifted and held at a different orientation
//    than it was calibrated at, SUSTAINED for a moment (as opposed to a
//    quick shake's brief transient).
// Both are modeled as episodes with a start and an end, so the caller
// can react once when something begins and once when it settles back
// down, rather than firing repeatedly the whole time.
namespace Accelerometer {

  // Calibrates against the bot's own resting orientation and noise
  // level, then starts measuring. Call once from setup(), AFTER
  // initDisplay() (which is what actually calls Wire.begin()). Blocks
  // for about a second -- the caller should show a "calibrating, keep me
  // still" message first.
  void begin();

  // Reads the sensor once and updates shake/pickup episode state. Call
  // exactly once per loop iteration.
  void update();

  // True only on the update() call where a shake episode began.
  bool shakeStarted();

  // True only on the update() call where a shake episode ended (things
  // have been quiet for a short moment after jolting).
  bool shakeStopped();

  // True for as long as a shake episode is ongoing.
  bool isShaking();

  // True only on the update() call where the bot was lifted/reoriented
  // long enough to count as a pickup (not just a passing shake).
  bool pickupDetected();

  // True only on the update() call where it was set back down (returned
  // close to its calibrated resting orientation).
  bool putDownDetected();

  // True for as long as it's being held away from its resting orientation.
  bool isPickedUp();

  // Tilt relative to the calibrated resting orientation, in degrees --
  // accelerometer-only roll/pitch (no gyro fusion), which is fine here
  // since this only drives a slow, subtle "eyes glance toward the tilt"
  // idle effect in Faces, not a fast/precise control input. Kept
  // separate from the shake/pickup detection above, which stays
  // magnitude- and cosine-similarity-based.
  float getTiltX();
  float getTiltY();

}

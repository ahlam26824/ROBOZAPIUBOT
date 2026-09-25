#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "Accelerometer.h"

namespace {
  // Most cheap ADXL345 breakouts default here (SDO pulled low). If the
  // Serial Monitor reports "ADXL345 not responding", your board's SDO
  // is likely pulled high -- change this to 0x1D instead.
  const uint8_t ADXL345_ADDR = 0x53;

  const uint8_t REG_DEVID       = 0x00;
  const uint8_t REG_DATA_FORMAT = 0x31;
  const uint8_t REG_POWER_CTL   = 0x2D;
  const uint8_t REG_DATAX0      = 0x32;
  const uint8_t EXPECTED_DEVID  = 0xE5;

  bool sensorOk = false;

  // ---- Calibration state ----
  // Filled in once at startup by calibrate() while the bot is assumed to
  // be sitting still. baseX/Y/Z is the resting gravity-direction vector
  // (used to detect being picked up and tilted away from it); the shake
  // threshold is derived from THIS unit's own measured noise level
  // rather than one hardcoded number that might be wrong for a given
  // sensor/mounting.
  bool calibrated = false;
  float baseX = 0, baseY = 0, baseZ = 0;
  float baseMagnitude = 1; // avoid div-by-zero before calibration completes
  float shakeThreshold = 150.0;

  const int CALIBRATION_SAMPLES = 100;
  const unsigned long CALIBRATION_SAMPLE_INTERVAL_MS = 10; // ~1s total
  const float MIN_SHAKE_THRESHOLD = 60.0;   // floor, even on a very quiet unit
  const float SHAKE_SIGMA_MULTIPLIER = 7.0; // "how many standard deviations above resting noise counts as a shake"

  // ---- Live reading state ----
  bool haveLastMagnitude = false;
  float lastMagnitude = 0;

  // ---- Shake episode state ----
  bool shaking = false;
  unsigned long lastJoltTime = 0;
  const unsigned long SHAKE_QUIET_MS = 600; // no jolts for this long -> episode over

  bool shakeStartedFlag = false;
  bool shakeStoppedFlag = false;

  // ---- Pickup episode state ----
  // A pickup is a SUSTAINED reorientation, which is what tells it apart
  // from a brief shake (which also disturbs the readings, but briefly).
  bool tiltDeviating = false;
  unsigned long tiltDeviationStart = 0;
  bool pickedUp = false;
  const float LIFT_SIMILARITY_THRESHOLD = 0.85; // cosine similarity; ~32 degrees of tilt
  const unsigned long LIFT_SUSTAIN_MS = 400;

  bool pickupDetectedFlag = false;
  bool putDownDetectedFlag = false;

  // ---- Tilt tracking (feeds the eyes' idle glance, not shake/pickup) ----
  float baseRollDeg = 0, basePitchDeg = 0;
  float tiltXDeg = 0, tiltYDeg = 0;

  float rollFromAccel(float x, float y, float z) {
    return atan2(y, z) * 180.0 / PI;
  }
  float pitchFromAccel(float x, float y, float z) {
    return atan2(-x, sqrt(y * y + z * z)) * 180.0 / PI;
  }

  void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }

  bool readAcceleration(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(REG_DATAX0);
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom((int)ADXL345_ADDR, 6) != 6) return false;

    x = (int16_t)(Wire.read() | (Wire.read() << 8));
    y = (int16_t)(Wire.read() | (Wire.read() << 8));
    z = (int16_t)(Wire.read() | (Wire.read() << 8));
    return true;
  }

  void calibrate() {
    float sumX = 0, sumY = 0, sumZ = 0;
    float sumMag = 0, sumMagSq = 0;
    int count = 0;

    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
      int16_t x, y, z;
      if (readAcceleration(x, y, z)) {
        sumX += x; sumY += y; sumZ += z;
        float mag = sqrt((float)x * x + (float)y * y + (float)z * z);
        sumMag += mag;
        sumMagSq += mag * mag;
        count++;
      }
      delay(CALIBRATION_SAMPLE_INTERVAL_MS);
    }

    if (count < CALIBRATION_SAMPLES / 2) {
      Serial.println("ADXL345: calibration got too few readings, using defaults");
      return;
    }

    baseX = sumX / count;
    baseY = sumY / count;
    baseZ = sumZ / count;
    baseMagnitude = sqrt(baseX * baseX + baseY * baseY + baseZ * baseZ);
    if (baseMagnitude < 1.0) baseMagnitude = 1.0;

    float meanMag = sumMag / count;
    float variance = (sumMagSq / count) - (meanMag * meanMag);
    if (variance < 0) variance = 0;
    float noiseStdDev = sqrt(variance);

    shakeThreshold = max(MIN_SHAKE_THRESHOLD, noiseStdDev * SHAKE_SIGMA_MULTIPLIER);

    // Whatever orientation the bot happens to be mounted/resting at counts
    // as "level" for tilt purposes -- roll/pitch are measured relative to
    // this baseline, not to gravity's absolute frame.
    baseRollDeg = rollFromAccel(baseX, baseY, baseZ);
    basePitchDeg = pitchFromAccel(baseX, baseY, baseZ);

    calibrated = true;

    Serial.print("ADXL345 calibrated: base=(");
    Serial.print(baseX); Serial.print(", ");
    Serial.print(baseY); Serial.print(", ");
    Serial.print(baseZ); Serial.print("), noiseStdDev=");
    Serial.print(noiseStdDev);
    Serial.print(", shakeThreshold=");
    Serial.println(shakeThreshold);
  }
}

namespace Accelerometer {

  void begin() {
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(REG_DEVID);
    if (Wire.endTransmission(false) != 0) {
      Serial.println("ADXL345 not responding on I2C bus");
      sensorOk = false;
      return;
    }

    Wire.requestFrom((int)ADXL345_ADDR, 1);
    uint8_t id = Wire.read();
    if (id != EXPECTED_DEVID) {
      Serial.print("ADXL345: unexpected device id 0x");
      Serial.println(id, HEX);
    }

    writeRegister(REG_DATA_FORMAT, 0x08); // full resolution, +-2g
    writeRegister(REG_POWER_CTL, 0x08);   // start measuring

    sensorOk = true;
    Serial.println("ADXL345 initialized, calibrating (keep the bot still)...");
    calibrate();
  }

  void update() {
    shakeStartedFlag = false;
    shakeStoppedFlag = false;
    pickupDetectedFlag = false;
    putDownDetectedFlag = false;

    if (!sensorOk || !calibrated) return;

    int16_t x, y, z;
    if (!readAcceleration(x, y, z)) return;

    float magnitude = sqrt((float)x * x + (float)y * y + (float)z * z);
    unsigned long now = millis();

    tiltXDeg = rollFromAccel(x, y, z) - baseRollDeg;
    tiltYDeg = pitchFromAccel(x, y, z) - basePitchDeg;

    // ---- Shake: a sharp, brief jump in overall acceleration ----
    if (haveLastMagnitude) {
      float delta = fabs(magnitude - lastMagnitude);
      if (delta > shakeThreshold) {
        lastJoltTime = now;
        if (!shaking) {
          shaking = true;
          shakeStartedFlag = true;
          Serial.print("Shake started (delta=");
          Serial.print(delta);
          Serial.println(")");
        }
      }
    }
    lastMagnitude = magnitude;
    haveLastMagnitude = true;

    if (shaking && (now - lastJoltTime) > SHAKE_QUIET_MS) {
      shaking = false;
      shakeStoppedFlag = true;
      Serial.println("Shake stopped");
    }

    // ---- Pickup: a SUSTAINED change in orientation vs. the calibrated
    // resting orientation (cosine similarity between current and resting
    // gravity-direction vectors) ----
    if (magnitude > 1.0) {
      float dot = (x * baseX + y * baseY + z * baseZ) / (magnitude * baseMagnitude);
      bool deviating = dot < LIFT_SIMILARITY_THRESHOLD;

      if (deviating) {
        if (!tiltDeviating) {
          tiltDeviating = true;
          tiltDeviationStart = now;
        } else if (!pickedUp && (now - tiltDeviationStart) > LIFT_SUSTAIN_MS) {
          pickedUp = true;
          pickupDetectedFlag = true;
          Serial.println("Picked up");
        }
      } else {
        tiltDeviating = false;
        if (pickedUp) {
          pickedUp = false;
          putDownDetectedFlag = true;
          Serial.println("Put down");
        }
      }
    }
  }

  bool shakeStarted() { return shakeStartedFlag; }
  bool shakeStopped() { return shakeStoppedFlag; }
  bool isShaking() { return shaking; }

  bool pickupDetected() { return pickupDetectedFlag; }
  bool putDownDetected() { return putDownDetectedFlag; }
  bool isPickedUp() { return pickedUp; }

  float getTiltX() { return tiltXDeg; }
  float getTiltY() { return tiltYDeg; }

}

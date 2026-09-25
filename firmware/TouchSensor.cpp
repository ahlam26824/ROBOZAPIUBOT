#include <Arduino.h>
#include "TouchSensor.h"

namespace {
  const int TOUCH_PIN = 10;
  const unsigned long DEBOUNCE_MS = 50;
  const unsigned long MEDIUM_HOLD_MS = 3000;
  const unsigned long LONG_HOLD_MS = 8000;

  bool lastState = false;
  unsigned long lastChangeTime = 0;

  bool touching = false;
  unsigned long touchStartTime = 0;
  bool longFired = false; // this hold already reached the 8s tier

  bool tappedFlag = false;
  bool shortReleasedFlag = false;
  bool mediumReleasedFlag = false;
  bool longPressedFlag = false;
}

namespace TouchSensor {

  void begin() {
    pinMode(TOUCH_PIN, INPUT);
  }

  void update() {
    tappedFlag = false;
    shortReleasedFlag = false;
    mediumReleasedFlag = false;
    longPressedFlag = false;

    // If your specific touch module is active-LOW instead, flip this to
    // `digitalRead(TOUCH_PIN) == LOW`.
    bool state = digitalRead(TOUCH_PIN) == HIGH;
    unsigned long now = millis();

    if (state != lastState && (now - lastChangeTime) > DEBOUNCE_MS) {
      lastChangeTime = now;
      lastState = state;

      if (state) {
        touching = true;
        touchStartTime = now;
        longFired = false;
        tappedFlag = true;
      } else {
        touching = false;
        if (!longFired) {
          unsigned long heldFor = now - touchStartTime;
          if (heldFor < MEDIUM_HOLD_MS) shortReleasedFlag = true;
          else mediumReleasedFlag = true; // between 3s and 8s
        }
        // if longFired was already true, the 8s gesture already fired
        // while held -- releasing afterward does nothing extra.
      }
    }

    if (touching && !longFired && (now - touchStartTime) >= LONG_HOLD_MS) {
      longFired = true;
      longPressedFlag = true;
    }
  }

  bool wasTapped() { return tappedFlag; }
  bool wasShortReleased() { return shortReleasedFlag; }
  bool wasMediumReleased() { return mediumReleasedFlag; }
  bool wasLongPressed() { return longPressedFlag; }

}

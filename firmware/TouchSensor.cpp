#include <Arduino.h>
#include "TouchSensor.h"

namespace {
  const int TOUCH_PIN = 10;
  const unsigned long DEBOUNCE_MS = 50;
  const unsigned long MEDIUM_HOLD_MS = 3000;
  const unsigned long FOCUS_HOLD_MS = 7000;
  const unsigned long LONG_HOLD_MS = 8000;

  bool lastState = false;
  unsigned long lastChangeTime = 0;

  bool touching = false;
  unsigned long touchStartTime = 0;
  bool focusFired = false;
  bool longFired = false;

  bool tappedFlag = false;
  bool shortReleasedFlag = false;
  bool mediumReleasedFlag = false;
  bool focusPressedFlag = false;
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
    focusPressedFlag = false;
    longPressedFlag = false;

    bool state = digitalRead(TOUCH_PIN) == HIGH;
    unsigned long now = millis();

    if (state != lastState && (now - lastChangeTime) > DEBOUNCE_MS) {
      lastChangeTime = now;
      lastState = state;

      if (state) {
        touching = true;
        touchStartTime = now;
        focusFired = false;
        longFired = false;
        tappedFlag = true;
      } else {
        touching = false;
        if (!focusFired && !longFired) {
          unsigned long heldFor = now - touchStartTime;
          if (heldFor < MEDIUM_HOLD_MS) shortReleasedFlag = true;
          else mediumReleasedFlag = true;
        }
      }
    }

    if (touching && !focusFired && (now - touchStartTime) >= FOCUS_HOLD_MS) {
      focusFired = true;
      focusPressedFlag = true;
    }

    if (touching && !longFired && (now - touchStartTime) >= LONG_HOLD_MS) {
      longFired = true;
      longPressedFlag = true;
    }
  }

  bool wasTapped() { return tappedFlag; }
  bool wasShortReleased() { return shortReleasedFlag; }
  bool wasMediumReleased() { return mediumReleasedFlag; }
  bool wasFocusPressed() { return focusPressedFlag; }
  bool wasLongPressed() { return longPressedFlag; }

}

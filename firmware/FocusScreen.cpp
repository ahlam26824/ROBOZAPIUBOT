#include <Arduino.h>
#include "FocusScreen.h"
#include "DisplayDriver.h"
#include "Sound.h"
#include "Behavior.h"

namespace {
  // Focus Mode State
  bool focusActive = false;
  unsigned long focusTotalSec = 1560; // default 26 min
  unsigned long focusStartMillis = 0;
  bool focusFinished = false;

  // Stopwatch State
  bool stopwatchActive = false;
  bool stopwatchRunning = false;
  unsigned long stopwatchElapsedMs = 0;
  unsigned long stopwatchStartMs = 0;
}

namespace FocusScreen {

  void startFocus(int minutes) {
    if (minutes <= 0) minutes = 26;
    focusTotalSec = (unsigned long)minutes * 60;
    focusStartMillis = millis();
    focusActive = true;
    focusFinished = false;
    stopwatchActive = false;
    Serial.printf("FocusScreen: Focus Started for %d min\n", minutes);
  }

  void stopFocus() {
    focusActive = false;
    focusFinished = false;
    Serial.println("FocusScreen: Focus Stopped");
  }

  bool isFocusActive() {
    return focusActive;
  }

  void startStopwatch() {
    if (!stopwatchRunning) {
      stopwatchStartMs = millis() - stopwatchElapsedMs;
      stopwatchRunning = true;
    }
    stopwatchActive = true;
    focusActive = false;
    Serial.println("FocusScreen: Stopwatch Started");
  }

  void pauseStopwatch() {
    if (stopwatchRunning) {
      stopwatchElapsedMs = millis() - stopwatchStartMs;
      stopwatchRunning = false;
    }
    Serial.println("FocusScreen: Stopwatch Paused");
  }

  void resetStopwatch() {
    stopwatchElapsedMs = 0;
    if (stopwatchRunning) {
      stopwatchStartMs = millis();
    }
    Serial.println("FocusScreen: Stopwatch Reset");
  }

  void exitStopwatch() {
    stopwatchActive = false;
    stopwatchRunning = false;
    stopwatchElapsedMs = 0;
    Serial.println("FocusScreen: Stopwatch Exited");
  }

  bool isStopwatchActive() {
    return stopwatchActive;
  }

  void update() {
    if (focusActive && !focusFinished) {
      unsigned long elapsedSec = (millis() - focusStartMillis) / 1000;
      if (elapsedSec >= focusTotalSec) {
        focusFinished = true;
        focusActive = false;
        Sound::playHappySound();
        Behavior::reactToGameResult(false); // Trigger celebration reaction
      }
    }
  }

  void draw() {
    if (focusActive) {
      unsigned long elapsedSec = (millis() - focusStartMillis) / 1000;
      long remainSec = (long)focusTotalSec - (long)elapsedSec;
      if (remainSec < 0) remainSec = 0;

      int m = remainSec / 60;
      int s = remainSec % 60;

      char timerBuf[16];
      snprintf(timerBuf, sizeof(timerBuf), "%02d:%02d", m, s);

      // Header
      display.setFont(u8g2_font_6x10_tf);
      display.drawStr(32, 12, "FOCUS MODE");

      // Big Timer Display
      display.setFont(u8g2_font_logisoso24_tf);
      int wTime = display.getStrWidth(timerBuf);
      display.drawStr((128 - wTime) / 2, 42, timerBuf);

      // Subtitle
      display.setFont(u8g2_font_5x7_tf);
      display.drawStr(36, 58, "Stay Focused!");

    } else if (stopwatchActive) {
      unsigned long totalMs = stopwatchElapsedMs;
      if (stopwatchRunning) {
        totalMs = millis() - stopwatchStartMs;
      }
      unsigned long totalSec = totalMs / 1000;
      int m = (totalSec / 60) % 100;
      int s = totalSec % 60;
      int ds = (totalMs % 1000) / 100;

      char swBuf[16];
      snprintf(swBuf, sizeof(swBuf), "%02d:%02d.%d", m, s, ds);

      // Header
      display.setFont(u8g2_font_6x10_tf);
      display.drawStr(34, 12, "STOPWATCH");

      // Big Timer Display
      display.setFont(u8g2_font_logisoso24_tf);
      int wTime = display.getStrWidth(swBuf);
      display.drawStr((128 - wTime) / 2, 42, swBuf);

      // Subtitle status
      display.setFont(u8g2_font_5x7_tf);
      if (stopwatchRunning) {
        display.drawStr(44, 58, "[ Running ]");
      } else {
        display.drawStr(46, 58, "[ Paused ]");
      }
    }
  }

}

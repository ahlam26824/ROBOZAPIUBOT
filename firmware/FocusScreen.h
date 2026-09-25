#pragma once
#include <Arduino.h>

namespace FocusScreen {
  void startFocus(int minutes = 26);
  void stopFocus();
  bool isFocusActive();

  void startStopwatch();
  void pauseStopwatch();
  void resetStopwatch();
  void exitStopwatch();
  bool isStopwatchActive();

  void update();
  void draw();
}

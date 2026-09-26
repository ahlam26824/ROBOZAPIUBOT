#pragma once
#include <Arduino.h>

namespace TextScreen {
  void begin();
  void setText(const String& text);
  void setSequence(uint32_t intervalMs, const String& pipeSeparatedTexts);
  void stopSequence();
  void update();
  void processCommand(const String& cmd);
  
  bool isActive();
  void start();
  void exit();
  void draw();
}

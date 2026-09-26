#pragma once
#include <Arduino.h>

namespace DrawScreen {
  void begin();
  void clear();
  void setPixel(int x, int y, bool state);
  void drawLine(int x1, int y1, int x2, int y2, bool state);
  void loadBitmapChunk(int chunkIdx, const String& hexStr);
  void processCommand(const String& cmd);
  
  bool isActive();
  void start();
  void exit();
  void draw();
}

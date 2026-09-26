#include "DrawScreen.h"
#include "DisplayDriver.h"

namespace {
  uint8_t canvasBuffer[1024]; // 128x64 bits / 8 = 1024 bytes
  bool active = false;

  uint8_t hexToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  }
}

namespace DrawScreen {

  void begin() {
    clear();
    active = false;
  }

  void clear() {
    memset(canvasBuffer, 0, sizeof(canvasBuffer));
  }

  void setPixel(int x, int y, bool state) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int byteIdx = y * 16 + (x / 8);
    int bitIdx = x % 8;
    if (state) {
      canvasBuffer[byteIdx] |= (1 << bitIdx);
    } else {
      canvasBuffer[byteIdx] &= ~(1 << bitIdx);
    }
  }

  void drawLine(int x0, int y0, int x1, int y1, bool state) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
      setPixel(x0, y0, state);
      if (x0 == x1 && y0 == y1) break;
      e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }

  void loadBitmapChunk(int chunkIdx, const String& hexStr) {
    int offset = chunkIdx * 256;
    int len = hexStr.length();
    for (int i = 0; i < len; i += 2) {
      if (offset + (i / 2) >= 1024) break;
      char b1 = hexStr[i];
      char b2 = (i + 1 < len) ? hexStr[i + 1] : '0';
      uint8_t val = (hexToNibble(b1) << 4) | hexToNibble(b2);
      canvasBuffer[offset + (i / 2)] = val;
    }
  }

  void processCommand(const String& cmd) {
    if (cmd == "draw:clear") {
      clear();
      active = true;
    } else if (cmd == "draw:exit") {
      active = false;
    } else if (cmd.startsWith("draw:pixel:")) {
      // draw:pixel:x,y,state
      int first = cmd.indexOf(':', 11);
      int second = cmd.indexOf(':', first + 1);
      if (first > 0 && second > 0) {
        int x = cmd.substring(11, first).toInt();
        int y = cmd.substring(first + 1, second).toInt();
        int st = cmd.substring(second + 1).toInt();
        setPixel(x, y, st != 0);
        active = true;
      }
    } else if (cmd.startsWith("draw:line:")) {
      // draw:line:x1,y1,x2,y2,state
      int idx1 = cmd.indexOf(':', 10);
      int idx2 = cmd.indexOf(':', idx1 + 1);
      int idx3 = cmd.indexOf(':', idx2 + 1);
      int idx4 = cmd.indexOf(':', idx3 + 1);
      if (idx1 > 0 && idx2 > 0 && idx3 > 0 && idx4 > 0) {
        int x1 = cmd.substring(10, idx1).toInt();
        int y1 = cmd.substring(idx1 + 1, idx2).toInt();
        int x2 = cmd.substring(idx2 + 1, idx3).toInt();
        int y2 = cmd.substring(idx3 + 1, idx4).toInt();
        int st = cmd.substring(idx4 + 1).toInt();
        drawLine(x1, y1, x2, y2, st != 0);
        active = true;
      }
    } else if (cmd.startsWith("draw:bmp:")) {
      // draw:bmp:chunkIdx:hexData
      int idx1 = cmd.indexOf(':', 9);
      if (idx1 > 0) {
        int chunkIdx = cmd.substring(9, idx1).toInt();
        String hexData = cmd.substring(idx1 + 1);
        loadBitmapChunk(chunkIdx, hexData);
        active = true;
      }
    }
  }

  bool isActive() {
    return active;
  }

  void start() {
    active = true;
  }

  void exit() {
    active = false;
  }

  void draw() {
    display.drawXBMP(0, 0, 128, 64, canvasBuffer);
  }
}

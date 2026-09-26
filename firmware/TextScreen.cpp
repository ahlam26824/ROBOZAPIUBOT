#include "TextScreen.h"
#include "DisplayDriver.h"

namespace {
  bool active = false;
  String currentText = "";
  
  // Animation Sequence variables
  String sequenceItems[16];
  int sequenceCount = 0;
  int currentSeqIndex = 0;
  uint32_t intervalMs = 2000;
  uint32_t lastAdvanceMs = 0;
  bool isAnimActive = false;

  void parseSequence(const String& data) {
    sequenceCount = 0;
    currentSeqIndex = 0;
    int start = 0;
    while (start < data.length() && sequenceCount < 16) {
      int pipeIdx = data.indexOf('|', start);
      if (pipeIdx < 0) {
        String item = data.substring(start);
        item.trim();
        if (item.length() > 0) {
          sequenceItems[sequenceCount++] = item;
        }
        break;
      } else {
        String item = data.substring(start, pipeIdx);
        item.trim();
        if (item.length() > 0) {
          sequenceItems[sequenceCount++] = item;
        }
        start = pipeIdx + 1;
      }
    }
  }
}

namespace TextScreen {

  void begin() {
    active = false;
    currentText = "";
    sequenceCount = 0;
    isAnimActive = false;
  }

  void setText(const String& text) {
    currentText = text;
    isAnimActive = false;
    active = true;
  }

  void setSequence(uint32_t interval, const String& pipeSeparatedTexts) {
    intervalMs = interval;
    if (intervalMs < 300) intervalMs = 300;
    parseSequence(pipeSeparatedTexts);
    if (sequenceCount > 0) {
      currentSeqIndex = 0;
      currentText = sequenceItems[0];
      lastAdvanceMs = millis();
      isAnimActive = true;
      active = true;
    }
  }

  void stopSequence() {
    isAnimActive = false;
  }

  void update() {
    if (!active || !isAnimActive || sequenceCount <= 1) return;
    uint32_t now = millis();
    if (now - lastAdvanceMs >= intervalMs) {
      lastAdvanceMs = now;
      currentSeqIndex = (currentSeqIndex + 1) % sequenceCount;
      currentText = sequenceItems[currentSeqIndex];
    }
  }

  void processCommand(const String& cmd) {
    if (cmd == "text:exit") {
      active = false;
      isAnimActive = false;
    } else if (cmd.startsWith("text:single:")) {
      setText(cmd.substring(12));
    } else if (cmd.startsWith("text:seq:")) {
      // text:seq:interval_ms:text1|text2|text3...
      int first = cmd.indexOf(':', 9);
      if (first > 0) {
        uint32_t ms = cmd.substring(9, first).toInt();
        String pipeStr = cmd.substring(first + 1);
        setSequence(ms, pipeStr);
      }
    } else if (cmd.startsWith("text:")) {
      setText(cmd.substring(5));
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
    isAnimActive = false;
  }

  void draw() {
    if (currentText.length() == 0) return;

    // Draw frame box
    display.drawRFrame(3, 3, 122, 58, 6);
    display.drawRFrame(5, 5, 118, 54, 4);

    // If animation active, draw progress indicator dot/badge in header
    if (isAnimActive && sequenceCount > 1) {
      display.setFont(u8g2_font_4x6_tf);
      String badge = "[" + String(currentSeqIndex + 1) + "/" + String(sequenceCount) + "]";
      display.drawStr(92, 12, badge.c_str());
    }

    // Split text into up to 3 lines if long
    if (currentText.length() <= 10) {
      // Single short line - big font
      display.setFont(u8g2_font_profont17_tf);
      int w = display.getStrWidth(currentText.c_str());
      int x = (128 - w) / 2;
      display.drawStr(x > 6 ? x : 6, 36, currentText.c_str());
    } else {
      // Medium to long text - wrap into lines
      display.setFont(u8g2_font_profont12_tf);
      String line1 = "", line2 = "", line3 = "";
      int len = currentText.length();
      
      if (len <= 18) {
        line1 = currentText;
      } else if (len <= 36) {
        int space = currentText.lastIndexOf(' ', 18);
        if (space <= 0) space = 18;
        line1 = currentText.substring(0, space);
        line2 = currentText.substring(space + 1);
      } else {
        int space1 = currentText.lastIndexOf(' ', 18);
        if (space1 <= 0) space1 = 18;
        line1 = currentText.substring(0, space1);
        
        int space2 = currentText.lastIndexOf(' ', space1 + 18);
        if (space2 <= space1) space2 = space1 + 18;
        line2 = currentText.substring(space1 + 1, space2);
        line3 = currentText.substring(space2 + 1);
      }

      int yBase = (line3.length() > 0) ? 22 : (line2.length() > 0 ? 28 : 36);

      if (line1.length() > 0) {
        int w = display.getStrWidth(line1.c_str());
        display.drawStr((128 - w) / 2, yBase, line1.c_str());
      }
      if (line2.length() > 0) {
        int w = display.getStrWidth(line2.c_str());
        display.drawStr((128 - w) / 2, yBase + 14, line2.c_str());
      }
      if (line3.length() > 0) {
        int w = display.getStrWidth(line3.c_str());
        display.drawStr((128 - w) / 2, yBase + 28, line3.c_str());
      }
    }
  }
}

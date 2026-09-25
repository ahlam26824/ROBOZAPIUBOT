#include <Arduino.h>
#include <Wire.h>
#include "DisplayDriver.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

void initDisplay() {
  Wire.begin(8, 9); // SDA, SCL
  display.begin();
}

void showBootSplash() {
  // The product name gets the big font since it's short enough to
  // afford it; "by ZAN Tech" is the small subtitle underneath.
  const char* line1 = "Zani";
  const char* line2 = "by ZAN Tech";

  display.setFont(u8g2_font_ncenB18_tr);
  int w1 = display.getStrWidth(line1);
  int x1 = (128 - w1) / 2;

  display.setFont(u8g2_font_ncenB08_tr);
  int w2 = display.getStrWidth(line2);
  int x2 = (128 - w2) / 2;

  const int y1 = 34;
  const int y2 = 50;

  // Hold the splash on screen for a moment.
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB18_tr);
  display.drawStr(x1, y1, line1);
  display.setFont(u8g2_font_ncenB08_tr);
  display.drawStr(x2, y2, line2);
  display.sendBuffer();
  delay(1500);

  // Slide both lines upward off the top edge instead of cutting away
  // abruptly.
  for (int offset = 0; offset <= 52; offset += 3) {
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB18_tr);
    display.drawStr(x1, y1 - offset, line1);
    display.setFont(u8g2_font_ncenB08_tr);
    display.drawStr(x2, y2 - offset, line2);
    display.sendBuffer();
    delay(15);
  }
}

void drawMessage(const char* line1, const char* line2) {
  display.setFont(u8g2_font_6x10_tf);
  int w1 = display.getStrWidth(line1);
  display.drawStr((128 - w1) / 2, 30, line1);

  if (line2 != nullptr) {
    display.setFont(u8g2_font_5x7_tf);
    int w2 = display.getStrWidth(line2);
    display.drawStr((128 - w2) / 2, 44, line2);
  }
}

void showMessage(const char* line1, const char* line2) {
  display.clearBuffer();
  drawMessage(line1, line2);
  display.sendBuffer();
}

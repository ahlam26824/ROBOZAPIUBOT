#include <Arduino.h>
#include "WatchScreen.h"
#include "DisplayDriver.h"
#include "BLEComm.h"

namespace WatchScreen {

  void draw() {
    display.setFont(u8g2_font_logisoso24_tf);
    String t = BLEComm::getTime();
    int wTime = display.getStrWidth(t.c_str());
    display.drawStr((128 - wTime) / 2, 38, t.c_str());

    display.setFont(u8g2_font_5x7_tf);
    String d = BLEComm::getDate();
    int wDate = display.getStrWidth(d.c_str());
    display.drawStr((128 - wDate) / 2, 50, d.c_str());

    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f C", BLEComm::getTemperature());
    display.setFont(u8g2_font_6x10_tf);
    int wTemp = display.getStrWidth(tempBuf);
    display.drawStr((128 - wTemp) / 2, 63, tempBuf);

    if (!BLEComm::isConnected()) {
      display.setFont(u8g2_font_5x7_tf);
      display.drawStr(2, 8, "(offline)");
    }
  }

}

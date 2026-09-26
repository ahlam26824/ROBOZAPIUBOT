#pragma once
#include <Arduino.h>

// BLE GATT server for the companion phone app & PC software.
// Characteristics:
//   TIME_CHAR    "HH:MM|Weekday, YYYY-MM-DD"
//   TEMP_CHAR    temperature as text
//   NOTIFY_CHAR  "Title|Message"
//   MODE_CHAR    "0" = simple face mode, "1" = watch mode
//   RESULT_CHAR  "win"/"lose"/"draw"
//   FOCUS_CHAR   "focus:mins", "sw:start", "sw:stop", "sw:reset", "sw:off"
//   DRAW_CHAR    "draw:clear", "draw:pixel:x,y,s", "draw:line:x1,y1,x2,y2,s", "draw:bmp:chunk:hex", "draw:exit"
//   TEXT_CHAR    "text:single:Text", "text:seq:interval_ms:T1|T2|T3", "text:exit"
namespace BLEComm {

  void begin();
  void startPairing();
  void stopPairing();

  bool isPairing();
  bool isConnected();

  bool hasNewTime();
  String getTime();
  String getDate();

  bool hasNewTemperature();
  float getTemperature();

  bool hasNewNotification();
  String getNotificationTitle();
  String getNotificationMessage();

  bool hasNewMode();
  bool wantsWatchMode();

  bool hasNewFocusCommand();
  String getFocusCommand();

  bool hasNewGameResult();
  String getGameResult();

  bool hasNewDrawCommand();
  String getDrawCommand();

  bool hasNewTextCommand();
  String getTextCommand();

}

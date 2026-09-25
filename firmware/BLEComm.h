#pragma once
#include <Arduino.h>

// BLE GATT server for the companion phone app. Uses the ESP32 core's
// built-in BLE library (BLEDevice.h etc.) rather than an extra installed
// library, for the same reason the rest of this project avoids optional
// dependencies -- one less thing to break across setups.
//
// Protocol: one custom service, five write-only characteristics, each a
// plain UTF-8 string (easy to debug with any generic BLE app, e.g.
// nRF Connect, even before the real companion app exists):
//   TIME_CHAR    "HH:MM|Weekday, YYYY-MM-DD"
//   TEMP_CHAR    a temperature as text, e.g. "23.5"
//   NOTIFY_CHAR  "Title|Message" -- a phone notification to display
//   MODE_CHAR    "0" = simple face mode, "1" = watch mode
//   RESULT_CHAR  "win"/"lose"/"draw" -- outcome of the in-app Tic-Tac-Toe
//                game, from the PHONE USER's perspective (see the app's
//                TicTacToeScreen); the bot reacts as the opponent, so
//                "win" (user won) makes it Sad and "lose" (user lost)
//                makes it Happy -- see Behavior::reactToGameResult()
namespace BLEComm {

  // Sets up the BLE stack, service, and characteristics. Call once from
  // setup(). Does NOT start advertising -- nothing is pairable until
  // startPairing() is called.
  void begin();

  // Starts advertising so a phone can find and connect to the bot.
  void startPairing();

  // Stops advertising without waiting for a connection (e.g. the user
  // cancelled, or a timeout).
  void stopPairing();

  bool isPairing();
  bool isConnected();

  // Each hasNewX() is true exactly once, the first time it's checked
  // after new data arrives from the phone -- checking it again returns
  // false until the phone sends something new.
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
  String getGameResult(); // "win", "lose", or "draw" -- see the header comment above

}

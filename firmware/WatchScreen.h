#pragma once

// The "smart watch" screen: time, date, and temperature as last received
// from the phone over BLE (see BLEComm.h).
namespace WatchScreen {

  // Draws into the shared display buffer. Does not call
  // clearBuffer()/sendBuffer() -- the caller owns that.
  void draw();

}

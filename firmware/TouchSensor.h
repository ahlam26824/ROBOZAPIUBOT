#pragma once

// Digital touch sensor module (e.g. TTP223-style breakout) on GPIO10,
// output HIGH while touched.
//
// Only wasShortReleased() is currently wired up (see Behavior.cpp: the
// pat reaction -- plays Happy, doesn't change the expression --
// deliberately NOT wasTapped(), see that method's comment below for
// why). Bluetooth no longer needs a gesture to become connectable --
// BLEComm advertises from boot (see firmware.ino) -- so wasLongPressed()
// and wasMediumReleased() aren't wired to anything right now. Both are
// kept anyway since the short/medium/long tiering logic itself doesn't
// depend on what it's used for, in case a future gesture wants it:
//   < 3s, released              -> short
//   3s-8s, released             -> medium
//   reaches 8s (fires at 8s,
//   no need to wait for release) -> long
namespace TouchSensor {

  // Call once from setup().
  void begin();

  // Reads the sensor once and updates tap/hold state. Call exactly once
  // per loop iteration, regardless of app mode.
  void update();

  // True on the update() call where a touch STARTED (the instant you
  // touch it). NOT currently used for the pat reaction -- pat plays a
  // real ~2s BLOCKING sound (see Sound.cpp), and firing that on
  // touch-DOWN would freeze this module's own polling for that whole
  // window. Since the 8s pairing hold starts with the very same
  // touch-down event, that blocked window would corrupt its timing
  // (this exact bug happened before with a 3-5s gesture -- see git
  // history/Behavior.cpp). wasShortReleased() avoids it: nothing blocks
  // until a touch is already confirmed to have been short.
  bool wasTapped();

  // True on the update() call where a touch ENDED after being held less
  // than 3 seconds -- a genuine short tap-and-release. This is what pat
  // actually uses.
  bool wasShortReleased();

  // True on the update() call where a touch ENDED after being held
  // between 3 and 8 seconds.
  bool wasMediumReleased();

  // True once, on the update() call where a continuous touch reaches
  // the 8-second mark (fires immediately, does not wait for release).
  bool wasLongPressed();

}

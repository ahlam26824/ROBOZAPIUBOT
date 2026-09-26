#pragma once

// Digital touch sensor module (e.g. TTP223-style breakout) on GPIO10,
// output HIGH while touched.
//
// Gesture Tiers:
//   < 5s, released               -> short tap (Pat reaction)
//   5s-7s, released              -> medium release (Dino Game open/close)
//   7s continuous hold           -> Focus Mode 26-min timer
//   8s continuous hold (8+ sec)  -> Stopwatch toggle on OLED
namespace TouchSensor {

  void begin();
  void update();

  bool wasTapped();
  bool wasShortReleased();
  bool wasMediumReleased();
  bool wasFocusPressed();
  bool wasLongPressed();

}

#pragma once
#include "types.h"

// Owns the current expression, its eye animation state, and blinking.
//
// Every expression is drawn with the SAME single eye shape (a rounded
// rectangle) -- there is no per-emotion icon set (no hearts, no
// sunglasses, no spirals). Emotion comes from that one shape's size,
// position, idle motion, and an optional "eyelid tilt" (an angled top
// corner -- furrowed V-brows for Angry, drooping outer corners for Sad).
// This keeps the visual language consistent no matter how many
// expressions get added later, and keeps each new expression cheap to
// add: just target parameters and a motion signature, not a new drawing
// routine. Blinking is a smooth height dip on the same shape, not a
// swap to a different flat-line icon.
namespace Faces {

  // Switches to expression `f`.
  //  - randomizeDirection: the eyes get a random left/right glance
  //    target (used when idling/cycling); false centers them.
  //  - instant: the eyes snap straight to their target size/position
  //    instead of gliding there over the next few frames. Use true for
  //    sensor-triggered reactions (a pat, a shake, a pickup), which
  //    should read as a sudden pop rather than a smooth transition;
  //    false for normal idle cycling.
  void setFace(Expression f, bool randomizeDirection = false, bool instant = false);

  // The expression last passed to setFace(). Used by firmware.ino to
  // decide whether something reactive is currently going on (anything
  // other than Normal) and the Face should be shown instead of the
  // Bluetooth watch-mode Clock screen, even while that screen is what
  // watch mode would otherwise be showing.
  Expression getCurrentExpression();

  // Advances the eye shape one step towards its target, and the blink
  // timer. Call once per loop iteration.
  void updateEyeAnimation();
  void updateBlink();

  // Feeds the bot's current physical tilt (degrees, relative to its
  // calibrated resting orientation -- see Accelerometer::getTiltX/Y)
  // into a subtle idle effect: the eyes drift a few pixels toward the
  // direction the bot is tilted, smoothed with a spring/damper so it
  // settles instead of snapping or oscillating. This is layered UNDER
  // the per-mood idle motion and glance targeting in draw(), not a mood
  // of its own. Call once per loop iteration whenever the bot isn't in
  // the middle of a shake/pickup reaction -- those already have their
  // own dedicated motion and would fight with this.
  void updateEyeTracking(float tiltXDeg, float tiltYDeg);

  // Draws the current expression into the shared display buffer. Does
  // not call clearBuffer()/sendBuffer() -- the caller owns that.
  void draw();

}

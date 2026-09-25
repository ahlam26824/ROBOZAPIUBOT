#include <Arduino.h>
#include "Behavior.h"
#include "types.h"
#include "Faces.h"
#include "Accelerometer.h"
#include "TouchSensor.h"
#include "Sound.h"

namespace {
  // How long with no interaction before the bot goes Sad. Fires ONCE
  // (face + sound) -- it does not keep repeating while still ignored;
  // only a fresh interaction (pat/shake/pickup) re-arms it. See
  // nextSadTriggerTime and the SAD_TRIGGER_DISARMED sentinel below.
  const unsigned long IDLE_SAD_TIMEOUT_MS = 3UL * 60UL * 1000UL;

  // Brief spin before a shake settles into Angry -- see Behavior.h.
  const unsigned long DIZZY_TRANSITION_MS = 500;

  // How long a temporary reaction (pat's Happy, or a game result's
  // Happy/Sad -- see reactToGameResult()) is held before reverting to
  // Normal.
  const unsigned long TIMED_REACTION_MS = 3500;

  // A nextSadTriggerTime this far out is effectively "never" (~49 days),
  // used to disarm the one-shot Sad trigger once it's already fired,
  // until the next interaction calls resetIdleClock() and re-arms it.
  const unsigned long SAD_TRIGGER_DISARMED = 0xFFFFFFFFUL;

  // When the Sad state (face + one sound play) should next fire. A pat
  // (or a shake) always pushes this a full IDLE_SAD_TIMEOUT_MS into the
  // future, no matter what state it was in before.
  unsigned long nextSadTriggerTime = 0;

  bool inDizzyTransition = false;
  unsigned long dizzyEndTime = 0;

  // True while a temporary reaction (pat's Happy, or a game result) is
  // being held before reverting to Normal -- not specific to Happy
  // despite the name history, see TIMED_REACTION_MS above.
  bool showingTimedReaction = false;
  unsigned long reactionUntil = 0;

  void startTimedReaction(unsigned long now, Expression face) {
    showingTimedReaction = true;
    reactionUntil = now + TIMED_REACTION_MS;
    Faces::setFace(face, /* randomizeDirection = */ false, /* instant = */ true);
  }

  void resetIdleClock(unsigned long now) {
    nextSadTriggerTime = now + IDLE_SAD_TIMEOUT_MS;
  }
}

namespace Behavior {

  void begin() {
    resetIdleClock(millis());
    showingTimedReaction = false;
    Faces::setFace(NORMAL);
  }

  void update(bool faceModeActive) {
    unsigned long now = millis();

    // Accelerometer::update() is called once per loop from firmware.ino
    // (not here) -- it needs to run every frame regardless of which
    // screen is showing, so a shake while looking at the Clock page
    // still gets noticed the instant the Face screen comes back up.

    // Shaking always takes priority -- it overrides the pat/sad logic
    // below entirely for as long as it's happening. A shake starts with
    // a brief Dizzy spin rather than snapping straight to Angry; only
    // reaches Angry (with its sound) once that transition elapses AND
    // it's still shaking by then -- see dizzyEndTime check below.
    if (Accelerometer::shakeStarted()) {
      resetIdleClock(now);
      showingTimedReaction = false;
      inDizzyTransition = true;
      dizzyEndTime = now + DIZZY_TRANSITION_MS;
      Faces::setFace(DIZZY, /* randomizeDirection = */ false, /* instant = */ true);
    } else if (Accelerometer::shakeStopped()) {
      resetIdleClock(now);
      inDizzyTransition = false; // in case it stopped before the transition even finished
      Faces::setFace(NORMAL, /* randomizeDirection = */ false, /* instant = */ true);
      Sound::playHappySound();
    }

    if (inDizzyTransition && now >= dizzyEndTime) {
      inDizzyTransition = false;
      Faces::setFace(ANGRY, /* randomizeDirection = */ false, /* instant = */ true);
      Sound::playAngrySound();
    }

    if (Accelerometer::isShaking()) {
      Faces::updateEyeAnimation();
      Faces::updateBlink();
      return;
    }

    // Being picked up takes priority over pat/sad too, but not over an
    // active shake (handled above) -- you can still shake it while held.
    if (Accelerometer::pickupDetected()) {
      resetIdleClock(now);
      Faces::setFace(SCARED, /* randomizeDirection = */ false, /* instant = */ true);
      Sound::playScaredSound();
    } else if (Accelerometer::putDownDetected()) {
      resetIdleClock(now);
      Faces::setFace(NORMAL, /* randomizeDirection = */ false, /* instant = */ true);
    }

    if (Accelerometer::isPickedUp()) {
      Faces::updateEyeAnimation();
      Faces::updateBlink();
      return;
    }

    // Only fed in while neither shaking nor picked up (both handled --
    // and returned from -- above): those already have their own
    // dedicated motion (jitter, held-Scared) and this subtle tilt-driven
    // glance would just fight with it.
    Faces::updateEyeTracking(Accelerometer::getTiltX(), Accelerometer::getTiltY());

    // A pat: shows Happy and plays its sound, holds it briefly, then
    // reverts to Normal -- and counts as interaction (resets the
    // idle-to-Sad clock) the same as any other. Deliberately
    // wasShortReleased(), not wasTapped(): this plays a real ~2s
    // BLOCKING sound (see Sound.cpp), and firing it on touch-DOWN would
    // freeze touch polling for that whole window, which would corrupt
    // the timing of any hold-based gesture sharing the same touch-down
    // event (this exact bug has already bitten a hold gesture once
    // before -- see TouchSensor.h).
    if (TouchSensor::wasShortReleased()) {
      resetIdleClock(now);
      startTimedReaction(now, HAPPY);
      Sound::playHappySound();
    }

    if (showingTimedReaction) {
      if (now > reactionUntil) {
        showingTimedReaction = false;
        Faces::setFace(NORMAL);
      }
    } else if (!faceModeActive) {
      // Watch mode is showing the Clock screen instead -- being idle
      // there doesn't mean "ignored" the way it does in Face mode, so
      // don't let the idle clock run down at all while it's active.
      // Keeps pushing the trigger out continuously rather than letting
      // it go stale, so switching back to Face mode later doesn't
      // immediately fire Sad from a timer that was quietly overdue the
      // whole time watch mode was on.
      resetIdleClock(now);
    } else if (now >= nextSadTriggerTime) {
      // One-shot: fires once, then disarmed until the next interaction
      // (pat/shake/pickup) calls resetIdleClock() again -- it does NOT
      // keep repeating on a cycle while still ignored.
      Faces::setFace(SAD);
      Sound::playSadSound();
      nextSadTriggerTime = SAD_TRIGGER_DISARMED;
    }

    Faces::updateEyeAnimation();
    Faces::updateBlink();
  }

  bool isShowingTimedReaction() { return showingTimedReaction; }

  void reactToGameResult(bool userWon) {
    unsigned long now = millis();
    resetIdleClock(now);
    // The bot reacts as the OPPONENT: the user winning is a loss for
    // the bot (Sad), and the user losing is a win for the bot (Happy).
    if (userWon) {
      startTimedReaction(now, SAD);
      Sound::playSadSound();
    } else {
      startTimedReaction(now, HAPPY);
      Sound::playHappySound();
    }
  }

}

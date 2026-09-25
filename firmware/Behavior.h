#pragma once

// The idle/reaction strategy:
//  - Default expression is Normal.
//  - Shaking the bot shows Dizzy for a brief ~500ms transition (a spin,
//    not a snap), then Angry with an angry sound once that transition
//    elapses -- but only if still shaking by then; a shake shorter than
//    that just goes straight back to Normal+happy without ever reaching
//    Angry. While shaking continues past that point it stays Angry.
//    Once it settles back down, it instantly returns to Normal with a
//    happy sound (once). Shaking takes priority over everything below.
//  - Picking the bot up (a sustained tilt away from its calibrated
//    resting orientation, as opposed to a brief shake) instantly shows
//    Scared with a scared sound (once). It stays Scared while held.
//    Setting it back down instantly returns it to Normal. Being picked
//    up takes priority over pat/sad below, but not over an active shake
//    -- you can still shake it while holding it.
//  - After 3 minutes with no interaction, switches to Sad and plays the
//    sad sound ONCE -- it does not keep repeating while still ignored.
//    Only a fresh pat/shake/pickup re-arms the idle clock for next time.
//    Face-mode-only: while watch mode is showing the Clock screen
//    instead, the idle clock doesn't run down at all (being idle there
//    isn't "being ignored" the way it is in Face mode) -- see
//    faceModeActive below.
//  - A pat (TouchSensor::wasShortReleased()) shows Happy with its sound,
//    holds it for a few seconds, then reverts to Normal -- and resets
//    the idle-to-Sad clock, same as any other interaction (so patting a
//    Sad bot does cheer it up, via that Happy-then-Normal transition).
//    Ignored while an active shake or pickup is dominating (same
//    priority order as everywhere else here).
//  - reactToGameResult() (see below) uses that same Happy/Sad-then-
//    Normal transition for the in-app Tic-Tac-Toe game's outcome.
namespace Behavior {

  // Call once from setup().
  void begin();

  // Call once per loop iteration, UNCONDITIONALLY -- regardless of
  // whether firmware.ino is currently drawing the Face or the (Bluetooth
  // watch-mode) Clock screen, so a shake/pickup/pat still gets noticed
  // and reacted to even while the Clock screen is what's on-screen; see
  // firmware.ino for how it picks which one to actually draw afterward,
  // based on Faces' current expression. Assumes Accelerometer::update()
  // and TouchSensor::update() already ran once this same frame.
  //
  // faceModeActive: pass false while watch mode is currently showing
  // the Clock screen (i.e. !BLEComm::wantsWatchMode() from firmware.ino)
  // -- this only gates the idle-to-Sad timeout (see above); shake/
  // pickup/pat reactions still work identically either way.
  //
  // Reads the sensors' latest flags, updates timers, drives
  // Faces::setFace() as needed, and steps the face's own animation/blink.
  void update(bool faceModeActive);

  // True while a temporary reaction (pat's Happy, or a game result's
  // Happy/Sad) is being held before reverting to Normal. Distinct from
  // just checking the current expression for Sad, because idle-timeout
  // Sad is NOT one of these -- firmware.ino uses this to decide whether
  // to pop the Face up over the watch-mode Clock screen: a temporary
  // reaction should be seen even in watch mode, but idle Sad shouldn't
  // interrupt the clock (see firmware.ino's screen-selection comment).
  bool isShowingTimedReaction();

  // Called when a Tic-Tac-Toe result arrives from the phone app (see
  // BLEComm's RESULT_CHAR). The bot reacts as the OPPONENT it was in
  // that game: userWon=true (the user won, so the bot lost) shows Sad
  // with its sound; userWon=false (the user lost, so the bot won) shows
  // Happy with its sound. Either way it holds briefly then reverts to
  // Normal, resetting the idle-to-Sad clock like any other interaction.
  // A draw is not surfaced here -- the caller (firmware.ino) only calls
  // this for an actual win/lose result.
  void reactToGameResult(bool userWon);

}

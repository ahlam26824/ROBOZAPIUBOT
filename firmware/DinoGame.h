#pragma once

// A small Chrome-Dino-style endless runner, played entirely with the
// touch sensor: tap to jump over cacti; tap again on the Game Over
// screen to play again. Entering/exiting the game itself is a ~3-second
// hold, handled by the caller (firmware.ino) -- this module only needs
// to know whether it's currently showing the Game Over screen, so the
// caller can tell a short "retry" tap apart from a long "exit" hold.
namespace DinoGame {

  // Resets to a fresh game. Call once when switching into game mode
  // (and internally on a Game-Over restart).
  void begin();

  // Advances the game one frame.
  //  - tapped: TouchSensor::wasTapped() for this frame -- jumps while
  //    playing.
  //  - released: TouchSensor::wasShortReleased() for this frame -- retries
  //    from the Game Over screen (a short tap-and-release only; a long
  //    press that's about to exit the game must NOT also retry it).
  void update(bool tapped, bool released);

  // Draws the current game state into the shared display buffer. Does
  // not call clearBuffer()/sendBuffer() -- the caller owns that.
  void draw();

  // True while the Game Over screen is showing -- the caller uses this
  // to decide whether a long press should exit back to normal mode.
  bool isGameOver();

}

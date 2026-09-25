// Zani (by ZAN Tech) -- Display + Motion + Sound + Eyes + Bluetooth + Game.
//
//   types.h              - the Expression enum
//   DisplayDriver.h/.cpp - owns the OLED display object, the boot splash
//                          ("Zani" / "by ZAN Tech"), and status messages
//   Faces.h/.cpp         - the expressions (one shared eye shape, plus
//                          Dizzy's orbiting punch-through dot and Sad's
//                          falling tear)
//   Accelerometer.h/.cpp - ADXL345 shake/pickup/tilt, with startup
//                          self-calibration
//   TouchSensor.h/.cpp   - the touch sensor on GPIO10 -- short release
//                          for the pat reaction (see Behavior.h), medium
//                          hold (~3s) to open/close the hidden game (see
//                          below)
//   SoundData.h          - real sound clips, embedded as PROGMEM arrays
//   Sound.h/.cpp         - plays those clips through the PAM8403 amp
//   Behavior.h/.cpp      - the reaction strategy (see its header) --
//                          shake -> brief Dizzy spin -> Angry (if still
//                          shaking) -> Normal+happy once it stops; a pat
//                          plays Happy without changing the expression
//   BLEComm.h/.cpp       - the BLE link to the "Zani Bot" phone app
//                          (time, temperature, watch-mode on/off, and
//                          forwarded phone notifications -- each plays
//                          the alarm sound, see loop()) -- advertising
//                          starts right at boot (see setup()), no
//                          gesture needed to make it connectable
//   WatchScreen.h/.cpp   - the time/date/temperature display, fed by
//                          BLEComm once the app is connected
//   DinoGame.h/.cpp      - the hidden mini-game (see its header)
//
// Screen selection (see loop()): while the app has watch mode turned on
// AND no genuine sensor reaction (Dizzy/Angry from a shake, Scared from
// a pickup) is currently happening, the Clock/watch screen shows instead
// of the animated face. Shaking, being picked up, or a pat all still
// work exactly as normal even while the Clock screen is showing --
// Behavior::update() runs every frame regardless of which screen is
// actually drawn afterward, so the face pops back up to react, then
// settles back to the Clock screen on its own once the reaction ends.
// The game is a separate top-level mode that takes over the whole
// screen exclusively -- Bluetooth/watch-mode/Behavior are all paused
// while playing (touch a: wasMediumReleased() -- ~3s hold, released --
// toggles in and out of it).

#include "types.h"
#include "DisplayDriver.h"
#include "Faces.h"
#include "Accelerometer.h"
#include "TouchSensor.h"
#include "Sound.h"
#include "Behavior.h"
#include "BLEComm.h"
#include "WatchScreen.h"
#include "DinoGame.h"

enum AppMode { MODE_NORMAL, MODE_GAME };
AppMode appMode = MODE_NORMAL;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  initDisplay();
  showBootSplash();

  TouchSensor::begin();
  BLEComm::begin();
  BLEComm::startPairing(); // advertise from boot -- always connectable, no gesture needed

  // The accelerometer needs a moment, sitting still, to learn its own
  // resting noise level (see Accelerometer.cpp) -- let the user know why
  // nothing else is happening yet.
  showMessage("Calibrating sensors...", "Please keep me still");
  Accelerometer::begin();
  Sound::begin();
  Behavior::begin();
}

void loop() {
  Accelerometer::update(); // once per loop -- see Behavior.h
  TouchSensor::update();

  display.clearBuffer();

  if (appMode == MODE_GAME) {
    DinoGame::update(TouchSensor::wasTapped(), TouchSensor::wasShortReleased());

    // Exiting the game is the same ~3s hold used to enter it -- only
    // meaningful once the game has actually ended.
    if (DinoGame::isGameOver() && TouchSensor::wasMediumReleased()) {
      appMode = MODE_NORMAL;
      Behavior::begin(); // fresh idle state -- coming back from the game counts as a reset
    }

    DinoGame::draw();

  } else { // MODE_NORMAL
    if (TouchSensor::wasMediumReleased()) {
      appMode = MODE_GAME;
      DinoGame::begin();
    }

    if (appMode == MODE_NORMAL) { // didn't just switch away above
      // A brief one-time flash when the phone app actually connects --
      // BLEComm keeps advertising in the background the rest of the
      // time (including automatically after a disconnect), so there's
      // no separate "pairing mode" to enter or leave anymore.
      static bool wasConnected = false;
      bool isConnected = BLEComm::isConnected();
      if (isConnected && !wasConnected) {
        drawMessage("Connected!", "Pisu app connected by Bluetooth");
        display.sendBuffer();
        delay(1200);
      }
      wasConnected = isConnected;

      // A phone notification arrived (the app forwards these once BLE
      // is connected and notification access is granted -- see the
      // app's NotificationForwarder) -- just an alert sound for now, no
      // on-screen banner; add one later if that's wanted.
      if (BLEComm::hasNewNotification()) {
        Sound::playAlarmSound();
      }

      // The in-app Tic-Tac-Toe game finished (see the app's
      // TicTacToeScreen) -- the bot reacts as the opponent it just
      // played. A draw isn't surfaced as a reaction; there's no natural
      // "how does the bot feel about a tie" response.
      if (BLEComm::hasNewGameResult()) {
        String result = BLEComm::getGameResult();
        if (result == "win") {
          Behavior::reactToGameResult(/* userWon = */ true);
        } else if (result == "lose") {
          Behavior::reactToGameResult(/* userWon = */ false);
        }
      }

      bool watchMode = BLEComm::wantsWatchMode();
      Behavior::update(/* faceModeActive = */ !watchMode);

      // A genuine reaction pops the Face up over the Clock screen: a
      // shake's Dizzy/Angry, a pickup's Scared, or Behavior's own
      // "temporary reaction" flag (a pat's Happy, or a game result's
      // Happy/Sad -- see Behavior::isShowingTimedReaction()). That flag
      // matters here specifically because a game-result Sad is NOT the
      // same thing as idle-timeout Sad -- checking the flag rather than
      // just "expr == SAD" is what keeps the two apart, since
      // idle-timeout Sad deliberately stays hidden behind the Clock
      // screen (it can't even fire while faceModeActive is false) while
      // a game result should still be seen even in watch mode.
      Expression expr = Faces::getCurrentExpression();
      bool reacting = (expr == DIZZY || expr == ANGRY || expr == SCARED || Behavior::isShowingTimedReaction());

      if (watchMode && !reacting) {
        WatchScreen::draw();
      } else {
        Faces::draw();
      }
    }
  }

  display.sendBuffer();
  delay(15);
}

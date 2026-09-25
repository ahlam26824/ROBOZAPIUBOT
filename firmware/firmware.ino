// Pisu Bot (by Roboza) -- Display + Motion + Sound + Eyes + Bluetooth + Game.
//
//   types.h              - the Expression enum
//   DisplayDriver.h/.cpp - owns the OLED display object, the boot splash
//                          ("Pisu Bot" / "by Roboza"), and status messages
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
//   BLEComm.h/.cpp       - the BLE link to the "Pisu Bot Bot" phone app
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
#include "FocusScreen.h"

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

  showMessage("Calibrating sensors...", "Please keep me still");
  Accelerometer::begin();
  Sound::begin();
  Behavior::begin();
}

void loop() {
  Accelerometer::update(); // once per loop -- see Behavior.h
  TouchSensor::update();
  FocusScreen::update();

  display.clearBuffer();

  if (appMode == MODE_GAME) {
    DinoGame::update(TouchSensor::wasTapped(), TouchSensor::wasShortReleased());

    if (DinoGame::isGameOver() && TouchSensor::wasMediumReleased()) {
      appMode = MODE_NORMAL;
      Behavior::begin();
    }

    DinoGame::draw();

  } else { // MODE_NORMAL
    // 7-second touch hold -> Automatic 26-minute Focus Mode Timer!
    if (TouchSensor::wasFocusPressed()) {
      FocusScreen::startFocus(26);
    }

    if (TouchSensor::wasMediumReleased()) {
      appMode = MODE_GAME;
      DinoGame::begin();
    }

    if (appMode == MODE_NORMAL) {
      static bool wasConnected = false;
      bool isConnected = BLEComm::isConnected();
      if (isConnected && !wasConnected) {
        drawMessage("Connected!", "Pisu app connected by Bluetooth");
        display.sendBuffer();
        delay(1200);
      }
      wasConnected = isConnected;

      if (BLEComm::hasNewNotification()) {
        Sound::playAlarmSound();
      }

      if (BLEComm::hasNewFocusCommand()) {
        String cmd = BLEComm::getFocusCommand();
        if (cmd.startsWith("focus:")) {
          int mins = cmd.substring(6).toInt();
          if (mins <= 0) FocusScreen::stopFocus();
          else FocusScreen::startFocus(mins);
        } else if (cmd == "sw:start") {
          FocusScreen::startStopwatch();
        } else if (cmd == "sw:stop") {
          FocusScreen::pauseStopwatch();
        } else if (cmd == "sw:reset") {
          FocusScreen::resetStopwatch();
        } else if (cmd == "sw:off") {
          FocusScreen::exitStopwatch();
        }
      }

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

      Expression expr = Faces::getCurrentExpression();
      bool reacting = (expr == DIZZY || expr == ANGRY || expr == SCARED || Behavior::isShowingTimedReaction());

      if ((FocusScreen::isFocusActive() || FocusScreen::isStopwatchActive()) && !reacting) {
        FocusScreen::draw();
      } else if (watchMode && !reacting) {
        WatchScreen::draw();
      } else {
        Faces::draw();
      }
    }
  }

  display.sendBuffer();
  delay(15);
}

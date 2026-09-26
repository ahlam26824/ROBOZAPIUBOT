// Pisu Bot (by Roboza) -- Display + Motion + Sound + Eyes + Bluetooth + Game + Focus + Draw + Text Animation.

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
#include "DrawScreen.h"
#include "TextScreen.h"

enum AppMode { MODE_NORMAL, MODE_GAME };
AppMode appMode = MODE_NORMAL;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  initDisplay();
  showBootSplash();

  TouchSensor::begin();
  DrawScreen::begin();
  TextScreen::begin();
  BLEComm::begin();
  BLEComm::startPairing();

  showMessage("Calibrating sensors...", "Please keep me still");
  Accelerometer::begin();
  Sound::begin();
  Behavior::begin();
}

void loop() {
  Accelerometer::update();
  TouchSensor::update();
  FocusScreen::update();
  TextScreen::update();

  display.clearBuffer();

  if (appMode == MODE_GAME) {
    DinoGame::update(TouchSensor::wasTapped(), TouchSensor::wasShortReleased());

    if (DinoGame::isGameOver() && (TouchSensor::wasMediumReleased() || TouchSensor::wasShortReleased())) {
      appMode = MODE_NORMAL;
      Behavior::begin();
    }

    DinoGame::draw();

  } else { // MODE_NORMAL
    // 7-second touch hold -> Automatic 26-minute Focus Mode Timer!
    if (TouchSensor::wasFocusPressed()) {
      FocusScreen::startFocus(26);
    }

    // 8+ second touch hold -> Stopwatch toggle on OLED!
    if (TouchSensor::wasLongPressed()) {
      if (FocusScreen::isStopwatchActive()) {
        FocusScreen::exitStopwatch();
      } else {
        FocusScreen::startStopwatch();
      }
    }

    // 5-second touch release -> Dino Game open/close!
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

      if (BLEComm::hasNewDrawCommand()) {
        String cmd = BLEComm::getDrawCommand();
        DrawScreen::processCommand(cmd);
      }

      if (BLEComm::hasNewTextCommand()) {
        String cmd = BLEComm::getTextCommand();
        TextScreen::processCommand(cmd);
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

      if (DrawScreen::isActive() && !reacting) {
        DrawScreen::draw();
      } else if (TextScreen::isActive() && !reacting) {
        TextScreen::draw();
      } else if ((FocusScreen::isFocusActive() || FocusScreen::isStopwatchActive()) && !reacting) {
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

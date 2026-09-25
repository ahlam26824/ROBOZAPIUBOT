#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "BLEComm.h"

namespace {
  const char* DEVICE_NAME = "Zani Bot";

  const char* SERVICE_UUID     = "a1b2c3d4-0001-4000-8000-00805f9b0001";
  const char* TIME_CHAR_UUID   = "a1b2c3d4-0001-4000-8000-00805f9b0002";
  const char* TEMP_CHAR_UUID   = "a1b2c3d4-0001-4000-8000-00805f9b0003";
  const char* NOTIFY_CHAR_UUID  = "a1b2c3d4-0001-4000-8000-00805f9b0004";
  const char* MODE_CHAR_UUID    = "a1b2c3d4-0001-4000-8000-00805f9b0005";
  const char* RESULT_CHAR_UUID  = "a1b2c3d4-0001-4000-8000-00805f9b0006";

  BLEServer* server = nullptr;
  BLECharacteristic* timeChar = nullptr;
  BLECharacteristic* tempChar = nullptr;
  BLECharacteristic* notifyChar = nullptr;
  BLECharacteristic* modeChar = nullptr;
  BLECharacteristic* resultChar = nullptr;

  bool connected = false;
  bool pairing = false;

  bool newTimeFlag = false;
  String latestTime = "--:--";
  String latestDate = "";

  bool newTempFlag = false;
  float latestTemp = 0;

  bool newNotifFlag = false;
  String notifTitle = "";
  String notifMessage = "";

  bool newModeFlag = false;
  bool watchModeRequested = false;

  bool newGameResultFlag = false;
  String lastGameResult = "";

  // Splits "left|right" into its two halves. If there's no '|', `right`
  // comes back empty.
  void splitOnPipe(const String& s, String& left, String& right) {
    int idx = s.indexOf('|');
    if (idx < 0) {
      left = s;
      right = "";
    } else {
      left = s.substring(0, idx);
      right = s.substring(idx + 1);
    }
  }

  class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* s) override {
      connected = true;
      pairing = false;
      Serial.println("BLE: phone connected");
    }
    void onDisconnect(BLEServer* s) override {
      connected = false;
      newModeFlag = true;
      watchModeRequested = false; // fall back to simple face mode -- no more live data coming
      Serial.println("BLE: phone disconnected");
      // Keep advertising so the app can reconnect on its own -- there's
      // no gesture needed to make the bot connectable again.
      s->getAdvertising()->start();
    }
  };

  class TimeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
      String value = c->getValue();
      splitOnPipe(value, latestTime, latestDate);
      newTimeFlag = true;
    }
  };

  class TempCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
      String value = c->getValue();
      latestTemp = value.toFloat();
      newTempFlag = true;
    }
  };

  class NotifyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
      String value = c->getValue();
      splitOnPipe(value, notifTitle, notifMessage);
      newNotifFlag = true;
    }
  };

  class ModeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
      String value = c->getValue();
      watchModeRequested = (value == "1");
      newModeFlag = true;
    }
  };

  class ResultCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
      lastGameResult = c->getValue();
      newGameResultFlag = true;
    }
  };

  ServerCallbacks serverCallbacks;
  TimeCallbacks timeCallbacks;
  TempCallbacks tempCallbacks;
  NotifyCallbacks notifyCallbacks;
  ModeCallbacks modeCallbacks;
  ResultCallbacks resultCallbacks;
}

namespace BLEComm {

  void begin() {
    BLEDevice::init(DEVICE_NAME);

    // The default BLE packet (ATT MTU) only fits ~20 usable bytes per
    // write -- easily overflowed by a real "Title|Message" notification
    // payload, silently truncating it. Requesting a larger MTU here (the
    // app's connect() call requests 512 on its side; negotiation settles
    // on the lower of the two) prevents that class of bug entirely
    // rather than hoping every message stays short.
    BLEDevice::setMTU(247);

    server = BLEDevice::createServer();
    server->setCallbacks(&serverCallbacks);

    BLEService* service = server->createService(SERVICE_UUID);

    timeChar = service->createCharacteristic(TIME_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    timeChar->setCallbacks(&timeCallbacks);

    tempChar = service->createCharacteristic(TEMP_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    tempChar->setCallbacks(&tempCallbacks);

    notifyChar = service->createCharacteristic(NOTIFY_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    notifyChar->setCallbacks(&notifyCallbacks);

    modeChar = service->createCharacteristic(MODE_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    modeChar->setCallbacks(&modeCallbacks);

    resultChar = service->createCharacteristic(RESULT_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    resultChar->setCallbacks(&resultCallbacks);

    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);

    Serial.println("BLE: ready (firmware.ino calls startPairing() right after this, so advertising begins immediately)");
  }

  void startPairing() {
    pairing = true;
    BLEDevice::getAdvertising()->start();
    Serial.println("BLE: advertising started");
  }

  void stopPairing() {
    pairing = false;
    BLEDevice::getAdvertising()->stop();
    Serial.println("BLE: advertising stopped");
  }

  bool isPairing() { return pairing && !connected; }
  bool isConnected() { return connected; }

  bool hasNewTime() {
    if (newTimeFlag) { newTimeFlag = false; return true; }
    return false;
  }
  String getTime() { return latestTime; }
  String getDate() { return latestDate; }

  bool hasNewTemperature() {
    if (newTempFlag) { newTempFlag = false; return true; }
    return false;
  }
  float getTemperature() { return latestTemp; }

  bool hasNewNotification() {
    if (newNotifFlag) { newNotifFlag = false; return true; }
    return false;
  }
  String getNotificationTitle() { return notifTitle; }
  String getNotificationMessage() { return notifMessage; }

  bool hasNewMode() {
    if (newModeFlag) { newModeFlag = false; return true; }
    return false;
  }
  bool wantsWatchMode() { return watchModeRequested; }

  bool hasNewGameResult() {
    if (newGameResultFlag) { newGameResultFlag = false; return true; }
    return false;
  }
  String getGameResult() { return lastGameResult; }

}

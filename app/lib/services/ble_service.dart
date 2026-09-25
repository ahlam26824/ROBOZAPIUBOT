import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

/// Talks to the desk bot's BLE GATT server (see BLEComm.h/.cpp in the
/// firmware). One custom service, five write-only UTF-8 characteristics.
/// These UUIDs must match the firmware exactly.
class BleService extends ChangeNotifier {
  static const String deviceName = "Zani Bot";

  static final Guid serviceUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0001");
  static final Guid timeCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0002");
  static final Guid tempCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0003");
  static final Guid notifyCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0004");
  static final Guid modeCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0005");
  static final Guid resultCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0006");

  BluetoothDevice? _device;
  BluetoothCharacteristic? _timeChar;
  BluetoothCharacteristic? _tempChar;
  BluetoothCharacteristic? _notifyChar;
  BluetoothCharacteristic? _modeChar;
  BluetoothCharacteristic? _resultChar;

  bool isScanning = false;
  bool isConnected = false;
  String statusMessage = "Not connected";

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connSub;

  Future<bool> _ensurePermissions() async {
    final statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
    return statuses.values.every((s) => s.isGranted || s.isLimited);
  }

  /// Checks whether Android already has a live GATT connection to the bot
  /// from a previous session (BLE connections can outlive an app restart)
  /// and adopts it if so, instead of leaving the UI stuck showing "not
  /// connected" for a link that's actually up.
  Future<void> checkForExistingConnection() async {
    for (final d in FlutterBluePlus.connectedDevices) {
      if (d.platformName == deviceName) {
        debugPrint("BLE: found an already-connected device from a previous session");
        await _onConnected(d);
        return;
      }
    }
  }

  /// Scans for the bot and connects once found. The bot advertises from
  /// the moment it powers on -- no gesture needed to make it connectable.
  Future<void> scanAndConnect() async {
    if (isScanning || isConnected) return;

    final granted = await _ensurePermissions();
    if (!granted) {
      statusMessage = "Bluetooth/location permission denied";
      notifyListeners();
      return;
    }

    isScanning = true;
    statusMessage = "Scanning for $deviceName ...";
    notifyListeners();

    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) async {
      for (final r in results) {
        if (r.device.platformName == deviceName) {
          debugPrint("BLE: found $deviceName, stopping scan and connecting");
          await _scanSub?.cancel();
          _scanSub = null;
          await FlutterBluePlus.stopScan();
          await _connect(r.device);
          break;
        }
      }
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 8));

    FlutterBluePlus.isScanning.where((s) => s == false).first.then((_) {
      isScanning = false;
      if (!isConnected) {
        statusMessage =
            "Bot not found. Make sure it's powered on and nearby, then try again.";
      }
      notifyListeners();
    });
  }

  Future<void> _connect(BluetoothDevice device) async {
    _device = device;
    statusMessage = "Connecting...";
    notifyListeners();

    try {
      await device.connect(timeout: const Duration(seconds: 10));
      await _onConnected(device);
      await sendTimeNow();
    } catch (e) {
      debugPrint("BLE: connect failed: $e");
      isConnected = false;
      statusMessage = "Couldn't connect: $e";
      notifyListeners();
    }
  }

  /// Common path once a device is actually connected (whether just now,
  /// or discovered already-connected from a previous session): find our
  /// characteristics and start watching for a genuine later disconnect.
  Future<void> _onConnected(BluetoothDevice device) async {
    _device = device;

    final services = await device.discoverServices();
    for (final s in services) {
      if (s.uuid == serviceUuid) {
        for (final c in s.characteristics) {
          if (c.uuid == timeCharUuid) _timeChar = c;
          if (c.uuid == tempCharUuid) _tempChar = c;
          if (c.uuid == notifyCharUuid) _notifyChar = c;
          if (c.uuid == modeCharUuid) _modeChar = c;
          if (c.uuid == resultCharUuid) _resultChar = c;
        }
      }
    }

    isConnected = true;
    statusMessage = "Connected to $deviceName";
    notifyListeners();

    // Only start watching for a disconnect AFTER we know we're actually
    // connected. Subscribing any earlier would immediately deliver the
    // device's pre-connection "disconnected" state and flip the UI back
    // to "not connected" a moment after a genuinely successful connect --
    // this was a real bug, not a hypothetical one.
    await _connSub?.cancel();
    _connSub = device.connectionState.listen((state) {
      debugPrint("BLE: connectionState -> $state");
      if (state == BluetoothConnectionState.disconnected) {
        isConnected = false;
        statusMessage = "Disconnected";
        notifyListeners();
      }
    });
  }

  Future<void> disconnect() async {
    await _device?.disconnect();
    isConnected = false;
    statusMessage = "Disconnected";
    notifyListeners();
  }

  Future<void> _writeString(BluetoothCharacteristic? c, String value) async {
    if (c == null) return;
    try {
      await c.write(utf8.encode(value), withoutResponse: false);
    } catch (e) {
      debugPrint("BLE write failed: $e");
    }
  }

  Future<void> sendTimeNow() async {
    final now = DateTime.now();
    final time =
        "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}";
    const weekdays = [
      "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    ];
    final date =
        "${weekdays[now.weekday - 1]}, ${now.year.toString().padLeft(4, '0')}-"
        "${now.month.toString().padLeft(2, '0')}-${now.day.toString().padLeft(2, '0')}";
    await _writeString(_timeChar, "$time|$date");
  }

  Future<void> sendTemperature(double celsius) async {
    await _writeString(_tempChar, celsius.toStringAsFixed(1));
  }

  Future<void> sendNotification(String title, String message) async {
    await _writeString(_notifyChar, "$title|$message");
  }

  Future<void> sendMode({required bool watchMode}) async {
    await _writeString(_modeChar, watchMode ? "1" : "0");
  }

  /// Reports a finished Tic-Tac-Toe game (see TicTacToeScreen), from the
  /// PHONE USER's perspective -- "win"/"lose"/"draw". The bot reacts as
  /// the opponent it just played (see Behavior::reactToGameResult() in
  /// the firmware): a user win makes it Sad, a user loss makes it Happy.
  Future<void> sendGameResult(String result) async {
    await _writeString(_resultChar, result);
  }
}

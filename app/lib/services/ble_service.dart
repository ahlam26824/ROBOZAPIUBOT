import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

/// Talks to the desk bot's BLE GATT server (see BLEComm.h/.cpp in the
/// firmware). One custom service, write-only UTF-8 characteristics.
/// These UUIDs must match the firmware exactly.
class BleService extends ChangeNotifier {
  static const String deviceName = "Pisu Bot";

  static final Guid serviceUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0001");
  static final Guid timeCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0002");
  static final Guid tempCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0003");
  static final Guid notifyCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0004");
  static final Guid modeCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0005");
  static final Guid resultCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0006");
  static final Guid focusCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0007");
  static final Guid drawCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0008");
  static final Guid textCharUuid = Guid("a1b2c3d4-0001-4000-8000-00805f9b0009");

  BluetoothDevice? _device;
  BluetoothCharacteristic? _timeChar;
  BluetoothCharacteristic? _tempChar;
  BluetoothCharacteristic? _notifyChar;
  BluetoothCharacteristic? _modeChar;
  BluetoothCharacteristic? _resultChar;
  BluetoothCharacteristic? _focusChar;
  BluetoothCharacteristic? _drawChar;
  BluetoothCharacteristic? _textChar;

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

  Future<void> checkForExistingConnection() async {
    for (final d in FlutterBluePlus.connectedDevices) {
      if (d.platformName == deviceName) {
        debugPrint("BLE: found an already-connected device from a previous session");
        await _onConnected(d);
        return;
      }
    }
  }

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
          if (c.uuid == focusCharUuid) _focusChar = c;
          if (c.uuid == drawCharUuid) _drawChar = c;
          if (c.uuid == textCharUuid) _textChar = c;
        }
      }
    }

    isConnected = true;
    statusMessage = "Connected to $deviceName";
    notifyListeners();

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

  Future<void> sendFocusCommand(String cmd) async {
    await _writeString(_focusChar ?? _modeChar, cmd);
  }

  Future<void> sendGameResult(String result) async {
    await _writeString(_resultChar, result);
  }

  Future<void> sendDrawCommand(String cmd) async {
    await _writeString(_drawChar ?? _modeChar, cmd);
  }

  Future<void> sendTextCommand(String cmd) async {
    await _writeString(_textChar ?? _modeChar, cmd);
  }
}

import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:notification_listener_service/notification_listener_service.dart';
import 'package:notification_listener_service/notification_event.dart';
import 'ble_service.dart';
import 'notification_prefs.dart';

/// One app that has sent at least one notification since the app started,
/// for display on the app-selection screen.
class KnownApp {
  final String packageName;
  final String label;
  final Uint8List? icon;

  const KnownApp({required this.packageName, required this.label, this.icon});
}

/// A short, deliberately incomplete list of common apps' friendly names --
/// Android gives us no display name from a NotificationListenerService
/// event, only the package identifier. Anything not listed here falls
/// back to a reasonable guess derived from the package name itself.
const Map<String, String> _knownLabels = {
  'com.whatsapp': 'WhatsApp',
  'com.facebook.orca': 'Messenger',
  'com.facebook.katana': 'Facebook',
  'com.instagram.android': 'Instagram',
  'com.google.android.gm': 'Gmail',
  'com.google.android.apps.messaging': 'Messages',
  'com.android.mms': 'Messages',
  'com.google.android.dialer': 'Phone',
  'com.android.dialer': 'Phone',
  'com.twitter.android': 'X (Twitter)',
  'com.snapchat.android': 'Snapchat',
  'com.telegram.messenger': 'Telegram',
  'org.telegram.messenger': 'Telegram',
  'com.discord': 'Discord',
  'com.microsoft.teams': 'Teams',
  'com.slack': 'Slack',
  'com.linkedin.android': 'LinkedIn',
};

String _friendlyLabel(String packageName) {
  final known = _knownLabels[packageName];
  if (known != null) return known;

  final lastSegment = packageName.split('.').last;
  if (lastSegment.isEmpty) return packageName;
  return lastSegment[0].toUpperCase() + lastSegment.substring(1);
}

/// Forwards Android system notifications to the bot over BLE, once the
/// user has granted "Notification access" in Settings -- Android has no
/// way to request that as a normal runtime permission dialog, only a
/// deep link into Settings (see requestPermission() below).
///
/// Every app seen is catalogued (in knownApps) so the selection screen
/// has something to show, but only apps in the allowed set (see
/// NotificationPrefs) actually get forwarded -- null means "all of them",
/// which is the default until the user customizes it.
class NotificationForwarder extends ChangeNotifier {
  final BleService bleService;
  StreamSubscription<ServiceNotificationEvent>? _sub;
  bool enabled = false;

  final Map<String, KnownApp> knownApps = {};
  Set<String>? _allowedPackages;

  NotificationForwarder(this.bleService) {
    NotificationPrefs.getAllowedPackages().then((allowed) {
      _allowedPackages = allowed;
      notifyListeners();
    });
  }

  Set<String>? get allowedPackages => _allowedPackages;

  bool isAllowed(String packageName) =>
      _allowedPackages == null || _allowedPackages!.contains(packageName);

  Future<void> setAllowedPackages(Set<String>? packages) async {
    _allowedPackages = packages;
    if (packages == null) {
      await NotificationPrefs.clearFilter();
    } else {
      await NotificationPrefs.setAllowedPackages(packages);
    }
    notifyListeners();
  }

  Future<bool> hasPermission() => NotificationListenerService.isPermissionGranted();

  Future<void> requestPermission() => NotificationListenerService.requestPermission();

  void start() {
    if (enabled) return;
    enabled = true;
    _sub = NotificationListenerService.notificationsStream.listen((event) {
      if (event.hasRemoved) return;

      final label = _friendlyLabel(event.packageName);
      final isNewApp = !knownApps.containsKey(event.packageName);
      knownApps[event.packageName] = KnownApp(
        packageName: event.packageName,
        label: label,
        icon: event.appIcon,
      );
      if (isNewApp) notifyListeners();

      if (!bleService.isConnected) return;
      if (!isAllowed(event.packageName)) return;

      final title = event.title.trim();
      final content = event.content.trim();
      if (title.isEmpty && content.isEmpty) return;

      bleService.sendNotification(title.isEmpty ? label : title, content);
    });
  }

  void stop() {
    enabled = false;
    _sub?.cancel();
    _sub = null;
  }
}

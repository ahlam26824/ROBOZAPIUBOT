import 'package:shared_preferences/shared_preferences.dart';

/// Persists which apps' notifications get forwarded to the bot.
///
/// `null` (the default, nothing saved yet) means "forward everything" --
/// the simplest thing that works out of the box. Once the user picks
/// specific apps on the selection screen, this becomes a concrete
/// (possibly empty) set and only those packages are forwarded.
class NotificationPrefs {
  static const _key = 'allowed_notification_packages';

  static Future<Set<String>?> getAllowedPackages() async {
    final prefs = await SharedPreferences.getInstance();
    if (!prefs.containsKey(_key)) return null;
    return prefs.getStringList(_key)!.toSet();
  }

  static Future<void> setAllowedPackages(Set<String> packages) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(_key, packages.toList());
  }

  /// Back to "forward everything".
  static Future<void> clearFilter() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_key);
  }
}

import 'dart:async';
import 'package:flutter/material.dart';
import '../main.dart';
import '../services/ble_service.dart';
import '../services/weather_service.dart';
import '../services/notification_forwarder.dart';
import 'notification_apps_screen.dart';
import 'tic_tac_toe_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final BleService _ble = BleService();
  late final NotificationForwarder _notifForwarder;

  bool _watchMode = false;
  double? _temperatureC;
  bool _notifPermissionGranted = false;
  bool _forwardingNotifications = false;

  DateTime _now = DateTime.now();
  Timer? _clockTicker;
  Timer? _syncTimer;
  Timer? _weatherTimer;

  @override
  void initState() {
    super.initState();
    _notifForwarder = NotificationForwarder(_ble);
    _ble.addListener(_onBleChanged);
    _checkNotificationPermission();
    _ble.checkForExistingConnection();
    _refreshWeather(); // show something in the watch face right away, even before connecting

    _clockTicker = Timer.periodic(const Duration(seconds: 1), (_) {
      if (mounted) setState(() => _now = DateTime.now());
    });
  }

  @override
  void dispose() {
    _ble.removeListener(_onBleChanged);
    _clockTicker?.cancel();
    _syncTimer?.cancel();
    _weatherTimer?.cancel();
    _notifForwarder.stop();
    _notifForwarder.dispose();
    _ble.dispose();
    super.dispose();
  }

  void _onBleChanged() {
    if (!mounted) return;
    setState(() {});
    if (_ble.isConnected) {
      _startAutoSync();
    } else {
      _stopAutoSync();
    }
  }

  void _startAutoSync() {
    _syncTimer?.cancel();
    _syncTimer = Timer.periodic(const Duration(seconds: 20), (_) => _ble.sendTimeNow());

    _weatherTimer?.cancel();
    _refreshWeather();
    _weatherTimer = Timer.periodic(const Duration(minutes: 5), (_) => _refreshWeather());
  }

  void _stopAutoSync() {
    _syncTimer?.cancel();
    _weatherTimer?.cancel();
  }

  Future<void> _refreshWeather() async {
    final temp = await WeatherService.fetchCurrentTemperatureC();
    if (!mounted) return;
    setState(() => _temperatureC = temp);
    if (temp != null && _ble.isConnected) {
      await _ble.sendTemperature(temp);
    }
  }

  Future<void> _checkNotificationPermission() async {
    final granted = await _notifForwarder.hasPermission();
    if (!mounted) return;
    setState(() => _notifPermissionGranted = granted);
    if (granted && !_forwardingNotifications) {
      _notifForwarder.start();
      setState(() => _forwardingNotifications = true);
    }
  }

  Future<void> _onToggleWatchMode(bool value) async {
    setState(() => _watchMode = value);
    await _ble.sendMode(watchMode: value);
  }

  Future<void> _onToggleNotifications(bool value) async {
    if (value && !_notifPermissionGranted) {
      await _notifForwarder.requestPermission();
      final granted = await _notifForwarder.hasPermission();
      setState(() => _notifPermissionGranted = granted);
      if (!granted) return;
    }
    setState(() => _forwardingNotifications = value);
    if (value) {
      _notifForwarder.start();
    } else {
      _notifForwarder.stop();
    }
  }

  static const _weekdays = [
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
  ];
  static const _months = [
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Zani Bot'),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
          children: [
            _WatchFaceHero(now: _now, temperatureC: _temperatureC, weekdays: _weekdays, months: _months),
            const SizedBox(height: 16),
            _ConnectionCard(ble: _ble),
            const SizedBox(height: 14),
            _ModeCard(watchMode: _watchMode, onChanged: _ble.isConnected ? _onToggleWatchMode : null),
            const SizedBox(height: 14),
            _GamesCard(
              onPlayTicTacToe: () {
                Navigator.of(context).push(
                  MaterialPageRoute(builder: (_) => TicTacToeScreen(ble: _ble)),
                );
              },
            ),
            const SizedBox(height: 14),
            _NotificationsCard(
              permissionGranted: _notifPermissionGranted,
              forwarding: _forwardingNotifications,
              onRequestPermission: () async {
                await _notifForwarder.requestPermission();
                await _checkNotificationPermission();
              },
              onToggle: _onToggleNotifications,
              onChooseApps: () {
                Navigator.of(context).push(
                  MaterialPageRoute(
                    builder: (_) => NotificationAppsScreen(forwarder: _notifForwarder),
                  ),
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

/// The "smart watch" centerpiece: a live, ticking clock with date and
/// temperature, sourced entirely from the phone (its own clock and its
/// location-based weather) -- shown here in the app itself, not just
/// silently relayed to the bot.
class _WatchFaceHero extends StatelessWidget {
  final DateTime now;
  final double? temperatureC;
  final List<String> weekdays;
  final List<String> months;

  const _WatchFaceHero({
    required this.now,
    required this.temperatureC,
    required this.weekdays,
    required this.months,
  });

  @override
  Widget build(BuildContext context) {
    final time =
        "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}";
    final seconds = now.second.toString().padLeft(2, '0');
    final date = "${weekdays[now.weekday - 1]}, ${months[now.month - 1]} ${now.day}";

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 32, horizontal: 20),
      decoration: BoxDecoration(
        color: AppColors.ink,
        borderRadius: BorderRadius.circular(24),
      ),
      child: Column(
        children: [
          RichText(
            text: TextSpan(
              children: [
                TextSpan(
                  text: time,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 56,
                    fontWeight: FontWeight.w300,
                    letterSpacing: 1,
                    height: 1,
                  ),
                ),
                TextSpan(
                  text: ':$seconds',
                  style: TextStyle(
                    color: Colors.white.withValues(alpha: 0.45),
                    fontSize: 24,
                    fontWeight: FontWeight.w300,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          Text(
            date,
            style: TextStyle(color: Colors.white.withValues(alpha: 0.75), fontSize: 15),
          ),
          const SizedBox(height: 18),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
            decoration: BoxDecoration(
              color: Colors.white.withValues(alpha: 0.08),
              borderRadius: BorderRadius.circular(999),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.wb_sunny_outlined, color: AppColors.clay, size: 16),
                const SizedBox(width: 6),
                Text(
                  temperatureC == null ? 'Fetching weather...' : '${temperatureC!.toStringAsFixed(1)}°C',
                  style: const TextStyle(color: Colors.white, fontSize: 13, fontWeight: FontWeight.w500),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _SectionCard extends StatelessWidget {
  final String title;
  final Widget child;
  const _SectionCard({required this.title, required this.child});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              title.toUpperCase(),
              style: const TextStyle(
                fontSize: 12,
                letterSpacing: 1.1,
                fontWeight: FontWeight.w600,
                color: AppColors.inkMuted,
              ),
            ),
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }
}

class _ConnectionCard extends StatelessWidget {
  final BleService ble;
  const _ConnectionCard({required this.ble});

  @override
  Widget build(BuildContext context) {
    final connected = ble.isConnected;

    return _SectionCard(
      title: 'Connection',
      child: Row(
        children: [
          Container(
            width: 10,
            height: 10,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: connected ? AppColors.good : AppColors.border,
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(ble.statusMessage, style: const TextStyle(fontSize: 14, color: AppColors.ink)),
          ),
          const SizedBox(width: 8),
          if (ble.isScanning)
            const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2))
          else if (connected)
            OutlinedButton(onPressed: ble.disconnect, child: const Text('Disconnect'))
          else
            FilledButton(onPressed: ble.scanAndConnect, child: const Text('Connect')),
        ],
      ),
    );
  }
}

class _ModeCard extends StatelessWidget {
  final bool watchMode;
  final ValueChanged<bool>? onChanged;
  const _ModeCard({required this.watchMode, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      title: 'Bot display mode',
      child: Row(
        children: [
          Expanded(
            child: _ModeChip(
              label: 'Simple Face',
              icon: Icons.face_retouching_natural,
              selected: !watchMode,
              enabled: onChanged != null,
              onTap: () => onChanged?.call(false),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: _ModeChip(
              label: 'Watch Mode',
              icon: Icons.watch,
              selected: watchMode,
              enabled: onChanged != null,
              onTap: () => onChanged?.call(true),
            ),
          ),
        ],
      ),
    );
  }
}

class _ModeChip extends StatelessWidget {
  final String label;
  final IconData icon;
  final bool selected;
  final bool enabled;
  final VoidCallback onTap;
  const _ModeChip({
    required this.label,
    required this.icon,
    required this.selected,
    required this.enabled,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Opacity(
      opacity: enabled ? 1 : 0.4,
      child: InkWell(
        borderRadius: BorderRadius.circular(12),
        onTap: enabled ? onTap : null,
        child: Container(
          padding: const EdgeInsets.symmetric(vertical: 14),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(12),
            color: selected ? AppColors.claySoft : AppColors.background,
            border: Border.all(color: selected ? AppColors.clay : AppColors.border),
          ),
          child: Column(
            children: [
              Icon(icon, color: selected ? AppColors.clay : AppColors.inkMuted),
              const SizedBox(height: 6),
              Text(
                label,
                style: TextStyle(
                  color: selected ? AppColors.clay : AppColors.inkMuted,
                  fontSize: 12,
                  fontWeight: selected ? FontWeight.w600 : FontWeight.w400,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _NotificationsCard extends StatelessWidget {
  final bool permissionGranted;
  final bool forwarding;
  final VoidCallback onRequestPermission;
  final ValueChanged<bool> onToggle;
  final VoidCallback onChooseApps;

  const _NotificationsCard({
    required this.permissionGranted,
    required this.forwarding,
    required this.onRequestPermission,
    required this.onToggle,
    required this.onChooseApps,
  });

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      title: 'Notifications',
      child: permissionGranted
          ? Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Expanded(
                      child: Text(
                        'Forward phone notifications to the bot',
                        style: TextStyle(fontSize: 14, color: AppColors.ink),
                      ),
                    ),
                    Switch(value: forwarding, onChanged: onToggle),
                  ],
                ),
                const SizedBox(height: 4),
                Align(
                  alignment: Alignment.centerLeft,
                  child: TextButton.icon(
                    onPressed: onChooseApps,
                    icon: const Icon(Icons.tune, size: 18),
                    label: const Text('Choose which apps'),
                    style: TextButton.styleFrom(
                      foregroundColor: AppColors.clay,
                      padding: EdgeInsets.zero,
                      minimumSize: const Size(0, 32),
                      tapTargetSize: MaterialTapTargetSize.shrinkWrap,
                    ),
                  ),
                ),
              ],
            )
          : Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Grant notification access so the bot can show your phone\'s notifications with an alert sound.',
                  style: TextStyle(fontSize: 13, color: AppColors.inkMuted),
                ),
                const SizedBox(height: 12),
                OutlinedButton(
                  onPressed: onRequestPermission,
                  child: const Text('Grant notification access'),
                ),
              ],
            ),
    );
  }
}

class _GamesCard extends StatelessWidget {
  final VoidCallback onPlayTicTacToe;
  const _GamesCard({required this.onPlayTicTacToe});

  @override
  Widget build(BuildContext context) {
    return _SectionCard(
      title: 'Game',
      child: Row(
        children: [
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              color: AppColors.claySoft,
              borderRadius: BorderRadius.circular(10),
            ),
            alignment: Alignment.center,
            child: const Icon(Icons.grid_3x3, color: AppColors.clay, size: 22),
          ),
          const SizedBox(width: 12),
          const Expanded(
            child: Text(
              'Tic-Tac-Toe',
              style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: AppColors.ink),
            ),
          ),
          FilledButton(onPressed: onPlayTicTacToe, child: const Text('Play')),
        ],
      ),
    );
  }
}

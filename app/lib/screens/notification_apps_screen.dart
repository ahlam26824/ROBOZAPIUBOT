import 'package:flutter/material.dart';
import '../main.dart';
import '../services/notification_forwarder.dart';

/// Lets the user choose which apps' notifications get forwarded to the
/// bot -- the same "pick your apps" pattern real smartwatch companion
/// apps use, rather than an all-or-nothing switch.
class NotificationAppsScreen extends StatefulWidget {
  final NotificationForwarder forwarder;
  const NotificationAppsScreen({super.key, required this.forwarder});

  @override
  State<NotificationAppsScreen> createState() => _NotificationAppsScreenState();
}

class _NotificationAppsScreenState extends State<NotificationAppsScreen> {
  @override
  void initState() {
    super.initState();
    widget.forwarder.addListener(_onChanged);
  }

  @override
  void dispose() {
    widget.forwarder.removeListener(_onChanged);
    super.dispose();
  }

  void _onChanged() {
    if (mounted) setState(() {});
  }

  bool get _forwardingAll => widget.forwarder.allowedPackages == null;

  Future<void> _setForwardAll(bool all) async {
    if (all) {
      await widget.forwarder.setAllowedPackages(null);
    } else {
      // Switching from "all" to "selected" -- start from everything
      // currently known so it doesn't look like forwarding just stopped.
      await widget.forwarder.setAllowedPackages(widget.forwarder.knownApps.keys.toSet());
    }
  }

  Future<void> _togglePackage(String packageName, bool allowed) async {
    final current = Set<String>.from(widget.forwarder.allowedPackages ?? {});
    if (allowed) {
      current.add(packageName);
    } else {
      current.remove(packageName);
    }
    await widget.forwarder.setAllowedPackages(current);
  }

  @override
  Widget build(BuildContext context) {
    final apps = widget.forwarder.knownApps.values.toList()
      ..sort((a, b) => a.label.compareTo(b.label));

    return Scaffold(
      appBar: AppBar(title: const Text('Notification apps')),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Row(
                children: [
                  const Expanded(
                    child: Text(
                      'Forward all apps',
                      style: TextStyle(fontSize: 15, fontWeight: FontWeight.w500, color: AppColors.ink),
                    ),
                  ),
                  Switch(value: _forwardingAll, onChanged: _setForwardAll),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 4),
            child: Text(
              apps.isEmpty ? 'APPS' : 'APPS (${apps.length} seen so far)',
              style: const TextStyle(
                fontSize: 12,
                letterSpacing: 1.1,
                fontWeight: FontWeight.w600,
                color: AppColors.inkMuted,
              ),
            ),
          ),
          const SizedBox(height: 10),
          if (apps.isEmpty)
            Card(
              child: Padding(
                padding: const EdgeInsets.all(18),
                child: Text(
                  'No notifications seen yet. Once an app sends you a notification, '
                  'it will show up here so you can choose whether to forward it.',
                  style: TextStyle(fontSize: 13, color: AppColors.inkMuted),
                ),
              ),
            )
          else
            Card(
              child: Column(
                children: [
                  for (int i = 0; i < apps.length; i++) ...[
                    if (i > 0) const Divider(height: 1, color: AppColors.border),
                    _AppRow(
                      app: apps[i],
                      enabled: !_forwardingAll,
                      checked: _forwardingAll || widget.forwarder.isAllowed(apps[i].packageName),
                      onChanged: (v) => _togglePackage(apps[i].packageName, v),
                    ),
                  ],
                ],
              ),
            ),
        ],
      ),
    );
  }
}

class _AppRow extends StatelessWidget {
  final KnownApp app;
  final bool enabled;
  final bool checked;
  final ValueChanged<bool> onChanged;

  const _AppRow({
    required this.app,
    required this.enabled,
    required this.checked,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Opacity(
      opacity: enabled ? 1 : 0.5,
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: AppColors.claySoft,
          backgroundImage: app.icon != null ? MemoryImage(app.icon!) : null,
          child: app.icon == null
              ? Text(app.label.isNotEmpty ? app.label[0].toUpperCase() : '?',
                  style: const TextStyle(color: AppColors.clay, fontWeight: FontWeight.w600))
              : null,
        ),
        title: Text(app.label, style: const TextStyle(color: AppColors.ink)),
        subtitle: Text(app.packageName, style: const TextStyle(fontSize: 11, color: AppColors.inkMuted)),
        trailing: Switch(value: checked, onChanged: enabled ? onChanged : null),
      ),
    );
  }
}

import 'package:flutter/material.dart';
import 'screens/home_screen.dart';

/// Color palette inspired by Anthropic's site: a warm paper background,
/// dark ink text, and a muted clay/terracotta accent -- calm and
/// editorial rather than a dark neon "tech" look.
class AppColors {
  static const background = Color(0xFFF5F4EE);
  static const surface = Color(0xFFFFFFFF);
  static const border = Color(0xFFE5E3DA);
  static const ink = Color(0xFF1F1E1D);
  static const inkMuted = Color(0xFF6B6963);
  static const clay = Color(0xFFCC785C);
  static const claySoft = Color(0xFFF3E3DC);
  static const good = Color(0xFF4F7A5C);
  static const bad = Color(0xFFB3563A);
}

void main() {
  runApp(const DeskBotApp());
}

class DeskBotApp extends StatelessWidget {
  const DeskBotApp({super.key});

  @override
  Widget build(BuildContext context) {
    final scheme = ColorScheme.fromSeed(
      seedColor: AppColors.clay,
      brightness: Brightness.light,
    ).copyWith(
      surface: AppColors.surface,
      primary: AppColors.clay,
      onPrimary: Colors.white,
    );

    return MaterialApp(
      title: 'Pisu Bot',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: scheme,
        scaffoldBackgroundColor: AppColors.background,
        textTheme: const TextTheme(
          bodyMedium: TextStyle(color: AppColors.ink, fontSize: 14, height: 1.4),
          bodySmall: TextStyle(color: AppColors.inkMuted, fontSize: 12, height: 1.4),
        ),
        cardTheme: CardThemeData(
          color: AppColors.surface,
          elevation: 0,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(18),
            side: const BorderSide(color: AppColors.border),
          ),
          margin: EdgeInsets.zero,
        ),
        appBarTheme: const AppBarTheme(
          backgroundColor: AppColors.background,
          foregroundColor: AppColors.ink,
          elevation: 0,
          centerTitle: false,
          titleTextStyle: TextStyle(
            color: AppColors.ink,
            fontSize: 20,
            fontWeight: FontWeight.w600,
          ),
        ),
        filledButtonTheme: FilledButtonThemeData(
          style: FilledButton.styleFrom(
            backgroundColor: AppColors.clay,
            foregroundColor: Colors.white,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
            textStyle: const TextStyle(fontWeight: FontWeight.w600),
          ),
        ),
        outlinedButtonTheme: OutlinedButtonThemeData(
          style: OutlinedButton.styleFrom(
            foregroundColor: AppColors.ink,
            side: const BorderSide(color: AppColors.border),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
            padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 12),
          ),
        ),
        switchTheme: SwitchThemeData(
          thumbColor: WidgetStateProperty.resolveWith(
            (states) => states.contains(WidgetState.selected) ? AppColors.clay : Colors.white,
          ),
          trackColor: WidgetStateProperty.resolveWith(
            (states) =>
                states.contains(WidgetState.selected) ? AppColors.claySoft : AppColors.border,
          ),
        ),
      ),
      home: const HomeScreen(),
    );
  }
}

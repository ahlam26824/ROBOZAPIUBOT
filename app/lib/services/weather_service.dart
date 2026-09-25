import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:geolocator/geolocator.dart';

/// Uses open-meteo.com -- a free weather API that needs no API key or
/// account signup, which matters for a hobby/small-batch product where
/// asking the user to go get and manage an API key would be a real
/// friction point.
class WeatherService {
  static Future<double?> fetchCurrentTemperatureC() async {
    try {
      final serviceEnabled = await Geolocator.isLocationServiceEnabled();
      if (!serviceEnabled) return null;

      var permission = await Geolocator.checkPermission();
      if (permission == LocationPermission.denied) {
        permission = await Geolocator.requestPermission();
      }
      if (permission == LocationPermission.denied ||
          permission == LocationPermission.deniedForever) {
        return null;
      }

      final position = await Geolocator.getCurrentPosition(
        locationSettings: const LocationSettings(accuracy: LocationAccuracy.low),
      );

      final uri = Uri.parse(
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=${position.latitude}&longitude=${position.longitude}"
        "&current=temperature_2m",
      );
      final response = await http.get(uri).timeout(const Duration(seconds: 10));
      if (response.statusCode != 200) return null;

      final data = jsonDecode(response.body) as Map<String, dynamic>;
      final temp = data["current"]?["temperature_2m"];
      if (temp == null) return null;
      return (temp as num).toDouble();
    } catch (_) {
      return null;
    }
  }
}

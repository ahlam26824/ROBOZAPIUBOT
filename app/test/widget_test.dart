// A minimal smoke test: confirms the app builds its widget tree and shows
// its title without throwing. The interesting logic (BLE, weather,
// notification forwarding) lives in services/ and needs real hardware/
// permissions to exercise meaningfully, so it isn't covered by a widget
// test here.

import 'package:flutter_test/flutter_test.dart';

import 'package:desk_bot_app/main.dart';

void main() {
  testWidgets('App boots and shows the title', (WidgetTester tester) async {
    await tester.pumpWidget(const DeskBotApp());

    expect(find.text('Zani Bot'), findsOneWidget);
    expect(find.text('Connection'), findsOneWidget);
  });
}

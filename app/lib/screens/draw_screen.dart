import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../main.dart';
import '../services/ble_service.dart';

class DrawScreen extends StatefulWidget {
  final BleService ble;
  const DrawScreen({super.key, required this.ble});

  @override
  State<DrawScreen> createState() => _DrawScreenState();
}

class _DrawScreenState extends State<DrawScreen> {
  static const int gridW = 128;
  static const int gridH = 64;
  final Uint8List _grid = Uint8List(gridW * gridH); // 1 = pixel drawn, 0 = clear
  
  bool _isEraser = false;
  bool _liveSync = true;
  Offset? _lastPoint;

  @override
  void initState() {
    super.initState();
    if (widget.ble.isConnected) {
      widget.ble.sendDrawCommand("draw:clear");
    }
  }

  void _clearCanvas() {
    setState(() {
      _grid.fillRange(0, _grid.length, 0);
    });
    if (widget.ble.isConnected) {
      widget.ble.sendDrawCommand("draw:clear");
    }
  }

  void _drawPoint(int x, int y, bool val) {
    if (x < 0 || x >= gridW || y < 0 || y >= gridH) return;
    int idx = y * gridW + x;
    if (_grid[idx] != (val ? 1 : 0)) {
      _grid[idx] = val ? 1 : 0;
      if (_liveSync && widget.ble.isConnected) {
        widget.ble.sendDrawCommand("draw:pixel:$x,$y,${val ? 1 : 0}");
      }
    }
  }

  void _drawLine(int x0, int y0, int x1, int y1, bool val) {
    int dx = (x1 - x0).abs(), sx = x0 < x1 ? 1 : -1;
    int dy = -(y1 - y0).abs(), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
      _drawPoint(x0, y0, val);
      if (x0 == x1 && y0 == y1) break;
      e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }

  void _handleTouch(Offset localPos, Size canvasSize) {
    double scaleX = gridW / canvasSize.width;
    double scaleY = gridH / canvasSize.height;
    int gx = (localPos.dx * scaleX).clamp(0, gridW - 1).toInt();
    int gy = (localPos.dy * scaleY).clamp(0, gridH - 1).toInt();

    setState(() {
      if (_lastPoint != null) {
        int lx = (_lastPoint!.dx * scaleX).clamp(0, gridW - 1).toInt();
        int ly = (_lastPoint!.dy * scaleY).clamp(0, gridH - 1).toInt();
        _drawLine(lx, ly, gx, gy, !_isEraser);
      } else {
        _drawPoint(gx, gy, !_isEraser);
      }
      _lastPoint = localPos;
    });
  }

  Future<void> _sendFullBitmap() async {
    if (!widget.ble.isConnected) return;
    // Pack 128x64 pixels into 1024 bytes XBMP hex string in 4 chunks
    Uint8List buffer = Uint8List(1024);
    for (int y = 0; y < gridH; y++) {
      for (int x = 0; x < gridW; x++) {
        if (_grid[y * gridW + x] == 1) {
          int byteIdx = y * 16 + (x ~/ 8);
          int bitIdx = x % 8;
          buffer[byteIdx] |= (1 << bitIdx);
        }
      }
    }

    widget.ble.sendDrawCommand("draw:clear");
    await Future.delayed(const Duration(milliseconds: 50));

    // Send in 4 chunks of 256 bytes (512 hex chars)
    for (int chunk = 0; chunk < 4; chunk++) {
      int offset = chunk * 256;
      StringBuffer hexSb = StringBuffer();
      for (int i = 0; i < 256; i++) {
        hexSb.write(buffer[offset + i].toRadixString(16).padLeft(2, '0'));
      }
      widget.ble.sendDrawCommand("draw:bmp:$chunk:${hexSb.toString()}");
      await Future.delayed(const Duration(milliseconds: 60));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Draw on Pisu Bot'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: 'Clear Canvas',
            onPressed: _clearCanvas,
          ),
          IconButton(
            icon: const Icon(Icons.send),
            tooltip: 'Sync Full Canvas',
            onPressed: _sendFullBitmap,
          ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              color: AppColors.surface,
              child: Row(
                children: [
                  SegmentedButton<bool>(
                    segments: const [
                      ButtonSegment(value: false, label: Text('Draw'), icon: Icon(Icons.edit)),
                      ButtonSegment(value: true, label: Text('Erase'), icon: Icon(Icons.auto_fix_normal)),
                    ],
                    selected: {_isEraser},
                    onSelectionChanged: (s) => setState(() => _isEraser = s.first),
                  ),
                  const Spacer(),
                  const Text('Live Sync: ', style: TextStyle(fontWeight: FontWeight.w600)),
                  Switch(
                    value: _liveSync,
                    onChanged: (v) => setState(() => _liveSync = v),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            const Text(
              'OLED Canvas (128 x 64)',
              style: TextStyle(fontSize: 13, color: AppColors.inkMuted, fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 8),
            Expanded(
              child: Center(
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: AspectRatio(
                    aspectRatio: 128 / 64,
                    child: Container(
                      decoration: BoxDecoration(
                        color: Colors.black,
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: AppColors.clay, width: 3),
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black.withValues(alpha: 0.2),
                            blurRadius: 10,
                            offset: const Offset(0, 4),
                          )
                        ],
                      ),
                      child: GestureDetector(
                        onPanStart: (d) {
                          _lastPoint = null;
                          RenderBox box = context.findRenderObject() as RenderBox;
                          _handleTouch(d.localPosition, box.size);
                        },
                        onPanUpdate: (d) {
                          RenderBox box = context.findRenderObject() as RenderBox;
                          _handleTouch(d.localPosition, box.size);
                        },
                        onPanEnd: (_) => _lastPoint = null,
                        child: CustomPaint(
                          painter: _GridPainter(grid: _grid),
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.all(16.0),
              child: Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _clearCanvas,
                      icon: const Icon(Icons.delete_outline),
                      label: const Text('Clear Canvas'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: FilledButton.icon(
                      onPressed: _sendFullBitmap,
                      icon: const Icon(Icons.display_settings),
                      label: const Text('Send to Pisu Bot'),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _GridPainter extends CustomPainter {
  final Uint8List grid;
  _GridPainter({required this.grid});

  @override
  void paint(Canvas canvas, Size size) {
    double cellW = size.width / 128;
    double cellH = size.height / 64;
    Paint p = Paint()..color = const Color(0xFF00FFCC); // Vibrant OLED Cyan

    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 128; x++) {
        if (grid[y * 128 + x] == 1) {
          canvas.drawRect(
            Rect.fromLTWH(x * cellW, y * cellH, cellW + 0.3, cellH + 0.3),
            p,
          );
        }
      }
    }
  }

  @override
  bool shouldRepaint(covariant _GridPainter oldDelegate) => true;
}

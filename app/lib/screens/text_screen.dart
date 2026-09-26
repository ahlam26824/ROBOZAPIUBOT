import 'package:flutter/material.dart';
import '../main.dart';
import '../services/ble_service.dart';

class TextScreen extends StatefulWidget {
  final BleService ble;
  const TextScreen({super.key, required this.ble});

  @override
  State<TextScreen> createState() => _TextScreenState();
}

class _TextScreenState extends State<TextScreen> with SingleTickerProviderStateMixin {
  late TabController _tabController;
  
  // Single Text
  final TextEditingController _singleTextController = TextEditingController(text: "Hii");

  // Text Animation Sequence
  final List<TextEditingController> _seqControllers = [
    TextEditingController(text: "Hii"),
    TextEditingController(text: "i'm pisu Bot"),
    TextEditingController(text: "How are You Guys ?"),
  ];

  double _transitionSecs = 2.0;
  bool _animPlaying = false;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    _singleTextController.dispose();
    for (var c in _seqControllers) {
      c.dispose();
    }
    super.dispose();
  }

  void _sendSingleText() {
    String txt = _singleTextController.text.trim();
    if (txt.isEmpty) return;
    if (widget.ble.isConnected) {
      widget.ble.sendTextCommand("text:single:$txt");
    }
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Sent "$txt" to Pisu Bot screen')),
    );
  }

  void _addSeqLine() {
    setState(() {
      _seqControllers.add(TextEditingController(text: "Text ${_seqControllers.length + 1}"));
    });
  }

  void _removeSeqLine(int idx) {
    if (_seqControllers.length <= 1) return;
    setState(() {
      _seqControllers[idx].dispose();
      _seqControllers.removeAt(idx);
    });
  }

  void _toggleAnimation() {
    if (_animPlaying) {
      setState(() => _animPlaying = false);
      if (widget.ble.isConnected) {
        widget.ble.sendTextCommand("text:exit");
      }
    } else {
      List<String> items = _seqControllers.map((c) => c.text.trim()).where((t) => t.isNotEmpty).toList();
      if (items.isEmpty) return;
      int intervalMs = (_transitionSecs * 1000).toInt();
      String payload = items.join("|");
      
      if (widget.ble.isConnected) {
        widget.ble.sendTextCommand("text:seq:$intervalMs:$payload");
      }
      setState(() => _animPlaying = true);
      
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Started text animation (${items.length} texts, ${_transitionSecs.toStringAsFixed(1)}s delay)')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Text & Animation'),
        bottom: TabBar(
          controller: _tabController,
          tabs: const [
            Tab(icon: Icon(Icons.text_fields), text: 'Single Text'),
            Tab(icon: Icon(Icons.animation), text: 'Text Animation'),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          // TAB 1: SINGLE TEXT
          SingleChildScrollView(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Type any text to show on Pisu Bot screen:',
                  style: TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: AppColors.ink),
                ),
                const SizedBox(height: 12),
                TextField(
                  controller: _singleTextController,
                  maxLines: 3,
                  decoration: InputDecoration(
                    hintText: 'Enter text here...',
                    filled: true,
                    fillColor: AppColors.surface,
                    border: OutlineInputBorder(
                      borderRadius: BorderRadius.circular(14),
                      borderSide: const BorderSide(color: AppColors.border),
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                const Text('Quick Presets:', style: TextStyle(fontSize: 13, color: AppColors.inkMuted)),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    "Hii",
                    "i'm pisu Bot",
                    "How are You Guys ?",
                    "Roboza Pisu",
                    "Focus Time!",
                  ].map((preset) {
                    return ActionChip(
                      label: Text(preset),
                      onPressed: () {
                        _singleTextController.text = preset;
                      },
                    );
                  }).toList(),
                ),
                const SizedBox(height: 24),
                SizedBox(
                  width: double.infinity,
                  height: 48,
                  child: FilledButton.icon(
                    onPressed: _sendSingleText,
                    icon: const Icon(Icons.send),
                    label: const Text('Send to Pisu Bot Screen', style: TextStyle(fontSize: 16)),
                  ),
                ),
                const SizedBox(height: 12),
                SizedBox(
                  width: double.infinity,
                  child: OutlinedButton(
                    onPressed: () {
                      if (widget.ble.isConnected) widget.ble.sendTextCommand("text:exit");
                    },
                    child: const Text('Exit Text Mode'),
                  ),
                ),
              ],
            ),
          ),

          // TAB 2: TEXT ANIMATION SEQUENCE
          SingleChildScrollView(
            padding: const EdgeInsets.all(20),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Expanded(
                      child: Text(
                        'Text Animation Lines:',
                        style: TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: AppColors.ink),
                      ),
                    ),
                    IconButton.filledTonal(
                      onPressed: _addSeqLine,
                      icon: const Icon(Icons.add),
                      tooltip: 'Add Text Line',
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                ListView.builder(
                  shrinkWrap: true,
                  physics: const NeverScrollableScrollPhysics(),
                  itemCount: _seqControllers.length,
                  itemBuilder: (context, idx) {
                    return Padding(
                      padding: const EdgeInsets.only(bottom: 10),
                      child: Row(
                        children: [
                          Container(
                            width: 32,
                            height: 32,
                            alignment: Alignment.center,
                            decoration: BoxDecoration(
                              color: AppColors.claySoft,
                              borderRadius: BorderRadius.circular(8),
                            ),
                            child: Text(
                              '${idx + 1}',
                              style: const TextStyle(fontWeight: FontWeight.bold, color: AppColors.clay),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: TextField(
                              controller: _seqControllers[idx],
                              decoration: InputDecoration(
                                hintText: 'Text ${idx + 1}',
                                isDense: true,
                                filled: true,
                                fillColor: AppColors.surface,
                                border: OutlineInputBorder(
                                  borderRadius: BorderRadius.circular(10),
                                  borderSide: const BorderSide(color: AppColors.border),
                                ),
                              ),
                            ),
                          ),
                          if (_seqControllers.length > 1) ...[
                            const SizedBox(width: 6),
                            IconButton(
                              icon: const Icon(Icons.remove_circle_outline, color: AppColors.bad),
                              onPressed: () => _removeSeqLine(idx),
                            ),
                          ],
                        ],
                      ),
                    );
                  },
                ),
                const Divider(height: 32),
                Row(
                  children: [
                    const Text('Transition Time:', style: TextStyle(fontWeight: FontWeight.w600)),
                    const Spacer(),
                    Text('${_transitionSecs.toStringAsFixed(1)} seconds per text', style: const TextStyle(color: AppColors.clay, fontWeight: FontWeight.bold)),
                  ],
                ),
                Slider(
                  value: _transitionSecs,
                  min: 0.5,
                  max: 8.0,
                  divisions: 15,
                  label: '${_transitionSecs.toStringAsFixed(1)}s',
                  onChanged: (val) => setState(() => _transitionSecs = val),
                ),
                const SizedBox(height: 16),
                SizedBox(
                  width: double.infinity,
                  height: 50,
                  child: FilledButton.icon(
                    onPressed: _toggleAnimation,
                    style: FilledButton.styleFrom(
                      backgroundColor: _animPlaying ? AppColors.bad : AppColors.clay,
                    ),
                    icon: Icon(_animPlaying ? Icons.stop : Icons.play_arrow),
                    label: Text(
                      _animPlaying ? 'Stop Text Animation' : 'Play Text Animation on Pisu Bot',
                      style: const TextStyle(fontSize: 16),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

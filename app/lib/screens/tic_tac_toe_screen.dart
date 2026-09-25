import 'dart:math';

import 'package:flutter/material.dart';
import '../main.dart';
import '../services/ble_service.dart';

/// A Tic-Tac-Toe game played entirely in the app -- the bot itself has
/// no display real estate to spare for this. When a game ends, the
/// result is sent to the bot over BLE (see BleService.sendGameResult())
/// so it can react as the opponent it just played: Sad if the user won,
/// Happy if the user lost. Nothing is sent for a draw -- there's no
/// natural "how does the bot feel about a tie" reaction.
class TicTacToeScreen extends StatefulWidget {
  final BleService ble;
  const TicTacToeScreen({super.key, required this.ble});

  @override
  State<TicTacToeScreen> createState() => _TicTacToeScreenState();
}

enum _Cell { empty, user, bot }

class _TicTacToeScreenState extends State<TicTacToeScreen> {
  static const _winLines = [
    [0, 1, 2], [3, 4, 5], [6, 7, 8], // rows
    [0, 3, 6], [1, 4, 7], [2, 5, 8], // columns
    [0, 4, 8], [2, 4, 6],            // diagonals
  ];

  final _random = Random();
  List<_Cell> _board = List.filled(9, _Cell.empty);
  bool _userTurn = true;
  bool _botThinking = false;
  bool _gameOver = false;
  String? _resultText;
  int _wins = 0, _losses = 0, _draws = 0;

  void _restart() {
    setState(() {
      _board = List.filled(9, _Cell.empty);
      _userTurn = true;
      _botThinking = false;
      _gameOver = false;
      _resultText = null;
    });
  }

  _Cell? _winner(List<_Cell> board) {
    for (final line in _winLines) {
      final a = board[line[0]], b = board[line[1]], c = board[line[2]];
      if (a != _Cell.empty && a == b && b == c) return a;
    }
    return null;
  }

  bool _isFull(List<_Cell> board) => board.every((c) => c != _Cell.empty);

  void _onCellTap(int index) {
    if (_gameOver || !_userTurn || _botThinking || _board[index] != _Cell.empty) return;

    setState(() {
      _board[index] = _Cell.user;
      _userTurn = false;
    });

    final winner = _winner(_board);
    if (winner != null) {
      _endGame(userWon: true);
      return;
    }
    if (_isFull(_board)) {
      _endGame(userWon: null);
      return;
    }

    // A short "thinking" pause reads much more natural than an instant
    // move -- an immediate reply feels less like an opponent and more
    // like the UI glitched.
    setState(() => _botThinking = true);
    Future.delayed(const Duration(milliseconds: 500), _botMove);
  }

  void _botMove() {
    if (!mounted || _gameOver) return;
    final move = _pickBotMove(_board);

    setState(() {
      _board[move] = _Cell.bot;
      _userTurn = true;
      _botThinking = false;
    });

    final winner = _winner(_board);
    if (winner != null) {
      _endGame(userWon: false);
    } else if (_isFull(_board)) {
      _endGame(userWon: null);
    }
  }

  /// Deliberately beatable. A perfect (minimax) player would never
  /// actually lose, which would make "if you win" a promise this game
  /// could never keep -- so only some of the time does it play well
  /// (win-if-possible, else block, else center/corner); the rest of the
  /// time it just picks randomly among the open cells.
  int _pickBotMove(List<_Cell> board) {
    final empty = <int>[for (int i = 0; i < 9; i++) if (board[i] == _Cell.empty) i];

    const smartMoveChance = 0.6;
    if (_random.nextDouble() < smartMoveChance) {
      for (final i in empty) {
        final trial = List<_Cell>.from(board);
        trial[i] = _Cell.bot;
        if (_winner(trial) == _Cell.bot) return i;
      }
      for (final i in empty) {
        final trial = List<_Cell>.from(board);
        trial[i] = _Cell.user;
        if (_winner(trial) == _Cell.user) return i;
      }
      if (board[4] == _Cell.empty) return 4;
      final corners = [0, 2, 6, 8]..shuffle(_random);
      for (final c in corners) {
        if (board[c] == _Cell.empty) return c;
      }
    }

    empty.shuffle(_random);
    return empty.first;
  }

  void _endGame({required bool? userWon}) {
    setState(() {
      _gameOver = true;
      if (userWon == null) {
        _draws++;
        _resultText = "It's a draw!";
      } else if (userWon) {
        _wins++;
        _resultText = 'You win! Zani is a little sad about it.';
      } else {
        _losses++;
        _resultText = 'Zani wins! It\'s pretty pleased with itself.';
      }
    });

    if (userWon != null && widget.ble.isConnected) {
      widget.ble.sendGameResult(userWon ? 'win' : 'lose');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Tic-Tac-Toe')),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: Column(
            children: [
              _ScoreRow(wins: _wins, losses: _losses, draws: _draws),
              const SizedBox(height: 20),
              Text(
                _gameOver
                    ? _resultText!
                    : _botThinking
                        ? 'Zani is thinking...'
                        : 'Your turn -- you\'re X',
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 17, fontWeight: FontWeight.w600, color: AppColors.ink),
              ),
              const SizedBox(height: 20),
              AspectRatio(
                aspectRatio: 1,
                child: GridView.builder(
                  physics: const NeverScrollableScrollPhysics(),
                  itemCount: 9,
                  gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                    crossAxisCount: 3,
                    crossAxisSpacing: 10,
                    mainAxisSpacing: 10,
                  ),
                  itemBuilder: (context, index) => _BoardCell(
                    cell: _board[index],
                    onTap: () => _onCellTap(index),
                  ),
                ),
              ),
              const SizedBox(height: 24),
              if (_gameOver)
                FilledButton(onPressed: _restart, child: const Text('Play Again')),
              if (!widget.ble.isConnected) ...[
                const SizedBox(height: 16),
                const Text(
                  'Not connected to Zani Bot -- it won\'t react to this game right now.',
                  textAlign: TextAlign.center,
                  style: TextStyle(fontSize: 12, color: AppColors.inkMuted),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _ScoreRow extends StatelessWidget {
  final int wins, losses, draws;
  const _ScoreRow({required this.wins, required this.losses, required this.draws});

  @override
  Widget build(BuildContext context) {
    Widget stat(String label, int value) => Column(
          children: [
            Text('$value', style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w700, color: AppColors.ink)),
            Text(label, style: const TextStyle(fontSize: 11, color: AppColors.inkMuted)),
          ],
        );

    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        stat('You', wins),
        stat('Draws', draws),
        stat('Zani', losses),
      ],
    );
  }
}

class _BoardCell extends StatelessWidget {
  final _Cell cell;
  final VoidCallback onTap;
  const _BoardCell({required this.cell, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final String label = switch (cell) {
      _Cell.user => 'X',
      _Cell.bot => 'O',
      _Cell.empty => '',
    };
    final Color color = cell == _Cell.user ? AppColors.clay : AppColors.ink;

    return InkWell(
      borderRadius: BorderRadius.circular(14),
      onTap: onTap,
      child: Container(
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(14),
          border: Border.all(color: AppColors.border),
        ),
        alignment: Alignment.center,
        child: Text(
          label,
          style: TextStyle(fontSize: 44, fontWeight: FontWeight.bold, color: color),
        ),
      ),
    );
  }
}

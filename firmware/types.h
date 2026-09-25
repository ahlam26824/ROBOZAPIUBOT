#pragma once

// Declared right after the includes in any file that uses it, before any
// function -- Arduino auto-generates function prototypes and inserts
// them just before the first function in a .ino file, so a type used as
// a function parameter needs to already be known by that point. Plain
// .cpp/.h files don't have this quirk, but keeping the type here (rather
// than duplicated per-file) keeps every module in agreement.
//
// Seven expressions, chosen to map directly onto real desk-bot behavior
// rather than being an arbitrary large set. Current wiring (see
// Behavior.cpp):
//   NORMAL  - calm idle default; also where it settles after a reaction
//   HAPPY   - a pat/touch reaction
//   SAD     - been ignored for a while (10 min, then every 3 min)
//   SCARED  - picked up / lifted
//   DIZZY   - the first ~500ms of a shake -- a brief spin before it
//             settles into Angry, rather than snapping straight there
//   ANGRY   - being shaken (after the brief Dizzy moment above)
//   SLEEPY  - not wired up yet; reserved for a future "very long idle"
//             state, sleepier than Sad
enum Expression {
  NORMAL,
  HAPPY,
  SAD,
  SCARED,
  DIZZY,
  ANGRY,
  SLEEPY,
  NUM_EXPRESSIONS
};

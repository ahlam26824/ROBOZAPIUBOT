#pragma once
#include <U8g2lib.h>

// The single shared OLED display instance. Every module that draws
// something includes this header and uses `display` directly.
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C display;

// Sets up I2C (SDA=GPIO8, SCL=GPIO9) and the display. Call once from
// setup(), before drawing anything.
void initDisplay();

// Shows the "Roboza / Pisu Bot" boot splash, holds it briefly, then
// slides it upward off the screen instead of cutting away abruptly. Call
// once from setup(), right after initDisplay().
void showBootSplash();

// Draws a simple two-line status message into the CURRENT buffer, without
// clearing or sending it -- use this inside the main loop, which already
// wraps each frame in its own single clearBuffer()/sendBuffer() pair.
void drawMessage(const char* line1, const char* line2 = nullptr);

// Same two-line message, but as a one-shot call outside the main loop's
// frame (e.g. during setup()): clears, draws, and sends immediately.
void showMessage(const char* line1, const char* line2 = nullptr);

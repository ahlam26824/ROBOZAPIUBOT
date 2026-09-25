#include <Arduino.h>
#include "DinoGame.h"
#include "DisplayDriver.h"

namespace {
  const int GROUND_Y = 54;
  const int DINO_X = 14;
  const int DINO_W = 14; // visual sprite bounding box -- see drawDino()
  const int DINO_H = 16;

  // Collision is checked against a box INSET from the visual sprite
  // (see checkCollision()), not the full DINO_W/H -- a few pixels of
  // forgiveness on every side so a near-miss still reads as a miss,
  // which matters a lot on a display this small where "exact" pixel
  // overlap is hard to read/react to in time. Same idea for obstacles,
  // via OBSTACLE_HITBOX_INSET below.
  const int DINO_HITBOX_INSET_X = 2;
  const int DINO_HITBOX_INSET_TOP = 3;    // extra forgiving up top (the head)
  const int DINO_HITBOX_INSET_BOTTOM = 1;
  const int OBSTACLE_HITBOX_INSET = 1;

  // Jump feel -- tuned by eye, not physics; adjust to taste once you've
  // actually played it on the hardware.
  const float JUMP_IMPULSE = 4.5; // initial upward speed
  const float GRAVITY = 0.35;     // subtracted from that speed each frame

  float dinoLift = 0;     // height above the ground, 0 = standing
  float liftVelocity = 0; // positive = currently moving upward
  bool isJumping = false;
  bool hasJumpedOnce = false; // controls the "Tap to jump!" hint -- see draw()

  unsigned long runPhaseTimer = 0;
  bool runLegSwap = false;

  struct Obstacle {
    float x;
    int width;
    int height;
    bool active;
  };
  const int MAX_OBSTACLES = 3;
  Obstacle obstacles[MAX_OBSTACLES];

  float scrollSpeed = 2.0;
  const float MAX_SCROLL_SPEED = 5.0;
  float totalDistance = 0; // just for the scrolling ground dashes

  float distanceSinceSpawn = 0;
  float nextSpawnDistance = 60;

  unsigned int score = 0;
  unsigned int bestScore = 0;
  bool gameOver = false;

  struct Cloud { float x; int y; };
  Cloud clouds[2] = {{40, 10}, {100, 16}};

  void resetObstacles() {
    for (int i = 0; i < MAX_OBSTACLES; i++) obstacles[i].active = false;
  }

  void spawnObstacle() {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) {
        obstacles[i].active = true;
        obstacles[i].x = 128;
        obstacles[i].width = random(7, 12);
        obstacles[i].height = random(10, 18);
        return;
      }
    }
  }

  bool checkCollision() {
    int spriteTop = GROUND_Y - DINO_H - (int)dinoLift;
    int spriteBottom = GROUND_Y - (int)dinoLift;
    int dinoTop = spriteTop + DINO_HITBOX_INSET_TOP;
    int dinoBottom = spriteBottom - DINO_HITBOX_INSET_BOTTOM;
    int dinoLeft = DINO_X + DINO_HITBOX_INSET_X;
    int dinoRight = DINO_X + DINO_W - DINO_HITBOX_INSET_X;

    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) continue;

      int obLeft = (int)obstacles[i].x + OBSTACLE_HITBOX_INSET;
      int obRight = (int)obstacles[i].x + obstacles[i].width - OBSTACLE_HITBOX_INSET;
      int obTop = GROUND_Y - obstacles[i].height + OBSTACLE_HITBOX_INSET;
      int obBottom = GROUND_Y;

      bool overlapX = dinoRight > obLeft && dinoLeft < obRight;
      bool overlapY = dinoBottom > obTop && dinoTop < obBottom;
      if (overlapX && overlapY) return true;
    }
    return false;
  }

  void updateClouds() {
    for (int i = 0; i < 2; i++) {
      clouds[i].x -= 0.3f;
      if (clouds[i].x < -14) {
        clouds[i].x = 128 + random(0, 30);
      }
    }
  }

  // A simple original dinosaur silhouette -- body, raised head, a tail
  // nub, and two legs that alternate while running (tucked together
  // while airborne). Not a copy of any specific game's sprite art, just
  // built from basic shapes to read as "dino" instead of a plain box.
  void drawDino(int x, int top) {
    display.drawBox(x, top + 6, 12, 7);       // body
    display.drawBox(x + 7, top, 7, 7);        // head, raised at the front
    display.drawBox(x - 2, top + 8, 3, 3);    // tail nub at the back

    display.setDrawColor(0);
    display.drawPixel(x + 11, top + 2);       // a small eye punched out
    display.setDrawColor(1);

    if (isJumping) {
      // legs tucked together while airborne
      display.drawBox(x + 3, top + 13, 2, 3);
      display.drawBox(x + 8, top + 13, 2, 3);
    } else {
      int legOffset = runLegSwap ? 2 : 0;
      display.drawBox(x + 2 + legOffset, top + 13, 2, 3);
      display.drawBox(x + 8 - legOffset, top + 13, 2, 3);
    }
  }

  // A simple cactus silhouette: a stem plus one curling arm, instead of
  // a plain rectangle.
  void drawCactus(int x, int top, int width, int height) {
    int stemX = x + width / 2 - 1;
    display.drawBox(stemX, top, 2, height);

    int armY = top + height / 2;
    display.drawBox(x, armY, width / 2, 2);
    display.drawBox(x, armY - 4, 2, 4);
  }

  void drawCloud(int x, int y) {
    display.drawRBox(x, y, 10, 4, 2);
    display.drawRBox(x + 6, y - 2, 8, 4, 2);
  }
}

namespace DinoGame {

  void begin() {
    dinoLift = 0;
    liftVelocity = 0;
    isJumping = false;
    hasJumpedOnce = false;

    resetObstacles();
    scrollSpeed = 2.0;
    distanceSinceSpawn = 0;
    // The very first obstacle gets a longer runway than later ones
    // (random(40, 90) once play is underway) -- a cactus arriving well
    // under a second in, before a first-time player has even seen the
    // "Tap to jump!" hint, isn't a fair first impression.
    nextSpawnDistance = random(150, 220);

    score = 0;
    gameOver = false;
  }

  void update(bool tapped, bool released) {
    updateClouds();

    if (gameOver) {
      if (released) begin(); // a genuine short tap-and-release retries
      return;                // a long press is handled by the caller (exits to normal mode)
    }

    if (tapped && !isJumping) {
      isJumping = true;
      hasJumpedOnce = true;
      liftVelocity = JUMP_IMPULSE;
    }

    if (isJumping) {
      dinoLift += liftVelocity;
      liftVelocity -= GRAVITY;
      if (dinoLift <= 0) {
        dinoLift = 0;
        liftVelocity = 0;
        isJumping = false;
      }
    }

    // 0.01f/frame used to reach top speed in well under 5 seconds --
    // brutal for a casual pick-up-and-play game. This ramps up over
    // roughly 45 seconds instead, so there's real time to get
    // comfortable before it gets genuinely hard.
    scrollSpeed = min(MAX_SCROLL_SPEED, 2.0f + score * 0.001f);
    totalDistance += scrollSpeed;

    if (!isJumping && millis() - runPhaseTimer > 120) {
      runPhaseTimer = millis();
      runLegSwap = !runLegSwap;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) continue;
      obstacles[i].x -= scrollSpeed;
      if (obstacles[i].x + obstacles[i].width < 0) obstacles[i].active = false;
    }

    distanceSinceSpawn += scrollSpeed;
    if (distanceSinceSpawn > nextSpawnDistance) {
      spawnObstacle();
      distanceSinceSpawn = 0;
      nextSpawnDistance = random(40, 90);
    }

    score++; // simple time-alive score, ticks once per frame while running

    if (checkCollision()) {
      gameOver = true;
      if (score > bestScore) bestScore = score;
    }
  }

  void draw() {
    for (int i = 0; i < 2; i++) drawCloud((int)clouds[i].x, clouds[i].y);

    int groundOffset = ((int)totalDistance) % 6;
    for (int gx = -groundOffset; gx < 128; gx += 6) {
      display.drawHLine(gx, GROUND_Y, 3);
    }

    int dinoTop = GROUND_Y - DINO_H - (int)dinoLift;
    drawDino(DINO_X, dinoTop);

    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) continue;
      int obTop = GROUND_Y - obstacles[i].height;
      drawCactus((int)obstacles[i].x, obTop, obstacles[i].width, obstacles[i].height);
    }

    display.setFont(u8g2_font_5x7_tf);
    char scoreBuf[12];
    snprintf(scoreBuf, sizeof(scoreBuf), "%u", score / 5); // scale the per-frame counter to a friendlier number
    int w = display.getStrWidth(scoreBuf);
    display.drawStr(128 - w - 2, 8, scoreBuf);

    // A first-time player has no way to know the controls otherwise --
    // this disappears for good the moment they jump once.
    if (!gameOver && !hasJumpedOnce) {
      const char* hint = "Tap to jump!";
      int wHint = display.getStrWidth(hint);
      display.drawStr((128 - wHint) / 2, 20, hint);
    }

    if (gameOver) {
      // Kept clear of both the ground line (y=GROUND_Y=54, still drawn
      // every frame even here) and the frozen dino sprite (occupies
      // roughly x=12-28, y=38-54) -- all three lines sit in the upper
      // third of the screen instead.
      display.setFont(u8g2_font_6x10_tf);
      const char* msg = "Game Over";
      int wMsg = display.getStrWidth(msg);
      display.drawStr((128 - wMsg) / 2, 16, msg);

      display.setFont(u8g2_font_5x7_tf);
      char subBuf[16];
      snprintf(subBuf, sizeof(subBuf), "Best: %u", bestScore / 5);
      int wSub = display.getStrWidth(subBuf);
      display.drawStr((128 - wSub) / 2, 27, subBuf);

      const char* hint = "Tap: retry -- Hold: exit";
      int wHint = display.getStrWidth(hint);
      display.drawStr((128 - wHint) / 2, 36, hint);
    }
  }

  bool isGameOver() {
    return gameOver;
  }

}

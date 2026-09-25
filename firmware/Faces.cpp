#include <Arduino.h>
#include <math.h>
#include "Faces.h"
#include "DisplayDriver.h"

// Every expression still shares one base shape (a rounded rectangle --
// the same proven look used by Vector/Cozmo-style robot faces), but this
// version adds the techniques that make reference implementations like
// FluxGarage's RoboEyes read as alive rather than static:
//   - eyelid tilt: an angled top-corner cut, per mood (converging "V"
//     brows for Angry, drooping outer corners for Sad) -- still the same
//     shape, just an optional diagonal mask, not a different icon.
//   - a smooth blink (a height dip that eases closed and back open)
//     instead of swapping to a flat line.
//   - a slight "curious" height boost when the eyes glance sideways.

namespace {
  // Target geometry per expression. tilt: positive angles the INNER
  // (center-facing) top corners down -- furrowed, angry-style brows when
  // mirrored between both eyes. Negative angles the OUTER top corners
  // down -- drooping, sad-style. 0 = no tilt (a plain rounded top).
  struct EyeShape {
    int width;
    int height;
    int radius;
    int offsetL;
    int offsetR;
    int tilt;
  };

  EyeShape paramsFor(Expression f) {
    switch (f) {
      case NORMAL:  return {25, 22, 5, 0, 0,  0};
      case HAPPY:   return {25,  9, 4, 0, 0,  0};
      case SAD:     return {25, 14, 5, 4, 4, -6}; // shifted down + outer corners drooping
      case SCARED:  return {30, 30, 14, 0, 0,  0}; // large, nearly circular
      case DIZZY:   return {28, 28, 14, 0, 0,  0}; // circular -- see draw()'s orbiting punch-through dot
      case ANGRY:   return {22, 11, 2, 0, 0,  7};  // narrow + inner corners furrowed ("V" brows)
      case SLEEPY:  return {25,  6, 3, 0, 0, -3};  // nearly closed, slight droop
      default:      return {25, 22, 5, 0, 0,  0};
    }
  }

  Expression currentFace = NORMAL;

  int eyeX = 0;
  int targetEyeX = 0;

  int curWidth = 25,  targetWidth = 25;
  int curHeight = 22, targetHeight = 22;
  int curRadius = 5,  targetRadius = 5;
  int curOffsetL = 0, targetOffsetL = 0;
  int curOffsetR = 0, targetOffsetR = 0;
  int curTilt = 0,    targetTilt = 0;

  bool blinking = false;
  unsigned long nextBlink = 0;
  unsigned long blinkStartTime = 0;
  const unsigned long BLINK_HOLD_MS = 160;

  // Idle "look around" -- while resting Normal (and only then; every
  // other expression already has its own dedicated motion, from Angry's
  // jitter to Dizzy's orbit, and this would just fight with it), the
  // eyes drift left/right/center every few seconds on their own,
  // without needing a shake/pat/tilt to trigger it. This is what a
  // desk companion needs to read as alive and attentive rather than a
  // static display -- a real product a customer buys and looks at all
  // day can't just sit frozen between reactions. Reuses the exact same
  // eyeX/targetEyeX machinery setFace()'s randomizeDirection already
  // drives (so the glint dot and the "curious" height boost both move
  // with it for free) -- just triggered by a timer instead of a sensor
  // event.
  unsigned long nextIdleGlanceTime = 0;

  // Tilt-driven idle "eye tracking" -- a mass/spring/damper on the tilt
  // target, same shape as a physics-based eye rig: each step, acceleration
  // pulls velocity toward the target and damping bleeds it off, so it
  // eases in and settles rather than snapping or oscillating forever.
  // Deliberately soft/slow (this is a subtle "alive" cue, not a
  // responsive control) and capped small so it can never distort the eye
  // shape or collide with the per-mood offsets it's layered under.
  float tiltOffsetX = 0, tiltOffsetY = 0;
  float tiltVelX = 0, tiltVelY = 0;
  const float TILT_SPRING_K = 0.12f;
  const float TILT_SPRING_D = 0.70f;
  const float TILT_MAX_OFFSET = 6.0f; // pixels

  int stepToward(int current, int target) {
    if (current < target) return current + 1;
    if (current > target) return current - 1;
    return current;
  }

  // Cuts a diagonal wedge off one top corner (in the background color)
  // to angle that side of the eyelid -- the "eyebrow" effect. isLeftEye
  // determines which physical edge is "inner" (facing the other eye).
  void applyEyelidTilt(int x, int y, int w, int h, int tilt, bool isLeftEye) {
    if (tilt == 0) return;

    int mag = abs(tilt);
    if (mag > h) mag = h;
    if (mag > w) mag = w;

    bool cutInnerSide = (tilt > 0); // positive tilt = furrow the inner corner
    bool cutRightEdge = isLeftEye ? cutInnerSide : !cutInnerSide;

    display.setDrawColor(0);
    if (cutRightEdge) {
      display.drawTriangle(x + w - mag, y, x + w, y, x + w, y + mag);
    } else {
      display.drawTriangle(x, y, x + mag, y, x, y + mag);
    }
    display.setDrawColor(1);
  }

  // The ONLY shape-drawing call in this module -- every expression,
  // without exception, goes through here.
  void drawEyeShape(int x, int y, int w, int h, int r, int tilt, bool isLeftEye) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int maxRadius = (w < h ? w : h) / 2;
    if (r > maxRadius) r = maxRadius;
    if (r < 1) r = 1;

    display.drawRBox(x, y, w, h, r);
    applyEyelidTilt(x, y, w, h, tilt, isLeftEye);
  }
}

namespace Faces {

  void setFace(Expression f, bool randomizeDirection, bool instant) {
    currentFace = f;

    EyeShape p = paramsFor(f);
    targetWidth = p.width;
    targetHeight = p.height;
    targetRadius = p.radius;
    targetOffsetL = p.offsetL;
    targetOffsetR = p.offsetR;
    targetTilt = p.tilt;

    if (randomizeDirection) {
      int direction = random(0, 3);
      if (direction == 0) targetEyeX = -5;
      else if (direction == 1) targetEyeX = 5;
      else targetEyeX = 0;
    } else {
      targetEyeX = 0;
    }

    if (instant) {
      eyeX = targetEyeX;
      curWidth = targetWidth;
      curHeight = targetHeight;
      curRadius = targetRadius;
      curOffsetL = targetOffsetL;
      curOffsetR = targetOffsetR;
      curTilt = targetTilt;
    }
  }

  Expression getCurrentExpression() { return currentFace; }

  void updateEyeAnimation() {
    if (currentFace == NORMAL) {
      unsigned long now = millis();
      if (now > nextIdleGlanceTime) {
        int direction = random(0, 3);
        if (direction == 0) targetEyeX = -5;
        else if (direction == 1) targetEyeX = 5;
        else targetEyeX = 0;
        nextIdleGlanceTime = now + random(2500, 5000);
      }
    }

    eyeX = stepToward(eyeX, targetEyeX);
    curWidth = stepToward(curWidth, targetWidth);
    curHeight = stepToward(curHeight, targetHeight);
    curRadius = stepToward(curRadius, targetRadius);
    curOffsetL = stepToward(curOffsetL, targetOffsetL);
    curOffsetR = stepToward(curOffsetR, targetOffsetR);
    curTilt = stepToward(curTilt, targetTilt);
  }

  void updateEyeTracking(float tiltXDeg, float tiltYDeg) {
    // Degrees -> pixels. /4 means a firm ~24 degree nudge (not a full
    // flip) already maxes out the offset, rather than needing a
    // barely-perceptible tilt to reach it.
    float targetX = constrain(tiltXDeg / 4.0f, -TILT_MAX_OFFSET, TILT_MAX_OFFSET);
    float targetY = constrain(tiltYDeg / 4.0f, -TILT_MAX_OFFSET, TILT_MAX_OFFSET);

    float ax = (targetX - tiltOffsetX) * TILT_SPRING_K;
    float ay = (targetY - tiltOffsetY) * TILT_SPRING_K;
    tiltVelX = (tiltVelX + ax) * TILT_SPRING_D;
    tiltVelY = (tiltVelY + ay) * TILT_SPRING_D;
    tiltOffsetX += tiltVelX;
    tiltOffsetY += tiltVelY;
  }

  void updateBlink() {
    unsigned long now = millis();
    if (!blinking && now > nextBlink) {
      blinking = true;
      blinkStartTime = now;
    } else if (blinking && now - blinkStartTime > BLINK_HOLD_MS) {
      blinking = false;
      // Occasionally blink again almost right away instead of always
      // waiting the full idle gap -- a real double-blink, not a
      // perfectly even metronome. Small irregularities like this read
      // as more alive than a mathematically regular blink cycle would.
      if (random(0, 100) < 25) {
        nextBlink = now + random(200, 400);
      } else {
        nextBlink = now + random(2000, 5000);
      }
    }
  }

  void draw() {
    int leftX  = 22 + eyeX + (int)tiltOffsetX;
    int rightX = 76 + eyeX + (int)tiltOffsetX;
    int baseY = 21;
    float t = millis() / 1000.0;

    int leftY  = baseY + curOffsetL + (int)tiltOffsetY;
    int rightY = baseY + curOffsetR + (int)tiltOffsetY;
    int w = curWidth;
    int leftH = curHeight;
    int rightH = curHeight;
    int r = curRadius;

    // A slight "curious" lift when the eyes glance sideways -- a small
    // touch of life on top of the shared motion, not a mood of its own.
    int curiosity = min(3, abs(eyeX));
    leftH += curiosity;
    rightH += curiosity;

    // Idle motion signature per expression -- this, not a different
    // shape, is what makes each one read distinctly.
    switch (currentFace) {

      case NORMAL: {
        int bob = (int)(sin(t * 1.5) * 1.0);
        leftY += bob;
        rightY += bob;

        // A slow "breathing" pulse -- width and height together, like a
        // gentle chest-rise, at a different speed than the bob above so
        // the two never fall into a repeating combined pattern. Purely
        // a life cue with no sensor behind it: without it, the resting
        // face is just the bob plus an occasional blink, which reads as
        // idle rather than alive. Small enough (+/-1-2px) to never
        // distort the eye shape or collide with the idle glance/glint.
        int breathe = (int)(sin(t * 0.7) * 1.5);
        w += breathe;
        leftH += breathe / 2;
        rightH += breathe / 2;
        break;
      }

      case HAPPY: {
        int bounce = (int)(fabs(sin(t * 6.0)) * 3.0);
        leftY -= bounce;
        rightY -= bounce;
        break;
      }

      case SAD: {
        int drift = (int)(sin(t * 0.5) * 1.0);
        leftY += drift;
        rightY += drift;
        break;
      }

      case SCARED: {
        int pulse = (int)(sin(t * 7.0) * 2.0);
        w += pulse;
        leftH += pulse;
        rightH += pulse;
        break;
      }

      case ANGRY: {
        int jitter = (int)((millis() / 70) % 3) - 1;
        leftY += jitter;
        rightY -= jitter;
        break;
      }

      case SLEEPY: {
        int drift = (int)(sin(t * 0.4) * 1.0);
        leftY += drift;
        rightY += drift;
        break;
      }

      default:
        break;
    }

    // A smooth blink: eases the eyes down to a sliver and back open
    // within the hold window, rather than swapping to a flat line.
    if (blinking) {
      float progress = (float)(millis() - blinkStartTime) / (float)BLINK_HOLD_MS;
      if (progress > 1.0) progress = 1.0;
      float openness = 1.0 - sin(progress * PI) * 0.9; // dips to ~10% at the midpoint
      leftH = max(2, (int)(leftH * openness));
      rightH = max(2, (int)(rightH * openness));
    }

    drawEyeShape(leftX, leftY, w, leftH, r, curTilt, /* isLeftEye = */ true);
    drawEyeShape(rightX, rightY, w, rightH, r, curTilt, /* isLeftEye = */ false);

    // A small fixed "glint" punched into the upper-inner corner of each
    // eye, on every expression except Dizzy (which already has its own
    // animated dot below instead of a static one) -- same cutout
    // technique as applyEyelidTilt, just a dot instead of a wedge. Gives
    // even the plainest expressions (Normal, Sleepy) a point of focus
    // instead of reading as a blank rounded rectangle, without touching
    // the base shape or needing a per-mood icon. Sized to match Dizzy's
    // own dot (radius 3) at full eye height, but clamped down for
    // shorter eyes (Sleepy's 6px-tall one, or any eye mid-blink) so it
    // can never grow larger than the eye itself.
    if (currentFace != DIZZY) {
      int leftGlintR = min(3, max(1, leftH / 3));
      int rightGlintR = min(3, max(1, rightH / 3));
      display.setDrawColor(0);
      display.drawDisc(leftX + (w * 3) / 4, leftY + leftH / 3, leftGlintR);
      display.drawDisc(rightX + w / 4, rightY + rightH / 3, rightGlintR);
      display.setDrawColor(1);
    }

    // Dizzy's "spinning pupil": a small punch-through dot (same
    // setDrawColor(0)-then-back-to-1 cutout technique applyEyelidTilt
    // uses) orbiting the eye's center. Driven straight off millis()
    // rather than a stored per-episode start time -- Dizzy is only ever
    // shown for a brief fixed window (see Behavior.cpp), so a
    // continuously-running phase reads the same as one restarted at
    // shake-start, without Faces needing to know anything about shake
    // timing. The two eyes orbit in opposite phase for a bit of visual
    // interest instead of moving in lockstep.
    if (currentFace == DIZZY) {
      float angle = millis() * 0.015f;
      int orbitRadius = w / 3;
      int dotRadius = 3;

      int lcx = leftX + w / 2, lcy = leftY + leftH / 2;
      display.setDrawColor(0);
      display.drawDisc(lcx + (int)(cos(angle) * orbitRadius), lcy + (int)(sin(angle) * orbitRadius), dotRadius);

      int rcx = rightX + w / 2, rcy = rightY + rightH / 2;
      display.drawDisc(rcx - (int)(cos(angle) * orbitRadius), rcy - (int)(sin(angle) * orbitRadius), dotRadius);
      display.setDrawColor(1);
    }

    // Sad's tear: a small drop that falls from the left eye's lower-
    // inner corner, then pauses and repeats -- a plain millis()-driven
    // loop rather than a per-episode timer, same reasoning as Dizzy's
    // orbit above (Sad can last indefinitely, so there's no "episode
    // start" to time it against).
    if (currentFace == SAD) {
      const float TEAR_CYCLE_MS = 3000.0f;   // one drip + pause, repeating
      const float TEAR_FALL_MS = 1500.0f;    // the drip itself is visible for this long
      const int TEAR_FALL_DISTANCE = 16;     // pixels traveled while falling

      float cyclePos = fmod((float)millis(), TEAR_CYCLE_MS);
      if (cyclePos < TEAR_FALL_MS) {
        float progress = cyclePos / TEAR_FALL_MS;
        int tearX = leftX + w - 6;
        int tearY = (leftY + leftH) + (int)(progress * TEAR_FALL_DISTANCE);
        display.drawFilledEllipse(tearX, tearY, 2, 3);
      }
    }
  }

}

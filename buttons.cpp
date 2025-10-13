/*
 * Button State Machine Implementation
 *
 * Implements debounced button reading with detection of:
 * - Single clicks
 * - Double clicks (two clicks within DOUBLE_MS)
 * - Long holds (press duration >= LONG_MS)
 * - Pairing sequence (6 quick clicks on BTN1)
 *
 * Uses INPUT_PULLUP mode: HIGH = button idle, LOW = button pressed
 */

#include "constants.h"

// Button state machines (exported)
BtnState b1, b2;

static inline bool pinRead(int pin) { return digitalRead(pin); }

void initButtons() {
  b1 = { BTN1_PIN, true, false, 0, 0, 0 };
  b2 = { BTN2_PIN, true, false, 0, 0, 0 };
}

ClickType handleButton(BtnState &b) {
  ClickType out = NONE;
  bool s = pinRead(b.pin);

  // Debounce edge
  if (b.last != s) { delay(DEBOUNCE_MS); s = pinRead(b.pin); }

  if (b.last != s) {
    b.last = s;
    if (s == LOW) {
      // press
      b.pressed = true;
      b.t_down = millis();
    } else {
      // release
      b.pressed = false;
      b.t_up = millis();
      if ((b.t_up - b.t_down) >= LONG_MS) {
        out = LONGHOLD;
        b.clicks = 0;
      } else {
        b.clicks++;
      }
    }
  }

  // Single/double decision window
  if (!b.pressed && b.clicks > 0) {
    if (millis() - b.t_up > DOUBLE_MS) {
      out = (b.clicks >= 2) ? DOUBLE_CLICK : SINGLE;
      b.clicks = 0;
    }
  }

  return out;
}

bool bothPressedEarly() {
  // Short confirmation window to avoid false positives
  uint32_t t0 = millis();
  while (millis() - t0 < 60) {
    if (pinRead(BTN1_PIN) == LOW && pinRead(BTN2_PIN) == LOW) return true;
    delay(5);
  }
  // Final check after confirmation window
  return (pinRead(BTN1_PIN) == LOW && pinRead(BTN2_PIN) == LOW);
}

bool detect_pairing_sequence() {
  // Do not start if BTN2 is active (keep it unambiguous)
  if (pinRead(BTN2_PIN) == LOW) return false;

  // Start only if BTN1 was just pressed
  if (pinRead(BTN1_PIN) == HIGH) return false;

  // Debounce first press
  delay(DEBOUNCE_MS);
  if (pinRead(BTN1_PIN) != LOW) return false;

  uint32_t t_start = millis();
  int clicks = 0;

  while (millis() - t_start <= PAIRING_WINDOW_MS) {
    // Any BTN2 activity cancels the pairing sequence
    if (pinRead(BTN2_PIN) == LOW) return false;

    // Wait until BTN1 is released to count a short click
    if (pinRead(BTN1_PIN) == LOW) {
      uint32_t down_t = millis();
      while (pinRead(BTN1_PIN) == LOW) {
        if (millis() - down_t >= LONG_MS) return false; // long press → not a "click"
        delay(5);
      }
      // Count this click
      clicks++;
      if (clicks >= PAIRING_CLICKS) return true;

      // Small window to start the next click
      uint32_t wait_next = millis();
      while (millis() - wait_next < 600) {
        if (pinRead(BTN2_PIN) == LOW) return false;
        if (pinRead(BTN1_PIN) == LOW) break; // next click started
        delay(5);
      }
    } else {
      delay(5);
    }
  }
  return false; // did not complete within window
}

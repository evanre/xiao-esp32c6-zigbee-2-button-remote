#include "constants.h"

static inline bool pinRead(int pin) { return digitalRead(pin); }

BtnState b1(BTN1_PIN);
BtnState b2(BTN2_PIN);

void initButtons()
{
  pinMode(b1.pin, INPUT_PULLUP);
  pinMode(b2.pin, INPUT_PULLUP);
}

ButtonEvent handleButton(BtnState &b)
{
  bool pressed = pinRead(b.pin) == LOW;
  uint32_t now = millis();

  if (pressed)
  {
    // reset inter-click window
    if (b.t_release)
    {
      b.t_release = 0;
    }

    // ignore until released
    if (b.state == ButtonModeEnum::STUCK)
    {
      return ButtonEvent::NONE;
    }

    // first press time
    if (b.t_press == 0)
    {
      b.t_press = now;
    }

    uint32_t delta_ms = now - b.t_press;

    // debounce
    if (delta_ms < DEBOUNCE_MS)
    {
      return ButtonEvent::NONE;
    }

    // Holding more than HOLD_MS (600ms) → HOLD event
    if (delta_ms >= HOLD_MS && delta_ms < MAX_HOLD_MS)
    {
      if (b.state == ButtonModeEnum::IDLE)
      {
        if (b.clicks == 0)
        {
          b.state = ButtonModeEnum::HOLDING;
          return ButtonEvent::HOLD_START;
        }
        else
        {
          b.state = ButtonModeEnum::CLICK_HOLDING;
          return ButtonEvent::CLICK_HOLD_START;
        }
      }
      return ButtonEvent::NONE; // already in HOLDING or CLICK_HOLDING
    }

    // Holding more than MAX_HOLD_MS (30s) → safety timeout
    if (delta_ms >= MAX_HOLD_MS)
    {
      ButtonEvent ev = ButtonEvent::NONE;
      if (b.state == ButtonModeEnum::HOLDING)
      {
        ev = ButtonEvent::HOLD_END;
      }
      else if (b.state == ButtonModeEnum::CLICK_HOLDING)
      {
        ev = ButtonEvent::CLICK_HOLD_END;
      }

      b.state = ButtonModeEnum::STUCK;
      b.t_press = 0;
      b.clicks = 0;
      return ev;
    }

    return ButtonEvent::NONE; // still debouncing or holding
  }
  else
  {
    if (b.t_press)
    {
      uint32_t delta_ms = now - b.t_press;
      b.t_press = 0;

      // debounce
      if (delta_ms < DEBOUNCE_MS)
      {
        return ButtonEvent::NONE;
      }

      // If we were in hold mode -> end it immediately
      if (b.state == ButtonModeEnum::HOLDING)
      {
        b.state = ButtonModeEnum::IDLE;
        b.clicks = 0;
        return ButtonEvent::HOLD_END;
      }
      if (b.state == ButtonModeEnum::CLICK_HOLDING)
      {
        b.state = ButtonModeEnum::IDLE;
        b.clicks = 0;
        return ButtonEvent::CLICK_HOLD_END;
      }

      b.clicks++;
      if (b.t_release == 0)
      {
        b.t_release = now;
      }
      return ButtonEvent::NONE;
    }

    if (b.state == ButtonModeEnum::STUCK)
    {
      b.state = ButtonModeEnum::IDLE;
      b.t_release = 0;
      b.clicks = 0;
      return ButtonEvent::NONE;
    }

    if (b.t_release)
    {
      uint32_t delta_ms = now - b.t_release;

      // debounce
      if (delta_ms < DEBOUNCE_MS)
      {
        return ButtonEvent::NONE;
      }

      // waiting for next click
      if (delta_ms < PRESS_MS)
      {
        return ButtonEvent::NONE;
      }

      // end of click sequence
      ButtonEvent ev = ButtonEvent::NONE;
      if (b.clicks == 1)
      {
        ev = ButtonEvent::CLICK;
      }
      else if (b.clicks == 2)
      {
        ev = ButtonEvent::DOUBLE_CLICK;
      }
      else if (b.clicks >= 3 && b.clicks < PAIRING_CLICKS)
      {
        ev = ButtonEvent::TRIPLE_CLICK;
      }
      else // b.clicks >= PAIRING_CLICKS
      {
        ev = ButtonEvent::PAIRING_SEQUENCE;
      }

      // reset state
      b.clicks = 0;
      b.t_release = 0;
      return ev;
    }
  }

  return ButtonEvent::NONE;
}

// bool bothPressedEarly() {
//   // Short confirmation window
//   uint32_t t0 = millis();
//   while (millis() - t0 < 60) {
//     if (pinRead(BTN1_PIN) == LOW && pinRead(BTN2_PIN) == LOW) return true;
//     delay(5);
//   }
//   return (pinRead(BTN1_PIN) == LOW && pinRead(BTN2_PIN) == LOW);
// }

// bool detect_pairing_sequence() {
//   // Cancel if BTN2 is active
//   if (pinRead(BTN2_PIN) == LOW) return false;

//   // Start only if BTN1 was just pressed
//   if (pinRead(BTN1_PIN) == HIGH) return false;

//   // Debounce first press
//   delay(DEBOUNCE_MS);
//   if (pinRead(BTN1_PIN) != LOW) return false;

//   uint32_t t_start = millis();
//   int clicks = 0;

//   while (millis() - t_start <= PAIRING_WINDOW_MS) {
//     // Any BTN2 activity cancels
//     if (pinRead(BTN2_PIN) == LOW) return false;

//     // Count a short click on BTN1
//     if (pinRead(BTN1_PIN) == LOW) {
//       uint32_t down_t = millis();
//       while (pinRead(BTN1_PIN) == LOW) {
//         if (millis() - down_t >= HOLD_MS) return false; // long → not a "click"
//         delay(5);
//       }
//       clicks++;
//       if (clicks >= PAIRING_CLICKS) return true;

//       // Small gap before next click
//       uint32_t wait_next = millis();
//       while (millis() - wait_next < 600) {
//         if (pinRead(BTN2_PIN) == LOW) return false;
//         if (pinRead(BTN1_PIN) == LOW) break; // next click started
//         delay(5);
//       }
//     } else {
//       delay(5);
//     }
//   }
//   return false;
// }

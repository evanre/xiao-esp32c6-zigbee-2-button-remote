#include "buttons.h"

static inline bool pinRead(gpio_num_t pin) {
    return gpio_get_level(pin) == 1;
}

BtnState b1(BTN1_PIN);
BtnState b2(BTN2_PIN);

void initButtons()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << b1.pin) | (1ULL << b2.pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

ButtonEvent handleButton(BtnState &b)
{
  bool pressed = pinRead(b.pin) == 0;
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

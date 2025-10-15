#include "constants.h"

static bool isHoldStart(ButtonEvent ev)
{
  return ev == ButtonEvent::HOLD_START || ev == ButtonEvent::CLICK_HOLD_START;
}
static bool isHoldEnd(ButtonEvent ev)
{
  return ev == ButtonEvent::HOLD_END || ev == ButtonEvent::CLICK_HOLD_END;
}
static ButtonEvent matchingEnd(ButtonEvent startEv)
{
  return (startEv == ButtonEvent::HOLD_START) ? ButtonEvent::HOLD_END
                                              : ButtonEvent::CLICK_HOLD_END;
}

static void debugPrint(LampId lamp, ButtonEvent ev)
{
  if (ev == ButtonEvent::NONE)
    return;

  Serial.print("[SEND] Lamp ");
  Serial.print(static_cast<uint8_t>(lamp));
  Serial.print(" → ");

  switch (ev)
  {
  case ButtonEvent::CLICK:
    Serial.println("CLICK");
    break;
  case ButtonEvent::HOLD_START:
    Serial.println("HOLD_START");
    break;
  case ButtonEvent::HOLD_END:
    Serial.println("HOLD_END");
    break;
  case ButtonEvent::CLICK_HOLD_START:
    Serial.println("CLICK_HOLD_START");
    break;
  case ButtonEvent::CLICK_HOLD_END:
    Serial.println("CLICK_HOLD_END");
    break;
  case ButtonEvent::DOUBLE_CLICK:
    Serial.println("DOUBLE_CLICK");
    break;
  case ButtonEvent::TRIPLE_CLICK:
    Serial.println("TRIPLE_CLICK");
    break;
  case ButtonEvent::PAIRING_SEQUENCE:
    Serial.println("PAIRING_SEQUENCE");
    break;
  default:
    break;
  }
}

void sendAction(LampId lamp, ButtonEvent ev)
{
#if DEBUG_MODE
  debugPrint(lamp, ev);
#else
  // ZigbeeSwitch* lamp = getLamp(ep);
  // if (lamp) lamp->lightToggle();
#endif
}

EventRouter router = {};

void routeEvents(ButtonEvent ev1, ButtonEvent ev2)
{
  const uint32_t now = millis();

  if (router.combo_holding)
  {
    ButtonEvent ev_final = ButtonEvent::NONE;
    if (isHoldEnd(ev1) && ev1 == router.ev1_pending)
    {
      ev_final = ev1;
      router.ev1_pending = ButtonEvent::NONE;
    }

    if (isHoldEnd(ev2) && ev2 == router.ev2_pending)
    {
      ev_final = ev2;
      router.ev2_pending = ButtonEvent::NONE;
    }

    if (router.ev1_pending == ButtonEvent::NONE && router.ev2_pending == ButtonEvent::NONE)
    {
      sendAction(LampId::L3, ev_final);
      router = {};
    }

    // While combo-holding, ignore other events
    return;
  }

  // 1) Ingest new arrivals (start window on the first one)
  if (ev1 != ButtonEvent::NONE)
  {
    if (router.t_start == 0)
      router.t_start = now;
    // avoid losing a previous pending from BTN1
    if (router.ev1_pending != ButtonEvent::NONE)
    {
      sendAction(LampId::L1, router.ev1_pending);
    }
    router.ev1_pending = ev1;
  }
  if (ev2 != ButtonEvent::NONE)
  {
    if (router.t_start == 0)
      router.t_start = now;
    // avoid losing a previous pending from BTN2
    if (router.ev2_pending != ButtonEvent::NONE)
    {
      sendAction(LampId::L2, router.ev2_pending);
    }
    router.ev2_pending = ev2;
  }

  // Nothing pending → nothing to do
  if (router.t_start == 0)
    return;

  // 2) If window expired → decide & flush
  if ((now - router.t_start) > COMBO_MS)
  {
    if (router.ev1_pending != ButtonEvent::NONE)
    {
      sendAction(LampId::L1, router.ev1_pending);
    }
    if (router.ev2_pending != ButtonEvent::NONE)
    {
      sendAction(LampId::L2, router.ev2_pending);
    }
    router = {};
    return;
  }

  // 3) If both pending and equal → fire Lamp3 immediately
  if (router.ev1_pending != ButtonEvent::NONE &&
      router.ev1_pending == router.ev2_pending)
  {
    ButtonEvent ev = router.ev1_pending;
    sendAction(LampId::L3, ev);
    router = {};

    if (isHoldStart(ev))
    {
      router.combo_holding = true;
      router.ev1_pending = matchingEnd(ev);
      router.ev2_pending = matchingEnd(ev);
    }
  }
}

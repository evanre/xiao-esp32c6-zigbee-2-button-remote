#include "router.h"
#include "zigbee.h"

#define TAG "ROUTER"

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

static void sendAction(void (*callback)(LampId), LampId lampId, const char *msg)
{
#if DEBUG_MODE
  ESP_LOGI(TAG, "[SEND] %s L%d", msg, static_cast<uint8_t>(lampId));
#else
  callback(lampId);
#endif
}

void dispatch(LampId lampId, ButtonEvent ev)
{
  if (ev == ButtonEvent::NONE)
    return;

  switch (ev)
  {
  case ButtonEvent::CLICK:
    sendAction(cmd_toggle, lampId, "CLICK");
    break;
  case ButtonEvent::HOLD_START:
    sendAction(cmd_level_start, lampId, "HOLD_START");
    break;
  case ButtonEvent::HOLD_END:
    sendAction(cmd_level_stop, lampId, "HOLD_END");
    break;
  case ButtonEvent::CLICK_HOLD_START:
    sendAction(cmd_ct_start, lampId, "CLICK_HOLD_START");
    break;
  case ButtonEvent::CLICK_HOLD_END:
    sendAction(cmd_ct_stop, lampId, "CLICK_HOLD_END");
    break;
  case ButtonEvent::DOUBLE_CLICK:
    sendAction(cmd_empty_action, lampId, "DOUBLE_CLICK");
    break;
  case ButtonEvent::TRIPLE_CLICK:
    sendAction(cmd_empty_action, lampId, "TRIPLE_CLICK");
    break;
  case ButtonEvent::PAIRING_SEQUENCE:
    sendAction(enter_pairing_mode, lampId, "PAIRING_SEQUENCE");
    break;
  default:
    break;
  }
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
      dispatch(LampId::L3, ev_final);
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
      dispatch(LampId::L1, router.ev1_pending);
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
      dispatch(LampId::L2, router.ev2_pending);
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
      dispatch(LampId::L1, router.ev1_pending);
    }
    if (router.ev2_pending != ButtonEvent::NONE)
    {
      dispatch(LampId::L2, router.ev2_pending);
    }
    router = {};
    return;
  }

  // 3) If both pending and equal → fire Lamp3 immediately
  if (router.ev1_pending != ButtonEvent::NONE &&
      router.ev1_pending == router.ev2_pending)
  {
    ButtonEvent ev = router.ev1_pending;
    dispatch(LampId::L3, ev);
    router = {};

    if (isHoldStart(ev))
    {
      router.combo_holding = true;
      router.ev1_pending = matchingEnd(ev);
      router.ev2_pending = matchingEnd(ev);
    }
  }
}

#include "constants.h"
#include "buttons.h"
#include "router.h"
#include "zigbee.h"

#include "constants.h"

// Last pairing loop time for non-blocking delay
static uint32_t last_pairing_loop_ms = 0;

void setup()
{
  // NVS preferences (stores direction flag)
  prefs.begin("zb-remote", false);
  dir_up = prefs.getBool("dir_up", true);

  // Heuristic wake source
  woke_by_timer = (digitalRead(BTN1_PIN) == HIGH && digitalRead(BTN2_PIN) == HIGH);

#if DEBUG_MODE
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== DEBUG MODE: Gestures test (no Zigbee) ===\n");
#else
  // Start Zigbee stack and register endpoints
  zigbeeInit();

  // Give stack time to connect (first join may take longer)
  uint32_t t0 = millis();
  bool connected = false;
  while (!Zigbee.connected() && millis() - t0 < ZIGBEE_CONNECT_TIMEOUT_MS)
  {
    Zigbee.run();
    delay(ZIGBEE_FLUSH_DELAY_MS);
  }
  connected = Zigbee.connected();

  // Timer wake → report battery and sleep
  if (woke_by_timer)
  {
    uint16_t mv = read_battery_mv();
    // Only report battery if connected, otherwise just sleep
    if (connected)
    {
      report_battery(mv);
    }
    goto_sleep(); // no return
  }

  // If not connected after timeout and not pairing, try to sleep and retry later
  if (!connected && !pairing_mode)
  {
    // Connection failed, sleep and retry on next button press or timer wake
    goto_sleep();
  }
#endif

  initButtons();
}

void loop()
{
#if !DEBUG_MODE
  Zigbee.run();
#endif

  // Keep awake during pairing window with non-blocking delay
  if (pairing_mode)
  {
    uint32_t now = millis();

    if (now < pairing_deadline_ms)
    {
      // Non-blocking delay: only process if enough time has passed
      if (now - last_pairing_loop_ms >= PAIRING_LOOP_DELAY_MS)
      {
        last_pairing_loop_ms = now;
        // Could add additional pairing-related processing here
      }
      return;
    }
    else
    {
      pairing_mode = false;
      goto_sleep();
    }
  }

  ButtonEvent ev1 = handleButton(b1);
  ButtonEvent ev2 = handleButton(b2);
  routeEvents(ev1, ev2);

  goto_sleep();
}

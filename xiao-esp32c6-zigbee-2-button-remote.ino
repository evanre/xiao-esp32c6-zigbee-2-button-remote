// #ifndef ZIGBEE_MODE_ED
// #error "Zigbee end device mode is not selected in Tools->Zigbee mode"
// #endif

#include "constants.h"

void setup()
{
  // #if DEBUG_MODE
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== DEBUG MODE: Gestures test (no Zigbee) ===\n");
  // #endif

  // NVS preferences (stores direction flag)
  // prefs.begin("zb-remote", false);
  // dir_up = prefs.getBool("dir_up", true);

  // Heuristic wake source
  // woke_by_timer = (digitalRead(BTN1_PIN) == HIGH && digitalRead(BTN2_PIN) == HIGH);

  // #if !DEBUG_MODE
  // Start Zigbee stack and register endpoints
  // zigbeeInit();

  // Give stack time to connect (first join may take longer)
  // uint32_t t0 = millis();
  // while (!Zigbee.connected() && millis() - t0 < 8000) {
  // Zigbee.run();
  // delay(50);
  // }
  // #endif

  initButtons();

  // #if !DEBUG_MODE
  // Timer wake → report battery and sleep
  // if (woke_by_timer) {
  // uint16_t mv = read_battery_mv();
  // report_battery(mv);
  // goto_sleep(); // no return
  // }
  // #endif
}

void loop()
{
  // #if !DEBUG_MODE
  // Zigbee.run();
  // #endif

  // Keep awake during pairing window
  //   if (pairing_mode) {
  //     if (millis() < pairing_deadline_ms) {
  //       delay(50);
  //       return;
  //     } else {
  //       pairing_mode = false;
  // #if !DEBUG_MODE
  //       goto_sleep();
  // #endif
  //     }
  //   }

  // STEP 1: Optional pairing sequence (6 clicks on BTN1)
  // if (detect_pairing_sequence()) {
  //   enter_pairing_mode();
  //   return;
  // }

  // STEP 2: Normal interactions
  // If you want “both buttons = Lamp 3”, uncomment this block:
  // if (bothPressedEarly()) {
  //   handleBothButtons(EP_L3);
  // } else {
  //   if (digitalRead(BTN1_PIN) == LOW) handleButton(EP_L1);
  //   if (digitalRead(BTN2_PIN) == LOW) handleButton(EP_L2);
  // }

  // Current: single buttons only (as in your code)
  ButtonEvent ev1 = handleButton(b1);
  ButtonEvent ev2 = handleButton(b2);
  routeEvents(ev1, ev2);

  // STEP 3: Sleep (or delay in DEBUG)
  // delay(20);
  // #if !DEBUG_MODE
  // goto_sleep(); // no return
  // #else
  delay(10);
  // #endif
}

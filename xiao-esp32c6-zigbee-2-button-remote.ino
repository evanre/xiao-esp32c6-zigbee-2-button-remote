/*
 * ESP32-C6 Zigbee 2-Button Remote Control
 *
 * Battery-powered Zigbee remote with 2 buttons controlling up to 3 devices/groups.
 *
 * Button Gestures:
 *   BTN1 / BTN2 (single button):
 *     - Single click:      Toggle light on/off
 *     - Long hold:         Adjust brightness (direction alternates)
 *     - Click + hold:      Adjust color temperature (direction alternates)
 *     - Double click:      Toggle light on/off
 *
 *   BTN1 + BTN2 (both pressed):
 *     - Click:             Toggle Lamp 3 on/off
 *     - Long hold:         Adjust Lamp 3 brightness
 *
 *   Pairing mode:
 *     - 6 quick clicks on BTN1 within 5s → Factory reset + network steering
 *
 * Power Management:
 *   - Deep sleep between button presses
 *   - Wakes on button press or every 6 hours for battery report
 *   - Automatic sleep timeout: 30s max button hold
 *
 * Hardware: XIAO ESP32-C6
 *   - BTN1: GPIO2 (D1)
 *   - BTN2: GPIO3 (D2)
 *   - VBAT: A0 (battery monitoring)
 */

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "constants.h"

void setup() {
  // Initialize NVS preferences (stores brightness/color temp direction flag)
  prefs.begin("zb-remote", false);
  dir_up = prefs.getBool("dir_up", true);

  // Configure buttons as inputs with internal pullups (LOW = pressed)
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  // Determine wake source heuristic:
  // If both buttons are idle (HIGH) at wake → timer wake (battery report)
  // If any button is LOW → button wake (user interaction)
  woke_by_timer = (digitalRead(BTN1_PIN) == HIGH && digitalRead(BTN2_PIN) == HIGH);

  // Initialize Zigbee stack as End Device and register 3 switch endpoints
  zigbeeInit();

  // Wait for network connection (up to 8s for initial pairing)
  uint32_t t0 = millis();
  while (!Zigbee.connected() && millis() - t0 < 8000) {
    Zigbee.run();
    delay(50);
  }

  // Initialize button state machines
  initButtons();

  // Timer wake path: report battery and return to sleep immediately
  if (woke_by_timer) {
    uint16_t mv = read_battery_mv();
    report_battery(mv);
    goto_sleep();  // Does not return
  }
  // If button wake: continue to loop() for user interaction handling
}

void loop() {
  // Service Zigbee stack (handle incoming messages, maintain connection)
  Zigbee.run();

  // PAIRING MODE: Keep radio active during network steering window
  if (pairing_mode) {
    if (millis() < pairing_deadline_ms) {
      delay(50);
      return;  // Stay awake until pairing window expires
    } else {
      pairing_mode = false;
      goto_sleep();  // Pairing window expired, return to sleep
    }
  }

  // STEP 1: Check for pairing sequence (6 quick clicks on BTN1)
  if (detect_pairing_sequence()) {
    enter_pairing_mode();  // Factory reset + network steering for STEER_SECONDS
    return;                // Stay awake during steering window
  }

  // STEP 2: Handle normal button interactions
  if (bothPressedEarly()) {
    // Both buttons pressed → control Lamp 3 (endpoint 3)
    handleTarget(EP_L3);
  } else {
    // Single button pressed → control respective lamp
    if (digitalRead(BTN1_PIN) == LOW) handleTarget(EP_L1);  // Button 1 → Lamp 1
    if (digitalRead(BTN2_PIN) == LOW) handleTarget(EP_L2);  // Button 2 → Lamp 2
  }

  // STEP 3: Return to deep sleep to conserve battery
  delay(20);
  goto_sleep();  // Does not return (wakes on button or timer)
}

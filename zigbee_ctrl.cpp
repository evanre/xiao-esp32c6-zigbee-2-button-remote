/*
 * Zigbee Control Logic
 *
 * Handles all Zigbee communication and device control:
 * - 3 ZigbeeSwitch client endpoints (EP_L1, EP_L2, EP_L3)
 * - On/Off, Level Control, and Color Temperature commands
 * - Battery voltage reading and reporting
 * - Pairing mode (factory reset + network steering)
 * - Deep sleep power management
 *
 * Button gesture handlers translate user input into Zigbee commands:
 * - Click → Toggle
 * - Hold → Brightness adjustment (start/stop)
 * - Click+Hold → Color temperature adjustment (start/stop)
 */

#include "constants.h"

// Global storage
// Preferences prefs;
// bool dir_up = true;
// bool woke_by_timer = false;
// bool pairing_mode = false;
// uint32_t pairing_deadline_ms = 0;

// Zigbee client endpoints (On/Off + Level + ColorTemp)
// ZigbeeSwitch lamp1(EP_L1);
// ZigbeeSwitch lamp2(EP_L2);
// ZigbeeSwitch lamp3(EP_L3);

// Array for easy dispatch (index 0 = EP_L1, index 1 = EP_L2, index 2 = EP_L3)
// static ZigbeeSwitch* lamps[] = { &lamp1, &lamp2, &lamp3 };

// static inline bool pinRead(int pin) { return digitalRead(pin); }

// Helper: validate and get lamp pointer
// static inline ZigbeeSwitch* getLamp(uint8_t ep) {
//   if (ep >= EP_L1 && ep <= EP_L3) {
//     return lamps[ep - 1];
//   }
//   return nullptr;
// }

/* ===================== Zigbee init ===================== */
void zigbeeInit()
{
  Zigbee.begin(ZIGBEE_END_DEVICE);

  lamp1.setManufacturerAndModel("DIY", "TwoBtnRemote");
  lamp2.setManufacturerAndModel("DIY", "TwoBtnRemote");
  lamp3.setManufacturerAndModel("DIY", "TwoBtnRemote");

  Zigbee.addEndpoint(&lamp1);
  Zigbee.addEndpoint(&lamp2);
  Zigbee.addEndpoint(&lamp3);
}

/* ===================== Battery ===================== */

// Li-ion battery voltage-to-percentage lookup table (single cell)
// Based on typical Li-ion discharge curve under light load
// static const struct {
//   uint16_t mv;
//   uint8_t pct;
// } battery_lut[] = {
//   {4200, 100}, {4150, 95},  {4110, 90},  {4080, 85},  {4020, 80},
//   {3980, 75},  {3950, 70},  {3910, 65},  {3870, 60},  {3850, 55},
//   {3840, 50},  {3820, 45},  {3800, 40},  {3790, 35},  {3770, 30},
//   {3750, 25},  {3730, 20},  {3710, 15},  {3690, 10},  {3610, 5},
//   {3300, 0}
// };

// uint16_t read_battery_mv()
// {
//   // ADC → mV conversion
//   // ESP32-C6 ADC: 12-bit (0-4095), with configurable attenuation
//   // Default attenuation typically allows reading 0-3.3V or 0-2.5V range
//   int raw = analogRead(VBAT_ADC_PIN); // 0..4095

//   // ADC reference voltage (mV) - ESP32-C6 typically uses ~1100mV internal reference
//   // May need calibration based on actual board configuration
//   float mv_adc = (raw / 4095.0f) * 1100.0f;

//   // Apply hardware divider and calibration factor
//   float vbat_mv = mv_adc * VBAT_DIVIDER * ADC_CAL_K;

//   // Clamp to reasonable range
//   if (vbat_mv < 0)
//     vbat_mv = 0;
//   if (vbat_mv > 5000)
//     vbat_mv = 5000;

//   return (uint16_t)(vbat_mv + 0.5f);
// }

// uint8_t vbat_percent(uint16_t mv)
// {
//   // Clamp to valid range
//   if (mv <= battery_lut[sizeof(battery_lut) / sizeof(battery_lut[0]) - 1].mv)
//   {
//     return 0;
//   }
//   if (mv >= battery_lut[0].mv)
//   {
//     return 100;
//   }

//   // Linear interpolation between lookup table entries
//   for (size_t i = 0; i < sizeof(battery_lut) / sizeof(battery_lut[0]) - 1; i++)
//   {
//     if (mv >= battery_lut[i + 1].mv && mv <= battery_lut[i].mv)
//     {
//       uint16_t mv_range = battery_lut[i].mv - battery_lut[i + 1].mv;
//       uint8_t pct_range = battery_lut[i].pct - battery_lut[i + 1].pct;

//       // Interpolate
//       float ratio = (float)(mv - battery_lut[i + 1].mv) / mv_range;
//       uint8_t pct = battery_lut[i + 1].pct + (uint8_t)(ratio * pct_range + 0.5f);

//       return pct > 100 ? 100 : pct;
//     }
//   }

//   return 0; // Fallback
// }

void report_battery(uint16_t mv)
{
  // uint8_t pct = vbat_percent(mv);

  // #if DEBUG_MODE
  // Serial.printf("[BATTERY] %dmV (%d%%)\n", mv, pct);
  // #else
  // Power Configuration cluster (0x0001):
  //  - BatteryVoltage (0x0020): tenths of volts (e.g. 3.95V -> 39)
  //  - BatteryPercentageRemaining (0x0021): half-percent units (87% -> 174)
  // uint8_t zcl_voltage = (uint8_t)((mv + 50) / 100);
  // uint8_t zcl_pct     = (uint8_t)(min<uint16_t>(pct * 2, 200));

  // Report from EP1 to coordinator
  // Zigbee.reportPowerConfiguration(EP_L1, zcl_voltage, zcl_pct);
  // #endif
}

/* ===================== Commands ===================== */

void cmd_toggle(uint8_t ep)
{
  // #if DEBUG_MODE
  Serial.printf("[CMD] EP%d: Toggle\n", ep);
  // #else
  // ZigbeeSwitch* lamp = getLamp(ep);
  // if (lamp) lamp->lightToggle();
  // #endif
}

void cmd_level_start(uint8_t ep, bool up)
{
  // #if DEBUG_MODE
  Serial.printf("[CMD] EP%d: Level %s (START)\n", ep, up ? "UP" : "DOWN");
  // #else
  // ZigbeeSwitch* lamp = getLamp(ep);
  // if (!lamp) return;

  // if (up) {
  // lamp->levelMoveUp();
  // } else {
  // lamp->levelMoveDown();
  // }
  // #endif
}

void cmd_level_stop(uint8_t ep)
{
  // #if DEBUG_MODE
  // Serial.printf("[CMD] EP%d: Level STOP\n", ep);
  // #else
  // ZigbeeSwitch* lamp = getLamp(ep);
  // if (lamp) lamp->levelStop();
  // #endif
}

void cmd_ct_start(uint8_t ep, bool up)
{
  // #if DEBUG_MODE
  // Serial.printf("[CMD] EP%d: Color Temp %s (START)\n", ep, up ? "UP" : "DOWN");
  // #else
  // ZigbeeSwitch* lamp = getLamp(ep);
  // if (!lamp) return;

  // if (up) {
  // lamp->colorTempMoveUp();
  // } else {
  // lamp->colorTempMoveDown();
  // }
  // #endif
}

void cmd_ct_stop(uint8_t ep)
{
  //   // #if DEBUG_MODE
  //   // Serial.printf("[CMD] EP%d: Color Temp STOP\n", ep);
  //   // #else
  //   // ZigbeeSwitch* lamp = getLamp(ep);
  //   // if (lamp) lamp->colorTempStop();
  //   // #endif
}

/* ===================== Pairing ===================== */
// void enter_pairing_mode()
// {
// #if DEBUG_MODE
// Serial.printf("[PAIRING] Enter pairing mode (would steer for %d sec)\n", STEER_SECONDS);
// #endif

// pairing_mode = true;
// pairing_deadline_ms = millis() + (STEER_SECONDS * 1000UL);

// #if !DEBUG_MODE
// Wipe network state and re-start as End Device
// Zigbee.factoryReset();
// delay(200);
// Zigbee.begin(ZIGBEE_END_DEVICE);

// Re-register endpoints
// Zigbee.addEndpoint(&lamp1);
// Zigbee.addEndpoint(&lamp2);
// Zigbee.addEndpoint(&lamp3);

// Open network (steering window)
// Zigbee.startSteering(STEER_SECONDS);
// #endif
// }

/* ===================== Handler helpers ===================== */

// Handle both buttons pressed (EP_L3)
// Click = toggle, Long hold = adjust brightness
// static void handleBothButtons(uint8_t ep)
// {
// #if DEBUG_MODE
//   Serial.println("[GESTURE] Both buttons pressed");
// #endif

//   uint32_t t_down = millis();

//   // Wait to see if it's a click or hold (with timeout protection)
//   while (pinRead(BTN1_PIN) == LOW && pinRead(BTN2_PIN) == LOW)
//   {
// #if !DEBUG_MODE
//     Zigbee.run();
// #endif

//     // Check for timeout (safety guard)
//     if (millis() - t_down >= 30000)
//     {
//       return; // Timeout after 30s
//     }

//     if (millis() - t_down >= HOLD_MS)
//     {
// #if DEBUG_MODE
//       Serial.println("[GESTURE] Long hold detected");
// #endif

//       // HOLD_START → brightness move
//       cmd_level_start(ep, dir_up);

//       // Wait for release → HOLD_STOP (with timeout)
//       uint32_t t_hold_start = millis();
//       while (pinRead(BTN1_PIN) == LOW || pinRead(BTN2_PIN) == LOW)
//       {
// #if !DEBUG_MODE
//         Zigbee.run();
// #endif
//         if (millis() - t_hold_start >= 30000)
//           break; // Safety timeout
//         delay(10);
//       }
//       cmd_level_stop(ep);

//       // Toggle direction for next hold
//       dir_up = !dir_up;
//       prefs.putBool("dir_up", dir_up);
//       return;
//     }
//     delay(10);
//   }

// #if DEBUG_MODE
//   Serial.println("[GESTURE] Click detected");
// #endif

//   // Released before HOLD_MS → simple click → toggle
//   cmd_toggle(ep);
// }

// Handle click + hold gesture for color temperature adjustment
// static bool handleClickAndHold(uint8_t ep, BtnState *bx)
// {
//   // Window to detect second press after first click
//   uint32_t t_wait = millis();
//   bool second_pressed = false;

//   while (millis() - t_wait < DOUBLE_MS)
//   {
// #if !DEBUG_MODE
//     Zigbee.run();
// #endif
//     if (pinRead(bx->pin) == LOW)
//     {
//       second_pressed = true;
//       break;
//     }
//     delay(5);
//   }

//   if (!second_pressed)
//   {
// #if DEBUG_MODE
//     Serial.println("[GESTURE] Single click");
// #endif
//     // Single click → toggle
//     cmd_toggle(ep);
//     return true; // Handled
//   }

//   // Check if second press is long → click + hold gesture
//   uint32_t t_hold = millis();
//   while (pinRead(bx->pin) == LOW)
//   {
// #if !DEBUG_MODE
//     Zigbee.run();
// #endif

//     // Timeout protection
//     if (millis() - t_hold >= 30000)
//     {
//       return true; // Safety exit
//     }

//     if (millis() - t_hold >= HOLD_MS)
//     {
// #if DEBUG_MODE
//       Serial.println("[GESTURE] Click + hold detected");
// #endif

//       // CLICK + HOLD_START → color temp move
//       cmd_ct_start(ep, dir_up);

//       // Wait for release → CLICK + HOLD_STOP
//       uint32_t t_ct_start = millis();
//       while (pinRead(bx->pin) == LOW)
//       {
// #if !DEBUG_MODE
//         Zigbee.run();
// #endif
//         if (millis() - t_ct_start >= 30000)
//           break; // Safety timeout
//         delay(10);
//       }
//       cmd_ct_stop(ep);

//       // Toggle direction for next hold
//       dir_up = !dir_up;
//       prefs.putBool("dir_up", dir_up);
//       return true; // Handled
//     }
//     delay(10);
//   }

// #if DEBUG_MODE
//   Serial.println("[GESTURE] Double click");
// #endif

//   // Second press was also short → treat as double click → toggle
//   cmd_toggle(ep);
//   return true; // Handled
// }

// Handle simple long hold for brightness adjustment
// static bool handleLongHold(uint8_t ep, BtnState *bx)
// {
// #if DEBUG_MODE
//   Serial.println("[GESTURE] Long hold");
// #endif

//   // HOLD_START → level move
//   cmd_level_start(ep, dir_up);

//   // Wait for release → HOLD_STOP (with timeout)
//   uint32_t t_hold_start = millis();
//   while (pinRead(bx->pin) == LOW)
//   {
// #if !DEBUG_MODE
//     Zigbee.run();
// #endif
//     if (millis() - t_hold_start >= 30000)
//       break; // Safety timeout
//     delay(10);
//   }
//   cmd_level_stop(ep);

//   // Toggle direction for next hold
//   dir_up = !dir_up;
//   prefs.putBool("dir_up", dir_up);
//   return true; // Handled
// }

// Handle single button (EP_L1 or EP_L2)
// Click = toggle, Hold = brightness, Click+Hold = color temp
// static void handleSingleButton(uint8_t ep)
// {
//   BtnState *bx = (ep == EP_L1) ? &b1 : &b2;
//   bool click_seen = false;
//   uint32_t t_start = millis();

//   // Event processing loop (with timeout)
//   while (millis() - t_start < 1500)
//   {
// #if !DEBUG_MODE
//     Zigbee.run();
// #endif
//     ClickType ev = handleButton(*bx);

//     if (ev == NONE)
//     {
//       delay(5);
//       continue;
//     }

//     if (ev == SINGLE)
//     {
//       if (!click_seen)
//       {
//         click_seen = true;
//         // Try to detect "click + hold" gesture
//         if (handleClickAndHold(ep, bx))
//         {
//           return; // Handled
//         }
//       }
//       else
//       {
//         // Already saw one click → double → toggle
//         cmd_toggle(ep);
//         return;
//       }
//     }
//     else if (ev == DOUBLE_CLICK)
//     {
//       cmd_toggle(ep);
//       return;
//     }
//     else if (ev == LONGHOLD)
//     {
//       if (handleLongHold(ep, bx))
//       {
//         return; // Handled
//       }
//     }
//   }
// }

/* ===================== Sleep ===================== */
// void goto_sleep()
// {
// #if DEBUG_MODE
//   // In debug mode, don't actually sleep
//   Serial.println("[DEBUG] Would sleep (staying awake for debug)");
// #else
//   const uint64_t us_6h = (uint64_t)PING_INTERVAL_HOURS * 3600ULL * 1000000ULL;

//   // Wake sources: GPIO (buttons) + timer
//   esp_deep_sleep_enable_gpio_wakeup(
//       (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
//       ESP_GPIO_WAKEUP_GPIO_LOW);
//   esp_sleep_enable_timer_wakeup(us_6h);

//   // Give stack time to flush TX
//   uint32_t t0 = millis();
//   while (millis() - t0 < 50)
//   {
//     Zigbee.run();
//     delay(5);
//   }

//   esp_deep_sleep_start();
// #endif
//}

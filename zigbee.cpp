/*
 * Zigbee Control Logic
 *
 * Handles all Zigbee communication and device control:
 * - 3 ZigbeeSwitch client endpoints (EP_L1, EP_L2, EP_L3)
 * - On/Off, Level Control, and Color Temperature commands
 * - Battery voltage reading and reporting
 * - Pairing mode (factory reset + network steering)
 * - Deep sleep power management
 */

#include "constants.h"

// Global storage
Preferences prefs;
bool dir_up = true;
bool woke_by_timer = false;
bool pairing_mode = false;
uint32_t pairing_deadline_ms = 0;

// Zigbee client endpoints (On/Off + Level + ColorTemp)
ZigbeeSwitch lamp1(EP_L1);
ZigbeeSwitch lamp2(EP_L2);
ZigbeeSwitch lamp3(EP_L3);

// Array for easy dispatch (index 0 = EP_L1, index 1 = EP_L2, index 2 = EP_L3)
static ZigbeeSwitch *lamps[] = {&lamp1, &lamp2, &lamp3};

static inline bool pinRead(int pin) { return digitalRead(pin); }

// Helper: validate and get lamp pointer
static inline ZigbeeSwitch *getLamp(LampId id)
{
  if (id >= EP_L1 && id <= EP_L3)
  {
    return lamps[id - 1];
  }
  return nullptr;
}

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
static const struct
{
  uint16_t mv;
  uint8_t pct;
} battery_lut[] = {
    {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75}, {3950, 70}, {3910, 65}, {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45}, {3800, 40}, {3790, 35}, {3770, 30}, {3750, 25}, {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5}, {3300, 0}};

uint16_t read_battery_mv()
{
  // ADC → mV conversion
  // ESP32-C6 ADC: 12-bit (0-4095), with configurable attenuation
  // Default attenuation typically allows reading 0-3.3V or 0-2.5V range
  int raw = analogRead(VBAT_ADC_PIN); // 0..4095

  // ADC reference voltage (mV) - ESP32-C6 typically uses ~3300mV internal reference
  // May need calibration based on actual board configuration
  float mv_adc = (raw / 4095.0f) * 3300.0f;

  // Apply hardware divider and calibration factor
  float vbat_mv = mv_adc * VBAT_DIVIDER * ADC_CAL_K;

  // Clamp to reasonable range
  if (vbat_mv < 0)
    vbat_mv = 0;
  if (vbat_mv > 5000)
    vbat_mv = 5000;

  return (uint16_t)(vbat_mv + 0.5f);
}

uint8_t vbat_percent(uint16_t mv)
{
  // Clamp to valid range
  if (mv <= battery_lut[sizeof(battery_lut) / sizeof(battery_lut[0]) - 1].mv)
  {
    return 0;
  }
  if (mv >= battery_lut[0].mv)
  {
    return 100;
  }

  // Linear interpolation between lookup table entries
  for (size_t i = 0; i < sizeof(battery_lut) / sizeof(battery_lut[0]) - 1; i++)
  {
    if (mv >= battery_lut[i + 1].mv && mv <= battery_lut[i].mv)
    {
      uint16_t mv_range = battery_lut[i].mv - battery_lut[i + 1].mv;
      uint8_t pct_range = battery_lut[i].pct - battery_lut[i + 1].pct;

      // Interpolate
      float ratio = (float)(mv - battery_lut[i + 1].mv) / mv_range;
      uint8_t pct = battery_lut[i + 1].pct + (uint8_t)(ratio * pct_range + 0.5f);

      return pct > 100 ? 100 : pct;
    }
  }

  return 0; // Fallback
}

void report_battery(uint16_t mv)
{
  uint8_t pct = vbat_percent(mv);

#if DEBUG_MODE
  Serial.printf("[BATTERY] %dmV (%d%%)\n", mv, pct);
#else
  // Power Configuration cluster (0x0001):
  //  - BatteryVoltage (0x0020): tenths of volts (e.g. 3.95V -> 39)
  //  - BatteryPercentageRemaining (0x0021): half-percent units (87% -> 174)
  uint8_t zcl_voltage = (uint8_t)((mv + 50) / 100);
  uint8_t zcl_pct = (uint8_t)(min<uint16_t>(pct * 2, 200));

  // Report from EP1 to coordinator
  Zigbee.reportPowerConfiguration(EP_L1, zcl_voltage, zcl_pct);
#endif
}

/* ===================== Helpers ====================== */
bool get_dir()
{
  dir_up = !dir_up;
  prefs.putBool("dir_up", dir_up);
  return dir_up;
}

/* ===================== Commands ===================== */
void cmd_toggle(LampId lampId)
{
  ZigbeeSwitch *lamp = getLamp(lampId);

  if (lamp)
    lamp->lightToggle();
}

void cmd_level_start(LampId lampId)
{
  bool up = get_dir();
  ZigbeeSwitch *lamp = getLamp(lampId);

  if (!lamp)
    return;

  if (up)
    lamp->levelMoveUp();
  else
    lamp->levelMoveDown();
}

void cmd_level_stop(LampId lampId)
{
  ZigbeeSwitch *lamp = getLamp(lampId);

  if (lamp)
    lamp->levelStop();
}

void cmd_ct_start(LampId lampId)
{
  bool up = get_dir();
  ZigbeeSwitch *lamp = getLamp(lampId);

  if (!lamp)
    return;

  if (up)
    lamp->colorTempMoveUp();
  else
    lamp->colorTempMoveDown();
}

void cmd_ct_stop(LampId lampId)
{
  ZigbeeSwitch *lamp = getLamp(lampId);

  if (lamp)
    lamp->colorTempStop();
}

void cmd_empty_action(LampId lampId)
{
}

/* ===================== Pairing ===================== */
void enter_pairing_mode(LampId lampId)
{
  pairing_mode = true;
  pairing_deadline_ms = millis() + (STEER_SECONDS * 1000UL);

  // Wipe network state and re-start as End Device
  Zigbee.factoryReset();
  delay(200);
  Zigbee.begin(ZIGBEE_END_DEVICE);

  // Re-register endpoints
  Zigbee.addEndpoint(&lamp1);
  Zigbee.addEndpoint(&lamp2);
  Zigbee.addEndpoint(&lamp3);

  // Open network (steering window)
  Zigbee.startSteering(STEER_SECONDS);
}

/* ===================== Sleep ===================== */
void goto_sleep()
{
#if DEBUG_MODE
  delay(10); // Don't sleep in debug mode
#else
  const uint64_t us_6h = (uint64_t)PING_INTERVAL_HOURS * 3600ULL * 1000000ULL;

  // Wake sources: GPIO (buttons) + timer
  esp_deep_sleep_enable_gpio_wakeup(
      (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
      ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_enable_timer_wakeup(us_6h);

  // Give stack time to flush TX
  uint32_t t0 = millis();
  while (millis() - t0 < 50)
  {
    Zigbee.run();
    delay(5);
  }

  esp_deep_sleep_start();
#endif
}

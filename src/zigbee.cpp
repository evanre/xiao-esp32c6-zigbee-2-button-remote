#include "zigbee.h"

#define TAG "ZIGBEE"

// Global storage
nvs_handle_t g_nvs_handle;
bool dir_up = true;
bool woke_by_timer = false;
bool pairing_mode = false;
uint32_t pairing_deadline_ms = 0;

// Zigbee client endpoints (On/Off + Level + ColorTemp)
// TODO: Update with ESP-IDF Zigbee API
// ZigbeeSwitch lamp1(EP_L1);
// ZigbeeSwitch lamp2(EP_L2);
// ZigbeeSwitch lamp3(EP_L3);

// Array for easy dispatch (index 0 = EP_L1, index 1 = EP_L2, index 2 = EP_L3)
// static ZigbeeSwitch *lamps[] = {&lamp1, &lamp2, &lamp3};

static inline bool pinRead(gpio_num_t pin) { return gpio_get_level(pin) == 1; }

// ADC handle for battery reading
static adc_oneshot_unit_handle_t adc_handle = NULL;

// Helper: validate and get lamp pointer
// TODO: Update with ESP-IDF Zigbee API
// static inline ZigbeeSwitch *getLamp(LampId id)
// {
//   uint8_t idx = static_cast<uint8_t>(id);
//
//   // Validate against LampId enum values
//   if (id == LampId::L1 || id == LampId::L2 || id == LampId::L3)
//   {
//     return lamps[idx - 1];
//   }
//
// #if DEBUG_MODE
//   ESP_LOGE(TAG, "Invalid LampId: %d", idx);
// #endif
//   return nullptr;
// }

/* ===================== Zigbee init ===================== */
void zigbeeInit()
{
  // TODO: Update with ESP-IDF Zigbee API
  // Zigbee.begin(ZIGBEE_END_DEVICE);
  //
  // lamp1.setManufacturerAndModel("DIY", "TwoBtnRemote");
  // lamp2.setManufacturerAndModel("DIY", "TwoBtnRemote");
  // lamp3.setManufacturerAndModel("DIY", "TwoBtnRemote");
  //
  // Zigbee.addEndpoint(&lamp1);
  // Zigbee.addEndpoint(&lamp2);
  // Zigbee.addEndpoint(&lamp3);

  ESP_LOGI(TAG, "Zigbee initialization placeholder");

  // Initialize ADC for battery reading
  adc_oneshot_unit_init_cfg_t adc_config = {
    .unit_id = VBAT_ADC_UNIT,
    .clk_src = (adc_oneshot_clk_src_t)0,  // Use default clock source
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_handle));

  adc_oneshot_chan_cfg_t chan_config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, VBAT_ADC_CHANNEL, &chan_config));
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
  if (adc_handle == NULL) {
    ESP_LOGE(TAG, "ADC not initialized");
    return 0;
  }

  // Read ADC value
  int raw = 0;
  esp_err_t ret = adc_oneshot_read(adc_handle, VBAT_ADC_CHANNEL, &raw);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
    return 0;
  }

  // ADC → mV conversion
  // ESP32-C6 ADC: 12-bit (0-4095), with configurable attenuation
  // ADC_ATTEN_DB_12 allows reading 0-3.3V range
  float mv_adc = (raw / static_cast<float>(ADC_MAX_VALUE)) * ADC_REF_MV;

  // Apply hardware divider and calibration factor
  float vbat_mv = mv_adc * VBAT_DIVIDER * ADC_CAL_K;

  // Clamp to reasonable range
  if (vbat_mv < VBAT_MIN_MV)
    vbat_mv = VBAT_MIN_MV;
  if (vbat_mv > VBAT_MAX_MV)
    vbat_mv = VBAT_MAX_MV;

  return (uint16_t)(vbat_mv + 0.5f);
}

uint8_t vbat_percent(uint16_t mv)
{
  constexpr size_t lut_size = sizeof(battery_lut) / sizeof(battery_lut[0]);

  // Validate LUT size
  if (lut_size < 2)
  {
#if DEBUG_MODE
    ESP_LOGE(TAG, "Battery LUT too small");
#endif
    return 0;
  }

  // Clamp to valid range
  if (mv <= battery_lut[lut_size - 1].mv)
  {
    return 0;
  }
  if (mv >= battery_lut[0].mv)
  {
    return 100;
  }

  // Linear interpolation between lookup table entries
  for (size_t i = 0; i < lut_size - 1; i++)
  {
    // Bounds check before accessing array
    if (i + 1 >= lut_size)
    {
#if DEBUG_MODE
      ESP_LOGE(TAG, "Battery LUT bounds error");
#endif
      return 0;
    }

    if (mv >= battery_lut[i + 1].mv && mv <= battery_lut[i].mv)
    {
      uint16_t mv_range = battery_lut[i].mv - battery_lut[i + 1].mv;
      uint8_t pct_range = battery_lut[i].pct - battery_lut[i + 1].pct;

      // Avoid division by zero
      if (mv_range == 0)
      {
        return battery_lut[i].pct;
      }

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
  ESP_LOGI(TAG, "[BATTERY] %dmV (%d%%)", mv, pct);
#else
  // Power Configuration cluster (0x0001):
  //  - BatteryVoltage (0x0020): tenths of volts (e.g. 3.95V -> 39)
  //  - BatteryPercentageRemaining (0x0021): half-percent units (87% -> 174)
  uint8_t zcl_voltage = (uint8_t)((mv + ZCL_VOLTAGE_OFFSET) / ZCL_VOLTAGE_SCALE);
  uint8_t zcl_pct = (uint8_t)(std::min<uint16_t>(pct * ZCL_PERCENTAGE_SCALE, ZCL_PERCENTAGE_MAX));

  // Report from EP1 to coordinator
  // TODO: Update with ESP-IDF Zigbee API
  // if (!Zigbee.reportPowerConfiguration(EP_L1, zcl_voltage, zcl_pct))
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Failed to report battery");
  // #endif
  // }
#endif
}

/* ===================== Helpers ====================== */
bool get_dir()
{
  dir_up = !dir_up;
  uint8_t val = dir_up ? 1 : 0;
  esp_err_t err = nvs_set_u8(g_nvs_handle, "dir_up", val);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save direction: %s", esp_err_to_name(err));
  } else {
    nvs_commit(g_nvs_handle);
  }
  return dir_up;
}

/* ===================== Commands ===================== */
void cmd_toggle(LampId lampId)
{
  // TODO: Update with ESP-IDF Zigbee API
  // ZigbeeSwitch *lamp = getLamp(lampId);
  //
  // if (!lamp)
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Invalid lamp ID for toggle");
  // #endif
  //   return;
  // }
  //
  // #if !DEBUG_MODE
  //   if (!lamp->lightToggle())
  //   {
  // #if DEBUG_MODE
  //     ESP_LOGE(TAG, "Failed to send toggle command to lamp %d", static_cast<uint8_t>(lampId));
  // #endif
  //   }
  // #else
  //   lamp->lightToggle();
  // #endif

  ESP_LOGI(TAG, "cmd_toggle L%d", static_cast<uint8_t>(lampId));
}

void cmd_level_start(LampId lampId)
{
  bool up = get_dir();
  // TODO: Update with ESP-IDF Zigbee API
  // ZigbeeSwitch *lamp = getLamp(lampId);
  //
  // if (!lamp)
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Invalid lamp ID for level_start");
  // #endif
  //   return;
  // }
  //
  // #if !DEBUG_MODE
  //   bool success = up ? lamp->levelMoveUp() : lamp->levelMoveDown();
  //   if (!success)
  //   {
  // #if DEBUG_MODE
  //     ESP_LOGE(TAG, "Failed to send level move command to lamp %d", static_cast<uint8_t>(lampId));
  // #endif
  //   }
  // #else
  //   if (up)
  //     lamp->levelMoveUp();
  //   else
  //     lamp->levelMoveDown();
  // #endif

  ESP_LOGI(TAG, "cmd_level_start L%d (%s)", static_cast<uint8_t>(lampId), up ? "UP" : "DOWN");
}

void cmd_level_stop(LampId lampId)
{
  // TODO: Update with ESP-IDF Zigbee API
  // ZigbeeSwitch *lamp = getLamp(lampId);
  //
  // if (!lamp)
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Invalid lamp ID for level_stop");
  // #endif
  //   return;
  // }
  //
  // #if !DEBUG_MODE
  //   if (!lamp->levelStop())
  //   {
  // #if DEBUG_MODE
  //     ESP_LOGE(TAG, "Failed to send level stop command to lamp %d", static_cast<uint8_t>(lampId));
  // #endif
  //   }
  // #else
  //   lamp->levelStop();
  // #endif

  ESP_LOGI(TAG, "cmd_level_stop L%d", static_cast<uint8_t>(lampId));
}

void cmd_ct_start(LampId lampId)
{
  bool up = get_dir();
  // TODO: Update with ESP-IDF Zigbee API
  // ZigbeeSwitch *lamp = getLamp(lampId);
  //
  // if (!lamp)
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Invalid lamp ID for ct_start");
  // #endif
  //   return;
  // }
  //
  // #if !DEBUG_MODE
  //   bool success = up ? lamp->colorTempMoveUp() : lamp->colorTempMoveDown();
  //   if (!success)
  //   {
  // #if DEBUG_MODE
  //     ESP_LOGE(TAG, "Failed to send color temp move command to lamp %d", static_cast<uint8_t>(lampId));
  // #endif
  //   }
  // #else
  //   if (up)
  //     lamp->colorTempMoveUp();
  //   else
  //     lamp->colorTempMoveDown();
  // #endif

  ESP_LOGI(TAG, "cmd_ct_start L%d (%s)", static_cast<uint8_t>(lampId), up ? "UP" : "DOWN");
}

void cmd_ct_stop(LampId lampId)
{
  // TODO: Update with ESP-IDF Zigbee API
  // ZigbeeSwitch *lamp = getLamp(lampId);
  //
  // if (!lamp)
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Invalid lamp ID for ct_stop");
  // #endif
  //   return;
  // }
  //
  // #if !DEBUG_MODE
  //   if (!lamp->colorTempStop())
  //   {
  // #if DEBUG_MODE
  //     ESP_LOGE(TAG, "Failed to send color temp stop command to lamp %d", static_cast<uint8_t>(lampId));
  // #endif
  //   }
  // #else
  //   lamp->colorTempStop();
  // #endif

  ESP_LOGI(TAG, "cmd_ct_stop L%d", static_cast<uint8_t>(lampId));
}

void cmd_empty_action(LampId lampId)
{
}

/* ===================== Pairing ===================== */
void enter_pairing_mode(LampId lampId)
{
  (void)lampId; // Unused parameter

  pairing_mode = true;
  pairing_deadline_ms = millis() + (STEER_SECONDS * 1000UL);

#if DEBUG_MODE
  ESP_LOGI(TAG, "[PAIRING] Entering pairing mode");
#endif

  // TODO: Update with ESP-IDF Zigbee API
  // Wipe network state and re-start as End Device
  // Zigbee.factoryReset();
  // delay(ZIGBEE_REINIT_DELAY_MS);
  //
  // if (!Zigbee.begin(ZIGBEE_END_DEVICE))
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Failed to restart Zigbee after factory reset");
  // #endif
  //   pairing_mode = false;
  //   return;
  // }
  //
  // // Re-register endpoints
  // Zigbee.addEndpoint(&lamp1);
  // Zigbee.addEndpoint(&lamp2);
  // Zigbee.addEndpoint(&lamp3);
  //
  // // Open network (steering window)
  // if (!Zigbee.startSteering(STEER_SECONDS))
  // {
  // #if DEBUG_MODE
  //   ESP_LOGE(TAG, "Failed to start steering");
  // #endif
  //   pairing_mode = false;
  // }
}

/* ===================== Sleep ===================== */
void goto_sleep()
{
#if DEBUG_MODE
  delay(10); // Don't sleep in debug mode
#else
  const uint64_t us_per_hour = 3600ULL * 1000000ULL;
  const uint64_t sleep_duration_us = (uint64_t)PING_INTERVAL_HOURS * us_per_hour;

  // Wake sources: GPIO (buttons) + timer
  esp_deep_sleep_enable_gpio_wakeup(
      (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
      ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_enable_timer_wakeup(sleep_duration_us);

  // Give stack time to flush TX
  // TODO: Update with ESP-IDF Zigbee API
  uint32_t t0 = millis();
  while (millis() - t0 < ZIGBEE_FLUSH_DELAY_MS)
  {
    // Zigbee.run();
    delay(ZIGBEE_FLUSH_INTERVAL_MS);
  }

  ESP_LOGI(TAG, "Entering deep sleep");
  esp_deep_sleep_start();
#endif
}

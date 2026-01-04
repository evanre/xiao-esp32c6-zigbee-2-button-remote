#include "zigbee.h"
#include "debug.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "ha/esp_zigbee_ha_standard.h"

#define TAG "ZIGBEE"

// Global storage
nvs_handle_t g_nvs_handle;
bool dir_up = true;
bool woke_by_timer = false;
bool pairing_mode = false;
uint32_t pairing_deadline_ms = 0;
bool zigbee_connected = false;
TaskHandle_t zigbee_task_handle = NULL;
TaskHandle_t led_task_handle = NULL;
SemaphoreHandle_t zigbee_state_mutex = NULL;

static inline bool pinRead(gpio_num_t pin) { return gpio_get_level(pin) == 1; }

// ADC handle for battery reading
static adc_oneshot_unit_handle_t adc_handle = NULL;

/* ===================== LED Blink Task ===================== */
// Task that blinks LED while in pairing mode
void led_blink_task(void *pvParameters)
{
  (void)pvParameters;
  const uint32_t blink_interval_ms = 1000; // Blink every 1000ms (16 Hz)
  bool led_state = false;

  ESP_LOGI(TAG, "LED blink task started");

  while (true) {
    if (get_pairing_mode()) {
      // In pairing mode - blink the LED
      led_state = !led_state;
      gpio_set_level(LED_PIN, led_state ? 1 : 0);
      vTaskDelay(pdMS_TO_TICKS(blink_interval_ms));
    } else {
      // Not in pairing mode - turn LED off
      gpio_set_level(LED_PIN, 0);
      led_state = false;
      // Check less frequently when not pairing
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

/* ===================== Retry Pairing Wrapper ===================== */
// Wrapper to avoid function pointer type warning
static void retry_steering(uint8_t param)
{
  (void)param;  // Unused
  DEBUG_LOG_ZIGBEE(TAG, "Retrying network steering...");
  esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
}

/* ===================== ZCL/ZDO Callback Handler ===================== */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
  esp_err_t ret = ESP_OK;

  switch (callback_id) {
  case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
    DEBUG_LOG_ZIGBEE(TAG, "Attribute write received");
    ret = ESP_OK;
    break;

  case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
    DEBUG_LOG_ZIGBEE(TAG, "Read attribute response");
    ret = ESP_OK;
    break;

  case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
    DEBUG_LOG_ZIGBEE(TAG, "Default response received");
    ret = ESP_OK;
    break;

  default:
    DEBUG_LOG_ZIGBEE(TAG, "Callback: 0x%04x", callback_id);
    ret = ESP_OK;
    break;
  }

  return ret;
}

/* ===================== Zigbee Signal Handler ===================== */
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;

  switch (sig_type) {
  case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
    ESP_LOGI(TAG, "Zigbee stack initialized");
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
    break;

  case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
  case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
      ESP_LOGI(TAG, "Device started up in %s factory-reset mode",
        esp_zb_bdb_is_factory_new() ? "" : "non");
      if (esp_zb_bdb_is_factory_new()) {
        ESP_LOGI(TAG, "Factory-new device - starting network steering for pairing");
        set_pairing_mode(true);
        set_pairing_deadline(millis() + (STEER_SECONDS * 1000UL));
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGI(TAG, "Device rebooted with existing network config");
      }
    } else {
      ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)",
        esp_err_to_name(err_status));
    }
    break;

  case ESP_ZB_BDB_SIGNAL_STEERING:
    if (err_status == ESP_OK) {
      esp_zb_ieee_addr_t extended_pan_id;
      esp_zb_get_extended_pan_id(extended_pan_id);
      ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
        extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
        extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
        esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
      set_zigbee_connected(true);
      set_pairing_mode(false);
      ESP_LOGI(TAG, "Device connected - LED blink task will turn off LED");
    } else {
      ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
      // Keep retrying during pairing window only
      if (get_pairing_mode()) {
        esp_zb_scheduler_alarm(retry_steering, 0, 1000);
      }
    }
    break;

  case ESP_ZB_ZDO_SIGNAL_LEAVE:
    ESP_LOGI(TAG, "Device left network - waiting for manual pairing sequence");
    set_zigbee_connected(false);
    // Do NOT automatically enter pairing mode
    // User must manually trigger pairing with PAIRING_CLICKS (6+ rapid clicks)
    break;

  default:
    // Log all other signals (including binding-related ones)
    const char* signal_name = esp_zb_zdo_signal_to_string(sig_type);

    // Always log signal in normal mode
    ESP_LOGI(TAG, "[SIGNAL] %s (0x%04x), status: %s",
      signal_name, sig_type, esp_err_to_name(err_status));

    // Enhanced debug logging for binding operations
    if (sig_type == 0x0021) {  // ESP_ZB_ZDO_SIGNAL_BIND
      DEBUG_LOG_BINDING(TAG, "Binding request received!");
      ESP_LOGI(TAG, "  └─ Binding operation detected!");
    } else if (sig_type == 0x0022) {  // ESP_ZB_ZDO_SIGNAL_UNBIND
      DEBUG_LOG_BINDING(TAG, "Unbinding request received!");
      ESP_LOGI(TAG, "  └─ Unbinding operation detected!");
    }

    // Additional debug info for any signal
    DEBUG_LOG_ZIGBEE(TAG, "Signal detail - Type: 0x%04x, Status: %s", sig_type, esp_err_to_name(err_status));
    break;
  }
}

/* ===================== Create Switch Cluster List ===================== */
static esp_zb_cluster_list_t* create_switch_cluster_list(uint8_t endpoint_id)
{
  // Create cluster list with CLIENT role clusters
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

  // Basic cluster (server role - provides device info)
  // Zigbee ZCL strings must be length-prefixed: first byte = length, then string
  // Format: "\xNN""string" where NN is hex length of string
  static const char manufacturer[] = "\x03""DIY";           // 3 characters
  static const char model[] = "\x0c""TwoBtnRemote";          // 12 characters

  // Create basic cluster with proper config including power source
  esp_zb_basic_cluster_cfg_t basic_cfg = {
    .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
    .power_source = 0x03,  // 0x03 = Battery (see ZCL spec)
  };
  esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);

  // Add manufacturer name and model identifier as ZCL string attributes
  ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster,
    ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)manufacturer));
  ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster,
    ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)model));

  ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

  // Identify cluster (server role)
  esp_zb_cluster_list_add_identify_cluster(cluster_list,
    esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  // Power Configuration cluster (server role - only on endpoint 1)
  if (endpoint_id == EP_L1) {
    esp_zb_attribute_list_t *power_config_cluster = esp_zb_power_config_cluster_create(NULL);
    // Add battery voltage attribute (0x0020) - in tenths of volts
    static uint8_t battery_voltage = 30;  // Default: 3.0V (static for persistence)
    esp_zb_power_config_cluster_add_attr(power_config_cluster, 0x0020, &battery_voltage);
    // Add battery percentage attribute (0x0021) - in half-percent units (200 = 100%)
    static uint8_t battery_percentage = 200;  // Default: 100% (static for persistence)
    esp_zb_power_config_cluster_add_attr(power_config_cluster, 0x0021, &battery_percentage);
    esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_config_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  }

  // On/Off cluster (CLIENT role - sends commands)
  esp_zb_cluster_list_add_on_off_cluster(cluster_list,
    esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

  // Level Control cluster (CLIENT role - sends commands)
  esp_zb_cluster_list_add_level_cluster(cluster_list,
    esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

  // Color Control cluster (CLIENT role - sends commands)
  esp_zb_cluster_list_add_color_control_cluster(cluster_list,
    esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

  return cluster_list;
}

/* ===================== Zigbee Task Entry ===================== */
void zigbee_task_entry(void *pvParameters)
{
  (void)pvParameters; // Unused parameter

  ESP_LOGI(TAG, "Zigbee task starting");

  // Call the blocking zigbeeInit() - this will run the Zigbee stack main loop
  zigbeeInit();

  // If we ever return from zigbeeInit (which shouldn't happen), delete the task
  ESP_LOGE(TAG, "Zigbee task unexpectedly exited");
  vTaskDelete(NULL);
}

/* ===================== Zigbee init ===================== */
void zigbeeInit()
{
  // Create mutex for thread synchronization
  if (zigbee_state_mutex == NULL) {
    zigbee_state_mutex = xSemaphoreCreateMutex();
    if (zigbee_state_mutex == NULL) {
      ESP_LOGE(TAG, "Failed to create zigbee_state_mutex");
    }
  }

  // Enable verbose Zigbee logging in debug mode
  if (g_debug_mode) {
    esp_log_level_set("ESP_ZB_APS", ESP_LOG_DEBUG);  // APS layer (includes binding)
    esp_log_level_set("ESP_ZB_ZDO", ESP_LOG_DEBUG);  // ZDO layer (includes binding requests)
    esp_log_level_set("ESP_ZB_ZCL", ESP_LOG_DEBUG);  // ZCL layer (includes commands)
    DEBUG_LOG_ZIGBEE(TAG, "Verbose Zigbee stack logging enabled");
  }

  // Initialize ADC for battery reading first
  adc_oneshot_unit_init_cfg_t adc_config = {
    .unit_id = VBAT_ADC_UNIT,
    .clk_src = (adc_oneshot_clk_src_t)0,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_handle));

  adc_oneshot_chan_cfg_t chan_config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, VBAT_ADC_CHANNEL, &chan_config));

  // Initialize LED pin for pairing indication
  gpio_config_t led_config = {
    .pin_bit_mask = (1ULL << LED_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&led_config);
  gpio_set_level(LED_PIN, 0); // LED off initially

  // Start LED blink task (runs in background, blinks during pairing)
  xTaskCreate(
    led_blink_task,           // Task function
    "led_blink",              // Task name
    2048,                     // Stack size (bytes)
    NULL,                     // Parameters
    5,                        // Priority
    &led_task_handle          // Task handle
  );
  ESP_LOGI(TAG, "LED blink task created");

  ESP_LOGI(TAG, "Initializing Zigbee stack");

  // Configure Zigbee platform
  esp_zb_platform_config_t platform_config = {
    .radio_config = {
      .radio_mode = ZB_RADIO_MODE_NATIVE,
      .radio_uart_config = {},  // Not used in native mode
    },
    .host_config = {
      .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
      .host_uart_config = {},  // Not used
    },
  };
  ESP_ERROR_CHECK(esp_zb_platform_config(&platform_config));

  // Configure network as End Device
  esp_zb_cfg_t zigbee_config = {
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
    .install_code_policy = false,
    .nwk_cfg = {
      .zed_cfg = {
        .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
        .keep_alive = 3000,
      },
    },
  };

  // Initialize Zigbee stack
  esp_zb_init(&zigbee_config);

  // Create endpoint list and add all 3 endpoints
  esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

  // Endpoint 1: Button 1 -> Lamp 1 (with battery reporting)
  esp_zb_cluster_list_t *cluster_list_1 = create_switch_cluster_list(EP_L1);
  esp_zb_endpoint_config_t endpoint_config_1 = {
    .endpoint = EP_L1,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0,
  };
  esp_zb_ep_list_add_ep(ep_list, cluster_list_1, endpoint_config_1);

  // Endpoint 2: Button 2 -> Lamp 2
  esp_zb_cluster_list_t *cluster_list_2 = create_switch_cluster_list(EP_L2);
  esp_zb_endpoint_config_t endpoint_config_2 = {
    .endpoint = EP_L2,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0,
  };
  esp_zb_ep_list_add_ep(ep_list, cluster_list_2, endpoint_config_2);

  // Endpoint 3: Both buttons -> Lamp 3
  esp_zb_cluster_list_t *cluster_list_3 = create_switch_cluster_list(EP_L3);
  esp_zb_endpoint_config_t endpoint_config_3 = {
    .endpoint = EP_L3,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0,
  };
  esp_zb_ep_list_add_ep(ep_list, cluster_list_3, endpoint_config_3);

  // Register device
  esp_zb_device_register(ep_list);

  // Register action callback handler for ZCL/ZDO operations (including binding)
  esp_zb_core_action_handler_register(zb_action_handler);
  esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

  // Start Zigbee stack
  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_stack_main_loop();

  ESP_LOGI(TAG, "Zigbee stack started");
}

/* ===================== Thread-Safe Accessors ===================== */
bool get_zigbee_connected()
{
  if (zigbee_state_mutex == NULL) {
    return zigbee_connected; // Fallback if mutex not initialized
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  bool result = zigbee_connected;
  xSemaphoreGive(zigbee_state_mutex);
  return result;
}

void set_zigbee_connected(bool connected)
{
  if (zigbee_state_mutex == NULL) {
    zigbee_connected = connected; // Fallback if mutex not initialized
    return;
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  zigbee_connected = connected;
  xSemaphoreGive(zigbee_state_mutex);
}

bool get_pairing_mode()
{
  if (zigbee_state_mutex == NULL) {
    return pairing_mode; // Fallback if mutex not initialized
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  bool result = pairing_mode;
  xSemaphoreGive(zigbee_state_mutex);
  return result;
}

void set_pairing_mode(bool mode)
{
  if (zigbee_state_mutex == NULL) {
    pairing_mode = mode; // Fallback if mutex not initialized
    return;
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  pairing_mode = mode;
  xSemaphoreGive(zigbee_state_mutex);
}

uint32_t get_pairing_deadline()
{
  if (zigbee_state_mutex == NULL) {
    return pairing_deadline_ms; // Fallback if mutex not initialized
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  uint32_t result = pairing_deadline_ms;
  xSemaphoreGive(zigbee_state_mutex);
  return result;
}

void set_pairing_deadline(uint32_t deadline)
{
  if (zigbee_state_mutex == NULL) {
    pairing_deadline_ms = deadline; // Fallback if mutex not initialized
    return;
  }
  xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  pairing_deadline_ms = deadline;
  xSemaphoreGive(zigbee_state_mutex);
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
    ESP_LOGE(TAG, "Battery LUT too small");
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
      ESP_LOGE(TAG, "Battery LUT bounds error");
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

  // Power Configuration cluster (0x0001):
  //  - BatteryVoltage (0x0020): tenths of volts (e.g. 3.95V -> 39)
  //  - BatteryPercentageRemaining (0x0021): half-percent units (87% -> 174)
  uint8_t zcl_voltage = (uint8_t)((mv + ZCL_VOLTAGE_OFFSET) / ZCL_VOLTAGE_SCALE);
  uint8_t zcl_pct = (uint8_t)(std::min<uint16_t>(pct * ZCL_PERCENTAGE_SCALE, ZCL_PERCENTAGE_MAX));

  ESP_LOGI(TAG, "[BATTERY] Reporting: %dmV (%d%%) -> ZCL: voltage=%d, pct=%d", mv, pct, zcl_voltage, zcl_pct);
  DEBUG_LOG_ZIGBEE(TAG, "Battery report - Raw: %dmV, Percentage: %d%%, ZCL voltage: %d, ZCL pct: %d",
                   mv, pct, zcl_voltage, zcl_pct);

  // Update battery voltage attribute (0x0020)
  esp_err_t err = esp_zb_zcl_set_attribute_val(
    EP_L1,
    ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    0x0020,  // ZCL_POWER_CONFIG_BATTERY_VOLTAGE attribute
    &zcl_voltage,
    false  // Don't check access rights
  );
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set battery voltage attribute: %s", esp_err_to_name(err));
  }

  // Update battery percentage attribute (0x0021)
  err = esp_zb_zcl_set_attribute_val(
    EP_L1,
    ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    0x0021,  // ZCL_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING attribute
    &zcl_pct,
    false  // Don't check access rights
  );
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set battery percentage attribute: %s", esp_err_to_name(err));
  }

  ESP_LOGI(TAG, "[BATTERY] Attributes updated successfully");
}

/* ===================== Helpers ====================== */
bool get_dir()
{
  // Protect dir_up access with mutex
  if (zigbee_state_mutex != NULL) {
    xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
  }

  dir_up = !dir_up;
  uint8_t val = dir_up ? 1 : 0;
  bool result = dir_up;

  if (zigbee_state_mutex != NULL) {
    xSemaphoreGive(zigbee_state_mutex);
  }

  esp_err_t err = nvs_set_u8(g_nvs_handle, "dir_up", val);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save direction: %s", esp_err_to_name(err));
  } else {
    nvs_commit(g_nvs_handle);
  }

  return result;
}

/* ===================== Commands ===================== */
void cmd_toggle(LampId lampId)
{
  uint8_t endpoint = static_cast<uint8_t>(lampId);

  // Send On/Off Toggle command via direct binding
  esp_zb_zcl_on_off_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT; // Use binding
  cmd.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;

  ESP_LOGI(TAG, "Sending toggle from endpoint %d", endpoint);
  DEBUG_LOG_ZIGBEE(TAG, "ZCL Command - Toggle (cluster: OnOff, endpoint: %d)", endpoint);
  esp_zb_zcl_on_off_cmd_req(&cmd);
}

void cmd_level_start(LampId lampId)
{
  bool up = get_dir();
  uint8_t endpoint = static_cast<uint8_t>(lampId);

  // Send Level Control Move command via direct binding
  esp_zb_zcl_level_move_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT; // Use binding
  cmd.move_mode = up ? 0x00 : 0x01; // 0=up, 1=down
  cmd.rate = 50; // Rate of level change (units per second)

  ESP_LOGI(TAG, "Sending level move %s from endpoint %d", up ? "UP" : "DOWN", endpoint);
  DEBUG_LOG_ZIGBEE(TAG, "ZCL Command - Level Move %s (cluster: LevelControl, endpoint: %d, rate: %d)",
                   up ? "UP" : "DOWN", endpoint, cmd.rate);
  esp_zb_zcl_level_move_cmd_req(&cmd);
}

void cmd_level_stop(LampId lampId)
{
  uint8_t endpoint = static_cast<uint8_t>(lampId);

  // Send Level Control Stop command via direct binding
  esp_zb_zcl_level_stop_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT; // Use binding

  ESP_LOGI(TAG, "Sending level stop from endpoint %d", endpoint);
  DEBUG_LOG_ZIGBEE(TAG, "ZCL Command - Level Stop (cluster: LevelControl, endpoint: %d)", endpoint);
  esp_zb_zcl_level_stop_cmd_req(&cmd);
}

void cmd_ct_start(LampId lampId)
{
  bool up = get_dir();
  uint8_t endpoint = static_cast<uint8_t>(lampId);

  // Send Color Control Move Color Temperature command via direct binding
  esp_zb_zcl_color_move_color_temperature_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT; // Use binding
  cmd.move_mode = up ? 0x01 : 0x03; // 1=up (cooler), 3=down (warmer)
  cmd.rate = 50; // Rate of color temp change (units per second)
  cmd.color_temperature_minimum = 153; // Coolest (from lamp spec)
  cmd.color_temperature_maximum = 370; // Warmest (from lamp spec)

  ESP_LOGI(TAG, "Sending color temp move %s from endpoint %d", up ? "UP" : "DOWN", endpoint);
  DEBUG_LOG_ZIGBEE(TAG, "ZCL Command - Color Temp Move %s (cluster: ColorControl, endpoint: %d, rate: %d, min: %d, max: %d)",
                   up ? "UP (cooler)" : "DOWN (warmer)", endpoint, cmd.rate, cmd.color_temperature_minimum, cmd.color_temperature_maximum);
  esp_zb_zcl_color_move_color_temperature_cmd_req(&cmd);
}

void cmd_ct_stop(LampId lampId)
{
  uint8_t endpoint = static_cast<uint8_t>(lampId);

  // Send Color Control Stop Move Step command via direct binding
  esp_zb_zcl_color_stop_move_step_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = endpoint;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT; // Use binding

  ESP_LOGI(TAG, "Sending color temp stop from endpoint %d", endpoint);
  DEBUG_LOG_ZIGBEE(TAG, "ZCL Command - Color Temp Stop (cluster: ColorControl, endpoint: %d)", endpoint);
  esp_zb_zcl_color_stop_move_step_cmd_req(&cmd);
}

void cmd_empty_action(LampId lampId)
{
}

/* ===================== Pairing ===================== */
void enter_pairing_mode(LampId lampId)
{
  (void)lampId; // Unused parameter

  ESP_LOGI(TAG, "[PAIRING] PAIRING_SEQUENCE detected - entering pairing mode");
  DEBUG_LOG_ZIGBEE(TAG, "Pairing mode activated - factory reset will be performed");

  set_pairing_mode(true);
  set_pairing_deadline(millis() + (STEER_SECONDS * 1000UL));

  ESP_LOGI(TAG, "[PAIRING] LED will start blinking in main loop");

  // Factory reset - clear network settings
  // This will trigger ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START which will start steering
  ESP_LOGI(TAG, "[PAIRING] Performing factory reset...");
  DEBUG_LOG_ZIGBEE(TAG, "Factory reset - clearing Zigbee network configuration");
  esp_zb_factory_reset();
  ESP_LOGI(TAG, "[PAIRING] Factory reset scheduled - device will restart and enter pairing");
}

/* ===================== Sleep ===================== */
void goto_sleep()
{
  // Debug mode: never sleep (device stays awake for continuous logging)
  if (g_debug_mode) {
    // Sleep skipped in debug mode - don't log (called every 10ms, would spam console)
    delay(10);
    return;
  }

  const uint64_t us_per_hour = 3600ULL * 1000000ULL;
  const uint64_t sleep_duration_us = (uint64_t)PING_INTERVAL_HOURS * us_per_hour;

  // Wake sources: GPIO (buttons) + timer
  esp_deep_sleep_enable_gpio_wakeup(
      (1ULL << BTN1_PIN) | (1ULL << BTN2_PIN),
      ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_sleep_enable_timer_wakeup(sleep_duration_us);

  // Small delay for any pending TX
  delay(ZIGBEE_FLUSH_DELAY_MS);

  ESP_LOGI(TAG, "Entering deep sleep (wake on button or %dh timer)", PING_INTERVAL_HOURS);
  esp_deep_sleep_start();
}

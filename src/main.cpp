#include "constants.h"
#include "buttons.h"
#include "router.h"
#include "zigbee.h"

#define TAG "MAIN"

// Last pairing loop time for non-blocking delay
static uint32_t last_pairing_loop_ms = 0;

extern "C" void app_main()
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Open NVS handle
  ret = nvs_open("zb-remote", NVS_READWRITE, &g_nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
  }

  // Read direction flag
  uint8_t dir_val = 1;
  ret = nvs_get_u8(g_nvs_handle, "dir_up", &dir_val);
  dir_up = (dir_val != 0);

  // Initialize buttons first (needed for GPIO)
  initButtons();

  // Heuristic wake source
  woke_by_timer = (gpio_get_level(BTN1_PIN) == 1 && gpio_get_level(BTN2_PIN) == 1);

#if DEBUG_MODE
  esp_log_level_set("*", ESP_LOG_INFO);
  delay(500);
  ESP_LOGI(TAG, "=== DEBUG MODE: Gestures test (no Zigbee) ===");
  ESP_LOGI(TAG, "Auto-reset enabled! Upload should work without BOOT button now.");
  ESP_LOGI(TAG, "BTN1_PIN=%d, BTN2_PIN=%d", BTN1_PIN, BTN2_PIN);
  ESP_LOGI(TAG, "Waiting for button presses...");
#else
  // Start Zigbee stack and register endpoints
  zigbeeInit();

  // Timer wake → report battery and sleep
  if (woke_by_timer)
  {
    uint16_t mv = read_battery_mv();
    // Only report battery if connected, otherwise just sleep
    if (zigbee_connected)
    {
      report_battery(mv);
    }
    goto_sleep(); // no return
  }

  // If not connected after timeout and not pairing, try to sleep and retry later
  if (!zigbee_connected && !pairing_mode)
  {
    // Connection failed, sleep and retry on next button press or timer wake
    goto_sleep();
  }
#endif

  // Main loop
  while (true) {
    // Keep awake during pairing window with LED blinking
    if (pairing_mode)
    {
      uint32_t now = millis();

      if (now < pairing_deadline_ms)
      {
        // Non-blocking delay: blink LED every 500ms to indicate pairing mode
        if (now - last_pairing_loop_ms >= PAIRING_LOOP_DELAY_MS)
        {
          last_pairing_loop_ms = now;
          // Toggle LED for visual feedback
          static bool led_state = false;
          led_state = !led_state;
          gpio_set_level(LED_PIN, led_state ? 1 : 0);
        }
        delay(10);
        continue;
      }
      else
      {
        // Pairing timeout - turn off LED and sleep
        pairing_mode = false;
        gpio_set_level(LED_PIN, 0);
        goto_sleep();
      }
    }

    ButtonEvent ev1 = handleButton(b1);
    ButtonEvent ev2 = handleButton(b2);
    routeEvents(ev1, ev2);

#if !DEBUG_MODE
    goto_sleep();
#else
    delay(10);  // Small delay in debug mode to not spin too fast
#endif
  }
}

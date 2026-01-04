#pragma once

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"

/* ===================== Debug Mode (Runtime Detection) ===================== */
// Debug mode is now runtime-detected via BOOT button at power-on
// When active:
//   - Disables sleep (device stays awake)
//   - Enables verbose logging (button events, Zigbee activity, binding)
//   - Zigbee ALWAYS initializes (debug is observational only)
extern bool g_debug_mode;  // Global debug state (set during boot)

/* ===================== Debug Pin ===================== */
constexpr gpio_num_t BOOT_PIN = GPIO_NUM_9;  // XIAO ESP32-C6: BOOT button on GPIO9

/* ===================== Debug Logging Macros ===================== */
// Debug logging macros - log only when debug mode is active
#define DEBUG_LOG_BUTTON(tag, fmt, ...) \
  do { if (g_debug_mode) ESP_LOGI(tag, "[BTN] " fmt, ##__VA_ARGS__); } while(0)

#define DEBUG_LOG_ZIGBEE(tag, fmt, ...) \
  do { if (g_debug_mode) ESP_LOGI(tag, "[ZB] " fmt, ##__VA_ARGS__); } while(0)

#define DEBUG_LOG_BINDING(tag, fmt, ...) \
  do { if (g_debug_mode) ESP_LOGI(tag, "[BIND] " fmt, ##__VA_ARGS__); } while(0)

#define DEBUG_LOG_ROUTER(tag, fmt, ...) \
  do { if (g_debug_mode) ESP_LOGI(tag, "[ROUTE] " fmt, ##__VA_ARGS__); } while(0)

/* ===================== Debug Helper Functions ===================== */
// Detects if BOOT button is pressed at power-on
bool detect_debug_mode();

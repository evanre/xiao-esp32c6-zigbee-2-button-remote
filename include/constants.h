#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <algorithm>
#include "nvs_flash.h"
#include "nvs.h"
// TODO: Add ESP-IDF Zigbee SDK support
// #include "esp_zigbee_core.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ===================== Debug Mode ===================== */
#define DEBUG_MODE 1 // Set to 1 to test gestures without Zigbee (prints to Serial), 0 to enable Zigbee

/* ===================== Pins ===================== */
constexpr gpio_num_t BTN1_PIN = GPIO_NUM_1;  // XIAO ESP32-C6: D1 = GPIO1
constexpr gpio_num_t BTN2_PIN = GPIO_NUM_2;  // XIAO ESP32-C6: D2 = GPIO2
constexpr adc_channel_t VBAT_ADC_CHANNEL = ADC_CHANNEL_0; // Battery voltage sense pin (adjust to match your board)
constexpr adc_unit_t VBAT_ADC_UNIT = ADC_UNIT_1; // ADC unit for battery monitoring

/* ===================== Timings (ms) ===================== */
constexpr uint16_t DEBOUNCE_MS = 25;    // Button debounce time
constexpr uint16_t COMBO_MS = 80;       // Max interval to detect both buttons pressed
constexpr uint16_t PRESS_MS = 300;      // Max interval between multiple short clicks
constexpr uint16_t HOLD_MS = 600;       // Min press duration to become "long hold"
constexpr uint32_t MAX_HOLD_MS = 30000; // Safety cutoff for very long holds
constexpr uint8_t PAIRING_CLICKS = 6;   // Secret knock threshold

/* ===================== Endpoints ===================== */
constexpr int EP_L1 = 1; // Endpoint 1: controlled by Button 1
constexpr int EP_L2 = 2; // Endpoint 2: controlled by Button 2
constexpr int EP_L3 = 3; // Endpoint 3: controlled by both buttons simultaneously

/* ===================== Battery ===================== */
// Battery voltage calculation: Vbat = Vadc * VBAT_DIVIDER * ADC_CAL_K
constexpr float VBAT_DIVIDER = 2.00f;      // Hardware voltage divider ratio (adjust per schematic)
constexpr float ADC_CAL_K = 1.00f;         // Fine calibration factor (measure & tune)
constexpr uint8_t PING_INTERVAL_HOURS = 6; // Wake interval for battery report (maintains network presence)

/* ===================== Pairing ===================== */
constexpr uint8_t STEER_SECONDS = 180;    // Network steering duration (device discovery window)
constexpr uint16_t PAIRING_LOOP_DELAY_MS = 50; // Non-blocking delay for pairing mode loop

/* ===================== Zigbee Constants ===================== */
constexpr uint16_t ZIGBEE_CONNECT_TIMEOUT_MS = 8000; // Max wait time for Zigbee connection
constexpr uint16_t ZIGBEE_FLUSH_DELAY_MS = 50;       // Time to flush Zigbee TX before sleep
constexpr uint16_t ZIGBEE_FLUSH_INTERVAL_MS = 5;     // Interval between Zigbee.run() calls during flush
constexpr uint16_t ZIGBEE_REINIT_DELAY_MS = 200;     // Delay after factory reset before reinit

/* ===================== Battery ZCL Constants ===================== */
constexpr uint16_t ZCL_VOLTAGE_SCALE = 100;    // ZCL BatteryVoltage units (tenths of volts)
constexpr uint8_t ZCL_VOLTAGE_OFFSET = 50;     // Rounding offset for voltage conversion
constexpr uint8_t ZCL_PERCENTAGE_SCALE = 2;    // ZCL percentage scale (half-percent units)
constexpr uint8_t ZCL_PERCENTAGE_MAX = 200;    // ZCL max percentage value (100% * 2)

/* ===================== ADC Constants ===================== */
constexpr uint16_t ADC_MAX_VALUE = 4095;       // 12-bit ADC maximum value
constexpr uint16_t ADC_REF_MV = 3300;          // ADC reference voltage (mV)
constexpr uint16_t VBAT_MIN_MV = 0;            // Minimum valid battery voltage (mV)
constexpr uint16_t VBAT_MAX_MV = 5000;         // Maximum valid battery voltage (mV)

/* ===================== Global state ===================== */
extern nvs_handle_t g_nvs_handle;
extern bool dir_up;        // true=up, false=down (toggles after each hold_stop)
extern bool woke_by_timer; // heuristic: timer wake vs button wake
extern bool pairing_mode;  // active pairing window
extern uint32_t pairing_deadline_ms;

/* ===================== Zigbee endpoints (clients) ===================== */
// TODO: Update with ESP-IDF Zigbee API
// extern ZigbeeSwitch lamp1; // EP_L1
// extern ZigbeeSwitch lamp2; // EP_L2
// extern ZigbeeSwitch lamp3; // EP_L3
enum class LampId : uint8_t
{
  L1 = EP_L1,
  L2 = EP_L2,
  L3 = EP_L3
};

/* ===================== Buttons API ===================== */
enum class ButtonEvent : uint8_t
{
  NONE = 0,
  CLICK,
  DOUBLE_CLICK,
  TRIPLE_CLICK,     // used for 3..(PAIRING_CLICKS-1) clicks
  PAIRING_SEQUENCE, // >= PAIRING_CLICKS
  HOLD_START,
  HOLD_END,
  CLICK_HOLD_START,
  CLICK_HOLD_END
};

enum class ButtonModeEnum : uint8_t
{
  IDLE,
  HOLDING,
  CLICK_HOLDING,
  STUCK
};

struct BtnState
{
  gpio_num_t pin;
  ButtonModeEnum state = ButtonModeEnum::IDLE;
  uint32_t t_press = 0;
  uint32_t t_release = 0;
  uint8_t clicks = 0;

  explicit BtnState(gpio_num_t p) : pin(p) {}
};

extern BtnState b1;
extern BtnState b2;

void initButtons();

ButtonEvent handleButton(BtnState &b);

/* ===================== Router API ===================== */
struct EventRouter
{
  ButtonEvent ev1_pending = ButtonEvent::NONE;
  ButtonEvent ev2_pending = ButtonEvent::NONE;
  bool combo_holding = false;
  uint32_t t_start = 0;
};

extern EventRouter router;

void routeEvents(ButtonEvent ev1, ButtonEvent ev2);

void sendAction(LampId lamp, ButtonEvent ev);

/* ===================== Zigbee / control API ===================== */
void zigbeeInit();

// On/Off, Level, Color Temperature commands
void cmd_toggle(LampId lampId);
void cmd_level_start(LampId lampId);
void cmd_level_stop(LampId lampId);
void cmd_ct_start(LampId lampId);
void cmd_ct_stop(LampId lampId);
void cmd_empty_action(LampId lampId);

// Battery helpers
uint16_t read_battery_mv();
uint8_t vbat_percent(uint16_t mv);
void report_battery(uint16_t mv);

// Pairing and sleep
void enter_pairing_mode();
void goto_sleep();

/* ===================== Helper functions ===================== */
// Timing helper (replaces millis())
static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// Delay helper (replaces delay())
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

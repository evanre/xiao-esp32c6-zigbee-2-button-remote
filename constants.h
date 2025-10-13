#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Zigbee.h>

/* ===================== Pins ===================== */
static int BTN1_PIN = 2;        // XIAO ESP32-C6: D1 = GPIO2 (controls Lamp 1)
static int BTN2_PIN = 3;        // XIAO ESP32-C6: D2 = GPIO3 (controls Lamp 2)
static int VBAT_ADC_PIN = A0;   // Battery voltage sense pin (adjust to match your board)

/* ===================== Timings (ms) ===================== */
#define DEBOUNCE_MS        25   // Button debounce time
#define DOUBLE_MS         300   // Max interval to detect double-click
#define LONG_MS           600   // Min press duration to count as "long hold"

/* ===================== Endpoints ===================== */
#define EP_L1  1   // Endpoint 1: controlled by Button 1
#define EP_L2  2   // Endpoint 2: controlled by Button 2
#define EP_L3  3   // Endpoint 3: controlled by both buttons simultaneously

/* ===================== Battery ===================== */
// Battery voltage calculation: Vbat = Vadc * VBAT_DIVIDER * ADC_CAL_K
static float VBAT_DIVIDER  = 2.00f;  // Hardware voltage divider ratio (adjust per schematic)
static float ADC_CAL_K     = 1.00f;  // Fine calibration factor (measure & tune)
#define PING_INTERVAL_HOURS 6        // Wake interval for battery report (maintains network presence)

/* ===================== Pairing ===================== */
// "Secret knock" pattern on BTN1 to enter pairing mode
#define PAIRING_CLICKS       6      // Number of quick clicks required
#define PAIRING_WINDOW_MS  5000     // Time window to complete the click sequence (ms)
#define STEER_SECONDS       180     // Network steering duration (device discovery window)

/* ===================== Global state ===================== */
extern Preferences prefs;

extern bool dir_up;               // true=up, false=down (toggles after each hold_stop)
extern bool woke_by_timer;        // heuristic: timer wake vs button wake
extern bool pairing_mode;         // active pairing window
extern uint32_t pairing_deadline_ms;

/* ===================== Zigbee endpoints (clients) ===================== */
extern ZigbeeSwitch lamp1;  // EP_L1
extern ZigbeeSwitch lamp2;  // EP_L2
extern ZigbeeSwitch lamp3;  // EP_L3

/* ===================== Button engine (shared types) ===================== */
enum ClickType { NONE, SINGLE, DOUBLE_CLICK, LONGHOLD };

struct BtnState {
  int  pin;
  bool last;        // PULLUP: HIGH=idle, LOW=pressed
  bool pressed;
  uint32_t t_down;
  uint32_t t_up;
  uint8_t  clicks;  // for double-click detection
};

// Exposed button states
extern BtnState b1, b2;

/* ===================== Buttons API ===================== */
void initButtons();
ClickType handleButton(BtnState &b);
bool bothPressedEarly();
bool detect_pairing_sequence();

/* ===================== Zigbee / control API ===================== */
void zigbeeInit();

// On/Off, Level, Color Temperature commands
void cmd_toggle(uint8_t ep);
void cmd_level_start(uint8_t ep, bool up);
void cmd_level_stop(uint8_t ep);
void cmd_ct_start(uint8_t ep, bool up);
void cmd_ct_stop(uint8_t ep);

// Battery helpers
uint16_t read_battery_mv();
uint8_t  vbat_percent(uint16_t mv);
void     report_battery(uint16_t mv);

// Target handler (per endpoint logic for click/hold/click+hold)
void handleTarget(uint8_t ep);

// Pairing and sleep
void enter_pairing_mode();
void goto_sleep();

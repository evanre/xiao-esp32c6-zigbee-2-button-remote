#pragma once
#include <Arduino.h>
// #include <Preferences.h>
#include <Zigbee.h>

/* ===================== Debug Mode ===================== */
// Set to true to test gestures without Zigbee (prints to Serial)
// constexpr bool DEBUG_MODE = true;

/* ===================== Pins ===================== */
constexpr int BTN1_PIN = 1;          // XIAO ESP32-C6: D1 = GPIO1
constexpr int BTN2_PIN = 2;          // XIAO ESP32-C6: D2 = GPIO2
constexpr uint8_t VBAT_ADC_PIN = A0; // Battery voltage sense pin (adjust to match your board)

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
// constexpr float VBAT_DIVIDER  = 2.00f; // Hardware voltage divider ratio (adjust per schematic)
// constexpr float ADC_CAL_K = 1.00f; // Fine calibration factor (measure & tune)
// constexpr int PING_INTERVAL_HOURS = 6 // Wake interval for battery report (maintains network presence)

/* ===================== Pairing ===================== */
// "Secret knock" pattern to enter pairing mode
constexpr uint16_t PAIRING_WINDOW_MS = 5000; // Time window to complete the click sequence (ms)
constexpr uint8_t STEER_SECONDS = 180;       // Network steering duration (device discovery window)

/* ===================== Global state ===================== */
// extern Preferences prefs;

extern bool dir_up;        // true=up, false=down (toggles after each hold_stop)
extern bool woke_by_timer; // heuristic: timer wake vs button wake
extern bool pairing_mode;  // active pairing window
extern uint32_t pairing_deadline_ms;

/* ===================== Zigbee endpoints (clients) ===================== */
// extern ZigbeeSwitch lamp1;  // EP_L1
// extern ZigbeeSwitch lamp2;  // EP_L2
// extern ZigbeeSwitch lamp3;  // EP_L3
enum class LampId : uint8_t
{
  L1 = 1,
  L2 = 2,
  L3 = 3
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
  int pin;
  ButtonModeEnum state = ButtonModeEnum::IDLE;
  uint32_t t_press = 0;
  uint32_t t_release = 0;
  uint8_t clicks = 0;

  explicit BtnState(int p) : pin(p) {}
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
// bool bothPressedEarly();
// bool detect_pairing_sequence();

/* ===================== Zigbee / control API ===================== */
// void zigbeeInit();

// On/Off, Level, Color Temperature commands
void cmd_toggle(uint8_t ep);
void cmd_level_start(uint8_t ep, bool up);
void cmd_level_stop(uint8_t ep);
void cmd_ct_start(uint8_t ep, bool up);
void cmd_ct_stop(uint8_t ep);

// Battery helpers
// uint16_t read_battery_mv();
// uint8_t  vbat_percent(uint16_t mv);
void report_battery(uint16_t mv);

// Pairing and sleep
// void enter_pairing_mode();
// void goto_sleep();

// Ми зчитуємо в loop() стан кнопки (HIGH/LOW) і передаємо його в функцію обробки.
// Кнопка натиснута:
// - press_timer запущений?
//   - так:
//     - press_timer < DEBOUNCE_MS → ігнор (дребезг)
//     - DEBOUNCE_MS ≤ press_timer < HOLD_MS → чекаємо (нічого не шлемо).
//     - HOLD_MS <= press_timer ≥ MAX_HOLD_MS - це довге натискання:
//       - last_state == IDLE:
//         - press_counter = 0 - відправляємо HOLD_START, last_state = HOLDING
//         - press_counter > 0 - відправляємо CLICK_HOLD_START, last_state = CLICK_HOLDING
//      - press_timer >= MAX_HOLD_MS
//        - last_state == HOLDING - відправляємо HOLD_END, last_state = STUCK
//        - last_state == CLICK_HOLDING - відправляємо CLICK_HOLD_END, last_state = STUCK
//        - зупиняємо press_timer
//   - ні:
//     - last_state != STUCK - запускаємо press_timer
// - release_timer запущений?
//   - так:
//     - зупиняємо release_timer

// Кнопка відпущена:
// - press_timer запущений?
//   - так:
//     - press_timer < DEBOUNCE_MS - ігноруємо (дрибезг)
//     - press_timer >= DEBOUNCE_MS - зупиняємо press_timer
// - last_state == HOLDING - відправляємо HOLD_END, last_state = IDLE
// - last_state == CLICK_HOLDING - відправляємо CLICK_HOLD_END, last_state = IDLE
// - last_state == IDLE:
// - release_timer запущений?
//   - так:
//     - release_timer < DEBOUNCE_MS - ігноруємо (дрибезг)
//     - release_timer >= DEBOUNCE_MS і < PRESS_MS - чекаємо
//     - release_timer >= PRESS_MS - це кінець вікна між натисканнями:
//       - press_counter == 1 - відправляємо CLICK
//       - press_counter == 2 - відправляємо DOUBLE_CLICK
//       - press_counter >= 3 i < PAIRING_CLICKS - відправляємо TRIPLE_CLICK
//       - press_counter >= PAIRING_CLICKS - відправляємо PAIRING_SEQUENCE
//       - скидаємо press_counter в 0
//   - ні:
//     - press_counter++
//     - запускаємо release_timer

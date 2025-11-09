# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a battery-powered Zigbee remote control firmware for the XIAO ESP32-C6. It uses **ESP-IDF framework** (not Arduino) and controls up to 3 Zigbee devices with 2 physical buttons through sophisticated gesture detection.

**CRITICAL**: This project was recently migrated from Arduino framework to ESP-IDF. The Zigbee functionality is currently stubbed out with TODO comments and needs ESP-IDF Zigbee SDK implementation.

## Build Commands

```bash
# Build the project
~/.platformio/penv/bin/platformio run

# Clean build
~/.platformio/penv/bin/platformio run --target clean

# Upload to device
~/.platformio/penv/bin/platformio run --target upload

# Open serial monitor
~/.platformio/penv/bin/platformio device monitor
```

## Architecture

### Core State Machine Flow

The firmware operates in a tight loop with deep sleep between button events:

1. **Wake from deep sleep** (button press or 6-hour timer)
2. **Button state machines** detect gestures (hold, click, multi-click)
3. **Event router** multiplexes 2 buttons → 3 virtual endpoints (combo detection)
4. **Zigbee commands** dispatched to appropriate lamp endpoint
5. **Deep sleep** entered to conserve battery

### Module Responsibilities

**[src/main.cpp](src/main.cpp)** - Entry point (`app_main()`)
- NVS initialization for persistent storage
- Wake source heuristic (timer vs button)
- Main loop with pairing mode handling
- Calls into button handler and event router

**[src/buttons.cpp](src/buttons.cpp)** - Button state machine
- Per-button state tracking (IDLE, HOLDING, CLICK_HOLDING, STUCK)
- Debouncing (25ms default)
- Multi-click detection (300ms window)
- Hold detection (600ms threshold, 30s safety timeout)
- Returns `ButtonEvent` enum for router

**[src/router.cpp](src/router.cpp)** - Event routing and combo detection
- Combo detection: Both buttons within 80ms → Lamp 3 endpoint
- Event buffering during combo window
- Dispatches events to Zigbee command functions
- Handles HOLD_END synchronization for combo holds

**[src/zigbee.cpp](src/zigbee.cpp)** - Zigbee control and peripherals
- ADC oneshot API for battery voltage reading
- NVS storage for direction toggle state
- Battery percentage lookup table (Li-ion discharge curve)
- Zigbee command stubs: `cmd_toggle()`, `cmd_level_start()`, `cmd_level_stop()`, `cmd_ct_start()`, `cmd_ct_stop()`
- Deep sleep configuration with GPIO + timer wakeup

**[include/constants.h](include/constants.h)** - Shared declarations
- All timing constants (debounce, combo, hold thresholds)
- Pin definitions using ESP-IDF types (`gpio_num_t`, `adc_channel_t`)
- Enums: `ButtonEvent`, `ButtonModeEnum`, `LampId`
- Structs: `BtnState`, `EventRouter`
- Helper functions: `millis()`, `delay()` (ESP-IDF wrappers)

### ESP-IDF API Usage

**GPIO**: `gpio_config()`, `gpio_get_level()` - Direct ESP-IDF GPIO driver
**ADC**: `adc_oneshot_unit_handle_t`, `adc_oneshot_read()` - One-shot ADC reads
**NVS**: `nvs_open()`, `nvs_get_u8()`, `nvs_set_u8()`, `nvs_commit()` - Persistent storage
**Timing**: `esp_timer_get_time()` for millis(), `vTaskDelay()` for delays
**Sleep**: `esp_deep_sleep_enable_gpio_wakeup()`, `esp_sleep_enable_timer_wakeup()`
**Logging**: `ESP_LOGI()`, `ESP_LOGE()` with TAG definitions

### Key Design Patterns

**Combo Detection Algorithm**:
- Both buttons report events into router's `ev1_pending`, `ev2_pending`
- 80ms window (`COMBO_MS`) to detect simultaneous press
- If events match → dispatch to Lamp 3, else dispatch to Lamp 1/2
- Special handling for HOLD events: tracks both buttons' HOLD_END

**Direction Toggle**:
- `dir_up` global persists in NVS
- Toggles each time `cmd_level_start()` or `cmd_ct_start()` called
- Allows alternating up/down without extra button

**Power Management**:
- Timer wake every 6 hours for battery report
- GPIO wake on either button LOW
- All button handling completes in < 1 second before sleep
- DEBUG_MODE disables sleep for testing

## Migration Status: Arduino → ESP-IDF

**Completed**:
- ✅ GPIO API (pinMode, digitalRead → gpio_config, gpio_get_level)
- ✅ Serial logging (Serial.print → ESP_LOGI/ESP_LOGE)
- ✅ Timing (millis, delay → esp_timer_get_time, vTaskDelay)
- ✅ NVS storage (Preferences → nvs_*)
- ✅ ADC (analogRead → adc_oneshot_read)
- ✅ Deep sleep (native ESP-IDF API)

**Pending**:
- ⚠️ **Zigbee Stack**: All Zigbee API calls commented out with `// TODO: Update with ESP-IDF Zigbee API`
  - Need to replace Arduino `Zigbee` library with ESP-IDF Zigbee SDK
  - Managed components already present in `managed_components/espressif__esp-zboss-lib/`
  - Key functions to implement:
    - `zigbeeInit()`: Initialize Zigbee end device, register 3 endpoints
    - `cmd_toggle()`, `cmd_level_*()`, `cmd_ct_*()`: Send ZCL commands
    - `report_battery()`: Power Configuration cluster reporting
    - `enter_pairing_mode()`: Factory reset + network steering

## Configuration

**Debug Mode** ([include/constants.h](include/constants.h)):
```cpp
constexpr bool DEBUG_MODE = true;  // Enable logging, disable sleep/Zigbee
```

**Timing Tuning** ([include/constants.h](include/constants.h)):
- `DEBOUNCE_MS = 25` - Increase if false triggers
- `COMBO_MS = 80` - Time window for "simultaneous" button press
- `PRESS_MS = 300` - Multi-click detection window
- `HOLD_MS = 600` - Threshold for hold vs click
- `PAIRING_CLICKS = 6` - Rapid clicks to enter pairing mode

**Battery Calibration** ([include/constants.h](include/constants.h)):
```cpp
constexpr float VBAT_DIVIDER = 2.00f;  // Hardware voltage divider ratio
constexpr float ADC_CAL_K = 1.00f;     // Fine calibration factor
```
Measure actual battery voltage, compare to reported, adjust `ADC_CAL_K = actual / reported`.

## Hardware Mapping

- **Button 1**: GPIO1, controls Lamp 1 (Endpoint 1)
- **Button 2**: GPIO2, controls Lamp 2 (Endpoint 2)
- **Both buttons**: Controls Lamp 3 (Endpoint 3)
- **Battery ADC**: ADC_CHANNEL_0 on ADC_UNIT_1 (typically A0)
- **Button logic**: Active LOW with internal pull-ups

## Common Development Workflows

### Testing Gestures Without Zigbee
1. Set `DEBUG_MODE = true` in [include/constants.h](include/constants.h)
2. Build and upload
3. Monitor serial output at 115200 baud
4. Press buttons to see event detection

### Implementing Zigbee Functionality
1. Search codebase for `// TODO: Update with ESP-IDF Zigbee API`
2. Reference ESP-IDF Zigbee examples in `esp-idf/examples/zigbee/`
3. Use managed components already installed: `espressif__esp-zboss-lib`
4. Implement endpoint registration, ZCL command sending, and attribute reporting

### Adding New Gestures
1. Define new `ButtonEvent` enum in [include/constants.h](include/constants.h)
2. Implement detection logic in `handleButton()` in [src/buttons.cpp](src/buttons.cpp)
3. Add dispatch case in `dispatch()` in [src/router.cpp](src/router.cpp)
4. Implement command function in [src/zigbee.cpp](src/zigbee.cpp)

## Important Notes

- **Global variable naming**: `g_nvs_handle` (not `nvs_handle`) to avoid name collision with deprecated typedef
- **Build warnings**: ADC and unused variable warnings are expected during development
- **Flash size**: Board configured for 2MB but sdkconfig expects 4MB (warning is benign)
- **Main entry point**: ESP-IDF uses `extern "C" void app_main()` not `setup()`/`loop()`

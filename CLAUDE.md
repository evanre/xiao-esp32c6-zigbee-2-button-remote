# CLAUDE.md

Agent instructions for working with this ESP32-C6 Zigbee remote firmware.

## Quick Reference

- **Framework**: ESP-IDF (not Arduino)
- **Hardware**: XIAO ESP32-C6
- **Status**: See [ROADMAP.md](ROADMAP.md) for current state and issues
- **User Docs**: See [README.md](README.md) for features and usage
- **Z2M Integration**: See [z2m/README.md](z2m/README.md) for Zigbee2MQTT setup

## Build Commands

```bash
~/.platformio/penv/bin/platformio run              # Build
~/.platformio/penv/bin/platformio run --target upload  # Upload
~/.platformio/penv/bin/platformio device monitor    # Monitor
```

## Architecture

### File Structure

```
src/
├── main.cpp      - Entry point (app_main), NVS init, main loop
├── buttons.cpp   - Button state machine (debounce, gestures)
├── router.cpp    - Event routing with combo detection
├── zigbee.cpp    - Zigbee stack, commands, battery, sleep
└── debug.cpp     - Debug mode detection and state

include/
├── constants.h   - Shared declarations, timing constants
└── debug.h       - Debug mode definitions and logging macros

z2m/
├── converter.js  - Zigbee2MQTT external converter (COMPLETE)
├── device-config.yaml - Alternative Z2M inline config
└── README.md     - Z2M installation guide
```

### Module Responsibilities

**src/main.cpp** - Main loop
- NVS initialization
- Wake source detection
- Button polling loop
- Calls `handleButton()` → `routeEvents()` → Zigbee commands

**src/buttons.cpp** - State machine per button
- States: IDLE, HOLDING, CLICK_HOLDING, STUCK
- Detects: CLICK, DOUBLE_CLICK, TRIPLE_CLICK, HOLD_START/END, CLICK_HOLD_START/END
- Returns `ButtonEvent` enum

**src/router.cpp** - Event multiplexing
- 2 physical buttons → 3 virtual endpoints (Lamp 1, Lamp 2, Lamp 3)
- Combo detection: both buttons within 80ms → endpoint 3
- Dispatches to Zigbee command functions

**src/zigbee.cpp** - Zigbee + peripherals
- Zigbee stack init (3 endpoints with OnOff + Level + ColorTemp clusters)
- Commands: `cmd_toggle()`, `cmd_level_start/stop()`, `cmd_ct_start/stop()`
- Battery ADC reading, percentage LUT, reporting
- Deep sleep configuration (GPIO + timer wakeup)

**include/constants.h** - Shared config
- Timing: `DEBOUNCE_MS=25`, `COMBO_MS=80`, `PRESS_MS=300`, `HOLD_MS=600`
- Pins: `BTN1_PIN=GPIO1`, `BTN2_PIN=GPIO2`
- Enums: `ButtonEvent`, `LampId`

**src/debug.cpp** - Debug mode
- Runtime debug detection via BOOT button
- Global debug state management

**include/debug.h** - Debug definitions
- Debug logging macros: `DEBUG_LOG_BUTTON()`, `DEBUG_LOG_ZIGBEE()`, etc.
- Debug mode detection function
- BOOT pin definition

### Key Design Patterns

**Combo Detection**:
- Both buttons within 80ms → dispatch to Endpoint 3 (Lamp 3)
- Otherwise dispatch to Endpoint 1/2 (Lamp 1/2)

**Direction Toggle**:
- `dir_up` persists in NVS, toggles each hold
- Allows alternating dim up/down without extra button

**Debug Mode** (Runtime Detection):
- Activated by holding BOOT button during power-on/reset
- Disables sleep (device stays awake)
- Enables verbose logging (buttons, Zigbee, binding)
- Zigbee ALWAYS initializes (debug is observational only)

**Power Management**:
- Deep sleep between button presses
- Wake on button GPIO or 6-hour timer (battery report)
- Debug mode disables sleep for continuous testing

### ESP-IDF API Usage

- **GPIO**: `gpio_config()`, `gpio_get_level()`
- **ADC**: `adc_oneshot_unit_handle_t`, `adc_oneshot_read()`
- **NVS**: `nvs_open()`, `nvs_get_u8()`, `nvs_set_u8()`, `nvs_commit()`
- **Timing**: `esp_timer_get_time()` for millis(), `vTaskDelay()` for delays
- **Sleep**: `esp_deep_sleep_enable_gpio_wakeup()`, `esp_sleep_enable_timer_wakeup()`
- **Logging**: `ESP_LOGI()`, `ESP_LOGE()` with TAG definitions
- **FreeRTOS**: `xTaskCreate()`, `xSemaphoreTake()`, `xSemaphoreGive()`

## Configuration

**Debug Mode** (Runtime Detection):
- **Activation**: Hold BOOT button (GPIO9) while powering on or pressing reset
- **Behavior**:
  - Sleep: DISABLED (device stays awake)
  - Logging: VERBOSE (all subsystems at DEBUG level)
  - Zigbee: ENABLED (operates normally with enhanced logging)
- **Use Cases**:
  - Test button gestures with live Zigbee feedback
  - Debug Zigbee binding operations
  - Monitor ZCL command transmission
  - Troubleshoot network joining issues

**Hardware Pins**:
```cpp
// include/constants.h
constexpr gpio_num_t BTN1_PIN = GPIO_NUM_1;      // Button 1 (D1)
constexpr gpio_num_t BTN2_PIN = GPIO_NUM_2;      // Button 2 (D2)
constexpr gpio_num_t LED_PIN = GPIO_NUM_15;      // Built-in LED
constexpr adc_channel_t BAT_ADC_CHANNEL = ADC_CHANNEL_0;

// include/debug.h
constexpr gpio_num_t BOOT_PIN = GPIO_NUM_9;      // BOOT button (debug detection)
```

**Timing Tuning** (include/constants.h):
- Increase `DEBOUNCE_MS` if false triggers
- Increase `COMBO_MS` if combo hard to trigger
- Increase `HOLD_MS` if holds trigger too easily

## Common Workflows

### Activate Debug Mode
1. Hold BOOT button on XIAO ESP32-C6
2. While holding BOOT, press RESET button (or plug in USB)
3. Release BOOT after device powers on
4. Monitor serial at 115200 baud
5. Press buttons to see comprehensive logging:
   - Button press/release events with timing
   - Button gesture detection (CLICK, HOLD, etc.)
   - Event routing decisions (single vs combo)
   - ZCL command details (cluster, endpoint, parameters)
   - Zigbee stack signals (join, bind, etc.)

### Normal Operation (Production Mode)
1. Power on or reset WITHOUT holding BOOT button
2. Device operates with standard logging
3. Enters deep sleep after button events
4. Wakes on button press or 6-hour timer

### Adding New Gestures
1. Add `ButtonEvent` enum in include/constants.h
2. Implement detection in `handleButton()` in src/buttons.cpp
3. Add dispatch case in `routeEvents()` in src/router.cpp
4. Implement command in src/zigbee.cpp

### Fixing Issues
Refer to [ROADMAP.md](ROADMAP.md) for:
- Current blocking issues
- Step-by-step fixes with code examples
- Testing procedures

## Important Notes

- **Main entry**: ESP-IDF uses `extern "C" void app_main()`, not `setup()`/`loop()`
- **Global naming**: Use `g_nvs_handle` (not `nvs_handle`) to avoid typedef collision
- **Build warnings**: ADC warnings are expected
- **Flash size**: Board=2MB, sdkconfig=4MB warning is benign
- **Model ID**: Must match between firmware (`src/zigbee.cpp:~200`) and Z2M converter (`z2m/converter.js:20`)

## Zigbee2MQTT Integration

**Status**: Converter complete and production-ready

**Files**:
- `z2m/converter.js` - External converter (3 endpoints, battery, actions)
- `z2m/device-config.yaml` - Alternative inline config
- `z2m/README.md` - Complete setup guide

**Critical**: Device uses binding mode. Users must manually bind endpoints via Z2M UI:
- Endpoint 1 → Lamp 1 (Button 1)
- Endpoint 2 → Lamp 2 (Button 2)
- Endpoint 3 → Lamp 3 (Both buttons)

See [z2m/README.md](z2m/README.md) for installation and binding instructions.
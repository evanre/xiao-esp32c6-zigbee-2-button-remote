# XIAO ESP32-C6 Zigbee 2-Button Remote

A battery-powered Zigbee remote control firmware for the XIAO ESP32-C6 development board. Control up to 3 different Zigbee devices (smart lights) with sophisticated gesture detection using just 2 physical buttons.

## Features

- **3 Virtual Endpoints**: Control 3 different devices with 2 physical buttons
  - Button 1 → Controls Lamp 1 (Endpoint 1)
  - Button 2 → Controls Lamp 2 (Endpoint 2)
  - Both buttons simultaneously → Controls Lamp 3 (Endpoint 3)

- **Rich Gesture Recognition**:
  - Single click → Toggle light on/off
  - Long hold → Dim up/down (direction toggles with each hold)
  - Click + Hold → Adjust color temperature up/down
  - Double click → Reserved for future use
  - Triple click → Reserved for future use
  - 6+ rapid clicks → Enter pairing mode

- **Power Efficient**:
  - Deep sleep between button presses
  - Periodic wakeup every 6 hours for battery reporting
  - Maintains Zigbee network presence

- **Battery Monitoring**:
  - Li-ion voltage sensing via ADC
  - Percentage calculation with discharge curve
  - Automatic battery status reporting to coordinator

## Hardware Requirements

### Components
- **XIAO ESP32-C6** development board
- **2x Push buttons** (normally open, momentary)
- **Battery**: Single-cell Li-ion (3.7V nominal)
- **Voltage divider** for battery monitoring (2:1 ratio recommended)
- Optional: Pull-up resistors (if not using internal pull-ups)

### Wiring

```
XIAO ESP32-C6 Pinout:
┌─────────────────┐
│     ESP32-C6    │
│                 │
│  D1 (GPIO1) ────┼──→ Button 1 (to GND when pressed)
│  D2 (GPIO2) ────┼──→ Button 2 (to GND when pressed)
│  A0 (ADC)   ────┼──→ Battery voltage divider
│  GND        ────┼──→ Common ground
│  5V/BAT+    ────┼──→ Battery positive
└─────────────────┘
```

**Button Connections:**
- Connect each button between GPIO pin and GND
- Firmware uses internal pull-up resistors
- Button pressed = LOW signal

**Battery Voltage Divider:**
```
Battery+ ──[R1]──┬──→ A0 (ADC input)
                 │
               [R2]
                 │
                GND
```
- Use 2:1 voltage divider (e.g., R1=10kΩ, R2=10kΩ)
- Adjust `VBAT_DIVIDER` in [constants.h](constants.h) if using different ratio
- Ensure divided voltage stays below 3.3V (ESP32-C6 ADC limit)

## Software Setup

### Prerequisites
- Arduino IDE 2.x or PlatformIO
- ESP32 Arduino Core 3.0+ (with Zigbee support)
- XIAO ESP32-C6 board support

### Installation

1. **Install Board Support**:
   - Open Arduino IDE
   - Go to File → Preferences
   - Add board manager URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Go to Tools → Board → Boards Manager
   - Search "esp32" and install latest version (3.0+)

2. **Configure Board Settings**:
   - Board: "XIAO_ESP32C6"
   - Zigbee mode: **"Zigbee ED (End Device)"** ← CRITICAL!
   - Partition Scheme: Default
   - Upload Speed: 921600

3. **Clone and Open**:
   ```bash
   git clone <repository-url>
   cd xiao-esp32c6-zigbee-2-button-remote
   ```
   Open `xiao-esp32c6-zigbee-2-button-remote.ino` in Arduino IDE

4. **Configure Hardware** (optional):
   Edit [constants.h](constants.h):
   ```cpp
   // Adjust pins if needed
   constexpr int BTN1_PIN = 1;
   constexpr int BTN2_PIN = 2;

   // Calibrate battery voltage divider
   constexpr float VBAT_DIVIDER = 2.00f;  // Adjust to your resistor ratio
   constexpr float ADC_CAL_K = 1.00f;     // Fine-tune after measurement
   ```

5. **Debug Mode** (optional):
   For testing gestures without Zigbee:
   ```cpp
   constexpr bool DEBUG_MODE = true;  // Enable Serial output, disable Zigbee
   ```
   Set to `false` for production use.

6. **Upload**:
   - Connect XIAO ESP32-C6 via USB-C
   - Select correct port in Tools → Port
   - Click Upload

## Usage

### First Time Setup (Pairing)

1. **Power on** the remote
2. **Quickly press any button 6+ times** within 1.5 seconds
3. Device enters pairing mode (180 seconds discovery window)
4. On your Zigbee coordinator (Home Assistant, Zigbee2MQTT, etc.):
   - Start device pairing/discovery
   - Look for device: "DIY TwoBtnRemote"
5. Once paired, the remote will sleep automatically

### Button Gestures

| Gesture | Action | Notes |
|---------|--------|-------|
| **Single click** | Toggle light on/off | Quick press and release |
| **Long hold** | Dim up/down | Hold > 600ms. Direction toggles each time |
| **Click + Hold** | Color temp adjust | Click once, then hold on next press |
| **Both buttons** (< 80ms apart) | Control Lamp 3 | Supports all same gestures |
| **6+ rapid clicks** | Enter pairing mode | Factory reset + 180s pairing window |

### Advanced Usage

**Combo Button Detection:**
- Press both buttons within 80ms to control the 3rd endpoint
- All gestures work with combo: click, hold, click+hold
- Example: Press both → hold → adjusts brightness of Lamp 3

**Direction Toggle:**
- First hold → Dims up
- Second hold → Dims down
- Third hold → Dims up (repeats)
- Direction persists across reboots (stored in NVS)

**Battery Monitoring:**
- Device wakes every 6 hours to report battery
- Battery percentage sent to coordinator
- Monitor in your home automation platform

## Configuration Reference

All timing and behavior constants are in [constants.h](constants.h):

```cpp
// Timing (milliseconds)
constexpr uint16_t DEBOUNCE_MS = 25;    // Button debounce
constexpr uint16_t COMBO_MS = 80;       // Combo button detection window
constexpr uint16_t PRESS_MS = 300;      // Multi-click detection window
constexpr uint16_t HOLD_MS = 600;       // Hold threshold
constexpr uint32_t MAX_HOLD_MS = 30000; // Safety timeout for stuck buttons
constexpr uint8_t PAIRING_CLICKS = 6;   // Clicks needed for pairing

// Battery
constexpr uint8_t PING_INTERVAL_HOURS = 6; // Battery report interval
constexpr float VBAT_DIVIDER = 2.00f;      // Voltage divider ratio
constexpr float ADC_CAL_K = 1.00f;         // Calibration constant

// Pairing
constexpr uint8_t STEER_SECONDS = 180; // Pairing window duration
```

## Calibration

### Battery Voltage Calibration

1. Measure actual battery voltage with multimeter
2. Enable DEBUG_MODE and monitor Serial output
3. Compare reported voltage to actual voltage
4. Adjust `ADC_CAL_K` in [constants.h](constants.h):
   ```cpp
   ADC_CAL_K = actual_voltage / reported_voltage
   ```
5. Re-upload and verify

### Button Timing Adjustment

If gestures feel unresponsive or too sensitive:
- `HOLD_MS`: Increase if holds trigger too easily
- `PRESS_MS`: Increase to allow more time between clicks
- `COMBO_MS`: Increase if combo buttons are hard to trigger
- `DEBOUNCE_MS`: Increase if getting false triggers

## Troubleshooting

**Device won't pair:**
- Ensure Zigbee mode is set to "Zigbee ED (End Device)" in Arduino IDE
- Try pairing mode again (6+ clicks)
- Move closer to coordinator during pairing
- Check coordinator logs for pairing errors

**Buttons not responsive:**
- Check wiring (buttons to GND when pressed)
- Verify pin numbers in [constants.h](constants.h)
- Enable DEBUG_MODE and monitor Serial for button events
- Check battery voltage (device may sleep if voltage too low)

**Battery percentage incorrect:**
- Calibrate voltage divider (see Calibration section)
- Verify divider ratio matches `VBAT_DIVIDER`
- Check battery LUT in [zigbee.cpp](zigbee.cpp) matches your battery type

**Device keeps sleeping immediately:**
- Check `goto_sleep()` logic in [xiao-esp32c6-zigbee-2-button-remote.ino](xiao-esp32c6-zigbee-2-button-remote.ino)
- Ensure not in timer wake path (both buttons HIGH at boot)
- Enable DEBUG_MODE to prevent sleep for testing

**Combo buttons not working:**
- Reduce `COMBO_MS` if buttons need to be pressed more simultaneously
- Practice pressing both buttons at exactly the same time
- Check Serial debug output to see event timing

## Architecture

### File Structure

```
├── xiao-esp32c6-zigbee-2-button-remote.ino  # Main entry point
├── constants.h          # Configuration and shared declarations
├── buttons.cpp          # Button state machine and gesture detection
├── router.cpp           # Event routing and combo detection
└── zigbee.cpp          # Zigbee communication and control
```

### Module Overview

**Button State Machine** ([buttons.cpp](buttons.cpp)):
- Handles debouncing, hold detection, multi-click counting
- States: IDLE, HOLDING, CLICK_HOLDING, STUCK
- Detects: CLICK, DOUBLE_CLICK, TRIPLE_CLICK, HOLD_START/END, CLICK_HOLD_START/END

**Event Router** ([router.cpp](router.cpp)):
- Multiplexes 2 buttons to 3 virtual endpoints
- Combo detection with 80ms window
- Dispatches events to appropriate Zigbee commands

**Zigbee Control** ([zigbee.cpp](zigbee.cpp)):
- 3 ZigbeeSwitch endpoints (On/Off + Level + Color Temp)
- Battery monitoring with Li-ion discharge curve
- Pairing mode (factory reset + network steering)
- Deep sleep power management

## Power Consumption

**Active (button press):** ~30-50mA for < 1 second
**Deep sleep:** ~10-20µA
**Estimated battery life:** 6-12 months on 1000mAh Li-ion (depends on usage)

## Development

### Debug Mode

Enable detailed Serial logging:

```cpp
// constants.h
constexpr bool DEBUG_MODE = true;
```

With DEBUG_MODE enabled:
- Serial output at 115200 baud
- Button events printed to console
- Zigbee disabled (test gestures only)
- Device won't enter deep sleep

### Testing Gestures

1. Enable DEBUG_MODE
2. Upload firmware
3. Open Serial Monitor (115200 baud)
4. Test button combinations and observe output:
   ```
   === DEBUG MODE: Gestures test (no Zigbee) ===

   [SEND]CLICK 1
   [SEND]HOLD_START 2
   [SEND]HOLD_END 2
   ```

## License

This project is open source. Use and modify as needed.

## Contributing

Contributions welcome! Areas for improvement:
- Additional gesture types
- OTA update support
- Visual feedback (LED blinks)
- Configurable endpoints via Zigbee attributes
- Support for other button configurations

## Credits

Built with ESP32 Arduino Core Zigbee library for XIAO ESP32-C6.

# XIAO ESP32-C6 Zigbee Button Remote

Battery-powered Zigbee remote control for the XIAO ESP32-C6. Control up to 3 different Zigbee devices (smart lights) with sophisticated gesture detection.

**Current Configuration**: 2 physical buttons, 3 endpoints (button 1, button 2, combo). Future versions will support 1-4 buttons with configurable endpoint mappings.

**Development Status**: See [ROADMAP.md](ROADMAP.md) for current implementation status and known issues.

## Features

### 3 Virtual Endpoints
Control 3 devices with 2 physical buttons:
- **Button 1** → Controls Lamp 1 (Endpoint 1)
- **Button 2** → Controls Lamp 2 (Endpoint 2)
- **Both buttons** (< 80ms apart) → Controls Lamp 3 (Endpoint 3)

### Button Gestures

| Gesture | Action |
|---------|--------|
| Single click | Toggle light on/off |
| Long hold (>600ms) | Dim up/down (direction toggles) |
| Click + Hold | Adjust color temperature |
| 6+ rapid clicks | Enter pairing mode |

### Power Efficiency
- Deep sleep between button presses (~10-20µA)
- Wakes every 6 hours for battery reporting
- Estimated battery life: 6-12 months on 1000mAh Li-ion

### Battery Monitoring
- Li-ion voltage sensing via ADC
- Percentage calculation with discharge curve
- Automatic reporting to Zigbee coordinator

## Hardware Requirements

### Components
- **XIAO ESP32-C6** development board
- **2x Push buttons** (normally open, momentary)
- **Battery**: Single-cell Li-ion (3.7V nominal)
- **Voltage divider** for battery monitoring (2:1 ratio recommended)

### Wiring

```
XIAO ESP32-C6:
  D1 (GPIO1) ──→ Button 1 (to GND when pressed)
  D2 (GPIO2) ──→ Button 2 (to GND when pressed)
  A0 (ADC)   ──→ Battery voltage divider (Vbat/2)
  GND        ──→ Common ground
  5V/BAT+    ──→ Battery positive
```

**Button Connections**:
- Connect each button between GPIO pin and GND
- Firmware uses internal pull-up resistors
- Button pressed = LOW signal

**Battery Voltage Divider**:
```
Battery+ ──[R1=10kΩ]──┬──→ A0 (ADC input, max 3.3V)
                      │
                   [R2=10kΩ]
                      │
                     GND
```

## Software Setup

### Prerequisites
- PlatformIO (recommended) or ESP-IDF CLI
- XIAO ESP32-C6 board support

### Installation

1. **Clone repository**:
   ```bash
   git clone <repository-url>
   cd xiao-esp32c6-zigbee-2-button-remote
   ```

2. **Build firmware**:
   ```bash
   ~/.platformio/penv/bin/platformio run
   ```

3. **Upload to device**:
   ```bash
   ~/.platformio/penv/bin/platformio run --target upload
   ```

4. **Monitor serial output**:
   ```bash
   ~/.platformio/penv/bin/platformio device monitor
   ```

### Configuration

Edit `include/constants.h` to adjust:

**Hardware pins** (if needed):
```cpp
constexpr gpio_num_t BTN1_PIN = GPIO_NUM_1;
constexpr gpio_num_t BTN2_PIN = GPIO_NUM_2;
```

**Battery calibration**:
```cpp
constexpr float VBAT_DIVIDER = 2.00f;  // Voltage divider ratio
constexpr float ADC_CAL_K = 1.00f;     // Fine calibration
```

**Debug mode** (disable Zigbee, enable logging):
```cpp
constexpr bool DEBUG_MODE = true;  // Set false for production
```

**Timing adjustments**:
```cpp
constexpr uint16_t DEBOUNCE_MS = 25;  // Button debounce
constexpr uint16_t COMBO_MS = 80;     // Combo detection window
constexpr uint16_t HOLD_MS = 600;     // Hold threshold
```

## Usage

### Pairing with Zigbee Coordinator

1. **Power on** the remote
2. **Quickly press any button 6+ times** within 1.5 seconds
3. Device enters pairing mode (180 seconds)
4. On your Zigbee coordinator (Zigbee2MQTT, Home Assistant, etc.):
   - Enable device pairing
   - Look for device: "DIY TwoBtnRemote"
5. **Configure bindings** (required for device to work)

**Important**: This device uses Zigbee binding mode. You must manually bind each endpoint to target lamps via your coordinator's interface. See [z2m/README.md](z2m/README.md) for Zigbee2MQTT setup.

### Button Operation

**Single Button**:
- Single click → Toggle lamp on/off
- Hold (>600ms) → Start dimming (direction alternates each hold)
- Release hold → Stop dimming
- Click then hold (on next press) → Adjust color temperature

**Combo (Both Buttons)**:
- Press both buttons within 80ms
- All same gestures work
- Controls the 3rd lamp endpoint

**Direction Toggle**:
- First hold → Dim up
- Second hold → Dim down
- Third hold → Dim up (repeats)
- Direction persists across reboots

### Battery Monitoring

Device wakes every 6 hours to report battery status. Monitor in your home automation platform (Home Assistant, etc.).

## Calibration

### Battery Voltage

1. Measure actual battery voltage with multimeter
2. Enable `DEBUG_MODE = true` and monitor serial output
3. Compare reported voltage to actual voltage
4. Adjust `ADC_CAL_K` in `include/constants.h`:
   ```cpp
   ADC_CAL_K = actual_voltage / reported_voltage
   ```
5. Rebuild, upload, and verify

### Gesture Timing

If gestures feel unresponsive or too sensitive, adjust in `include/constants.h`:
- `HOLD_MS`: Increase if holds trigger too easily
- `COMBO_MS`: Increase if combo buttons hard to trigger simultaneously
- `DEBOUNCE_MS`: Increase if getting false triggers

## Troubleshooting

**Device won't pair**:
- Ensure `DEBUG_MODE = false` in include/constants.h
- Try pairing mode again (6+ clicks)
- Move closer to coordinator during pairing
- Check coordinator logs for errors

**Buttons not responsive**:
- Check wiring (buttons to GND when pressed)
- Verify pin numbers in include/constants.h
- Enable DEBUG_MODE and monitor serial output
- Check battery voltage

**Battery percentage incorrect**:
- Calibrate voltage divider (see Calibration section)
- Verify divider ratio matches `VBAT_DIVIDER`

**Lights don't respond to button presses**:
- **Most likely**: Bindings not configured
- Check Zigbee coordinator for binding settings
- See [z2m/README.md](z2m/README.md) for binding instructions

**Combo buttons not working**:
- Press both buttons more simultaneously
- Try reducing `COMBO_MS` in constants.h
- Check serial debug output to see timing

## Zigbee2MQTT Integration

Complete Zigbee2MQTT integration files are in the `z2m/` directory:

- `z2m/converter.js` - External converter (recommended method)
- `z2m/device-config.yaml` - Alternative inline configuration
- `z2m/README.md` - Complete installation and setup guide

**Quick Start**:
1. Copy `z2m/converter.js` to Z2M data directory as `diy_two_button_remote.js`
2. Add to Z2M `configuration.yaml`:
   ```yaml
   external_converters:
     - diy_two_button_remote.js
   ```
3. Restart Zigbee2MQTT
4. Pair device (6 rapid clicks)
5. **Create bindings** for each endpoint via Z2M web UI

See [z2m/README.md](z2m/README.md) for detailed instructions and troubleshooting.

## Development

### Debug Mode

Enable detailed serial logging without Zigbee:

```cpp
// include/constants.h
constexpr bool DEBUG_MODE = true;
```

Provides:
- Serial output at 115200 baud
- Button event logging
- Zigbee disabled
- No deep sleep

### Current Status

See [ROADMAP.md](ROADMAP.md) for:
- Implementation status
- Known issues
- Development roadmap
- Step-by-step fixes

## Architecture

The firmware uses a state machine architecture with event routing:

1. Button state machines detect gestures (debounce, hold, multi-click)
2. Event router multiplexes 2 buttons → 3 virtual endpoints
3. Combo detection: both buttons within 80ms → Endpoint 3
4. Zigbee commands dispatched to bound lamps
5. Deep sleep between events

See [CLAUDE.md](CLAUDE.md) for detailed architecture documentation.

## Power Consumption

- **Active** (button press): ~30-50mA for <1 second
- **Deep sleep**: ~10-20µA
- **Estimated battery life**: 6-12 months on 1000mAh Li-ion (usage dependent)

## License

This project is open source. Use and modify as needed.

## Future Enhancements

Planned improvements (see [ROADMAP.md](ROADMAP.md)):
- **Configurable button mappings**: Support 1-4 buttons with custom endpoint configurations
- Additional gesture types
- OTA update support
- Visual feedback (LED)
- Power consumption optimization

## Contributing

Contributions welcome!

## Support

- **Issues**: [ROADMAP.md](ROADMAP.md) for known issues
- **Zigbee2MQTT**: [z2m/README.md](z2m/README.md)
- **Development**: [CLAUDE.md](CLAUDE.md)
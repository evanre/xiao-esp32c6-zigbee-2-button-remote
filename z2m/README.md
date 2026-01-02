# Zigbee2MQTT Integration

This directory contains Zigbee2MQTT (Z2M) integration files for the ESP32-C6 Two-Button Remote.

## Implementation Status

**Overall Status**: ✅ **COMPLETE** - Ready for testing

| Component | Status | Notes |
|-----------|--------|-------|
| Device Converter | ✅ Complete | External converter with full feature support |
| Device Configuration | ✅ Complete | Alternative inline config method |
| Battery Reporting | ✅ Implemented | Auto-configured with bindings |
| Multi-Endpoint Support | ✅ Implemented | 3 endpoints properly defined |
| Action Exposes | ✅ Implemented | All button gestures exposed |
| Binding Configuration | ⚠️ Manual | User must create bindings via Z2M UI |
| Documentation | ✅ Complete | This README |

## Files in This Directory

### 1. `converter.js` - **RECOMMENDED METHOD**

**Status**: ✅ Ready to use

**Description**: External Zigbee2MQTT converter that adds support for the DIY Two-Button Remote.

**Features**:
- 3-endpoint device definition (Button 1, Button 2, Both buttons)
- Battery voltage and percentage reporting
- Command pass-through for all button gestures
- Automatic binding configuration for battery reporting
- Multi-endpoint support with proper routing

**What It Does**:
- Registers the device as "DIY-TwoBtnRemote" in Zigbee2MQTT
- Exposes battery status in Home Assistant
- Exposes action triggers for each endpoint
- Configures battery reporting intervals automatically

**Implementation Details**:
- Uses Zigbee Herdsman Converters library
- FromZigbee: `battery`, `command_*` (on/off/toggle/move/stop/color_temp)
- ToZigbee: None (device is command sender only)
- Exposes: Battery (voltage + percentage) + 3 action enums (one per endpoint)

---

### 2. `device-config.yaml` - **ALTERNATIVE METHOD**

**Status**: ⚠️ Needs customization

**Description**: Inline device configuration for Zigbee2MQTT `configuration.yaml`.

**When to Use**:
- If external converter doesn't work
- For quick testing without converter installation
- If you prefer inline configuration

**⚠️ IMPORTANT**: You must replace the IEEE address `0x9888e0fffe7fa01c` with your actual device's IEEE address (found in Z2M logs during pairing).

---

## Installation

### Method 1: External Converter (Recommended)

**Advantages**:
- Cleaner configuration
- Easier to update
- Works for multiple devices
- Standard Z2M approach

**Steps**:

1. **Locate Zigbee2MQTT data directory**:
   - Docker: `/opt/zigbee2mqtt/data` or mapped volume
   - Home Assistant Add-on: `/config/zigbee2mqtt`
   - Manual install: Check your Z2M installation path

2. **Copy the converter file**:
   ```bash
   # Copy converter.js to Z2M data directory
   cp z2m/converter.js /opt/zigbee2mqtt/data/diy_two_button_remote.js
   ```

3. **Edit Zigbee2MQTT configuration**:

   Open `configuration.yaml` in your Z2M data directory and add:
   ```yaml
   # Add external converter
   external_converters:
     - diy_two_button_remote.js
   ```

4. **Restart Zigbee2MQTT**:
   ```bash
   # Docker
   docker restart zigbee2mqtt

   # Home Assistant Add-on
   # Restart from Add-ons page

   # Systemd service
   sudo systemctl restart zigbee2mqtt
   ```

5. **Verify installation**:
   - Check Z2M logs for "Loaded external converter: diy_two_button_remote.js"
   - No errors about missing dependencies

---

### Method 2: Inline Configuration (Alternative)

**Use this if Method 1 doesn't work for you.**

**Steps**:

1. **Pair device first** (see Pairing section below)

2. **Find device IEEE address**:
   - Look in Z2M logs during pairing
   - Or check Z2M web UI → Devices → your device
   - Format: `0xXXXXXXXXXXXXXXXX` (16 hex digits)

3. **Edit Z2M configuration**:

   Open `configuration.yaml` and add under `devices:` section:
   ```yaml
   devices:
     '0xYOUR_ACTUAL_IEEE_ADDRESS':  # ⚠️ Replace with your device's IEEE!
       friendly_name: 'two_button_remote'
       definition:
         model: 'TwoBtnRemote'
         vendor: 'DIY'
         description: 'ESP32-C6 two-button Zigbee remote control'

         exposes:
           - type: battery
             access: 1
           - type: voltage
             access: 1
             property: voltage
             unit: mV
             value_min: 2500
             value_max: 4200

           # ... rest of config from device-config.yaml
   ```

4. **Restart Zigbee2MQTT**

---

## Pairing the Device

### Prerequisites
- Firmware flashed to ESP32-C6 with Zigbee enabled (`DEBUG_MODE = false`)
- Zigbee2MQTT running and accessible
- Zigbee coordinator within range

### Pairing Steps

1. **Power on the remote** (or press reset button)

2. **Enter pairing mode**:
   - Quickly press any button 6+ times within ~1.5 seconds
   - Device will enter network steering mode for 180 seconds

3. **Permit joining in Zigbee2MQTT**:
   - Web UI: Click "Permit Join" button (top right)
   - MQTT: Publish `true` to `zigbee2mqtt/bridge/request/permit_join`
   - Home Assistant: Integrations → Zigbee2MQTT → Configure → Permit Join

4. **Wait for device to appear**:
   - Check Z2M logs for pairing messages
   - Device should appear as "DIY-TwoBtnRemote" or "TwoBtnRemote"
   - May take 10-30 seconds

5. **Verify**:
   - Device appears in Z2M web UI
   - Battery status visible
   - 3 endpoints listed (ep1, ep2, ep3)

### Troubleshooting Pairing

**Device not appearing:**
- Ensure coordinator is in permit-join mode
- Check ESP32 serial logs for Zigbee connection messages
- Move device closer to coordinator
- Try power-cycling both device and coordinator
- Check firmware is built with `DEBUG_MODE = false`

**Device appears but no battery:**
- Converter may not be loaded
- Check Z2M logs for converter errors
- Verify converter file path in `configuration.yaml`

**Multiple pairing attempts:**
- Device may already be paired
- Check Z2M for existing device with similar name
- Factory reset device: Remove power, wait 10s, power on, enter pairing mode again

---

## Binding Configuration (REQUIRED)

**⚠️ CRITICAL**: This device uses **binding mode** for all commands. You **MUST** create bindings for the device to control lamps.

### What are Bindings?

Bindings tell the Zigbee network which device should receive commands from each endpoint of the remote. Without bindings, button presses will be logged but won't control any lights.

### How to Create Bindings (Zigbee2MQTT Web UI)

1. **Navigate to device**:
   - Open Zigbee2MQTT web UI
   - Go to "Devices" tab
   - Click on your "two_button_remote" device

2. **Create bindings for each endpoint**:

   **For Endpoint 1 (Button 1 → Lamp 1):**
   - Click "Bind" tab
   - Select "Endpoint: 1" from dropdown
   - Click "Bind to device"
   - Select your target Lamp 1 from device list
   - Bind these clusters:
     - `genOnOff` (0x0006) - For toggle commands
     - `genLevelCtrl` (0x0008) - For dimming
     - `lightingColorCtrl` (0x0300) - For color temperature
   - Click "Bind"

   **For Endpoint 2 (Button 2 → Lamp 2):**
   - Repeat above steps with "Endpoint: 2" → bind to Lamp 2

   **For Endpoint 3 (Both Buttons → Lamp 3):**
   - Repeat above steps with "Endpoint: 3" → bind to Lamp 3

3. **Verify bindings**:
   - Bindings should appear in the "Bind" tab
   - Test: Press button and verify lamp responds

### Binding via MQTT (Alternative)

If web UI binding doesn't work:

```bash
# Bind Button 1 (EP1) to Lamp 1
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "two_button_remote/ep1",
  "to": "lamp_1",
  "clusters": ["genOnOff", "genLevelCtrl", "lightingColorCtrl"]
}'

# Bind Button 2 (EP2) to Lamp 2
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "two_button_remote/ep2",
  "to": "lamp_2",
  "clusters": ["genOnOff", "genLevelCtrl", "lightingColorCtrl"]
}'

# Bind Both Buttons (EP3) to Lamp 3
mosquitto_pub -t 'zigbee2mqtt/bridge/request/device/bind' -m '{
  "from": "two_button_remote/ep3",
  "to": "lamp_3",
  "clusters": ["genOnOff", "genLevelCtrl", "lightingColorCtrl"]
}'
```

---

## Home Assistant Integration

Once the device is paired with Zigbee2MQTT, it will automatically appear in Home Assistant.

### Entities Created

**Sensor Entities:**
- `sensor.two_button_remote_battery` - Battery percentage
- `sensor.two_button_remote_voltage` - Battery voltage (mV)

**Event Entities (one per endpoint):**
- `sensor.two_button_remote_action_ep1` - Button 1 actions
- `sensor.two_button_remote_action_ep2` - Button 2 actions
- `sensor.two_button_remote_action_ep3` - Both buttons actions

### Action Values

Each action sensor can report:
- `toggle` - Single click
- `brightness_move_up` - Hold (when direction is up)
- `brightness_move_down` - Hold (when direction is down)
- `brightness_stop` - Release after hold
- `color_temperature_move_up` - Double-click + hold (up)
- `color_temperature_move_down` - Double-click + hold (down)
- `color_temperature_stop` - Release after color temp hold

### Automation Example

**Trigger light on button press:**

```yaml
automation:
  - alias: "Remote Button 1 Toggle"
    trigger:
      - platform: state
        entity_id: sensor.two_button_remote_action_ep1
        to: 'toggle'
    action:
      - service: light.toggle
        target:
          entity_id: light.living_room

  - alias: "Remote Button 1 Dimming"
    trigger:
      - platform: state
        entity_id: sensor.two_button_remote_action_ep1
        to: 'brightness_move_up'
    action:
      - service: light.turn_on
        target:
          entity_id: light.living_room
        data:
          brightness_step_pct: 10
```

**Note**: If using bindings (recommended), the remote controls lights directly via Zigbee without going through Home Assistant. The action entities are for monitoring/logging only.

---

## Testing Checklist

After installation, test all features:

### Battery Reporting
- [ ] Battery percentage appears in Z2M web UI
- [ ] Battery voltage appears in Z2M web UI
- [ ] Values are reasonable (e.g., 3700-4200mV, 0-100%)
- [ ] Battery updates periodically (every 6 hours)

### Button 1 (Endpoint 1)
- [ ] Single click toggles Lamp 1
- [ ] Hold starts dimming Lamp 1
- [ ] Release stops dimming
- [ ] Double-click adjusts color temperature
- [ ] Actions appear in Z2M logs

### Button 2 (Endpoint 2)
- [ ] Single click toggles Lamp 2
- [ ] Hold starts dimming Lamp 2
- [ ] Release stops dimming
- [ ] Double-click adjusts color temperature
- [ ] Actions appear in Z2M logs

### Combo (Endpoint 3)
- [ ] Press both buttons within 80ms
- [ ] Single click toggles Lamp 3
- [ ] Hold starts dimming Lamp 3
- [ ] All gestures work same as single buttons

### Pairing Mode
- [ ] 6 rapid clicks enters pairing mode
- [ ] Device rejoins network after pairing mode
- [ ] Factory reset works (if implemented)

---

## Troubleshooting

### Converter Not Loading

**Symptom**: Device appears but no battery/actions exposed

**Solutions**:
1. Check `external_converters` path in `configuration.yaml`
2. Verify file name matches exactly (case-sensitive)
3. Check Z2M logs for "Failed to load converter" errors
4. Ensure converter file has correct JavaScript syntax
5. Restart Z2M and watch startup logs

### Buttons Don't Control Lights

**Symptom**: Actions appear in logs but lights don't respond

**Solutions**:
1. **Most likely**: Bindings not configured (see Binding Configuration section)
2. Check if lamps support the command clusters
3. Verify lamps are powered on and connected to network
4. Check lamp manufacturer (some require specific command formats)
5. Test with Z2M "Bind" tab → send test command

### Battery Not Reporting

**Symptom**: Battery shows as "unknown" or not updating

**Solutions**:
1. Verify converter configure() function ran (check Z2M logs)
2. Wait up to 6 hours for first report (timer wake cycle)
3. Manually trigger report: Press buttons to wake device
4. Check firmware: Ensure `report_battery()` is implemented
5. Verify ADC is properly calibrated in firmware

### Actions Not Appearing in Home Assistant

**Symptom**: Device in HA but no action sensors

**Solutions**:
1. Check if entities are disabled (HA → Settings → Entities)
2. Restart Home Assistant after pairing
3. Check Z2M integration is properly configured
4. Verify MQTT broker is working
5. Check entity naming (may be different if `friendly_name` changed)

### Device Keeps Dropping Off Network

**Symptom**: Device frequently shows as "unavailable"

**Solutions**:
1. Check battery voltage (low battery causes disconnections)
2. Move coordinator closer to device
3. Add Zigbee router between coordinator and device
4. Check deep sleep configuration in firmware
5. Verify wake timers are configured correctly (6-hour interval)

---

## Advanced Configuration

### Custom Endpoint Names

Edit `friendly_name` in converter.js `endpoint()` function:

```javascript
endpoint: (device) => {
    return {
        'living_room': 1,  // Instead of 'ep1'
        'bedroom': 2,      // Instead of 'ep2'
        'kitchen': 3,      // Instead of 'ep3'
    };
},
```

### Adjust Battery Reporting Interval

Default: Every 6 hours

To change, modify firmware `include/constants.h`:
```cpp
constexpr uint8_t PING_INTERVAL_HOURS = 12;  // Report every 12 hours
```

Rebuild and flash firmware.

### Custom Zigbee Model Name

Edit converter.js:
```javascript
zigbeeModel: ['MyCustomRemote'],  // Change from 'TwoBtnRemote'
```

Must also update firmware `src/zigbee.cpp` to match:
```cpp
esp_zb_cfg_basic_attr.zcl_version = 0x03;
esp_zb_cfg_basic_attr.model_identifier = "MyCustomRemote";  // Match converter
```

---

## Firmware ↔ Converter Alignment

**CRITICAL**: Ensure firmware and converter are aligned on these parameters:

| Parameter | Firmware Location | Converter Location | Must Match? |
|-----------|------------------|-------------------|-------------|
| Model ID | `src/zigbee.cpp` line ~200 | `converter.js` line 20 | ✅ YES |
| Endpoint Count | `src/zigbee.cpp` endpoint creation | `converter.js` exposes | ✅ YES |
| Cluster IDs | `src/zigbee.cpp` cluster lists | `converter.js` fromZigbee | ✅ YES |
| Battery Attributes | `src/zigbee.cpp` Power Config | `converter.js` battery expose | ✅ YES |

**If these don't match**: Device may appear but features won't work correctly.

---

## Known Limitations

1. **No OTA Support**: Firmware updates require USB connection (no over-the-air updates)
2. **Manual Binding Required**: Bindings must be created via Z2M UI (not auto-discovered)
3. **No Visual Feedback**: Device has no LED to indicate pairing/connection status (unless added to hardware)
4. **Battery Reporting Delay**: First report after pairing may take up to 6 hours
5. **Command-Only Device**: Device sends commands but doesn't respond to commands (no way to remotely trigger actions)

---

## Converter Version History

**v1.0** (Current):
- Initial implementation
- 3-endpoint support
- Battery reporting
- All button gesture commands
- Multi-endpoint metadata

---

## Contributing

Improvements welcome!

**Areas for enhancement**:
- OTA update support
- Auto-binding on pairing
- LED feedback cluster
- Configurable battery report interval via attribute
- Support for more button patterns

**Testing needed**:
- Different lamp manufacturers
- Group binding support
- Network throughput with many button presses
- Battery life validation

---

## Support

**Issues?**
1. Check troubleshooting section above
2. Review Zigbee2MQTT logs for errors
3. Enable firmware DEBUG_MODE for detailed logs
4. Check firmware and converter are version-aligned
5. Open GitHub issue with logs and hardware details

**Logs to include when reporting issues**:
- Zigbee2MQTT startup logs (first 100 lines)
- Device pairing logs
- Button press logs (with timestamps)
- ESP32 serial output (if DEBUG_MODE enabled)
- Z2M database entry for device (anonymize IEEE address)

---

**Last Updated**: 2026-01-02
**Converter Version**: 1.0
**Compatible Firmware**: ESP-IDF based (post-migration)
**Zigbee2MQTT Version**: 1.35.0+ (tested on 1.38.0)
# ESP32-C6 Zigbee Remote - Development Roadmap

**Last Updated**: 2026-01-02
**Current Status**: 95% Complete - Blocked by Critical Threading Issues
**Estimated Completion**: 12-17 hours (core) | 25-30 hours (all features)

## Executive Summary

The project is architecturally sound with excellent code quality, but has **critical threading issues that prevent operation in production mode**. This roadmap prioritizes fixes by impact.

**Phase Status**:
- ✅ **Phase 0**: ESP-IDF migration, button logic, Zigbee commands, Z2M converter (COMPLETE)
- 🔴 **Phase 1**: Fix critical threading (REQUIRED - 6-9 hours)
- 🟡 **Phase 2**: Complete features (battery reporting, wake detection - 3-4 hours)
- 🟢 **Phase 3**: Production optimization (optional - 8-10 hours)
- 📋 **Phase 4**: Documentation and field testing (4-5 hours)
- 📋 **Phase 5**: Zigbee2MQTT integration testing (3-4 hours)

## Current State

### ✅ What's Complete (95%)

**Firmware**:
- ✅ GPIO API migration (Arduino → ESP-IDF)
- ✅ Serial logging (Serial → ESP_LOGI/ESP_LOGE)
- ✅ Timing (millis/delay → esp_timer/vTaskDelay)
- ✅ NVS storage (Preferences → nvs_*)
- ✅ ADC (analogRead → adc_oneshot_read)
- ✅ Deep sleep API
- ✅ Zigbee stack initialization (platform, end device, 3 endpoints)
- ✅ Zigbee signal handler (pairing, connection)
- ✅ Zigbee commands (toggle, level, color temp)
- ✅ Button state machines (debounce, multi-click, hold)
- ✅ Event routing and combo detection

**Zigbee2MQTT Integration**:
- ✅ External converter (`z2m/converter.js`) - production ready
- ✅ Device config (`z2m/device-config.yaml`) - alternative method
- ✅ Complete setup documentation (`z2m/README.md`)

**Build Status**:
- ✅ Compiles: 509KB flash (48.6%), 26KB RAM (8.0%)
- ✅ All components linked

### ⚠️ What's Blocking (3 Critical Issues)

#### 🔴 CRITICAL #1: Zigbee Stack Blocks Main Thread
**Location**: `src/zigbee.cpp:229`
**Problem**: `esp_zb_stack_main_loop()` blocks indefinitely, preventing button loop from executing
**Impact**: Firmware cannot function in production mode (only DEBUG_MODE works)

**Fix Required**: Move Zigbee stack to separate FreeRTOS task
```cpp
// Create task wrapper
void zigbee_task_entry(void *pvParameters) {
    zigbeeInit();  // Contains blocking esp_zb_stack_main_loop()
    vTaskDelete(NULL);
}

// In app_main()
xTaskCreate(zigbee_task_entry, "zigbee", 4096, NULL, 5, &zigbee_task_handle);
// Main loop now executes
```

#### 🔴 CRITICAL #2: No Thread Synchronization
**Location**: Multiple files
**Problem**: Globals accessed from multiple tasks without protection:
- `zigbee_connected` - written in Zigbee task, read in main
- `pairing_mode` - written in both tasks
- `dir_up` - accessed during command dispatch

**Impact**: Race conditions, data corruption

**Fix Required**: Add FreeRTOS mutexes
```cpp
SemaphoreHandle_t zigbee_state_mutex;

void set_zigbee_connected(bool connected) {
    xSemaphoreTake(zigbee_state_mutex, portMAX_DELAY);
    zigbee_connected = connected;
    xSemaphoreGive(zigbee_state_mutex);
}
```

#### 🟡 MEDIUM #3: Wake Source Detection Fragility
**Location**: `src/main.cpp:36`
**Problem**: Uses unreliable GPIO heuristic instead of proper API
**Impact**: May misclassify wake source

**Fix Required**: Use `esp_sleep_get_wakeup_cause()` API

### 🟡 What's Incomplete

- Battery reporting implementation (`src/zigbee.cpp:349` - has TODO)
- Wake source detection (uses GPIO heuristic vs proper API)
- OTA partition support (not critical)

## Phase 1: Fix Critical Threading (REQUIRED)

**Objective**: Make firmware functional
**Effort**: 6-9 hours
**Status**: 🔴 BLOCKING

### Task 1.1: Refactor Zigbee to Separate Task (4-6 hours)

**Files**: `src/main.cpp`, `src/zigbee.cpp`, `include/constants.h`

**Steps**:
1. Create `zigbee_task_entry()` wrapper in `src/zigbee.cpp`
2. Add FreeRTOS task creation in `app_main()`
3. Add `vTaskDelay(pdMS_TO_TICKS(10))` yield in main loop
4. Test build

**Validation**:
- Code compiles
- Button loop executes when Zigbee enabled

### Task 1.2: Add Thread Synchronization (2-3 hours)

**Files**: `include/constants.h`, `src/zigbee.cpp`, `src/main.cpp`

**Steps**:
1. Create `SemaphoreHandle_t zigbee_state_mutex`
2. Implement accessor functions: `set/get_zigbee_connected()`, `set/get_pairing_mode()`
3. Replace direct global access with accessor calls
4. Protect `dir_up` in command functions
5. Test with rapid button presses during Zigbee connection

**Validation**:
- No crashes during concurrent operations
- Clean state transitions in logs

### Task 1.3: Fix Wake Source Detection (15 min)

**File**: `src/main.cpp:36`

**Change**:
```cpp
// Replace GPIO heuristic
esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
woke_by_timer = (cause == ESP_SLEEP_WAKEUP_TIMER);
```

**Validation**: Log shows correct wake cause

## Phase 2: Complete Features (3-4 hours)

**Objective**: Implement missing features
**Status**: 🟡 MEDIUM PRIORITY

### Task 2.1: Battery Reporting (2 hours)

**File**: `src/zigbee.cpp:349`

**Steps**:
1. Research ESP-IDF ZCL attribute reporting API (use Context7)
2. Implement `report_battery()` using `esp_zb_zcl_report_attr_cmd_req()`
3. Test with Zigbee2MQTT coordinator

**Validation**: Battery appears in Z2M device info

### Task 2.2: Document Binding Requirements (1 hour)

**Status**: Already documented in `z2m/README.md`

No firmware changes needed. Users must manually bind endpoints via Z2M UI.

## Phase 3: Production Optimization (8-10 hours, OPTIONAL)

**Objective**: Add robustness
**Status**: 🟢 LOW PRIORITY - Defer until after Phase 1-2

### Task 3.1: Error Handling (3-4 hours)
- Add return codes to command functions
- Implement retry logic for critical operations
- Add error state tracking

### Task 3.2: Power Profiling (4-6 hours)
- Measure actual current consumption
- Validate battery life estimates
- Optimize if needed (target: <20µA deep sleep)

### Task 3.3: OTA Support (6-8 hours, OPTIONAL)
- Update partition table
- Implement OTA handler
- Test firmware updates

## Phase 4: Documentation & Testing (4-5 hours)

**Objective**: Validate and document
**Status**: 📋 After Phase 1-2

### Task 4.1: Update Documentation (2 hours)
- [ ] Update build instructions
- [ ] Document pairing procedure
- [ ] Expand troubleshooting
- [ ] Battery calibration guide

### Task 4.2: Field Testing (2-3 hours)

**Test Cases**:
- [ ] Pairing (6 clicks, network join)
- [ ] All button gestures (click, hold, combo)
- [ ] Power management (sleep, wake, battery report)
- [ ] Reliability (100+ presses, rapid mashing)

## Phase 5: Zigbee2MQTT Integration (3-4 hours)

**Objective**: End-to-end integration testing
**Status**: 📋 After firmware fixes

### Task 5.1: Install Z2M Converter (30 min)
1. Copy `z2m/converter.js` to Z2M data directory
2. Add to `configuration.yaml`
3. Restart Z2M
4. Verify converter loaded

### Task 5.2: Pair Device (15 min)
1. Enable permit join in Z2M
2. Enter pairing mode (6 clicks)
3. Verify device appears and interviews

### Task 5.3: Configure Bindings (60 min, CRITICAL)
1. Bind Endpoint 1 → Lamp 1 (genOnOff, genLevelCtrl, lightingColorCtrl)
2. Bind Endpoint 2 → Lamp 2
3. Bind Endpoint 3 → Lamp 3
4. Test button presses control lamps

**Without bindings, device won't work!**

### Task 5.4: Home Assistant Testing (1 hour)
1. Verify entities appear in HA
2. Test battery reporting
3. Test action state changes
4. Create test automations

### Task 5.5: End-to-End Testing (1 hour)

**Test Matrix**:
- [ ] Button 1 single click → Toggle Lamp 1
- [ ] Button 1 hold → Dim Lamp 1
- [ ] Button 2 single click → Toggle Lamp 2
- [ ] Both buttons → Toggle Lamp 3
- [ ] Timer wake → Battery report

## Timeline Estimate

**Core Development** (Phases 1-2): 9-13 hours
- Phase 1: 6-9 hours (critical threading fixes)
- Phase 2: 3-4 hours (battery, wake detection)

**With Integration Testing** (Phases 1-2, 5): 12-17 hours
- Add Phase 5: +3-4 hours

**Production Ready** (Phases 1-2, 4-5): 19-26 hours
- Add Phase 4: +4-5 hours (docs & field testing)

**All Features** (All phases): 27-36 hours
- Add Phase 3: +8-10 hours (optimization)

## Recommended Approach

1. **Complete Phase 1 in one focused session** (6-9 hours)
   - Don't context-switch
   - Test thoroughly before proceeding

2. **Test Phase 1 with hardware**
   - Verify button loop executes
   - Check for race conditions

3. **Complete Phase 2** (3-4 hours)
   - Battery reporting
   - Feature-complete milestone

4. **Integration test with Phase 5** (3-4 hours)
   - Install Z2M converter
   - Test end-to-end functionality

5. **Defer Phase 3 optimizations**
   - Only if time permits
   - Not blocking for MVP

6. **Iterate on docs (Phase 4)**
   - Based on field testing feedback

## Success Criteria

**Phase 1 Complete** (Minimum Viable Product):
- ✅ Button loop executes without blocking
- ✅ No crashes during concurrent operations
- ✅ Wake source detection reliable
- ✅ Can pair with coordinator

**Phase 2 Complete** (Feature Complete):
- ✅ Battery reporting works
- ✅ All gestures functional
- ✅ Bindings documented

**Ready to Ship** (Phases 1-2, 4-5):
- ✅ Z2M converter installed
- ✅ Device controls lamps via bindings
- ✅ Battery visible in Home Assistant
- ✅ Documentation complete

## Next Steps

**Start Here**:
1. ✅ Read this roadmap
2. ⬜ Set up PlatformIO environment
3. ⬜ Commit current codebase to git
4. ⬜ Begin Phase 1, Task 1.1 (Zigbee task refactor)
5. ⬜ Test incrementally after each change

**Detailed Implementation**:
- See git history for previous version with full code examples
- Use Context7 for ESP-IDF API questions
- Test build after each task

---

**Version**: 2.0 (Streamlined)
**Previous Version**: 1.0 (Archived in git history with detailed code examples)
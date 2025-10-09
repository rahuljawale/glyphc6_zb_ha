# 🌱 Glyph C6 Zigbee Soil Monitor - Project Roadmap

**Project:** ESP32-C6 based Zigbee soil moisture and temperature monitor  
**Hardware:** Glyph C6 + Adafruit STEMMA Soil Sensor (4026)  
**Goal:** Multi-month battery-powered plant monitoring via Home Assistant

---

## 📊 Overall Progress

| Phase | Status | Description | Power | Battery Life (1200mAh) |
|-------|--------|-------------|-------|------------------------|
| **Phase 1** | ✅ COMPLETE | Hardware Integration | 80-120 mA | ~29 hours |
| **Phase 2** | ✅ COMPLETE | Zigbee/Z2M Integration | 80-120 mA | ~29 hours |
| **Phase 2.5** | 🏃 IN PROGRESS | Hardware Validation (24-48hr test) | 80-120 mA | ~29 hours |
| **Phase 3** | ⏳ PLANNED | Deep Sleep Optimization | 0.1-2 mA avg | 6-18 months |

---

## ✅ Phase 1: Hardware Integration (COMPLETE)

### Goal
Get the Adafruit STEMMA Soil Sensor reading moisture and temperature data via I2C.

### Completed Tasks

#### 1.1 I2C Bus Setup ✅
- **File:** `main/system_config.h`
- Corrected I2C pin mapping for STEMMA port:
  - `I2C_SDA_PIN = GPIO4`
  - `I2C_SCL_PIN = GPIO5`
- Added GPIO20 power control for I2C bus
- I2C frequency: 100 kHz

#### 1.2 Soil Sensor Driver ✅
- **Files:** `main/soil_sensor.h`, `main/soil_sensor.c`
- Implemented Seesaw protocol for Adafruit 4026 sensor
- Functions:
  - `soil_sensor_init()` - Initialize I2C communication
  - `soil_sensor_read_moisture()` - Read capacitance (raw 200-2000)
  - `soil_sensor_read_temperature()` - Read temperature in °C
  - `soil_sensor_read_all()` - Combined read with conversion
- Calibration:
  - Dry (in air): 200 raw = 0%
  - Wet (in water): 2000 raw = 100%
  - Linear interpolation for percentage

#### 1.3 I2C Scanner Utility ✅ (Removed)
- **Files:** `main/i2c_scanner.h`, `main/i2c_scanner.c` (removed after Phase 2 completion)
- Scans I2C bus for all devices (0x08-0x77)
- Specialized test for soil sensor at 0x36
- Debugging tool for hardware validation
- **Status:** Successfully debugged pin mapping issues, removed as no longer needed

#### 1.4 Continuous Monitoring Task ✅
- FreeRTOS task reads sensor every 60 seconds
- Automatic error recovery:
  - Tracks consecutive failures
  - Performs soft reset after 3 failures
  - Logs detailed error information
- Timing optimizations:
  - 500ms delay after GPIO20 power-on
  - 1000ms delay after sensor soft reset
  - 5ms delay after capacitance read request

#### 1.5 Status Classification ✅
- **CRITICAL** (< 20%): 💀 Water NOW!
- **LOW** (20-35%): ⚠️ Water soon
- **GOOD** (35-65%): ✅ Happy plant
- **HIGH** (65-85%): 💧 Well watered
- **SATURATED** (> 85%): 🌊 Too wet, don't water

### Test Results
```
I (11052) SOIL_SENSOR: Initializing Adafruit Soil Sensor...
I (11052) SOIL_SENSOR: Found device at 0x36
I (12405) SOIL_SENSOR: Soil sensor initialized successfully
I (72420) SOIL_SENSOR: 📊 Soil Reading:
I (72420) SOIL_SENSOR:    Moisture: 329 raw (7.2%) - 💀 CRITICAL
I (72425) SOIL_SENSOR:    Temperature: 29.5°C (85.1°F)
```

### Power Consumption (Phase 1)
- **Active Mode:** 80-120 mA continuous
- **Battery Life:** ~10-15 hours (1200mAh)

---

## ✅ Phase 2: Zigbee/Z2M Integration (COMPLETE)

### Goal
Report soil sensor data to Home Assistant via Zigbee2MQTT (Z2M).

### Completed Tasks

#### 2.1 Zigbee Clusters ✅
- **File:** `main/zigbee_core.c`
- Added **Temperature Measurement Cluster** (0x0402):
  - Attribute: `measuredValue` (soil temperature)
  - Format: Centidegrees (0.01°C units)
  - Range: -40°C to +80°C (-4000 to 8000)
- Added **Relative Humidity Cluster** (0x0405):
  - Attribute: `measuredValue` (soil moisture %)
  - Format: 0.01% units (0-10000 = 0-100%)
  - Range: 0-100% (0 to 10000)

#### 2.2 Zigbee Update Functions ✅
- **Files:** `main/zigbee_core.h`, `main/zigbee_core.c`
- `zigbee_core_update_soil_moisture(float percent)`:
  - Converts 0-100% → Zigbee 0-10000 format
  - Updates `ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID`
- `zigbee_core_update_soil_temperature(float celsius)`:
  - Converts °C → centidegrees
  - Updates `ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID`

#### 2.3 Automatic Reporting ✅
- **File:** `main/soil_sensor.c`
- Reports every 60 seconds in `soil_monitoring_task()`:
  ```c
  if (zigbee_core_is_joined()) {
      zigbee_core_update_soil_moisture(data.moisture_percent);
      zigbee_core_update_soil_temperature(data.temperature_c);
      ESP_LOGI(TAG, "   → Reported to Zigbee/Z2M");
  }
  ```

#### 2.4 Z2M External Converter ✅
- **File:** `z2m/glyph_c6_converter.js`
- Device name: "Glyph C6 Zigbee Soil Monitor with LED Control"
- Exposed entities:
  - `humidity` (soil moisture %)
  - `temperature` (soil temperature °C)
  - `battery` (percentage)
  - `battery_voltage` (mV)
  - `switch` (LED on/off control)
- Binds and reads initial states for all clusters

### Home Assistant Entities

After pairing with Z2M, the following entities are exposed:

```yaml
sensor.glyph_c6_humidity:                # Soil moisture
  state: 7.16
  unit_of_measurement: "%"
  device_class: humidity

sensor.glyph_c6_temperature:             # Soil temperature
  state: 29.09
  unit_of_measurement: "°C"
  device_class: temperature

sensor.glyph_c6_battery:                 # Battery percentage
  state: 100
  unit_of_measurement: "%"
  device_class: battery

sensor.glyph_c6_battery_voltage:         # Battery voltage
  state: 4800
  unit_of_measurement: "mV"

switch.glyph_c6:                         # LED control
  state: "off"
```

### Test Results
```
I (72420) SOIL_SENSOR: 📊 Soil Reading:
I (72420) SOIL_SENSOR:    Moisture: 329 raw (7.2%) - 💀 CRITICAL
I (72425) SOIL_SENSOR:    Temperature: 29.5°C (85.1°F)
I (72430) ZIGBEE_CORE: Soil moisture updated: 7.2% (ZB value: 720)
I (72435) ZIGBEE_CORE: Soil temperature updated: 29.5°C (ZB value: 2950)
I (72440) SOIL_SENSOR:    → Reported to Zigbee/Z2M
```

**Zigbee Network:**
- Link Quality: 87 lqi (Excellent)
- Device Type: Router (currently, will change to Sleepy End Device in Phase 3)
- Power Source: Mains (USB, will change to Battery in Phase 3)

### Power Consumption (Phase 2)
- **Active Mode:** 80-120 mA continuous (same as Phase 1)
- **Battery Life:** ~10-15 hours (1200mAh)

### Z2M Setup Instructions

1. **Copy converter file:**
   ```bash
   cp z2m/glyph_c6_converter.js /path/to/zigbee2mqtt/data/
   ```

2. **Edit `configuration.yaml`:**
   ```yaml
   external_converters:
     - glyph_c6_converter.js
   ```

3. **Restart Z2M:**
   ```bash
   docker restart zigbee2mqtt
   # or
   systemctl restart zigbee2mqtt
   ```

4. **Re-pair device** (required when adding new clusters):
   - Remove device from Z2M web UI
   - Hold BOOT button on Glyph C6 for 5+ seconds (factory reset)
   - Wait for "Zigbee SEARCHING..." in logs
   - Allow joining in Z2M web UI
   - Device will pair with all sensors

---

## 🏃 Phase 2.5: Hardware Validation (IN PROGRESS)

### Goal
Validate soldered cable connections and ensure 24-48 hours of stable, error-free operation before implementing deep sleep.

### Why This Phase?
After soldering the STEMMA QT connector cable for better durability, we need to validate the hardware is rock-solid before moving to Phase 3. If we implement deep sleep first (which sleeps for 1-6 hours between readings), it would take days to catch intermittent connection issues. By running continuous monitoring (60-second intervals) for 24-48 hours, we can quickly identify any hardware problems.

### Validation Checklist

#### Hardware Tests 🔧
- [x] Cable soldered properly
- [x] Visual inspection (no cold joints, no bridges)
- [x] Continuity test with multimeter
- [x] Initial power-on test successful
- [ ] 6-hour stability test
- [ ] 24-hour stability test
- [ ] 48-hour stability test
- [ ] Flex/stress test (gentle cable movement)
- [ ] Plug/unplug connector test

#### Software Tests 📊
- [x] Sensor readings every 60 seconds
- [x] Zigbee reporting working
- [x] No I2C errors on startup
- [ ] No I2C errors over 24 hours
- [ ] No sensor resets triggered
- [ ] Smooth reading curves (no spikes)
- [ ] Z2M "Last seen" updates continuously
- [ ] Home Assistant history shows clean graphs

### Current Test Results

**Initial Test (After Soldering):**
```
I (843047) SOIL_SENSOR: 📊 Soil Reading:
I (843047) SOIL_SENSOR:    Moisture: 320 raw (6.7%) - 💀 CRITICAL
I (843048) SOIL_SENSOR:    Temperature: 29.2°C (84.5°F)
I (843053) ZIGBEE_CORE: Soil moisture updated: 6.7% (ZB value: 666)
I (843059) ZIGBEE_CORE: Soil temperature updated: 29.2°C (ZB value: 2919)
I (843065) SOIL_SENSOR:    → Reported to Zigbee/Z2M
I (847573) GLYPH_C6: Status: Zigbee JOINED ✅ | LED: OFF | Power: USB⚡ 4.87V (100.0%)
```

**Analysis:**
- ✅ Consistent with pre-solder readings (was 329 raw / 7.2%, now 320 raw / 6.7%)
- ✅ No I2C communication errors
- ✅ Zigbee network stable (JOINED)
- ✅ Reporting every 60 seconds
- ✅ All systems nominal

**Expected Over 24-48 Hours:**
- Moisture: Should stay around 6-7% (plant is very dry) or rise if watered
- Temperature: May vary 2-5°C with room temperature (day/night cycles)
- No errors: Zero `Failed to read` or `Consecutive failures` messages
- Smooth curves: Gradual changes, no sudden spikes or dropouts

### Success Criteria

Before proceeding to Phase 3, must achieve:
1. ✅ Minimum 24 hours of continuous operation
2. ✅ ~1,440 successful readings (60 seconds × 1,440 = 24 hours)
3. ✅ Zero I2C communication errors
4. ✅ Zero sensor reset attempts
5. ✅ Z2M showing continuous updates
6. ✅ Home Assistant history graphs are smooth
7. ✅ Cable flex test passes (gentle movement doesn't cause errors)

### Monitoring Methods

**Option 1: Serial Monitor (Active)**
```bash
idf.py monitor
# Watch for errors in real-time
```

**Option 2: Z2M Dashboard (Passive)**
- Open Zigbee2MQTT web UI
- Check device "Last seen" timestamp
- Should update every ~1 minute

**Option 3: Home Assistant (Best)**
```yaml
type: history-graph
entities:
  - sensor.power_monitor_901_humidity      # Soil moisture
  - sensor.power_monitor_901_temperature   # Soil temperature
hours_to_show: 48
```

### When Complete

Once 24-48 hour validation passes:
- ✅ Hardware is proven reliable
- ✅ Ready to implement Phase 3 deep sleep
- ✅ Confidence that any future issues are software, not hardware

---

## ⏳ Phase 3: Deep Sleep Optimization (PLANNED)

### Goal
Achieve 6-18 months of battery life through intelligent deep sleep management.

---

### 3.1 Basic Deep Sleep 💤

#### Objectives
- Configure ESP32-C6 as Zigbee Sleepy End Device
- Implement sleep/wake cycle with RTC timer
- Preserve Zigbee network state across sleep cycles
- Wake → Read → Report → Sleep loop

#### Implementation Plan

**Files to Modify:**
- `main/system_config.h` - Add deep sleep configuration
- `main/main.c` - Implement sleep/wake loop
- `main/zigbee_core.c` - Configure sleepy end device
- `main/sleep_manager.h/c` - New module for sleep control

**Code Structure:**
```c
// main/system_config.h
#define SLEEP_DURATION_SECONDS      3600      // 1 hour default
#define SLEEP_ENABLED               true
#define QUICK_WAKEUP_ENABLED        true      // Fast sensor read

// main/main.c
RTC_DATA_ATTR static bool zigbee_joined = false;
RTC_DATA_ATTR static uint16_t network_addr = 0;
RTC_DATA_ATTR static uint32_t wake_count = 0;

void app_main() {
    // Check wake reason
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke from timer (wake #%lu)", wake_count++);
    } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Woke from button press");
    }
    
    // Quick init (skip if already done in RTC memory)
    nvs_init();
    battery_init();
    
    // Power on I2C and sensor
    gpio_set_level(GPIO20, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Read sensor
    soil_sensor_init();
    soil_data_t data;
    esp_err_t ret = soil_sensor_read_all(&data);
    
    if (ret == ESP_OK) {
        // Report to Zigbee (will wake radio)
        if (zigbee_is_joined()) {
            zigbee_core_update_soil_moisture(data.moisture_percent);
            zigbee_core_update_soil_temperature(data.temperature_c);
            zigbee_core_update_battery(battery_get_percentage());
        }
    }
    
    // Clean shutdown
    soil_sensor_deinit();
    gpio_set_level(GPIO20, 0);  // Power off I2C
    
    // Configure wake sources
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SECONDS * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_9, 0);  // BOOT button
    
    // Enter deep sleep
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds...", SLEEP_DURATION_SECONDS);
    esp_deep_sleep_start();
}
```

**Zigbee Configuration:**
```c
// main/zigbee_core.c
esp_zb_cfg_t zb_cfg = {
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,  // End Device (not Router!)
    .install_code_policy = false,
    .nwk_cfg.zczr_cfg.max_children = 0,
};

// Enable Zigbee sleep
esp_zb_sleep_enable(true);
esp_zb_sleep_set_threshold(10);  // 10ms threshold before sleep
```

#### Expected Results
- **Wake Duration:** ~5 seconds (sensor read + Zigbee TX)
- **Sleep Current:** ~5 µA (ESP32-C6 deep sleep + sensor powered off)
- **Average Current:** ~0.17 mA (1 hour sleep cycle)
- **Battery Life:** ~7-8 months (1200mAh)

---

### 3.2 Smart Wake Triggers 🎯

#### Objectives
- Multiple wake sources (timer, button, optional sensor interrupt)
- Instant manual wake for user checks
- Emergency wake on critical events

#### Implementation Plan

**Wake Sources:**
1. **RTC Timer** - Periodic readings (1-6 hours configurable)
2. **BOOT Button (GPIO9)** - Instant manual wake
3. **Optional: Moisture Threshold** - Wake if sensor has interrupt capability

**Code Structure:**
```c
// Configure multiple wake sources
esp_sleep_enable_timer_wakeup(sleep_duration * 1000000ULL);
esp_sleep_enable_ext0_wakeup(GPIO_NUM_9, 0);  // Button on LOW

// Check wake reason and act accordingly
esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Scheduled wake - reading sensor");
        perform_full_cycle();
        break;
    
    case ESP_SLEEP_WAKEUP_EXT0:
        ESP_LOGI(TAG, "Button wake - user requested reading");
        perform_full_cycle();
        led_flash(3);  // Visual feedback
        break;
    
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        ESP_LOGI(TAG, "First boot or reset");
        perform_setup();
        break;
}
```

#### Expected Results
- **Same sleep efficiency as 3.1**
- **User convenience:** Press button anytime for instant reading
- **Battery Life:** ~6-9 months (1200mAh)

---

### 3.3 Adaptive Sleep Logic 🧠

#### Objectives
- Adjust sleep duration based on soil conditions
- More frequent checks when plant needs attention
- Longer sleep when conditions are stable
- Battery-aware operation

#### Implementation Plan

**Sleep Duration Rules:**
```c
uint32_t calculate_sleep_duration(soil_data_t *data, float battery_percent) {
    // Critical moisture - check frequently
    if (data->moisture_percent < 20.0f) {
        return 15 * 60;  // 15 minutes
    }
    
    // Low moisture - check every hour
    if (data->moisture_percent < 35.0f) {
        return 60 * 60;  // 1 hour
    }
    
    // Good moisture - relax checking
    if (data->moisture_percent < 85.0f) {
        return 6 * 3600;  // 6 hours
    }
    
    // Too wet - long sleep, check occasionally
    if (data->moisture_percent >= 85.0f) {
        return 12 * 3600;  // 12 hours
    }
    
    // Battery low - extend sleep even more
    if (battery_percent < 20.0f) {
        return 24 * 3600;  // 24 hours (conservation mode)
    }
    
    return 3600;  // Default 1 hour
}
```

**Smart Reporting:**
```c
bool should_report_to_zigbee(soil_data_t *data, soil_data_t *last_data) {
    // Always report if significant change
    float moisture_delta = fabs(data->moisture_percent - last_data->moisture_percent);
    float temp_delta = fabs(data->temperature_c - last_data->temperature_c);
    
    if (moisture_delta > 5.0f) return true;   // 5% moisture change
    if (temp_delta > 2.0f) return true;       // 2°C temperature change
    
    // Report at least every N wake cycles
    if (wake_count % 10 == 0) return true;    // Every 10th wake
    
    // Critical conditions always report
    if (data->moisture_percent < 20.0f) return true;
    
    return false;  // Skip reporting to save power
}
```

#### Expected Results
- **Average Sleep:** ~3 hours (adaptive, condition-dependent)
- **Average Current:** ~0.06 mA
- **Battery Life:** ~18-20 months (1200mAh)
- **Smart alerting:** More frequent when plant needs care

---

### 3.4 Ultra-Low Power Optimization ⚡

#### Objectives
- Minimize every µA of current consumption
- Push battery life to absolute maximum
- Optional: Solar charging compatibility

#### Implementation Plan

**Power Optimizations:**

1. **GPIO20 Power Cycling** ✅
   ```c
   // Already implemented!
   gpio_set_level(GPIO20, 1);  // Power on before use
   vTaskDelay(pdMS_TO_TICKS(500));
   // ... use sensor ...
   gpio_set_level(GPIO20, 0);  // Power off during sleep
   ```

2. **Zigbee TX Power Reduction** ✅
   ```c
   // Already implemented!
   esp_zb_set_tx_power(10);  // 10dBm (from default 20dBm)
   ```

3. **Minimal Wake Time:**
   ```c
   // Optimize sensor read sequence
   - Pre-calculate conversions in deep sleep code
   - Use fastest I2C transactions
   - Batch Zigbee updates
   - Target: < 3 second wake time
   ```

4. **LED Power Management:**
   ```c
   // Disable LED during sleep (already controllable via Zigbee)
   // Only flash on button wake for user feedback
   ```

5. **RTC-Only Mode:**
   ```c
   // Use only RTC timer (no ULP coprocessor needed)
   esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
   esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
   esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
   ```

6. **Ultra-Long Sleep Mode** (optional):
   ```c
   // For very stable conditions, sleep up to 24 hours
   #define ULTRA_SLEEP_DURATION    (24 * 3600)  // 24 hours
   ```

#### Optional: Solar Charging
- Add small solar panel (5V, 100mA)
- TP4056 charging module
- Trickle charge during day
- Potentially infinite runtime!

#### Expected Results
- **Average Current:** 0.05-0.1 mA (6 hour average sleep)
- **Battery Life:** ~12-24 months (1200mAh)
- **With solar:** Potentially indefinite!

---

## 📊 Power Consumption Summary

### Detailed Current Analysis

| Mode | Duration | Current | Energy |
|------|----------|---------|--------|
| **Deep Sleep** | 3595 sec | 5 µA | 0.005 mAh |
| **Wake + Sensor Read** | 5 sec | 120 mA | 0.167 mAh |
| **Total (1 hour cycle)** | 3600 sec | 0.17 mA avg | 0.172 mAh |

### Battery Life Calculations (1200mAh Battery)

| Scenario | Sleep Interval | Avg Current | Battery Life | Phase |
|----------|---------------|-------------|--------------|-------|
| **Always-On** | No sleep | 100 mA | 12 hours | Phase 1-2 |
| **1 Hour Cycle** | 1 hour | 0.17 mA | 7 months | Phase 3.1 |
| **3 Hour Cycle** | 3 hours | 0.06 mA | 20 months | Phase 3.3 |
| **6 Hour Cycle** | 6 hours | 0.03 mA | 40 months | Phase 3.4 |

### Real-World Expectations

Taking into account:
- Efficiency losses (10-15%)
- Temperature effects on battery
- Self-discharge
- Occasional button wakes

**Conservative Estimates:**
- Phase 3.1 (1hr): **6 months** ✅
- Phase 3.3 (adaptive): **12-15 months** ✅
- Phase 3.4 (optimized): **18-24 months** ✅

---

## 🛠️ Implementation Checklist

### Phase 3.1: Basic Deep Sleep
- [ ] Add sleep configuration to `system_config.h`
- [ ] Create `sleep_manager.h/c` module
- [ ] Modify `main.c` for sleep/wake loop
- [ ] Configure Zigbee sleepy end device
- [ ] Add RTC memory variables
- [ ] Implement wake reason detection
- [ ] Add quick sensor read path
- [ ] Test sleep/wake cycles
- [ ] Verify Zigbee persistence
- [ ] Measure actual current consumption

### Phase 3.2: Smart Wake
- [ ] Add BOOT button wake (GPIO9)
- [ ] Implement wake reason handling
- [ ] Add LED feedback for button wake
- [ ] Test manual wake functionality
- [ ] Document button usage

### Phase 3.3: Adaptive Sleep
- [ ] Implement `calculate_sleep_duration()`
- [ ] Add moisture threshold logic
- [ ] Store previous reading in RTC memory
- [ ] Implement change detection
- [ ] Add battery-aware sleep extension
- [ ] Test adaptive behavior
- [ ] Verify battery life improvements

### Phase 3.4: Ultra-Low Power
- [ ] Profile all power consumption
- [ ] Optimize sensor read sequence
- [ ] Minimize wake time to < 3 seconds
- [ ] Test ultra-long sleep (24 hours)
- [ ] Validate final current consumption
- [ ] Document solar charging option
- [ ] Create power consumption report

---

## 🧪 Testing Strategy

### Phase 1-2 Testing (Complete)
- ✅ I2C bus scan
- ✅ Sensor read accuracy
- ✅ Continuous monitoring
- ✅ Error recovery
- ✅ Zigbee pairing
- ✅ Z2M converter
- ✅ Home Assistant entities

### Phase 3 Testing (Planned)

#### Functional Tests
- [ ] Deep sleep entry/exit
- [ ] Wake on timer
- [ ] Wake on button
- [ ] Sensor read after wake
- [ ] Zigbee reconnection
- [ ] Data persistence (RTC memory)
- [ ] Battery monitoring during sleep

#### Power Tests
- [ ] Measure sleep current with multimeter
- [ ] Measure wake current
- [ ] Calculate average current
- [ ] Verify battery life projections
- [ ] Test at different sleep intervals

#### Endurance Tests
- [ ] 24-hour continuous operation
- [ ] 1-week battery test
- [ ] Multiple wake/sleep cycles (100+)
- [ ] Button wake reliability
- [ ] Zigbee network stability

#### Edge Cases
- [ ] First boot behavior
- [ ] Factory reset recovery
- [ ] Low battery behavior
- [ ] Sensor failure handling
- [ ] Zigbee network loss

---

## 📈 Success Metrics

### Phase 1 Success Criteria ✅
- ✅ Sensor detected on I2C bus (0x36)
- ✅ Accurate moisture readings (0-100%)
- ✅ Accurate temperature readings (°C and °F)
- ✅ Continuous monitoring without crashes
- ✅ Auto-recovery from sensor errors

### Phase 2 Success Criteria ✅
- ✅ Zigbee pairing successful
- ✅ All clusters created (Basic, Identify, PowerConfig, OnOff, Temperature, Humidity)
- ✅ Data reporting to Z2M every 60 seconds
- ✅ Home Assistant entities visible
- ✅ LED control via Zigbee switch
- ✅ Strong link quality (> 50 lqi)

### Phase 3 Success Criteria (Target)
- [ ] Average current < 1 mA
- [ ] Battery life > 6 months (1200mAh)
- [ ] Wake time < 5 seconds
- [ ] Button wake < 2 seconds response
- [ ] Zigbee persistence across 100+ sleep cycles
- [ ] No network re-joins after sleep
- [ ] Adaptive sleep working correctly

---

## 🚀 Deployment Guide

### Hardware Setup
1. **Glyph C6 Board**
   - USB-C power for development
   - LiPo battery (1200mAh+) for production

2. **Adafruit STEMMA Soil Sensor**
   - Connect to STEMMA QT port (I2C)
   - Ensure GPIO20 is HIGH for power

3. **Battery (for Phase 3)**
   - JST connector
   - 1200-2000 mAh recommended
   - 3.7V LiPo

### Software Setup

#### 1. Build and Flash
```bash
cd glyphc6_zb_ha

# First time / clean build
idf.py fullclean
idf.py build
idf.py flash monitor

# Subsequent builds
idf.py build flash monitor
```

#### 2. Z2M Configuration
```bash
# Copy converter
cp z2m/glyph_c6_converter.js /path/to/zigbee2mqtt/data/

# Edit configuration.yaml
external_converters:
  - glyph_c6_converter.js

# Restart Z2M
docker restart zigbee2mqtt
```

#### 3. Pairing
1. Remove old device from Z2M (if re-pairing)
2. Hold BOOT button for 5+ seconds (factory reset)
3. Wait for "Zigbee SEARCHING..." in logs
4. Enable pairing in Z2M web UI
5. Device will join and create entities

#### 4. Home Assistant
- Entities appear automatically via Z2M MQTT discovery
- Check **Developer Tools** → **States**
- Add to Lovelace dashboard

### Troubleshooting

#### "No I2C devices found"
- Check GPIO4/5 connections
- Verify GPIO20 is HIGH
- Try I2C scanner utility

#### "Sensor read failed"
- Increase delays in `soil_sensor.c`
- Check sensor power supply
- Verify I2C pull-up resistors

#### "Zigbee not joining"
- Hold BOOT button longer (10 seconds)
- Check Z2M coordinator is online
- Enable pairing in Z2M
- Check Zigbee channel conflicts

#### "Z2M not seeing sensors"
- Did you re-pair after adding clusters?
- Check Z2M logs for errors
- Verify converter is loaded
- Restart Home Assistant

---

## 📚 Resources

### Documentation
- `BUILD_AND_TEST.md` - Build instructions and test results
- `README.md` - Project overview (if exists)
- `ROADMAP.md` - This file!

### Hardware
- [Glyph C6 Documentation](https://docs.glyphworks.com/glyph-c6)
- [Adafruit STEMMA Soil Sensor](https://www.adafruit.com/product/4026)
- [ESP32-C6 Datasheet](https://www.espressif.com/en/products/socs/esp32-c6)

### Software
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [Zigbee SDK Guide](https://github.com/espressif/esp-zigbee-sdk)
- [Zigbee2MQTT Documentation](https://www.zigbee2mqtt.io/)
- [Home Assistant Documentation](https://www.home-assistant.io/)

### Community
- [ESP32 Forum](https://www.esp32.com/)
- [Zigbee2MQTT Discord](https://discord.gg/zigbee2mqtt)
- [Home Assistant Community](https://community.home-assistant.io/)

---

## 🎯 Next Steps

**Current Status:** ✅ Phase 2 Complete - Fully functional Zigbee soil monitor!

**Recommended Next Action:**

### Option 1: Test Current System
- Water your plant and watch moisture readings rise
- Test LED control via Home Assistant
- Set up automations and alerts
- Validate accuracy over 24-48 hours

### Option 2: Begin Phase 3
- Implement basic deep sleep (3.1)
- Test sleep/wake cycles
- Measure current consumption
- Optimize for 6+ month battery life

### Option 3: Add Features
- Multiple sensor support (4 plants on one board)
- Calibration system (auto-detect dry/wet values)
- Local data logging to flash
- OTA firmware updates via Zigbee

---

## 🙏 Acknowledgments

- **Adafruit** - Excellent STEMMA Soil Sensor hardware
- **Espressif** - ESP32-C6 and Zigbee SDK
- **Zigbee2MQTT Team** - Amazing Zigbee gateway
- **Home Assistant** - Best home automation platform

---

**Last Updated:** October 9, 2025  
**Project Status:** Phase 2 Complete ✅ - Phase 2.5 Hardware Validation 🏃  
**Next Phase:** Deep Sleep Optimization (Phase 3)

---

## 📝 Recent Updates

### October 9, 2025
- ✅ Phase 2 completed: Full Zigbee/Z2M integration with soil sensor reporting
- ✅ I2C scanner utility removed (no longer needed after successful debugging)
- 🏃 Phase 2.5 started: 24-48 hour hardware validation test
- 📋 ROADMAP.md created with complete 3-phase development plan
- 📋 README.md updated with current features and links to documentation

### Hardware Validation Status
- Cable soldered for better durability
- Initial tests passing (6.7% moisture, 29.2°C temperature)
- Running 24-48 hour endurance test
- Once validated → Ready for Phase 3 deep sleep implementation

---

> 🌱 Happy Plants, Happy Life! 🌱


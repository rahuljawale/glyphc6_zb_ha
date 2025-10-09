# 🌱 Glyph C6 Zigbee Soil Monitor

Zigbee-enabled plant monitoring project for Glyph C6 board (ESP32-C6-MINI-1 module) with soil moisture/temperature sensing, battery monitoring, and remote LED control via Home Assistant.

## 📋 Project Documentation

- **[ROADMAP.md](ROADMAP.md)** - Complete 3-phase development plan, power consumption analysis, and implementation details
- **[BUILD_AND_TEST.md](BUILD_AND_TEST.md)** - Build instructions and test results
- **Current Status:** Phase 2 Complete ✅ - Phase 2.5 Hardware Validation 🏃

## Hardware

- **Board**: Glyph C6 (based on Adafruit ESP32-C6 Feather design)
- **Module**: ESP32-C6-MINI-1
- **Flash**: 4MB
- **CPU**: RISC-V single-core @ 160MHz
- **Protocols**: WiFi 6, Bluetooth 5.3 LE, Zigbee, Thread (802.15.4)

## Pin Mapping

Based on Adafruit ESP32-C6 Feather pinout:

### Status LEDs
- `GPIO14`: Onboard LED (controlled via Zigbee On/Off cluster)
- `GPIO15`: Onboard Red LED (alternative, not used in this project)
- `GPIO9`: NeoPixel (shared with Boot button)
- `GPIO20`: NeoPixel/I2C Power (must be HIGH for I2C and NeoPixel)

### Communication Interfaces
- **I2C (STEMMA QT)** - Connected to Adafruit 4026 Soil Sensor:
  - `GPIO4`: SDA (5kΩ pullup)
  - `GPIO5`: SCL (5kΩ pullup)
  - `GPIO20`: Power control (must be HIGH for I2C operation)

- **UART**:
  - `GPIO16`: TX
  - `GPIO17`: RX

- **SPI**:
  - `GPIO21`: SCK
  - `GPIO22`: MOSI
  - `GPIO23`: MISO

### Analog Inputs
- `GPIO0`: A0 (ADC1_CH0) - **Battery Voltage Monitoring** (2.0x divider)
- `GPIO1`: A1 (ADC1_CH1)
- `GPIO6`: A2/IO6 (ADC1_CH6) - **Shared**
- `GPIO5`: A3/IO5 (ADC1_CH5) - **Shared**
- `GPIO3`: A4 (ADC1_CH3)
- `GPIO4`: A5 (ADC1_CH4)

### Digital I/O
Available GPIOs: IO0, IO5, IO6, IO7, IO8, IO9, IO12, IO15
- Note: IO9 shared with Boot/NeoPixel
- Note: IO6 shared with A2, IO5 shared with A3

## Prerequisites

### Required Software
1. **ESP-IDF** (v5.5+)
   - Follow [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/index.html)
   
2. **ESP Zigbee SDK** (v1.6+)
   - Included as a managed component via `idf_component.yml`

### Environment Setup
```bash
# Set up ESP-IDF environment (path may vary based on your installation)
. $HOME/esp/esp-idf/export.sh
```

## Build Instructions

```bash
# Navigate to project directory
cd glyphc6_zb_ha

# Set up environment (if not already done)
. $HOME/esp/esp-idf/export.sh

# Configure the project (optional)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor (replace PORT with your actual port)
idf.py -p PORT flash monitor

# Examples of common ports:
# macOS: /dev/tty.usbmodem* or /dev/tty.usbserial*
# Linux: /dev/ttyUSB* or /dev/ttyACM*
# Windows: COM3, COM4, etc.

# Or just monitor (if already flashed)
idf.py -p PORT monitor

# To erase flash (needed when changing Zigbee clusters or after major changes)
idf.py -p PORT erase-flash
```

## Project Structure

```
glyphc6_zb_ha/
├── CMakeLists.txt              # Root CMake file
├── README.md                   # This file
├── ROADMAP.md                  # 3-phase development roadmap
├── BUILD_AND_TEST.md           # Build instructions and tests
├── sdkconfig.defaults          # Default SDK configuration
├── partitions.csv              # Flash partition table
├── z2m/
│   └── glyph_c6_converter.js   # Zigbee2MQTT external converter
└── main/
    ├── CMakeLists.txt          # Main component CMake
    ├── idf_component.yml       # Component dependencies
    ├── Kconfig.projbuild       # Project configuration options
    ├── main.c                  # Main application code
    ├── zigbee_core.c           # Zigbee stack implementation
    ├── zigbee_core.h           # Zigbee core header
    ├── battery_monitoring.c    # Battery voltage/percentage monitoring
    ├── battery_monitoring.h    # Battery monitoring header
    ├── soil_sensor.c           # Adafruit STEMMA Soil Sensor driver
    ├── soil_sensor.h           # Soil sensor header
    └── system_config.h         # System-wide configuration
```

## Current Features

- ✅ **Soil Monitoring** (NEW!)
  - Adafruit STEMMA Soil Sensor (4026) integration
  - Capacitive moisture sensing (0-100%)
  - Temperature measurement (°C and °F)
  - Automatic readings every 60 seconds
  - Status classification (Critical, Low, Good, High, Saturated)
  - Auto-recovery on sensor failures
  - Seesaw I2C protocol implementation
  
- ✅ **Zigbee Integration**
  - Simple Sensor device (HA_ESP_SENSOR_ENDPOINT)
  - Temperature Measurement cluster (0x0402) - Soil temperature
  - Relative Humidity cluster (0x0405) - Soil moisture
  - Power Configuration cluster (0x0001) - Battery monitoring
  - On/Off cluster (0x0006) - LED control
  - Network steering with automatic retry
  - Persistent network credentials (NVRAM)
  
- ✅ **Battery Monitoring**
  - ADC-based voltage measurement (GPIO0/A0)
  - Voltage divider compensation (2.0x ratio)
  - LiPo discharge curve mapping (3.0-4.2V)
  - USB power detection (reports 100% when USB connected)
  - Zigbee attribute reporting via scheduler
  
- ✅ **Remote LED Control**
  - GPIO14 LED controlled via Zigbee2MQTT
  - On/Off commands from Z2M
  
- ✅ **System Features**
  - GPIO initialization (LED, I2C power control)
  - GPIO20 power management for I2C sensors
  - Basic system information logging
  - NVS (Non-Volatile Storage) initialization
  - FreeRTOS task management
  - 10dBm TX power for stability

## Configuration Options

Access via `idf.py menuconfig` → "Glyph C6 Monitor Configuration":

- **Blink Period**: LED blink interval (100-10000ms)
- **Enable Zigbee**: Enable/disable Zigbee functionality
- **Zigbee Channel**: Channel selection (11-26)
- **Zigbee PAN ID**: Network PAN ID

## Troubleshooting

### Port Not Found
```bash
# List available serial ports (macOS)
ls /dev/tty.usb*

# Linux
ls /dev/ttyUSB*
```

### Permission Denied (Linux)
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Build Errors
```bash
# Clean build
idf.py fullclean

# Rebuild
idf.py build
```

## Zigbee2MQTT Integration

This device requires a custom external converter for full functionality.

### Setup Instructions

1. **Copy the converter file:**
   ```bash
   cp z2m/glyph_c6_converter.js /path/to/zigbee2mqtt/data/
   ```

2. **Edit Zigbee2MQTT `configuration.yaml`:**
   ```yaml
   external_converters:
     - glyph_c6_converter.js
   ```

3. **Restart Zigbee2MQTT:**
   ```bash
   docker restart zigbee2mqtt
   # or
   systemctl restart zigbee2mqtt
   ```

4. **Pair the device:**
   - Enable pairing mode in Zigbee2MQTT web UI
   - Power on the Glyph C6 device
   - Wait for "Zigbee JOINED ✅" in serial logs
   - Device will appear in Z2M with all sensors

### Exposed Entities in Home Assistant

- `sensor.glyph_c6_humidity` - Soil moisture (0-100%)
- `sensor.glyph_c6_temperature` - Soil temperature (°C)
- `sensor.glyph_c6_battery` - Battery percentage
- `sensor.glyph_c6_battery_voltage` - Battery voltage (mV)
- `switch.glyph_c6` - LED control

## Future Enhancements (Phase 3)

See [ROADMAP.md](ROADMAP.md) for detailed implementation plans.

### Phase 3: Deep Sleep Optimization (Planned)

**Goal:** Achieve 6-18 months of battery life through intelligent power management

#### Phase 3.1: Basic Deep Sleep
- Configure ESP32-C6 as Zigbee Sleepy End Device
- Sleep/wake cycle with RTC timer (1-6 hours)
- Wake → Read → Report → Sleep loop
- **Target:** 7-8 months battery life (1200mAh)

#### Phase 3.2: Smart Wake Triggers
- Multiple wake sources (timer, button press)
- Instant manual wake via BOOT button
- Emergency wake on critical events
- **Target:** 6-9 months battery life

#### Phase 3.3: Adaptive Sleep Logic
- Adjust sleep duration based on soil moisture
- Critical moisture = check every 15 minutes
- Good moisture = sleep for 6 hours
- Battery-aware operation
- **Target:** 12-18 months battery life

#### Phase 3.4: Ultra-Low Power
- Minimize wake time to < 3 seconds
- Optimize sensor read sequence
- Optional solar charging support
- **Target:** 18-24 months battery life

### Other Future Ideas
- Multiple soil sensor support (monitor multiple plants)
- Calibration system (auto-detect dry/wet values)
- Local data logging to flash memory
- OTA (Over-The-Air) firmware updates via Zigbee
- NeoPixel status indicators

## References

### Hardware
- [Glyph C6 Documentation](https://docs.glyphworks.com/glyph-c6)
- [Adafruit ESP32-C6 Feather Pinout](https://learn.adafruit.com/adafruit-esp32-c6-feather)
- [Adafruit STEMMA Soil Sensor](https://www.adafruit.com/product/4026)
- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)

### Software
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [ESP Zigbee SDK Documentation](https://github.com/espressif/esp-zigbee-sdk)
- [Zigbee2MQTT Documentation](https://www.zigbee2mqtt.io/)
- [Home Assistant Documentation](https://www.home-assistant.io/)

### Project Documentation
- [ROADMAP.md](ROADMAP.md) - Complete project roadmap and implementation details
- [BUILD_AND_TEST.md](BUILD_AND_TEST.md) - Build instructions and test results

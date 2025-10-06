# Glyph C6 Zigbee Monitor

Zigbee-enabled monitoring project for Glyph C6 board (ESP32-C6-MINI-1 module) with battery monitoring and remote LED control.

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
- **I2C (STEMMA QT)**:
  - `GPIO19`: SDA (5kΩ pullup)
  - `GPIO18`: SCL (5kΩ pullup)

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
├── sdkconfig.defaults          # Default SDK configuration
├── partitions.csv              # Flash partition table
└── main/
    ├── CMakeLists.txt          # Main component CMake
    ├── idf_component.yml       # Component dependencies
    ├── Kconfig.projbuild       # Project configuration options
    ├── main.c                  # Main application code
    ├── zigbee_core.c           # Zigbee stack implementation
    ├── zigbee_core.h           # Zigbee core header
    ├── battery_monitoring.c    # Battery voltage/percentage monitoring
    ├── battery_monitoring.h    # Battery monitoring header
    └── system_config.h         # System-wide configuration
```

## Current Features

- ✅ **Zigbee Integration**
  - Simple Sensor device (HA_ESP_SENSOR_ENDPOINT)
  - On/Off cluster for remote LED control
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
  - GPIO initialization (LED, NeoPixel/I2C power)
  - Basic system information logging
  - NVS (Non-Volatile Storage) initialization
  - FreeRTOS task management
  - 10dBm TX power for stability with USB power

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

To use this device with Zigbee2MQTT:

1. **Enable pairing mode** in Zigbee2MQTT
2. **Power on** the Glyph C6 device
3. **Wait for pairing** - device will show "Zigbee JOINED ✅" in logs
4. **Add custom converter** (if needed):
   - The device uses model ID `GLYPH_C6_M1`
   - You may need to create a custom Z2M converter for full functionality
   - Exposes: LED switch, battery percentage, battery voltage

## Future Enhancements

1. Add temperature/humidity sensor support
2. Implement current/power monitoring
3. Add OTA (Over-The-Air) update support
4. Implement deep sleep for battery optimization
5. Add NeoPixel status indicators

## References

- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [ESP Zigbee SDK Documentation](https://github.com/espressif/esp-zigbee-sdk)
- [Adafruit ESP32-C6 Feather Pinout](https://learn.adafruit.com/adafruit-esp32-c6-feather)

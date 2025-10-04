# Glyph C6 Monitor

AC Power monitoring project for Glyph C6 board (ESP32-C6-MINI-1 module) with Zigbee support.

## Hardware

- **Board**: Glyph C6 (based on Adafruit ESP32-C6 Feather design)
- **Module**: ESP32-C6-MINI-1
- **Flash**: 4MB
- **CPU**: RISC-V single-core @ 160MHz
- **Protocols**: WiFi 6, Bluetooth 5.3 LE, Zigbee, Thread (802.15.4)

## Pin Mapping

Based on Adafruit ESP32-C6 Feather pinout:

### Status LEDs
- `GPIO15`: Onboard Red LED
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
- `GPIO0`: A0 (ADC1_CH0)
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
1. **ESP-IDF** (v5.3+):
   ```bash
   # Already installed at:
   /opt/esp/esp-idf/
   ```

2. **ESP Zigbee SDK** (v1.0+):
   ```bash
   # Already installed at:
   /opt/esp-zigbee-sdk/
   ```

### Environment Setup
```bash
# Set up ESP-IDF environment
. /opt/esp/esp-idf/export.sh

# Set Zigbee SDK path
export ESP_ZIGBEE_SDK=/opt/esp-zigbee-sdk
```

## Build Instructions

```bash
# Navigate to project directory
cd /Users/rjawale/GDrive/Tech/esp32_ac_monitor/glyph-c6-monitor

# Set up environment (if not already done)
. /opt/esp/esp-idf/export.sh
export ESP_ZIGBEE_SDK=/opt/esp-zigbee-sdk

# Configure the project (optional)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/tty.usbserial-<YOUR_PORT> flash monitor

# Or just monitor (if already flashed)
idf.py -p /dev/tty.usbserial-<YOUR_PORT> monitor
```

## Project Structure

```
glyph-c6-monitor/
├── CMakeLists.txt              # Root CMake file
├── README.md                   # This file
├── sdkconfig.defaults          # Default SDK configuration
├── partitions.csv              # Flash partition table
└── main/
    ├── CMakeLists.txt          # Main component CMake
    ├── idf_component.yml       # Component dependencies
    ├── Kconfig.projbuild       # Project configuration options
    └── main.c                  # Main application code
```

## Current Features

- ✅ GPIO initialization (LED, NeoPixel/I2C power)
- ✅ LED blink demonstration
- ✅ Basic system information logging
- ✅ NVS (Non-Volatile Storage) initialization
- ⏳ Zigbee support (TODO)
- ⏳ Sensor integration (TODO)
- ⏳ Power monitoring logic (TODO)

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

## Next Steps

1. Add Zigbee end device implementation
2. Integrate power monitoring sensors
3. Implement data reporting logic
4. Add OTA (Over-The-Air) update support

## References

- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [ESP Zigbee SDK Documentation](https://github.com/espressif/esp-zigbee-sdk)
- [Adafruit ESP32-C6 Feather Pinout](https://learn.adafruit.com/adafruit-esp32-c6-feather)

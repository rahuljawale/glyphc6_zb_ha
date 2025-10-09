# Quick Start Guide - Glyph C6 Monitor

## First Time Setup

### 1. Set Environment Variables
```bash
# Add to your ~/.zshrc or ~/.bashrc
export IDF_PATH=/opt/esp/esp-idf
export ESP_ZIGBEE_SDK=/opt/esp-zigbee-sdk

# Then reload
source ~/.zshrc  # or source ~/.bashrc
```

### 2. Activate ESP-IDF Environment
```bash
# Navigate to project
cd glyphc6_zb_ha

# Activate ESP-IDF
. /opt/esp/esp-idf/export.sh
```

## Common Commands

### Build Project
```bash
idf.py build
```

### Flash to Device
```bash
# Find your port first
ls /dev/tty.usb*

# Flash (replace with your actual port)
idf.py -p /dev/tty.usbserial-XXXXXX flash
```

### Monitor Serial Output
```bash
idf.py -p /dev/tty.usbserial-XXXXXX monitor

# Exit monitor: Ctrl+]
```

### Flash + Monitor (Combined)
```bash
idf.py -p /dev/tty.usbserial-XXXXXX flash monitor
```

### Clean Build
```bash
# Soft clean (recommended)
idf.py clean

# Full clean (removes all build artifacts)
idf.py fullclean
```

### Configure Project
```bash
idf.py menuconfig

# Navigate to "Glyph C6 Monitor Configuration" for project-specific options
```

## Expected Output

When you first flash and run the project, you should see:

```
===========================================
  Glyph C6 Monitor
  Board: ESP32-C6-MINI-1
  Version: 1.0.0
===========================================
I (xxx) GLYPH_C6: NVS initialized
I (xxx) GLYPH_C6: Initializing GPIO pins...
I (xxx) GLYPH_C6: GPIO initialization complete
I (xxx) GLYPH_C6: LED pin: GPIO15
I (xxx) GLYPH_C6: NeoPixel/I2C Power: GPIO20 (enabled)
I (xxx) GLYPH_C6: Chip: ESP32-C6
I (xxx) GLYPH_C6: CPU Cores: 1
I (xxx) GLYPH_C6: Silicon Revision: 0
I (xxx) GLYPH_C6: Flash: 4 MB embedded
I (xxx) GLYPH_C6: Application started successfully
I (xxx) GLYPH_C6: Free heap: XXXXX bytes
I (xxx) GLYPH_C6: Starting LED blink task
I (xxx) GLYPH_C6: LED: ON
I (xxx) GLYPH_C6: LED: OFF
...
```

The onboard LED (GPIO15) should blink every second.

## Troubleshooting

### Can't Find Serial Port
```bash
# macOS - look for usbserial devices
ls -la /dev/tty.usb*

# Check if device is connected
system_profiler SPUSBDataType | grep -A 10 "USB Serial"
```

### Permission Denied on Port
```bash
# macOS - usually not needed, but if required:
sudo chmod 666 /dev/tty.usbserial-XXXXXX
```

### Build Fails with "component not found"
```bash
# Make sure ESP_ZIGBEE_SDK is set
echo $ESP_ZIGBEE_SDK

# Should output: /opt/esp-zigbee-sdk
# If not, export it again
export ESP_ZIGBEE_SDK=/opt/esp-zigbee-sdk
```

### "idf.py: command not found"
```bash
# Re-run the export script
. /opt/esp/esp-idf/export.sh
```

## File Locations

- **Project Root**: `glyphc6_zb_ha/`
- **ESP-IDF**: `/opt/esp/esp-idf` (or your ESP-IDF installation path)
- **Zigbee SDK**: Managed via `idf_component.yml`
- **Build Output**: `./build/`
- **Binary File**: `./build/glyph_c6_monitor.bin`

## Next Development Steps

1. Test basic LED blink (current state)
2. Add I2C sensor support
3. Implement Zigbee end device
4. Add power monitoring logic
5. Implement data reporting

## Useful Monitor Commands

While in monitor mode (`idf.py monitor`):
- `Ctrl+]` - Exit monitor
- `Ctrl+T` then `Ctrl+H` - Show all keyboard shortcuts
- `Ctrl+T` then `Ctrl+R` - Reset chip
- `Ctrl+T` then `Ctrl+F` - Toggle output display

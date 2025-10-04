# Glyph C6 Monitor - Zigbee Integration

## Overview

This project has been upgraded with Zigbee support, enabling integration with Home Assistant, Zigbee2MQTT, and other Zigbee coordinators.

## Features Added

1. **Zigbee Core Module** (`zigbee_core.c/h`)
   - Complete Zigbee stack initialization
   - Network joining and commissioning
   - Cluster management (Basic, Identify, On/Off)
   - Action handler registration

2. **LED Remote Control**
   - Control the onboard LED (GPIO15) via Zigbee
   - LED blinks fast when not joined to network
   - LED blinks slow when joined to network
   - LED can be controlled remotely via On/Off cluster

3. **System Configuration** (`system_config.h`)
   - Centralized configuration for all system parameters
   - Pin definitions, task priorities, Zigbee settings
   - Easy to modify and maintain

## File Structure

```
glyph-c6-monitor/
├── main/
│   ├── main.c                  # Application entry point with Zigbee integration
│   ├── zigbee_core.c          # Zigbee stack implementation
│   ├── zigbee_core.h          # Zigbee module interface
│   ├── system_config.h        # System configuration
│   ├── CMakeLists.txt         # Build configuration
│   └── idf_component.yml      # Dependencies
├── CMakeLists.txt             # Root build configuration
├── sdkconfig.defaults         # SDK configuration (Zigbee enabled)
├── partitions.csv             # Partition table
└── ZIGBEE_SETUP.md           # This file
```

## Building the Project

### Prerequisites

1. **ESP-IDF 5.5 or later**
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

2. **ESP32-C6 toolchain** (automatically installed with ESP-IDF)

### Build Steps

1. **Clean build directory** (recommended for first Zigbee build):
   ```bash
   cd /path/to/glyph-c6-monitor
   rm -rf build
   ```

2. **Build the project**:
   ```bash
   idf.py build
   ```

3. **Flash to device**:
   ```bash
   idf.py -p /dev/tty.usbmodem101 flash monitor
   ```

## Zigbee Configuration

### Device Information

- **Manufacturer**: ESPRESSIF
- **Model**: GLYPH_C6_M1
- **Device Type**: End Device (ZED)
- **Power Source**: DC Source
- **Profile**: Home Automation (HA)
- **Device ID**: On/Off Light

### Clusters Supported

1. **Basic Cluster** (0x0000)
   - ZCL Version
   - Manufacturer Name
   - Model Identifier
   - Power Source
   - Device Enabled

2. **Identify Cluster** (0x0003)
   - Identify Time

3. **On/Off Cluster** (0x0006)
   - On/Off state for LED control

### Network Settings

- **Role**: End Device (sleepy end device capability)
- **Keep Alive**: 3000ms
- **Aging Timeout**: 64 minutes
- **Install Code Policy**: Disabled (for easier pairing)
- **Channel Mask**: All channels (11-26)

## Pairing with Zigbee Coordinator

### Zigbee2MQTT

1. **Enable pairing mode** in Zigbee2MQTT web interface
2. **Power on** the Glyph C6 device or press reset
3. **Wait for device** to appear in Zigbee2MQTT
4. **Device will show up as**: "ESPRESSIF GLYPH_C6_M1"
5. **Control the LED**: Turn on/off via Zigbee2MQTT interface

### Home Assistant (ZHA)

1. **Go to** Settings → Devices & Services → Zigbee Home Automation
2. **Click** "Add Device"
3. **Power on** the Glyph C6 device or press reset
4. **Wait** for device discovery
5. **The device** will appear as a light switch
6. **Control** via Home Assistant dashboard

### Manual Pairing

If the device doesn't pair automatically:

1. **Factory reset** the device by erasing flash:
   ```bash
   idf.py -p /dev/tty.usbmodem101 erase_flash
   idf.py -p /dev/tty.usbmodem101 flash
   ```

2. **Enable pairing mode** on your coordinator
3. **Reset the device** within 60 seconds

## LED Behavior

### Status Indication

- **Fast Blink (500ms)**: Not joined to Zigbee network
- **Slow Blink (2s)**: Joined to Zigbee network
- **Steady On/Off**: Remote control active

### Remote Control

Once paired, you can control the LED via:
- Zigbee2MQTT web interface
- Home Assistant dashboard
- Any Zigbee-compatible controller

The LED will maintain its remote-controlled state until changed again.

## Serial Monitor Output

### Startup Sequence

```
I (261) GLYPH_C6: Glyph C6 Monitor with Zigbee
I (264) GLYPH_C6: Board: ESP32-C6-MINI-1
I (268) GLYPH_C6: Version: 1.1.0
I (283) GLYPH_C6: NVS initialized
I (284) GLYPH_C6: GPIO initialization complete
I (312) GLYPH_C6: Initializing Zigbee SDK...
I (350) ZIGBEE_CORE: Zigbee stack initialized successfully
I (360) GLYPH_C6: Zigbee device ready for commissioning
```

### Network Joining

```
I (5234) ZIGBEE_CORE: Start network steering
I (12458) ZIGBEE_CORE: Joined network successfully!
I (12460) ZIGBEE_CORE: PAN ID: 0x1234, Channel:15
I (12462) ZIGBEE_CORE: Zigbee reporting ready
I (12465) GLYPH_C6: LED: ON (Zigbee: JOINED)
```

### Remote Control

```
I (45678) GLYPH_C6: Remote LED control: ON
I (45680) GLYPH_C6: LED: ON
```

## Adding More Features

The modular architecture makes it easy to add new features:

### Example: Adding a Temperature Sensor

1. **Add sensor reading** in `main.c`:
   ```c
   float temperature = read_temperature_sensor();
   ```

2. **Create Temperature Cluster** in `zigbee_core.c`:
   ```c
   esp_zb_temperature_meas_cluster_cfg_t temp_cfg = {
       .measured_value = (int16_t)(temperature * 100),
       .min_value = -4000,
       .max_value = 8000,
   };
   ```

3. **Report to Zigbee** using scheduler:
   ```c
   esp_zb_zcl_set_attribute_val(...);
   ```

### Example: Adding Battery Monitoring

1. **Add battery reading** in `main.c`
2. **Create Power Config Cluster** with battery attributes
3. **Report battery percentage** and voltage

See the `ac_power_monitor` project for a complete example with battery monitoring, ADC reading, and NeoPixel control.

## Troubleshooting

### Build Errors

**Error**: `esp_zigbee_core.h: No such file or directory`
- **Solution**: Ensure `CONFIG_ZB_ENABLED=y` is in `sdkconfig.defaults`
- **Solution**: Run `idf.py fullclean && idf.py build`

**Error**: Linker errors about undefined Zigbee functions
- **Solution**: Check `managed_components` directory has `esp-zigbee-lib`
- **Solution**: Run `idf.py reconfigure`

### Runtime Issues

**Device not joining network**:
1. Check coordinator is in pairing mode
2. Verify Zigbee channel matches coordinator
3. Check device logs for errors
4. Try factory reset (erase flash)

**LED not responding to remote control**:
1. Verify device is joined (slow blink)
2. Check Zigbee2MQTT or HA logs
3. Ensure On/Off cluster is properly configured
4. Try sending command manually via Zigbee2MQTT

**Device keeps resetting**:
1. Check power supply (USB should provide stable 5V)
2. Review serial monitor for crash logs
3. Reduce stack sizes if memory is constrained
4. Check for infinite loops or blocking operations

## Next Steps

1. **Add sensors**: Temperature, humidity, motion, etc.
2. **Add actuators**: Relays, motors, servos
3. **Battery monitoring**: Add MAX17048 fuel gauge
4. **NeoPixel status**: Visual feedback with RGB LED
5. **Deep sleep**: Reduce power consumption
6. **OTA updates**: Over-the-air firmware updates
7. **Custom clusters**: Create application-specific clusters

## Resources

- [ESP-IDF Zigbee Documentation](https://docs.espressif.com/projects/esp-zigbee-sdk/)
- [Zigbee2MQTT](https://www.zigbee2mqtt.io/)
- [Home Assistant ZHA](https://www.home-assistant.io/integrations/zha/)
- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [Adafruit ESP32-C6 Feather Guide](https://learn.adafruit.com/adafruit-esp32-c6-feather)

## Support

For issues or questions:
1. Check serial monitor output for error messages
2. Review ESP-IDF Zigbee examples
3. Refer to the `ac_power_monitor` project for advanced examples
4. Check ESP-IDF forums and GitHub issues

## License

This project uses ESP-IDF which is licensed under Apache 2.0.


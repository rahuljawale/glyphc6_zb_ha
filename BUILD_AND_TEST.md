# Quick Build and Test Guide

## Prerequisites

```bash
# Ensure ESP-IDF is installed and activated
. $HOME/esp/esp-idf/export.sh

# Or if installed elsewhere
. /opt/esp/esp-idf/export.sh
```

## Build Commands

```bash
# Navigate to project directory
cd glyphc6_zb_ha

# Clean build (recommended for first Zigbee build)
rm -rf build

# Build the project
idf.py build

# If you see dependency errors, try:
idf.py reconfigure
idf.py fullclean
idf.py build
```

## Flash and Monitor

```bash
# Flash to device (adjust port as needed)
idf.py -p /dev/tty.usbmodem101 flash

# Monitor output
idf.py -p /dev/tty.usbmodem101 monitor

# Or do both in one command
idf.py -p /dev/tty.usbmodem101 flash monitor

# Exit monitor: Ctrl+]
```

## Expected Output

### Startup
```
I (261) GLYPH_C6: ===========================================
I (264) GLYPH_C6:   Glyph C6 Monitor with Zigbee
I (268) GLYPH_C6:   Board: ESP32-C6-MINI-1
I (271) GLYPH_C6:   Version: 1.1.0
I (283) GLYPH_C6: NVS initialized
I (284) GLYPH_C6: GPIO initialization complete
I (312) GLYPH_C6: Initializing Zigbee SDK...
I (350) ZIGBEE_CORE: Zigbee stack initialized successfully
I (360) GLYPH_C6: Zigbee device ready for commissioning
```

### LED Blinking (Not Joined)
```
I (1000) GLYPH_C6: LED: ON (Zigbee: NOT JOINED)
I (1500) GLYPH_C6: LED: OFF (Zigbee: NOT JOINED)
I (2000) GLYPH_C6: LED: ON (Zigbee: NOT JOINED)
```

### Network Joining
```
I (5234) ZIGBEE_CORE: Start network steering
I (12458) ZIGBEE_CORE: Joined network successfully!
I (12460) ZIGBEE_CORE: Extended PAN ID: 01:23:45:67:89:AB:CD:EF
I (12462) ZIGBEE_CORE: PAN ID: 0x1A2B, Channel:15, Short Address: 0x4C3D
I (12465) GLYPH_C6: LED: ON (Zigbee: JOINED)
```

### Remote Control
```
I (45678) GLYPH_C6: Remote LED control: ON
I (45680) GLYPH_C6: LED: ON
I (52341) GLYPH_C6: Remote LED control: OFF
I (52343) GLYPH_C6: LED: OFF
```

## Testing Checklist

### 1. Build Test
- [ ] Project builds without errors
- [ ] No warnings about missing headers
- [ ] Zigbee library is properly linked

### 2. Boot Test
- [ ] Device boots successfully
- [ ] GPIO initialization completes
- [ ] Zigbee stack initializes
- [ ] LED starts blinking fast (500ms intervals)

### 3. Pairing Test

#### With Zigbee2MQTT:
- [ ] Open Zigbee2MQTT web interface
- [ ] Enable pairing mode
- [ ] Device appears as "ESPRESSIF GLYPH_C6_M1"
- [ ] Device joins successfully
- [ ] LED changes to slow blink (2s intervals)

#### With Home Assistant:
- [ ] Go to ZHA integration
- [ ] Click "Add Device"
- [ ] Device is discovered
- [ ] Device appears as light switch
- [ ] LED changes to slow blink (2s intervals)

### 4. Control Test
- [ ] Turn LED ON via Zigbee2MQTT/HA
- [ ] LED turns on
- [ ] Serial output shows "Remote LED control: ON"
- [ ] Turn LED OFF via Zigbee2MQTT/HA
- [ ] LED turns off
- [ ] Serial output shows "Remote LED control: OFF"

### 5. Reconnection Test
- [ ] Power cycle the device
- [ ] Device automatically rejoins network
- [ ] No manual pairing needed
- [ ] Remote control still works

## Troubleshooting

### Build Issues

**Problem**: `esp_zigbee_core.h not found`
```bash
# Solution 1: Reconfigure
idf.py reconfigure

# Solution 2: Clean build
rm -rf build managed_components
idf.py build

# Solution 3: Check sdkconfig.defaults
grep CONFIG_ZB_ENABLED sdkconfig.defaults
# Should show: CONFIG_ZB_ENABLED=y
```

**Problem**: Linker errors
```bash
# Ensure all dependencies are correct
cat main/CMakeLists.txt
# Should include esp-zigbee-lib in REQUIRES

# Clean and rebuild
idf.py fullclean
idf.py build
```

### Runtime Issues

**Problem**: Device won't pair
1. Check coordinator is in pairing mode
2. Try factory reset:
   ```bash
   idf.py -p /dev/tty.usbmodem101 erase_flash
   idf.py -p /dev/tty.usbmodem101 flash
   ```
3. Check serial output for errors

**Problem**: LED not responding
1. Verify device is joined (slow blink)
2. Check Zigbee2MQTT logs
3. Try sending command manually:
   ```yaml
   # In Zigbee2MQTT MQTT messages
   Topic: zigbee2mqtt/FRIENDLY_NAME/set
   Payload: {"state": "ON"}
   ```

**Problem**: Device keeps resetting
1. Check USB power supply
2. Look for crash dumps in serial output
3. Increase task stack sizes in system_config.h

## Next Steps After Successful Test

1. **Add Your Features**:
   - See ZIGBEE_SETUP.md for examples
   - Reference ac_power_monitor for complex implementations

2. **Configure for Your Use Case**:
   - Modify system_config.h for your pins/settings
   - Add custom clusters in zigbee_core.c
   - Implement your application logic in main.c

3. **Optimize**:
   - Tune task priorities
   - Adjust reporting intervals
   - Implement deep sleep if needed

## Quick Commands Reference

```bash
# Build only
idf.py build

# Flash only
idf.py -p /dev/tty.usbmodem101 flash

# Monitor only
idf.py -p /dev/tty.usbmodem101 monitor

# Flash + Monitor
idf.py -p /dev/tty.usbmodem101 flash monitor

# Clean build
idf.py fullclean

# Size analysis
idf.py size

# Menuconfig (advanced)
idf.py menuconfig

# Erase flash (factory reset)
idf.py -p /dev/tty.usbmodem101 erase_flash
```

## Serial Monitor Tips

```bash
# Monitor with custom baud rate
idf.py -p /dev/tty.usbmodem101 -b 115200 monitor

# Save monitor output to file
idf.py -p /dev/tty.usbmodem101 monitor | tee monitor_output.log

# Exit monitor
# Press: Ctrl + ]

# Reset device in monitor
# Press: Ctrl + T, then Ctrl + R
```

## Success Criteria

Your integration is successful when:
- ✅ Project builds without errors
- ✅ Device boots and initializes
- ✅ LED blinks fast when not paired
- ✅ Device pairs with coordinator
- ✅ LED blinks slow when paired
- ✅ Remote control works correctly
- ✅ Device survives power cycle
- ✅ All expected log messages appear

## Support

If you encounter issues:
1. Check ZIGBEE_SETUP.md for detailed troubleshooting
2. Review CHANGES_SUMMARY.md to understand the implementation
3. Compare with ac_power_monitor for reference
4. Check ESP-IDF forums and documentation


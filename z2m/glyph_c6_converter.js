/**
 * Zigbee2MQTT External Converter for Glyph C6 Monitor
 * Based on working Adafruit Feather ESP32-C6 converter pattern
 * 
 * Installation:
 * 1. Copy this file to your Z2M data directory: data/glyph_c6_converter.js
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - glyph_c6_converter.js
 * 3. Restart Z2M
 * 4. Pair your device
 */

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const definition = {
    zigbeeModel: ['GLYPH_C6_M1'],
    model: 'GLYPH_C6_M1',
    vendor: 'Custom',
    description: 'Glyph C6 Zigbee Monitor with LED Control',
    
    fromZigbee: [
        // LED On/Off control (0x0006 cluster)
        {
            cluster: 'genOnOff',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                if (msg.data.onOff !== undefined) {
                    result.state = msg.data.onOff ? 'ON' : 'OFF';
                }
                return result;
            },
        },
        
        // Battery reports (0x0001 cluster) - if you add Power Config later
        {
            cluster: 'genPowerCfg',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                
                // Battery Percentage (0x0021)
                if (msg.data.batteryPercentageRemaining !== undefined) {
                    result.battery_percent = Math.round(msg.data.batteryPercentageRemaining / 2);
                    result.battery = result.battery_percent;
                }
                
                // Battery Voltage (0x0020) - decisvolts to mV
                // Zigbee uses decisvolts (0.1V units), Z2M expects millivolts
                if (msg.data.batteryVoltage !== undefined) {
                    result.voltage = msg.data.batteryVoltage * 100;  // decisvolts * 100 = mV
                }
                
                return result;
            },
        },
    ],
    
    toZigbee: [
        // LED On/Off control using commands (not attribute writes)
        {
            key: ['state'],
            convertSet: async (entity, key, value, meta) => {
                const endpoint = meta.device.getEndpoint(1);
                const command = value.toUpperCase() === 'ON' ? 'on' : 'off';
                await endpoint.command('genOnOff', command, {});
                return {state: {state: value.toUpperCase()}};
            },
            convertGet: async (entity, key, meta) => {
                const endpoint = meta.device.getEndpoint(1);
                await endpoint.read('genOnOff', ['onOff']);
            },
        },
    ],
    
    exposes: [
        // LED Control
        e.switch().withEndpoint('l1').withDescription('LED remote on/off control'),
        
        // Battery monitoring
        e.battery(),
        e.battery_voltage(),
    ],
    
    // Configure binding and reporting
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        
        try {
            // Bind clusters
            await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff', 'genPowerCfg']);
            logger.info('Glyph C6: Binding successful');
            
            // Try to configure reporting (may not be supported)
            try {
                await endpoint.configureReporting('genPowerCfg', [{
                    attribute: 'batteryPercentageRemaining',
                    minimumReportInterval: 60,     // 1 minute min
                    maximumReportInterval: 3600,   // 1 hour max
                    reportableChange: 20,          // 10% change (0-200 scale)
                }]);
                logger.info('Glyph C6: Battery percentage reporting configured');
            } catch (e) {
                logger.warn('Glyph C6: Could not configure battery percentage reporting: ' + e.message);
            }
            
            try {
                await endpoint.configureReporting('genPowerCfg', [{
                    attribute: 'batteryVoltage',
                    minimumReportInterval: 60,
                    maximumReportInterval: 3600,
                    reportableChange: 2,
                }]);
                logger.info('Glyph C6: Battery voltage reporting configured');
            } catch (e) {
                logger.warn('Glyph C6: Could not configure battery voltage reporting: ' + e.message);
            }
            
            // Read initial states
            await endpoint.read('genOnOff', ['onOff']);
            await endpoint.read('genPowerCfg', ['batteryPercentageRemaining', 'batteryVoltage']);
            logger.info('Glyph C6: Initial states read successfully');
            
        } catch (error) {
            logger.error('Failed to configure Glyph C6: ' + error);
            throw error;
        }
    },
    
    endpoint: (device) => {
        return {'l1': 1};
    },
};

module.exports = definition;


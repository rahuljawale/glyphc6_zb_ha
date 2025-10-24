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
    zigbeeModel: ['PlantMonitor-C6'],
    model: 'PlantMonitor-C6',
    vendor: 'FloraTech',
    description: 'Smart Zigbee Plant Monitor with Soil Moisture & Temperature',
    
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
        
        // Soil Moisture as Humidity (0x0405 cluster)
        {
            cluster: 'msRelativeHumidity',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                if (msg.data.measuredValue !== undefined) {
                    // Zigbee uses 0.01% units (0-10000), convert to 0-100%
                    result.soil_moisture = msg.data.measuredValue / 100.0;
                    result.humidity = result.soil_moisture;  // Also expose as humidity for compatibility
                }
                return result;
            },
        },
        
        // Soil Temperature (0x0402 cluster)
        {
            cluster: 'msTemperatureMeasurement',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                if (msg.data.measuredValue !== undefined) {
                    // Zigbee uses 0.01°C units, convert to °C
                    result.soil_temperature = msg.data.measuredValue / 100.0;
                    result.temperature = result.soil_temperature;  // Also expose as temperature
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
        
        // Soil moisture (as humidity)
        e.humidity().withDescription('Soil moisture percentage'),
        
        // Soil temperature
        e.temperature().withDescription('Soil temperature'),
    ],
    
    // Configure binding and reporting
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(1);
        
        // Bind clusters
        await reporting.bind(endpoint, coordinatorEndpoint, [
            'genOnOff', 
            'genPowerCfg', 
            'msRelativeHumidity', 
            'msTemperatureMeasurement'
        ]);
        
        // Configure reporting
        await endpoint.configureReporting('genPowerCfg', [{
            attribute: 'batteryPercentageRemaining',
            minimumReportInterval: 14400,  // 4 hours min
            maximumReportInterval: 43200,  // 12 hours max
            reportableChange: 20,          // 10% change (0-200 scale)
        }]);
        
        await endpoint.configureReporting('genPowerCfg', [{
            attribute: 'batteryVoltage',
            minimumReportInterval: 14400,  // 4 hours min
            maximumReportInterval: 43200,  // 12 hours max
            reportableChange: 2,
        }]);
        
        // Read initial states
        await endpoint.read('genOnOff', ['onOff']);
        await endpoint.read('genPowerCfg', ['batteryPercentageRemaining', 'batteryVoltage']);
        await endpoint.read('msRelativeHumidity', ['measuredValue']);
        await endpoint.read('msTemperatureMeasurement', ['measuredValue']);
    },
    
    endpoint: (device) => {
        return {'l1': 1};
    },
};

module.exports = definition;


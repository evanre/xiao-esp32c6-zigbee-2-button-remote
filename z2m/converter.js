/**
 * Custom Zigbee2MQTT converter for DIY ESP32-C6 Two-Button Remote
 *
 * Installation:
 * 1. Copy this file to your Zigbee2MQTT data directory (usually /opt/zigbee2mqtt/data)
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - diy_two_button_remote.js
 * 3. Restart Zigbee2MQTT
 */

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const definition = {
    zigbeeModel: ['TwoBtnRemote'],
    model: 'DIY-TwoBtnRemote',
    vendor: 'DIY',
    description: 'ESP32-C6 two-button Zigbee remote with 3 virtual endpoints',

    // This device has 3 client endpoints that send commands to bound lights
    // It doesn't expose server attributes, so we mainly track battery
    fromZigbee: [
        fz.battery,
        fz.command_on,
        fz.command_off,
        fz.command_toggle,
        fz.command_move,
        fz.command_stop,
        fz.command_move_to_color_temp,
        fz.command_move_color_temperature,
        fz.command_stop_move_step,
    ],

    toZigbee: [],

    exposes: [
        e.battery(),
        e.battery_voltage(),
        // Add action exposes for each endpoint
        exposes.enum('action_ep1', ea.STATE, [
            'on', 'off', 'toggle',
            'brightness_move_up', 'brightness_move_down', 'brightness_stop',
            'color_temperature_move_up', 'color_temperature_move_down', 'color_temperature_stop'
        ]).withEndpoint('1').withDescription('Actions from endpoint 1 (Button 1)'),
        exposes.enum('action_ep2', ea.STATE, [
            'on', 'off', 'toggle',
            'brightness_move_up', 'brightness_move_down', 'brightness_stop',
            'color_temperature_move_up', 'color_temperature_move_down', 'color_temperature_stop'
        ]).withEndpoint('2').withDescription('Actions from endpoint 2 (Button 2)'),
        exposes.enum('action_ep3', ea.STATE, [
            'on', 'off', 'toggle',
            'brightness_move_up', 'brightness_move_down', 'brightness_stop',
            'color_temperature_move_up', 'color_temperature_move_down', 'color_temperature_stop'
        ]).withEndpoint('3').withDescription('Actions from endpoint 3 (Both buttons)'),
    ],

    meta: {
        multiEndpoint: true,
    },

    endpoint: (device) => {
        return {
            'ep1': 1,  // Button 1 -> Lamp 1
            'ep2': 2,  // Button 2 -> Lamp 2
            'ep3': 3,  // Both buttons -> Lamp 3
        };
    },

    configure: async (device, coordinatorEndpoint, logger) => {
        // Bind each endpoint for battery reporting
        const endpoint1 = device.getEndpoint(1);
        const endpoint2 = device.getEndpoint(2);
        const endpoint3 = device.getEndpoint(3);

        // Bind power configuration cluster from endpoint 1 for battery reporting
        await reporting.bind(endpoint1, coordinatorEndpoint, ['genPowerCfg']);
        await reporting.batteryVoltage(endpoint1);
        await reporting.batteryPercentageRemaining(endpoint1);

        // Optional: Bind identify cluster for network management
        await reporting.bind(endpoint1, coordinatorEndpoint, ['genIdentify']);
        await reporting.bind(endpoint2, coordinatorEndpoint, ['genIdentify']);
        await reporting.bind(endpoint3, coordinatorEndpoint, ['genIdentify']);

        logger.info('DIY Two-Button Remote configured successfully');
    },
};

module.exports = definition;

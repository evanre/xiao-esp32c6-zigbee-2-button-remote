/**
 * Custom Zigbee2MQTT converter for DIY ESP32-C6 Two-Button Remote
 *
 * Version: 1.2.0
 * Device Model: TwoBtnRemote
 * Compatible Firmware: ESP-IDF based (post v2.0)
 * Zigbee2MQTT: 1.35.0+
 *
 * Installation:
 * 1. Copy this file to your Zigbee2MQTT data directory (usually /opt/zigbee2mqtt/data)
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - diy_two_button_remote.js
 * 3. Restart Zigbee2MQTT
 *
 * Binding Configuration:
 * - For direct lamp control: Bind endpoint to lamp endpoint (fast, local control)
 * - For event reporting to HA: Bind endpoint to coordinator (actions appear in Z2M/HA)
 * - Best practice: Bind BOTH - you get direct control AND event visibility
 *
 * Note: Multiple bindings per cluster are supported and recommended.
 */

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const definition = {
    zigbeeModel: ['TwoBtnRemote'],
    model: 'DIY-TwoBtnRemote',
    vendor: 'DIY',
    description: 'ESP32-C6 two-button Zigbee remote with 3 virtual endpoints',

    // Device has client endpoints that send commands
    // Commands can be received via binding to coordinator for action reporting
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
        // Action exposes for each endpoint
        // Will report events if endpoints are bound to coordinator
        e.action([
            'on', 'off', 'toggle',
            'brightness_move_up', 'brightness_move_down', 'brightness_stop',
            'color_temperature_move_up', 'color_temperature_move_down', 'color_temperature_stop'
        ]),
    ],

    meta: {
        multiEndpoint: true,
        multiEndpointSkip: ['battery', 'battery_voltage'],
    },

    endpoint: (device) => ({
        'ep1': 1,  // Button 1 -> Lamp 1
        'ep2': 2,  // Button 2 -> Lamp 2
        'ep3': 3,  // Both buttons -> Lamp 3
    }),

    configure: async (device, coordinatorEndpoint, logger) => {
        try {
            const endpoint1 = device.getEndpoint(1);
            const endpoint2 = device.getEndpoint(2);
            const endpoint3 = device.getEndpoint(3);

            // Bind power configuration for battery reporting
            await reporting.bind(endpoint1, coordinatorEndpoint, ['genPowerCfg']);
            await reporting.batteryVoltage(endpoint1);
            await reporting.batteryPercentageRemaining(endpoint1);

            // Bind all endpoints to coordinator for action reporting to Z2M/HA
            // Users can also bind to lamps for direct control - multiple bindings are supported
            await reporting.bind(endpoint1, coordinatorEndpoint, ['genOnOff', 'genLevelCtrl', 'lightingColorCtrl']);
            await reporting.bind(endpoint2, coordinatorEndpoint, ['genOnOff', 'genLevelCtrl', 'lightingColorCtrl']);
            await reporting.bind(endpoint3, coordinatorEndpoint, ['genOnOff', 'genLevelCtrl', 'lightingColorCtrl']);

            logger.info('DIY Two-Button Remote configured successfully');
            logger.info('Endpoints bound to coordinator for action reporting');
            logger.info('Users can also bind endpoints to lamps for direct control');
        } catch (error) {
            logger.error(`Configuration failed: ${error}`);
            // Don't throw - allow device to be usable even if binding fails
        }
    },
};

module.exports = definition;

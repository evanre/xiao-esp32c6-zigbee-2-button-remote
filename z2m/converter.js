/**
 * Custom Zigbee2MQTT converter for DIY ESP32-C6 Two-Button Remote
 *
 * Version: 1.3.0
 * Device Model: TwoBtnRemote
 * Compatible Firmware: ESP-IDF based (post v2.0)
 * Zigbee2MQTT: 1.35.0+
 *
 * Installation:
 * 1. Copy this file to your Zigbee2MQTT data directory (usually /opt/zigbee2mqtt/data)
 * 2. Add to configuration.yaml:
 *    external_converters:
 *      - TwoBtnRemote.js
 * 3. Restart Zigbee2MQTT
 *
 * Binding Configuration:
 * - For direct lamp control: Bind endpoint to lamp endpoint (fast, local control)
 * - For event reporting to HA: Bind endpoint to coordinator (actions appear in Z2M/HA)
 * - Best practice: Bind BOTH - you get direct control AND event visibility
 *
 * Note: Multiple bindings per cluster are supported and recommended.
 */

const {battery, commandsOnOff, commandsLevelCtrl, commandsColorCtrl} = require('zigbee-herdsman-converters/lib/modernExtend');
const {reporting} = require('zigbee-herdsman-converters/lib/reporting');

const definition = {
    zigbeeModel: ['TwoBtnRemote'],
    model: 'TwoBtnRemote',
    vendor: 'DIY',
    description: 'ESP32-C6 two-button Zigbee remote with 3 virtual endpoints',

    // Use modern extends for battery and command handling
    extend: [
        battery({percentageReporting: true, voltageReporting: true}),
        commandsOnOff(),
        commandsLevelCtrl(),
        commandsColorCtrl(),
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
        const endpoint1 = device.getEndpoint(1);
        const endpoint2 = device.getEndpoint(2);
        const endpoint3 = device.getEndpoint(3);

        logger.info('TwoBtnRemote: Starting configuration...');

        try {
            // Bind endpoint 1 for battery reporting
            logger.info('TwoBtnRemote: Binding EP1 genPowerCfg...');
            await endpoint1.bind('genPowerCfg', coordinatorEndpoint);

            // Configure battery reporting using proper reporting API
            logger.info('TwoBtnRemote: Configuring battery reporting...');
            await reporting.batteryVoltage(endpoint1);
            await reporting.batteryPercentageRemaining(endpoint1);

            // Bind all endpoints to coordinator for action reporting
            logger.info('TwoBtnRemote: Binding EP1 control clusters...');
            await endpoint1.bind('genOnOff', coordinatorEndpoint);
            await endpoint1.bind('genLevelCtrl', coordinatorEndpoint);
            await endpoint1.bind('lightingColorCtrl', coordinatorEndpoint);

            logger.info('TwoBtnRemote: Binding EP2 control clusters...');
            await endpoint2.bind('genOnOff', coordinatorEndpoint);
            await endpoint2.bind('genLevelCtrl', coordinatorEndpoint);
            await endpoint2.bind('lightingColorCtrl', coordinatorEndpoint);

            logger.info('TwoBtnRemote: Binding EP3 control clusters...');
            await endpoint3.bind('genOnOff', coordinatorEndpoint);
            await endpoint3.bind('genLevelCtrl', coordinatorEndpoint);
            await endpoint3.bind('lightingColorCtrl', coordinatorEndpoint);

            logger.info('TwoBtnRemote: Configuration complete!');
        } catch (error) {
            logger.error(`TwoBtnRemote: Configuration failed: ${error.message}`);
            logger.error(`TwoBtnRemote: Stack trace: ${error.stack}`);
            throw error;
        }
    },
};

module.exports = definition;

#pragma once

#include "constants.h"

void zigbeeInit();

// On/Off, Level, Color Temperature commands
void cmd_toggle(LampId lampId);
void cmd_level_start(LampId lampId);
void cmd_level_stop(LampId lampId);
void cmd_ct_start(LampId lampId);
void cmd_ct_stop(LampId lampId);
void cmd_empty_action(LampId lampId);

// Battery helpers
uint16_t read_battery_mv();
uint8_t vbat_percent(uint16_t mv);
void report_battery(uint16_t mv);

// Pairing and sleep
void enter_pairing_mode(LampId lampId);
void goto_sleep();

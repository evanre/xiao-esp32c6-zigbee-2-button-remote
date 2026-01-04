#include "debug.h"
#include "driver/gpio.h"

// Global debug mode flag (defined here, declared extern in debug.h)
bool g_debug_mode = false;

// Function to detect debug mode via BOOT button
bool detect_debug_mode()
{
  // Configure BOOT pin as input with pullup
  gpio_config_t boot_cfg = {};
  boot_cfg.intr_type = GPIO_INTR_DISABLE;
  boot_cfg.mode = GPIO_MODE_INPUT;
  boot_cfg.pin_bit_mask = (1ULL << BOOT_PIN);
  boot_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  boot_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&boot_cfg);

  // BOOT button is active-low (pressed = 0)
  bool boot_pressed = (gpio_get_level(BOOT_PIN) == 0);

  return boot_pressed;
}

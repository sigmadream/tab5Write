#include "app_display.h"

#include "bsp/m5stack_tab5.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "TABWRITE_DISPLAY";

void display_set_backlight(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  ESP_ERROR_CHECK(bsp_display_brightness_set(percent));
  ESP_LOGI(TAG, "Backlight set to %u%%", percent);
}

bool app_display_lock(uint32_t timeout_ms) {
  return bsp_display_lock(timeout_ms);
}

void app_display_unlock() { bsp_display_unlock(); }

void app_display_init() {
  ESP_LOGI(TAG, "Initializing Tab5 display via ESP-BSP...");

  if (bsp_display_start() == NULL) {
    ESP_LOGE(TAG, "BSP display initialization failed");
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  display_set_backlight(50);
  ESP_LOGI(TAG, "Display initialized");
}

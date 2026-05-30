#include "app_display.h"

#include "bsp/m5stack_tab5.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

  lv_display_t *display = bsp_display_start();
  if (display == NULL) {
    ESP_LOGE(TAG, "BSP display initialization failed");
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  bsp_display_rotate(display, LV_DISPLAY_ROTATION_90);
  ESP_LOGI(TAG, "Display rotation set to landscape: %ldx%ld",
           static_cast<long>(lv_display_get_horizontal_resolution(display)),
           static_cast<long>(lv_display_get_vertical_resolution(display)));

  display_set_backlight(0);
  vTaskDelay(pdMS_TO_TICKS(150));
  display_set_backlight(50);
  vTaskDelay(pdMS_TO_TICKS(150));
  display_set_backlight(100);
  vTaskDelay(pdMS_TO_TICKS(150));
  display_set_backlight(50);
  ESP_LOGI(TAG, "Display initialized");
}

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>

// LVGL Port
#include "esp_lvgl_port.h"
#include "lvgl.h"

#if __has_include("esp_psram.h")
#include "esp_psram.h"
#endif

#include "app_build_info.h"
#include "app_display.h"
#include "app_queue.h"
#include "app_ui.h"

static const char *TAG = "SCRIBE_MAIN";

QueueHandle_t ui_queue;
QueueHandle_t storage_queue;

void app_queue_init() {
  ui_queue = xQueueCreate(10, sizeof(AppEvent));
  storage_queue = xQueueCreate(10, sizeof(AppEvent));
}

void ui_task(void *pvParameters) {
  ESP_LOGI("SCRIBE_UI", "UI Task started");

  // Show the splash screen
  app_ui_show_splash();

  while (1) {
    // Handle LVGL timers and UI events
    if (lvgl_port_lock(0)) {
      lv_timer_handler();
      lvgl_port_unlock();
    }

    AppEvent evt;
    if (xQueueReceive(ui_queue, &evt, pdMS_TO_TICKS(5))) {
      // Process UI Event
      ESP_LOGI("SCRIBE_UI", "Received UI event");
    }
  }
}

void storage_task(void *pvParameters) {
  ESP_LOGI("SCRIBE_STORAGE", "Storage Task started");
  AppEvent evt;
  while (1) {
    if (xQueueReceive(storage_queue, &evt, portMAX_DELAY)) {
      ESP_LOGI("SCRIBE_STORAGE", "Received storage request");
    }
  }
}

extern "C" void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "Scribe Firmware Booting...");
  ESP_LOGI(TAG, "Build Timestamp: %s", SCRIBE_BUILD_TIMESTAMP);
  ESP_LOGI(TAG, "Target: %s", CONFIG_IDF_TARGET);

  // Print heap info
  ESP_LOGI(TAG, "Free heap size: %lu bytes",
           (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free internal heap: %lu bytes",
           (unsigned long)esp_get_free_internal_heap_size());

  // Initialize display and LVGL
  app_display_init();

  // Initialize queues
  app_queue_init();

  // Create tasks
  xTaskCreate(ui_task, "ui_task", 8192, NULL, 5, NULL);
  xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);

  // Watchdog prevention loop
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

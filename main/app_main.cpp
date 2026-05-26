#include "app_build_info.h"
#include "app_display.h"
#include "app_queue.h"
#include "app_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "TABWRITE_MAIN";

QueueHandle_t ui_queue;
QueueHandle_t storage_queue;

void app_queue_init() {
  ui_queue = xQueueCreate(10, sizeof(AppEvent));
  storage_queue = xQueueCreate(10, sizeof(AppEvent));
}

void ui_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI("TABWRITE_UI", "UI Task started");

  // Show the splash screen
  if (app_display_lock(0)) {
    app_ui_show_splash();
    app_display_unlock();
  }

  while (1) {
    AppEvent evt;
    if (xQueueReceive(ui_queue, &evt, pdMS_TO_TICKS(10))) {
      // Process UI Event
      ESP_LOGI("TABWRITE_UI", "Received UI event");
    }
  }
}

void storage_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI("TABWRITE_STORAGE", "Storage Task started");
  AppEvent evt;
  while (1) {
    if (xQueueReceive(storage_queue, &evt, portMAX_DELAY)) {
      ESP_LOGI("TABWRITE_STORAGE", "Received storage request");
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

  ESP_LOGI(TAG, "TABWRITE Firmware Booting...");
  ESP_LOGI(TAG, "Build Timestamp: %s", TABWRITE_BUILD_TIMESTAMP);
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

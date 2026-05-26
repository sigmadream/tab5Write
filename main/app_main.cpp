#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>

// If using ESP-IDF 5.x/6.x for PSRAM, the API might be esp_psram_get_size()
// or we can just rely on heap caps.
#if __has_include("esp_psram.h")
#include "esp_psram.h"
#endif

#include "app_build_info.h"
#include "app_queue.h"

static const char *TAG = "SCRIBE_MAIN";

QueueHandle_t ui_queue;
QueueHandle_t storage_queue;

void app_queue_init() {
  ui_queue = xQueueCreate(10, sizeof(AppEvent));
  storage_queue = xQueueCreate(10, sizeof(AppEvent));
}

void ui_task(void *pvParameters) {
  ESP_LOGI("SCRIBE_UI", "UI Task started");
  while (1) {
    ESP_LOGI("SCRIBE_UI", "Heartbeat...");
    vTaskDelay(pdMS_TO_TICKS(5000));
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

  // Print PSRAM info
  size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  ESP_LOGI(TAG, "Total PSRAM size: %zu bytes", psram_size);

  // Print partition table
  esp_partition_iterator_t it = esp_partition_find(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  ESP_LOGI(TAG, "Partition Table:");
  while (it != NULL) {
    const esp_partition_t *part = esp_partition_get(it);
    ESP_LOGI(TAG, " - '%s' at offset 0x%lx, size 0x%lx", part->label,
             (unsigned long)part->address, (unsigned long)part->size);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  // Initialize queues
  app_queue_init();

  // Create tasks
  xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
  xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);

  // Watchdog prevention loop
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

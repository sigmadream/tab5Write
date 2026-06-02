#include "app_build_info.h"
#include "app_display.h"
#include "app_input.h"
#include "app_queue.h"
#include "app_ui.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "nvs_flash.h"
#include "text_input_composer.h"


static const char *TAG = "TABWRITE_MAIN";

QueueHandle_t ui_queue;
QueueHandle_t storage_queue;

static void log_partition_table() {
  esp_partition_iterator_t it = esp_partition_find(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (it == NULL) {
    ESP_LOGW(TAG, "Partition table: no partitions found");
    return;
  }

  ESP_LOGI(TAG, "Partition table:");
  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    ESP_LOGI(TAG, "  %s type=0x%02x subtype=0x%02x offset=0x%08" PRIx32
                  " size=0x%08" PRIx32,
             partition->label, partition->type, partition->subtype,
             partition->address, partition->size);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

void app_queue_init() {
  ui_queue = xQueueCreate(10, sizeof(AppEvent));
  storage_queue = xQueueCreate(10, sizeof(AppEvent));
}

void ui_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI("TABWRITE_UI", "UI Task started");

  TextInputComposer composer;
  std::string committed_text = "";

  // Show the splash screen
  if (app_display_lock(0)) {
    app_ui_show_splash();
    app_display_unlock();
  }
  const int64_t splash_until_us = esp_timer_get_time() + 2500000;
  bool startup_screen_active = true;

  while (1) {
    AppEvent evt;
    if (xQueueReceive(ui_queue, &evt, pdMS_TO_TICKS(10))) {
      startup_screen_active = false;
      if (evt.type == AppEventType::KEY_EVENT) {
        // IME를 통해 키 이벤트를 처리하여 변환
        auto tie_events = composer.handle_key_event(evt.key);

        for (const auto &tie : tie_events) {
          if (tie.type == TextInputEventType::COMMIT_TEXT) {
            committed_text += tie.text;
            ESP_LOGI("TABWRITE_UI", "COMMIT_TEXT: %s", tie.text.c_str());
          } else if (tie.type == TextInputEventType::DELETE_BACKWARD) {
            // 안전한 UTF-8 백스페이스
            if (!committed_text.empty()) {
              size_t pop_count = 1;
              while (pop_count < committed_text.size() &&
                     (static_cast<unsigned char>(committed_text[committed_text.size() - pop_count]) & 0xC0) == 0x80) {
                pop_count++;
              }
              committed_text.resize(committed_text.size() - pop_count);
            }
            ESP_LOGI("TABWRITE_UI", "DELETE_BACKWARD");
          } else if (tie.type == TextInputEventType::COMMAND) {
            ESP_LOGI("TABWRITE_UI", "COMMAND: %d", (int)tie.command_code);
            if (tie.command_code == KeyCode::ESCAPE) {
              committed_text.clear();
            }
          }
        }

        if (app_display_lock(50)) {
          app_ui_show_ime_status(committed_text, composer.get_composing_text(),
                                 composer.get_input_mode() == InputMode::KOREAN, evt.key);
          app_display_unlock();
        }
      } else if (evt.type == AppEventType::INPUT_STATUS) {
        ESP_LOGI("TABWRITE_UI", "Input status connected=%d",
                 evt.input_status.connected);
        if (app_display_lock(50)) {
          app_ui_show_input_status(evt.input_status.connected,
                                   evt.input_status.vid,
                                   evt.input_status.pid);
          app_display_unlock();
        }
      } else {
        ESP_LOGI("TABWRITE_UI", "Received UI event");
      }
    } else if (startup_screen_active &&
               esp_timer_get_time() >= splash_until_us) {
      if (app_display_lock(50)) {
        app_ui_show_editor_placeholder();
        app_display_unlock();
      }
      startup_screen_active = false;
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
  ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
  ESP_LOGI(TAG, "Build Timestamp: %s", TABWRITE_BUILD_TIMESTAMP);
  ESP_LOGI(TAG, "Target: %s", CONFIG_IDF_TARGET);

  // Print heap info
  ESP_LOGI(TAG, "Free heap size: %lu bytes",
           (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free internal heap: %lu bytes",
           (unsigned long)esp_get_free_internal_heap_size());
  ESP_LOGI(TAG, "Total PSRAM heap: %lu bytes",
           (unsigned long)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  ESP_LOGI(TAG, "Free PSRAM heap: %lu bytes",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  log_partition_table();

  // Initialize display and LVGL
  app_display_init();

  // Initialize queues
  app_queue_init();

  // Initialize USB keyboard input
  app_input_init();

  // Create tasks
  xTaskCreate(ui_task, "ui_task", 8192, NULL, 5, NULL);
  xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);

  // Watchdog prevention loop
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

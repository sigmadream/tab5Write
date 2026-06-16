#include "app_build_info.h"
#include "app_display.h"
#include "app_input.h"
#include "app_queue.h"
#include "app_ui.h"
#include "editor_core.h"
#include "keybinding.h"
#include "driver/usb_serial_jtag.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include "nvs_flash.h"
#include "text_input_composer.h"


static const char *TAG = "TABWRITE_MAIN";
static const int64_t KEY_TO_RENDER_TARGET_US = 20000;

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

  EditorCore editor;
  TextInputComposer composer;
  std::string toast = "Ready to write";

  if (app_display_lock(0)) {
    app_ui_show_splash();
    app_display_unlock();
  }
  const int64_t splash_until_us = esp_timer_get_time() + 2500000;
  bool startup_screen_active = true;
  uint32_t key_latency_samples = 0;
  int64_t max_key_latency_us = 0;

  while (1) {
    AppEvent evt;
    if (xQueueReceive(ui_queue, &evt, pdMS_TO_TICKS(10))) {
      startup_screen_active = false;
      if (evt.type == AppEventType::KEY_EVENT) {
        const KeybindingResult result = keybinding_dispatch(editor, composer, evt.key);
        if (result.toast_changed) {
          toast = result.toast;
        }

        if (app_display_lock(50)) {
          app_ui_update_writing_screen(editor, composer, toast, &evt.key);
          if (result.snapshot_requested) {
            app_ui_dump_snapshot_over_serial();
          }
          app_display_unlock();
        }

        if (evt.enqueued_at_us > 0) {
          const int64_t latency_us = esp_timer_get_time() - evt.enqueued_at_us;
          max_key_latency_us = latency_us > max_key_latency_us ? latency_us : max_key_latency_us;
          ++key_latency_samples;
          if (latency_us > KEY_TO_RENDER_TARGET_US) {
            ESP_LOGW("TABWRITE_UI", "Key-to-render latency over target: last=%lldus target=%lldus samples=%lu",
                     static_cast<long long>(latency_us),
                     static_cast<long long>(KEY_TO_RENDER_TARGET_US),
                     static_cast<unsigned long>(key_latency_samples));
          } else if (key_latency_samples % 100 == 0) {
            ESP_LOGI("TABWRITE_UI", "Key-to-render latency OK: last=%lldus max=%lldus samples=%lu",
                     static_cast<long long>(latency_us),
                     static_cast<long long>(max_key_latency_us),
                     static_cast<unsigned long>(key_latency_samples));
          }
        }
      } else if (evt.type == AppEventType::INPUT_STATUS) {
        ESP_LOGI("TABWRITE_UI", "Input status connected=%d",
                 evt.input_status.connected);
        if (app_display_lock(50)) {
          app_ui_show_input_status(evt.input_status.connected,
                                   evt.input_status.vid,
                                   evt.input_status.pid);
          app_ui_update_writing_screen(editor, composer, toast, nullptr);
          app_display_unlock();
        }
      } else if (evt.type == AppEventType::UI_COMMAND) {
        if (evt.ui.command == UiCommand::DUMP_SNAPSHOT && app_display_lock(50)) {
          app_ui_update_writing_screen(editor, composer, toast, nullptr);
          app_ui_dump_snapshot_over_serial();
          app_display_unlock();
        }
      } else {
        ESP_LOGI("TABWRITE_UI", "Received UI event");
      }
    } else if (startup_screen_active &&
               esp_timer_get_time() >= splash_until_us) {
      if (app_display_lock(50)) {
        app_ui_show_editor_placeholder();
        app_ui_update_writing_screen(editor, composer, toast, nullptr);
        app_display_unlock();
      }
      startup_screen_active = false;
    }
  }
}


static bool init_usb_serial_jtag_command_rx() {
  if (usb_serial_jtag_is_driver_installed()) {
    return true;
  }

  usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  config.rx_buffer_size = 1024;
  config.tx_buffer_size = 4096;
  const esp_err_t err = usb_serial_jtag_driver_install(&config);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW("TABWRITE_SERIAL", "USB Serial/JTAG driver unavailable: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

static void serial_command_task(void *pvParameters) {
  (void)pvParameters;
  const bool use_usb_serial_jtag = init_usb_serial_jtag_command_rx();
  ESP_LOGI("TABWRITE_SERIAL", "Serial command task started; send 'snapshot' to capture screen");

  char line[32] = {};
  size_t len = 0;
  while (1) {
    char ch = 0;
    const int n = use_usb_serial_jtag
                      ? usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(20))
                      : read(STDIN_FILENO, &ch, 1);
    if (n != 1) {
      if (!use_usb_serial_jtag) {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      continue;
    }

    if (ch == '\r' || ch == '\n') {
      line[len] = '\0';
      if (len > 0 && (strcmp(line, "snapshot") == 0 || strcmp(line, "snap") == 0)) {
        AppEvent evt = {};
        evt.type = AppEventType::UI_COMMAND;
        evt.enqueued_at_us = esp_timer_get_time();
        evt.ui.command = UiCommand::DUMP_SNAPSHOT;
        if (xQueueSend(ui_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
          ESP_LOGW("TABWRITE_SERIAL", "Failed to enqueue snapshot command");
        }
      }
      len = 0;
      continue;
    }

    if (len + 1 < sizeof(line)) {
      line[len++] = ch;
    } else {
      len = 0;
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
  xTaskCreate(ui_task, "ui_task", 16384, NULL, 5, NULL);
  xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);
  xTaskCreate(serial_command_task, "serial_command_task", 4096, NULL, 3, NULL);

  // Watchdog prevention loop
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

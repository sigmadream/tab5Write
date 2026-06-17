#include "app_font.h"

#include "esp_log.h"

LV_FONT_DECLARE(x12y12pxMaruMinyaHangul_36);

namespace {

const char *TAG = "TABWRITE_FONT";
bool initialized = false;

} // namespace

void app_font_init() {
  if (initialized) {
    return;
  }
  initialized = true;
  ESP_LOGI(TAG, "Using preconverted LVGL C font for editor text");
}

const lv_font_t *app_font_editor() {
  return &x12y12pxMaruMinyaHangul_36;
}

const lv_font_t *app_font_ui() {
  return &lv_font_montserrat_20;
}

const lv_font_t *app_font_ui_small() {
  return &lv_font_montserrat_14;
}

const char *app_font_status_text() {
  return "Font built-in C";
}

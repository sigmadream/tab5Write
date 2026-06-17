#pragma once

#include "lvgl.h"

void app_font_init();
const lv_font_t *app_font_editor();
const lv_font_t *app_font_ui();
const lv_font_t *app_font_ui_small();
const char *app_font_status_text();

#pragma once

#include "key_event.h"

void app_ui_show_splash();
void app_ui_show_editor_placeholder();
void app_ui_show_input_status(bool connected, uint16_t vid, uint16_t pid);
void app_ui_show_key_event(const KeyEvent &event);

#pragma once

#include "key_event.h"
#include <string>


void app_ui_show_splash();
void app_ui_show_editor_placeholder();
void app_ui_show_input_status(bool connected, uint16_t vid, uint16_t pid);
void app_ui_show_key_event(const KeyEvent &event);
void app_ui_show_ime_status(const std::string &committed_text, const std::string &composing_text, bool is_korean, const KeyEvent &event);


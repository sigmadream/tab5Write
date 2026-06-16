#pragma once

#include "editor_core.h"
#include "key_event.h"
#include "text_input_composer.h"

#include <string>

void app_ui_show_splash();
void app_ui_show_editor_placeholder();
void app_ui_show_input_status(bool connected, uint16_t vid, uint16_t pid);
void app_ui_show_key_event(const KeyEvent &event);
void app_ui_update_writing_screen(const EditorCore &editor,
                                  const TextInputComposer &composer,
                                  const std::string &toast,
                                  const KeyEvent *last_event = nullptr);

void app_ui_dump_snapshot_over_serial();

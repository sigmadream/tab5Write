#pragma once

#include "lvgl.h"

#include <stddef.h>
#include <string>

lv_obj_t *text_view_create(lv_obj_t *parent);
void text_view_set_document(lv_obj_t *view, const std::string &document_text,
                            size_t cursor_position,
                            const std::string &composing_text);
void text_view_reset_cursor_blink(lv_obj_t *view);
int32_t text_view_visible_line_count(lv_obj_t *view);

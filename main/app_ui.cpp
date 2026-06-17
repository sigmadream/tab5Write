#include "app_ui.h"

#include <stdio.h>

#include "app_font.h"
#include "app_snapshot.h"
#include "esp_log.h"
#include "lvgl.h"
#include "text_view.h"
#include "theme.h"

static const char *TAG = "TABWRITE_UI";

static lv_obj_t *writing_screen;
static lv_obj_t *text_view;
static lv_obj_t *input_status_label;
static lv_obj_t *word_count_label;
static lv_obj_t *mode_label;
static lv_obj_t *memory_label;
static lv_obj_t *toast_label;
static lv_obj_t *debug_label;

static bool keyboard_connected;
static uint16_t keyboard_vid;
static uint16_t keyboard_pid;
static std::string latest_toast;

static const char *printable_text(char printable, char *buffer,
                                  size_t buffer_len) {
  switch (printable) {
  case 0:
    return "-";
  case '\n':
    return "\\n";
  case '\t':
    return "\\t";
  case '\b':
    return "\\b";
  case ' ':
    return "space";
  default:
    snprintf(buffer, buffer_len, "%c", printable);
    return buffer;
  }
}

static void style_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color) {
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
}

static void update_input_status_label() {
  if (input_status_label == nullptr) {
    return;
  }
  if (keyboard_connected) {
    lv_label_set_text_fmt(input_status_label, "Keyboard %04x:%04x", keyboard_vid, keyboard_pid);
  } else {
    lv_label_set_text(input_status_label, "Keyboard disconnected");
  }
}

static void ensure_writing_screen() {
  if (writing_screen != nullptr) {
    return;
  }

  writing_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(writing_screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(writing_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(writing_screen, 0, 0);

  lv_obj_t *header = lv_obj_create(writing_screen);
  lv_obj_set_size(header, lv_pct(100), 48);
  lv_obj_set_style_bg_color(header, THEME_BG_SECONDARY, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_left(header, 24, 0);
  lv_obj_set_style_pad_right(header, 24, 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "TABWRITE");
  style_label(title, app_font_ui(), THEME_TEXT_PRIMARY);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  mode_label = lv_label_create(header);
  lv_label_set_text(mode_label, "EN");
  style_label(mode_label, app_font_ui(), THEME_ACCENT);
  lv_obj_align(mode_label, LV_ALIGN_RIGHT_MID, 0, 0);

  text_view = text_view_create(writing_screen);
  lv_obj_set_size(text_view, lv_pct(100), 620);
  lv_obj_align(text_view, LV_ALIGN_TOP_MID, 0, 48);

  lv_obj_t *footer = lv_obj_create(writing_screen);
  lv_obj_set_size(footer, lv_pct(100), 52);
  lv_obj_set_style_bg_color(footer, THEME_BG_SECONDARY, 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_pad_left(footer, 24, 0);
  lv_obj_set_style_pad_right(footer, 24, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);

  word_count_label = lv_label_create(footer);
  lv_label_set_text(word_count_label, "0 words");
  style_label(word_count_label, app_font_ui_small(), THEME_TEXT_PRIMARY);
  lv_obj_align(word_count_label, LV_ALIGN_LEFT_MID, 0, -9);

  memory_label = lv_label_create(footer);
  lv_label_set_text_fmt(memory_label, "Memory only · %s", app_font_status_text());
  style_label(memory_label, app_font_ui_small(), THEME_TEXT_SECONDARY);
  lv_obj_align(memory_label, LV_ALIGN_LEFT_MID, 0, 12);

  input_status_label = lv_label_create(footer);
  lv_label_set_text(input_status_label, "Keyboard disconnected");
  style_label(input_status_label, app_font_ui_small(), THEME_TEXT_SECONDARY);
  lv_obj_align(input_status_label, LV_ALIGN_RIGHT_MID, 0, -9);

  toast_label = lv_label_create(footer);
  lv_label_set_text(toast_label, "Ready to write");
  style_label(toast_label, app_font_ui_small(), THEME_ACCENT);
  lv_obj_align(toast_label, LV_ALIGN_RIGHT_MID, 0, 12);

  debug_label = lv_label_create(writing_screen);
  lv_label_set_text(debug_label, "Ctrl+Space: EN/KO · Esc: menu placeholder");
  style_label(debug_label, app_font_ui_small(), THEME_TEXT_SECONDARY);
  lv_obj_align(debug_label, LV_ALIGN_BOTTOM_MID, 0, -58);

  update_input_status_label();
}

void app_ui_show_splash() {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ESP_LOGI(TAG, "Showing splash screen");
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "TABWRITE");
  style_label(title, app_font_ui(), THEME_TEXT_PRIMARY);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t *tagline = lv_label_create(screen);
  lv_label_set_text(tagline, "Open. Type. Your words are safe.");
  style_label(tagline, app_font_ui_small(), THEME_TEXT_SECONDARY);
  lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 20);

  lv_scr_load(screen);
}

void app_ui_show_editor_placeholder() {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ESP_LOGI(TAG, "Showing minimal writing screen");
  ensure_writing_screen();
  lv_scr_load(writing_screen);
}

void app_ui_show_input_status(bool connected, uint16_t vid, uint16_t pid) {
  if (lv_display_get_default() == NULL) {
    return;
  }
  keyboard_connected = connected;
  keyboard_vid = vid;
  keyboard_pid = pid;
  ensure_writing_screen();
  update_input_status_label();
}

void app_ui_show_key_event(const KeyEvent &event) {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ensure_writing_screen();
  char printable_buffer[2] = {};
  lv_label_set_text_fmt(debug_label, "%s %s · char %s · modifiers 0x%02x",
                        key_action_name(event.action), key_code_name(event.code),
                        printable_text(event.printable, printable_buffer,
                                       sizeof(printable_buffer)),
                        event.modifiers);
}

void app_ui_update_writing_screen(const EditorCore &editor,
                                  const TextInputComposer &composer,
                                  const std::string &toast,
                                  const KeyEvent *last_event) {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ensure_writing_screen();
  if (lv_scr_act() != writing_screen) {
    lv_scr_load(writing_screen);
  }

  text_view_set_document(text_view, editor.text(), editor.cursor().position,
                         composer.get_composing_text());
  lv_label_set_text_fmt(word_count_label, "%u words · line %u/%u",
                        static_cast<unsigned>(editor.word_count()),
                        static_cast<unsigned>(editor.cursor().line + 1),
                        static_cast<unsigned>(editor.line_count()));
  lv_label_set_text_fmt(memory_label, "Memory only · %s", app_font_status_text());
  lv_label_set_text(mode_label,
                    composer.get_input_mode() == InputMode::KOREAN ? "KO" : "EN");
  lv_obj_set_style_text_color(mode_label,
                              composer.get_input_mode() == InputMode::KOREAN
                                  ? lv_color_hex(0xFFB86C)
                                  : THEME_ACCENT,
                              0);

  if (!toast.empty()) {
    latest_toast = toast;
  }
  lv_label_set_text(toast_label, latest_toast.empty() ? "Ready to write" : latest_toast.c_str());

  if (last_event != nullptr) {
    app_ui_show_key_event(*last_event);
  }
  text_view_reset_cursor_blink(text_view);
}

void app_ui_dump_snapshot_over_serial() {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ensure_writing_screen();
  app_snapshot_dump_obj_over_serial(lv_scr_act(), "active_screen");
}

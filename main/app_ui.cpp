#include "app_ui.h"

#include <stdio.h>

#include "lvgl.h"
#include "esp_log.h"
#include "theme.h"

static const char *TAG = "TABWRITE_UI";

static lv_obj_t *input_screen;
static lv_obj_t *editor_placeholder_screen;
static lv_obj_t *status_label;
static lv_obj_t *key_label;
static lv_obj_t *char_label;
static lv_obj_t *modifier_label;

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

static void ensure_input_screen() {
  if (input_screen != NULL) {
    return;
  }

  input_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(input_screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(input_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(input_screen, 28, 0);

  lv_obj_t *title = lv_label_create(input_screen);
  lv_label_set_text(title, "TABWRITE INPUT");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  status_label = lv_label_create(input_screen);
  lv_label_set_text(status_label, "keyboard: disconnected");
  lv_obj_set_style_text_color(status_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 46);

  key_label = lv_label_create(input_screen);
  lv_label_set_text(key_label, "key: -");
  lv_obj_set_style_text_color(key_label, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(key_label, &lv_font_montserrat_20, 0);
  lv_obj_align(key_label, LV_ALIGN_TOP_LEFT, 0, 96);

  char_label = lv_label_create(input_screen);
  lv_label_set_text(char_label, "char: -");
  lv_obj_set_style_text_color(char_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(char_label, &lv_font_montserrat_14, 0);
  lv_obj_align(char_label, LV_ALIGN_TOP_LEFT, 0, 142);

  modifier_label = lv_label_create(input_screen);
  lv_label_set_text(modifier_label, "modifiers: 0x00");
  lv_obj_set_style_text_color(modifier_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(modifier_label, &lv_font_montserrat_14, 0);
  lv_obj_align(modifier_label, LV_ALIGN_TOP_LEFT, 0, 184);

  lv_scr_load(input_screen);
}

void app_ui_show_splash() {
  if (lv_display_get_default() == NULL) {
    return; // No display initialized yet
  }
  ESP_LOGI(TAG, "Showing splash screen");
  // Create screen
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Main title
  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "TABWRITE");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  // Tagline
  lv_obj_t *tagline = lv_label_create(screen);
  lv_label_set_text(tagline, "Open. Type. Your words are safe.");
  lv_obj_set_style_text_color(tagline, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(tagline, &lv_font_montserrat_14, 0);
  lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 20);

  lv_scr_load(screen);
}

void app_ui_show_editor_placeholder() {
  if (lv_display_get_default() == NULL) {
    return;
  }
  ESP_LOGI(TAG, "Showing editor placeholder screen");

  if (editor_placeholder_screen == NULL) {
    editor_placeholder_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(editor_placeholder_screen, THEME_BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(editor_placeholder_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(editor_placeholder_screen, 28, 0);

    lv_obj_t *title = lv_label_create(editor_placeholder_screen);
    lv_label_set_text(title, "Editor ready");
    lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *body = lv_label_create(editor_placeholder_screen);
    lv_label_set_text(body, "Keyboard input debug is available.");
    lv_obj_set_style_text_color(body, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 46);
  }

  lv_scr_load(editor_placeholder_screen);
}

void app_ui_show_input_status(bool connected, uint16_t vid, uint16_t pid) {
  if (lv_display_get_default() == NULL) {
    return;
  }

  ensure_input_screen();
  if (connected) {
    lv_label_set_text_fmt(status_label, "keyboard: connected VID=%04x PID=%04x",
                          vid, pid);
  } else {
    lv_label_set_text(status_label, "keyboard: disconnected");
  }
}

void app_ui_show_key_event(const KeyEvent &event) {
  if (lv_display_get_default() == NULL) {
    return;
  }

  ensure_input_screen();
  char printable_buffer[2] = {};
  lv_label_set_text_fmt(key_label, "key: %s %s", key_action_name(event.action),
                        key_code_name(event.code));
  lv_label_set_text_fmt(char_label, "char: %s",
                        printable_text(event.printable, printable_buffer,
                                       sizeof(printable_buffer)));
  lv_label_set_text_fmt(modifier_label, "modifiers: 0x%02x",
                        event.modifiers);
}

#include "app_ui.h"

#include <stdio.h>
#include <string>

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

// 프리미엄 IME 에디터 뷰용 추가 오브젝트
static lv_obj_t *mode_badge;
static lv_obj_t *editor_container;
static lv_obj_t *editor_text_label;
static lv_obj_t *debug_panel;
static lv_obj_t *ime_debug_label;

LV_FONT_DECLARE(x12y12pxMaruMinyaHangul_36);


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
  lv_obj_set_style_pad_all(input_screen, 24, 0);

  // 상단 바 컨테이너
  lv_obj_t *header = lv_obj_create(input_screen);
  lv_obj_set_size(header, lv_pct(100), 48);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

  // 상단 타이틀
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "TABWRITE COMPOSER");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  // 입력 모드 뱃지 (KO / EN)
  mode_badge = lv_obj_create(header);
  lv_obj_set_size(mode_badge, 64, 30);
  lv_obj_set_style_bg_color(mode_badge, lv_color_hex(0x8BE9FD), 0); // 기본 민트색 (EN)
  lv_obj_set_style_bg_opa(mode_badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(mode_badge, 8, 0);
  lv_obj_set_style_border_width(mode_badge, 0, 0);
  lv_obj_set_style_pad_all(mode_badge, 0, 0);
  lv_obj_align(mode_badge, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *mode_label = lv_label_create(mode_badge);
  lv_label_set_text(mode_label, "EN");
  lv_obj_set_style_text_color(mode_label, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_14, 0);
  lv_obj_align(mode_label, LV_ALIGN_CENTER, 0, 0);

  // 프리미엄 에디터 박스 컨테이너
  editor_container = lv_obj_create(input_screen);
  lv_obj_set_size(editor_container, lv_pct(100), 220);
  lv_obj_set_style_bg_color(editor_container, THEME_BG_SECONDARY, 0);
  lv_obj_set_style_bg_opa(editor_container, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(editor_container, 12, 0);
  lv_obj_set_style_border_color(editor_container, THEME_ACCENT, 0);
  lv_obj_set_style_border_width(editor_container, 2, 0);
  lv_obj_set_style_pad_all(editor_container, 16, 0);
  lv_obj_align(editor_container, LV_ALIGN_TOP_MID, 0, 60);

  // 에디터 텍스트 라벨 (가장 핵심적인 텍스트 노출 공간)
  editor_text_label = lv_label_create(editor_container);
  lv_label_set_text(editor_text_label, "Type something here...");
  lv_obj_set_style_text_color(editor_text_label, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(editor_text_label, &x12y12pxMaruMinyaHangul_36, 0);
  lv_obj_set_width(editor_text_label, lv_pct(100));
  lv_obj_align(editor_text_label, LV_ALIGN_TOP_LEFT, 0, 0);

  // 하단 디버그 패널
  debug_panel = lv_obj_create(input_screen);
  lv_obj_set_size(debug_panel, lv_pct(100), 140);
  lv_obj_set_style_bg_color(debug_panel, THEME_BG_SECONDARY, 0);
  lv_obj_set_style_bg_opa(debug_panel, LV_OPA_80, 0);
  lv_obj_set_style_radius(debug_panel, 8, 0);
  lv_obj_set_style_border_width(debug_panel, 0, 0);
  lv_obj_set_style_pad_all(debug_panel, 12, 0);
  lv_obj_align(debug_panel, LV_ALIGN_BOTTOM_MID, 0, -32);

  // 디버그 라벨들 배치
  status_label = lv_label_create(debug_panel);
  lv_label_set_text(status_label, "keyboard: disconnected");
  lv_obj_set_style_text_color(status_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 0, 0);

  key_label = lv_label_create(debug_panel);
  lv_label_set_text(key_label, "key: -");
  lv_obj_set_style_text_color(key_label, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(key_label, &lv_font_montserrat_14, 0);
  lv_obj_align(key_label, LV_ALIGN_TOP_LEFT, 0, 22);

  char_label = lv_label_create(debug_panel);
  lv_label_set_text(char_label, "char: -");
  lv_obj_set_style_text_color(char_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(char_label, &lv_font_montserrat_14, 0);
  lv_obj_align(char_label, LV_ALIGN_TOP_LEFT, 0, 42);

  modifier_label = lv_label_create(debug_panel);
  lv_label_set_text(modifier_label, "modifiers: 0x00");
  lv_obj_set_style_text_color(modifier_label, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(modifier_label, &lv_font_montserrat_14, 0);
  lv_obj_align(modifier_label, LV_ALIGN_TOP_LEFT, 0, 60);

  ime_debug_label = lv_label_create(debug_panel);
  lv_label_set_text(ime_debug_label, "IME events: waiting...");
  lv_obj_set_style_text_color(ime_debug_label, THEME_ACCENT, 0);
  lv_obj_set_style_text_font(ime_debug_label, &lv_font_montserrat_14, 0);
  lv_obj_align(ime_debug_label, LV_ALIGN_TOP_LEFT, 0, 80);

  // 하단 상태바
  lv_obj_t *footer = lv_label_create(input_screen);
  lv_label_set_text(footer, "Press Ctrl+Space to switch layout (EN / KO)");
  lv_obj_set_style_text_color(footer, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_scr_load(input_screen);
}

void app_ui_show_splash() {
  if (lv_display_get_default() == NULL) {
    return; // 아직 디스플레이가 없음
  }
  ESP_LOGI(TAG, "Showing splash screen");
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "TABWRITE");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

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

void app_ui_show_ime_status(const std::string &committed_text, const std::string &composing_text, bool is_korean, const KeyEvent &event) {
  if (lv_display_get_default() == NULL) {
    return;
  }

  ensure_input_screen();

  // 1. 입력 모드 뱃지 실시간 갱신 (KO: 주황색 피치 계열, EN: 민트 블루 계열)
  lv_obj_t *badge_label = lv_obj_get_child(mode_badge, 0);
  if (is_korean) {
    lv_obj_set_style_bg_color(mode_badge, lv_color_hex(0xFFB86C), 0); // 주황색 피치
    if (badge_label != NULL) {
      lv_label_set_text(badge_label, "KO");
    }
  } else {
    lv_obj_set_style_bg_color(mode_badge, lv_color_hex(0x8BE9FD), 0); // 민트 블루
    if (badge_label != NULL) {
      lv_label_set_text(badge_label, "EN");
    }
  }

  // 2. 가상 에디터 라벨 내용 구성 (committed + composing + cursor)
  // 조합 중인 텍스트는 시각적으로 구분이 쉽도록 [ ] 기호로 감쌉니다.
  std::string display_str = committed_text;
  if (!composing_text.empty()) {
    display_str += "[" + composing_text + "]";
  }
  display_str += "|"; // 커서 추가

  lv_label_set_text(editor_text_label, display_str.c_str());

  // 3. 디버그 및 상태 패널 정보 갱신
  char printable_buffer[2] = {};
  lv_label_set_text_fmt(key_label, "key: %s %s", key_action_name(event.action),
                        key_code_name(event.code));
  lv_label_set_text_fmt(char_label, "char: %s",
                        printable_text(event.printable, printable_buffer,
                                       sizeof(printable_buffer)));
  lv_label_set_text_fmt(modifier_label, "modifiers: 0x%02x",
                        event.modifiers);

  // IME 디버그 정보 업데이트
  if (!composing_text.empty()) {
    lv_label_set_text_fmt(ime_debug_label, "IME state: composing=%s", composing_text.c_str());
  } else {
    lv_label_set_text(ime_debug_label, "IME state: idle");
  }
}

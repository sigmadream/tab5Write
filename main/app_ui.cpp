#include "app_ui.h"
#include "lvgl.h"
#include "theme.h"

void app_ui_show_splash() {
  if (lv_display_get_default() == NULL) {
    return; // No display initialized yet
  }
  // Create screen
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // Main Title
  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "Scribe");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  // Use a larger font if available, fallback to Montserrat 20/14
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  // Tagline
  lv_obj_t *tagline = lv_label_create(screen);
  lv_label_set_text(tagline, "Open. Type. Your words are safe.");
  lv_obj_set_style_text_color(tagline, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(tagline, &lv_font_montserrat_14, 0);
  lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 20);

  lv_scr_load(screen);
}

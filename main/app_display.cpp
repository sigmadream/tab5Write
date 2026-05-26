#include "app_display.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

// LVGL Port
#include "esp_lvgl_port.h"
#include "lvgl.h"

// LCD DSI (Skeleton for Tab5)
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "SCRIBE_DISPLAY";

#define BACKLIGHT_GPIO (22)
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0

void display_set_backlight(uint8_t percent) {
  if (percent > 100)
    percent = 100;
  uint32_t duty = (8191 * percent) / 100; // 13-bit resolution
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
  ESP_LOGI(TAG, "Backlight set to %d%%", percent);
}

static void backlight_init() {
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
  ledc_timer.timer_num = LEDC_TIMER;
  ledc_timer.freq_hz = 5000;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {};
  ledc_channel.gpio_num = BACKLIGHT_GPIO;
  ledc_channel.speed_mode = LEDC_MODE;
  ledc_channel.channel = LEDC_CHANNEL;
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.timer_sel = LEDC_TIMER;
  ledc_channel.duty = 0;
  ledc_channel.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  // Set default to 50%
  display_set_backlight(50);
}

void app_display_init() {
  ESP_LOGI(TAG, "Initializing Display & LVGL...");

  // Backlight Init
  backlight_init();

  // LVGL Port Initialization
  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

  // Note: To fully support M5Stack Tab5 ST7123 MIPI-DSI,
  // the BSP initialization should happen here.
  // If espp/m5stack-tab5 provides it, we would call the bsp init function.
  // For now, we initialize LVGL port.
  // The panel needs to be added to lvgl port using lvgl_port_add_disp()

  ESP_LOGI(TAG, "Display initialized (Placeholder).");
}

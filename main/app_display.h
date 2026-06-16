#pragma once

#include <stdint.h>

void app_display_init();
bool app_display_lock(uint32_t timeout_ms);
void app_display_unlock();
void display_set_backlight(uint8_t percent);

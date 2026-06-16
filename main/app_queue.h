#pragma once

#include "app_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t ui_queue;
extern QueueHandle_t storage_queue;

void app_queue_init();

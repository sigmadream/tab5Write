#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "app_event.h"

extern QueueHandle_t ui_queue;
extern QueueHandle_t storage_queue;

void app_queue_init();

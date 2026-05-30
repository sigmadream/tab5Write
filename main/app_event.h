#pragma once

#include "key_event.h"

#include <stdint.h>

enum class AppEventType { KEY_EVENT, INPUT_STATUS, STORAGE_REQUEST, UI_COMMAND };

struct AppEvent {
  AppEventType type;
  union {
    KeyEvent key;
    struct {
      bool connected;
      uint16_t vid;
      uint16_t pid;
      uint8_t protocol;
    } input_status;
    struct {
      uint32_t command;
    } ui;
    struct {
      uint32_t request_id;
    } storage;
  };
};

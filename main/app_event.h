#pragma once

#include "key_event.h"

#include <stdint.h>

enum class AppEventType { KEY_EVENT, INPUT_STATUS, STORAGE_REQUEST, UI_COMMAND };

enum class UiCommand : uint32_t {
  DUMP_SNAPSHOT = 1,
};

struct AppEvent {
  AppEventType type;
  int64_t enqueued_at_us;
  union {
    KeyEvent key;
    struct {
      bool connected;
      uint16_t vid;
      uint16_t pid;
      uint8_t protocol;
    } input_status;
    struct {
      UiCommand command;
    } ui;
    struct {
      uint32_t request_id;
    } storage;
  };
};

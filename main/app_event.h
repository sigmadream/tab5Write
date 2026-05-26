#pragma once

#include <stdint.h>

enum class AppEventType { KEY_EVENT, STORAGE_REQUEST, UI_COMMAND };

struct AppEvent {
  AppEventType type;
  union {
    struct {
      uint32_t key_code;
      uint32_t action;
      uint32_t modifiers;
      char printable;
    } key;
    struct {
      uint32_t command;
    } ui;
    struct {
      uint32_t request_id;
    } storage;
  };
};

#include "app_input.h"

#include "app_event.h"
#include "app_queue.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "bsp/m5stack_tab5.h"
#include "usb/hid.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/usb_host.h"

static const char *TAG = "TABWRITE_INPUT";

static constexpr TickType_t HID_EVENT_QUEUE_TIMEOUT = pdMS_TO_TICKS(20);
static constexpr uint32_t KEY_REPEAT_DELAY_MS = 500;
static constexpr uint32_t KEY_REPEAT_INTERVAL_MS = 50;
static constexpr size_t HID_REPORT_BUFFER_SIZE = 64;

enum class InputEventGroup : uint8_t {
  HID_HOST,
};

struct InputEvent {
  InputEventGroup group;
  hid_host_device_handle_t handle;
  hid_host_driver_event_t event;
  void *arg;
};

struct PressedKeyState {
  bool active;
  uint8_t usage;
  uint8_t modifiers;
  uint32_t next_repeat_ms;
  uint32_t repeat_count;
};

struct KeyMapExpectation {
  uint8_t usage;
  KeyCode code;
  char normal;
  char shifted;
};

struct ModifierExpectation {
  uint8_t mask;
  KeyCode code;
};

static QueueHandle_t s_hid_event_queue;
static portMUX_TYPE s_key_state_lock = portMUX_INITIALIZER_UNLOCKED;
static PressedKeyState s_pressed_keys[HID_KEYBOARD_KEY_MAX];
static uint8_t s_prev_boot_keys[HID_KEYBOARD_KEY_MAX];
static uint8_t s_prev_modifiers;
static bool s_initialized;
static uint32_t s_keyboard_connect_count;
static uint32_t s_keyboard_disconnect_count;

static uint32_t now_ms() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static bool is_shift_pressed(uint8_t modifiers) {
  return (modifiers & (HID_LEFT_SHIFT | HID_RIGHT_SHIFT)) != 0;
}

static bool key_found(const uint8_t *keys, uint8_t usage) {
  for (int i = 0; i < HID_KEYBOARD_KEY_MAX; ++i) {
    if (keys[i] == usage) {
      return true;
    }
  }
  return false;
}

static KeyCode key_code_from_hid_usage(uint8_t usage) {
  switch (usage) {
  case HID_KEY_A:
    return KeyCode::A;
  case HID_KEY_B:
    return KeyCode::B;
  case HID_KEY_C:
    return KeyCode::C;
  case HID_KEY_D:
    return KeyCode::D;
  case HID_KEY_E:
    return KeyCode::E;
  case HID_KEY_F:
    return KeyCode::F;
  case HID_KEY_G:
    return KeyCode::G;
  case HID_KEY_H:
    return KeyCode::H;
  case HID_KEY_I:
    return KeyCode::I;
  case HID_KEY_J:
    return KeyCode::J;
  case HID_KEY_K:
    return KeyCode::K;
  case HID_KEY_L:
    return KeyCode::L;
  case HID_KEY_M:
    return KeyCode::M;
  case HID_KEY_N:
    return KeyCode::N;
  case HID_KEY_O:
    return KeyCode::O;
  case HID_KEY_P:
    return KeyCode::P;
  case HID_KEY_Q:
    return KeyCode::Q;
  case HID_KEY_R:
    return KeyCode::R;
  case HID_KEY_S:
    return KeyCode::S;
  case HID_KEY_T:
    return KeyCode::T;
  case HID_KEY_U:
    return KeyCode::U;
  case HID_KEY_V:
    return KeyCode::V;
  case HID_KEY_W:
    return KeyCode::W;
  case HID_KEY_X:
    return KeyCode::X;
  case HID_KEY_Y:
    return KeyCode::Y;
  case HID_KEY_Z:
    return KeyCode::Z;
  case HID_KEY_1:
    return KeyCode::DIGIT_1;
  case HID_KEY_2:
    return KeyCode::DIGIT_2;
  case HID_KEY_3:
    return KeyCode::DIGIT_3;
  case HID_KEY_4:
    return KeyCode::DIGIT_4;
  case HID_KEY_5:
    return KeyCode::DIGIT_5;
  case HID_KEY_6:
    return KeyCode::DIGIT_6;
  case HID_KEY_7:
    return KeyCode::DIGIT_7;
  case HID_KEY_8:
    return KeyCode::DIGIT_8;
  case HID_KEY_9:
    return KeyCode::DIGIT_9;
  case HID_KEY_0:
    return KeyCode::DIGIT_0;
  case HID_KEY_ENTER:
    return KeyCode::ENTER;
  case HID_KEY_ESC:
    return KeyCode::ESCAPE;
  case HID_KEY_DEL:
    return KeyCode::BACKSPACE;
  case HID_KEY_TAB:
    return KeyCode::TAB;
  case HID_KEY_SPACE:
    return KeyCode::SPACE;
  case HID_KEY_MINUS:
    return KeyCode::MINUS;
  case HID_KEY_EQUAL:
    return KeyCode::EQUAL;
  case HID_KEY_OPEN_BRACKET:
    return KeyCode::LEFT_BRACKET;
  case HID_KEY_CLOSE_BRACKET:
    return KeyCode::RIGHT_BRACKET;
  case HID_KEY_BACK_SLASH:
    return KeyCode::BACKSLASH;
  case HID_KEY_SHARP:
    return KeyCode::NON_US_HASH;
  case HID_KEY_COLON:
    return KeyCode::SEMICOLON;
  case HID_KEY_QUOTE:
    return KeyCode::QUOTE;
  case HID_KEY_TILDE:
    return KeyCode::GRAVE;
  case HID_KEY_LESS:
    return KeyCode::COMMA;
  case HID_KEY_GREATER:
    return KeyCode::PERIOD;
  case HID_KEY_SLASH:
    return KeyCode::SLASH;
  case HID_KEY_CAPS_LOCK:
    return KeyCode::CAPS_LOCK;
  case HID_KEY_F1:
    return KeyCode::F1;
  case HID_KEY_F2:
    return KeyCode::F2;
  case HID_KEY_F3:
    return KeyCode::F3;
  case HID_KEY_F4:
    return KeyCode::F4;
  case HID_KEY_F5:
    return KeyCode::F5;
  case HID_KEY_F6:
    return KeyCode::F6;
  case HID_KEY_F7:
    return KeyCode::F7;
  case HID_KEY_F8:
    return KeyCode::F8;
  case HID_KEY_F9:
    return KeyCode::F9;
  case HID_KEY_F10:
    return KeyCode::F10;
  case HID_KEY_F11:
    return KeyCode::F11;
  case HID_KEY_F12:
    return KeyCode::F12;
  case HID_KEY_PRINT_SCREEN:
    return KeyCode::PRINT_SCREEN;
  case HID_KEY_SCROLL_LOCK:
    return KeyCode::SCROLL_LOCK;
  case HID_KEY_PAUSE:
    return KeyCode::PAUSE;
  case HID_KEY_INSERT:
    return KeyCode::INSERT;
  case HID_KEY_HOME:
    return KeyCode::HOME;
  case HID_KEY_PAGEUP:
    return KeyCode::PAGE_UP;
  case HID_KEY_DELETE:
    return KeyCode::DELETE_FORWARD;
  case HID_KEY_END:
    return KeyCode::END;
  case HID_KEY_PAGEDOWN:
    return KeyCode::PAGE_DOWN;
  case HID_KEY_RIGHT:
    return KeyCode::ARROW_RIGHT;
  case HID_KEY_LEFT:
    return KeyCode::ARROW_LEFT;
  case HID_KEY_DOWN:
    return KeyCode::ARROW_DOWN;
  case HID_KEY_UP:
    return KeyCode::ARROW_UP;
  case HID_KEY_NUM_LOCK:
    return KeyCode::NUM_LOCK;
  case HID_KEY_KEYPAD_DIV:
    return KeyCode::KEYPAD_DIVIDE;
  case HID_KEY_KEYPAD_MUL:
    return KeyCode::KEYPAD_MULTIPLY;
  case HID_KEY_KEYPAD_SUB:
    return KeyCode::KEYPAD_SUBTRACT;
  case HID_KEY_KEYPAD_ADD:
    return KeyCode::KEYPAD_ADD;
  case HID_KEY_KEYPAD_ENTER:
    return KeyCode::KEYPAD_ENTER;
  case HID_KEY_KEYPAD_1:
    return KeyCode::KEYPAD_1;
  case HID_KEY_KEYPAD_2:
    return KeyCode::KEYPAD_2;
  case HID_KEY_KEYPAD_3:
    return KeyCode::KEYPAD_3;
  case HID_KEY_KEYPAD_4:
    return KeyCode::KEYPAD_4;
  case HID_KEY_KEYPAD_5:
    return KeyCode::KEYPAD_5;
  case HID_KEY_KEYPAD_6:
    return KeyCode::KEYPAD_6;
  case HID_KEY_KEYPAD_7:
    return KeyCode::KEYPAD_7;
  case HID_KEY_KEYPAD_8:
    return KeyCode::KEYPAD_8;
  case HID_KEY_KEYPAD_9:
    return KeyCode::KEYPAD_9;
  case HID_KEY_KEYPAD_0:
    return KeyCode::KEYPAD_0;
  case HID_KEY_KEYPAD_DELETE:
    return KeyCode::KEYPAD_DECIMAL;
  default:
    return KeyCode::UNKNOWN;
  }
}

static KeyCode modifier_key_code(uint8_t modifier_bit) {
  switch (modifier_bit) {
  case HID_LEFT_CONTROL:
    return KeyCode::LEFT_CTRL;
  case HID_LEFT_SHIFT:
    return KeyCode::LEFT_SHIFT;
  case HID_LEFT_ALT:
    return KeyCode::LEFT_ALT;
  case HID_LEFT_GUI:
    return KeyCode::LEFT_GUI;
  case HID_RIGHT_CONTROL:
    return KeyCode::RIGHT_CTRL;
  case HID_RIGHT_SHIFT:
    return KeyCode::RIGHT_SHIFT;
  case HID_RIGHT_ALT:
    return KeyCode::RIGHT_ALT;
  case HID_RIGHT_GUI:
    return KeyCode::RIGHT_GUI;
  default:
    return KeyCode::UNKNOWN;
  }
}

static char printable_from_hid_usage(uint8_t usage, uint8_t modifiers) {
  const bool shifted = is_shift_pressed(modifiers);

  if (usage >= HID_KEY_A && usage <= HID_KEY_Z) {
    const char base = static_cast<char>('a' + (usage - HID_KEY_A));
    return shifted ? static_cast<char>(base - 'a' + 'A') : base;
  }

  static constexpr char digits[][2] = {
      {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'},
      {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'},
  };
  if (usage >= HID_KEY_1 && usage <= HID_KEY_0) {
    return digits[usage - HID_KEY_1][shifted ? 1 : 0];
  }

  switch (usage) {
  case HID_KEY_ENTER:
  case HID_KEY_KEYPAD_ENTER:
    return '\n';
  case HID_KEY_DEL:
    return '\b';
  case HID_KEY_TAB:
    return '\t';
  case HID_KEY_SPACE:
    return ' ';
  case HID_KEY_MINUS:
    return shifted ? '_' : '-';
  case HID_KEY_EQUAL:
    return shifted ? '+' : '=';
  case HID_KEY_OPEN_BRACKET:
    return shifted ? '{' : '[';
  case HID_KEY_CLOSE_BRACKET:
    return shifted ? '}' : ']';
  case HID_KEY_BACK_SLASH:
  case HID_KEY_SHARP:
    return shifted ? '|' : '\\';
  case HID_KEY_COLON:
    return shifted ? ':' : ';';
  case HID_KEY_QUOTE:
    return shifted ? '"' : '\'';
  case HID_KEY_TILDE:
    return shifted ? '~' : '`';
  case HID_KEY_LESS:
    return shifted ? '<' : ',';
  case HID_KEY_GREATER:
    return shifted ? '>' : '.';
  case HID_KEY_SLASH:
    return shifted ? '?' : '/';
  case HID_KEY_KEYPAD_DIV:
    return '/';
  case HID_KEY_KEYPAD_MUL:
    return '*';
  case HID_KEY_KEYPAD_SUB:
    return '-';
  case HID_KEY_KEYPAD_ADD:
    return '+';
  case HID_KEY_KEYPAD_1:
  case HID_KEY_KEYPAD_2:
  case HID_KEY_KEYPAD_3:
  case HID_KEY_KEYPAD_4:
  case HID_KEY_KEYPAD_5:
  case HID_KEY_KEYPAD_6:
  case HID_KEY_KEYPAD_7:
  case HID_KEY_KEYPAD_8:
  case HID_KEY_KEYPAD_9:
    return static_cast<char>('1' + (usage - HID_KEY_KEYPAD_1));
  case HID_KEY_KEYPAD_0:
    return '0';
  case HID_KEY_KEYPAD_DELETE:
    return '.';
  default:
    return 0;
  }
}

static const char *printable_for_log(char printable, char *buffer,
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
  default:
    snprintf(buffer, buffer_len, "%c", printable);
    return buffer;
  }
}

static bool expect_key_mapping(const KeyMapExpectation &expectation,
                               uint32_t &checks) {
  ++checks;
  const KeyCode actual_code = key_code_from_hid_usage(expectation.usage);
  const char actual_normal = printable_from_hid_usage(expectation.usage, 0);
  const char actual_shifted =
      printable_from_hid_usage(expectation.usage, HID_LEFT_SHIFT);

  if (actual_code == expectation.code && actual_normal == expectation.normal &&
      actual_shifted == expectation.shifted) {
    return true;
  }

  ESP_LOGE(TAG,
           "HID key mapping mismatch usage=0x%02x code=%s/%s normal=0x%02x/0x%02x shifted=0x%02x/0x%02x",
           expectation.usage, key_code_name(actual_code),
           key_code_name(expectation.code),
           static_cast<unsigned char>(actual_normal),
           static_cast<unsigned char>(expectation.normal),
           static_cast<unsigned char>(actual_shifted),
           static_cast<unsigned char>(expectation.shifted));
  return false;
}

static bool expect_modifier_mapping(const ModifierExpectation &expectation,
                                    uint32_t &checks) {
  ++checks;
  const KeyCode actual_code = modifier_key_code(expectation.mask);
  if (actual_code == expectation.code) {
    return true;
  }

  ESP_LOGE(TAG, "HID modifier mapping mismatch mask=0x%02x code=%s/%s",
           expectation.mask, key_code_name(actual_code),
           key_code_name(expectation.code));
  return false;
}

static void run_key_mapping_self_test() {
  uint32_t checks = 0;
  uint32_t failures = 0;

  for (uint8_t usage = HID_KEY_A; usage <= HID_KEY_Z; ++usage) {
    const auto letter_index = static_cast<uint8_t>(usage - HID_KEY_A);
    const KeyMapExpectation expectation = {
        usage,
        static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::A) + letter_index),
        static_cast<char>('a' + letter_index),
        static_cast<char>('A' + letter_index),
    };
    if (!expect_key_mapping(expectation, checks)) {
      ++failures;
    }
  }

  static constexpr KeyMapExpectation key_expectations[] = {
      {HID_KEY_1, KeyCode::DIGIT_1, '1', '!'},
      {HID_KEY_2, KeyCode::DIGIT_2, '2', '@'},
      {HID_KEY_3, KeyCode::DIGIT_3, '3', '#'},
      {HID_KEY_4, KeyCode::DIGIT_4, '4', '$'},
      {HID_KEY_5, KeyCode::DIGIT_5, '5', '%'},
      {HID_KEY_6, KeyCode::DIGIT_6, '6', '^'},
      {HID_KEY_7, KeyCode::DIGIT_7, '7', '&'},
      {HID_KEY_8, KeyCode::DIGIT_8, '8', '*'},
      {HID_KEY_9, KeyCode::DIGIT_9, '9', '('},
      {HID_KEY_0, KeyCode::DIGIT_0, '0', ')'},
      {HID_KEY_ENTER, KeyCode::ENTER, '\n', '\n'},
      {HID_KEY_ESC, KeyCode::ESCAPE, 0, 0},
      {HID_KEY_DEL, KeyCode::BACKSPACE, '\b', '\b'},
      {HID_KEY_TAB, KeyCode::TAB, '\t', '\t'},
      {HID_KEY_SPACE, KeyCode::SPACE, ' ', ' '},
      {HID_KEY_MINUS, KeyCode::MINUS, '-', '_'},
      {HID_KEY_EQUAL, KeyCode::EQUAL, '=', '+'},
      {HID_KEY_OPEN_BRACKET, KeyCode::LEFT_BRACKET, '[', '{'},
      {HID_KEY_CLOSE_BRACKET, KeyCode::RIGHT_BRACKET, ']', '}'},
      {HID_KEY_BACK_SLASH, KeyCode::BACKSLASH, '\\', '|'},
      {HID_KEY_SHARP, KeyCode::NON_US_HASH, '\\', '|'},
      {HID_KEY_COLON, KeyCode::SEMICOLON, ';', ':'},
      {HID_KEY_QUOTE, KeyCode::QUOTE, '\'', '"'},
      {HID_KEY_TILDE, KeyCode::GRAVE, '`', '~'},
      {HID_KEY_LESS, KeyCode::COMMA, ',', '<'},
      {HID_KEY_GREATER, KeyCode::PERIOD, '.', '>'},
      {HID_KEY_SLASH, KeyCode::SLASH, '/', '?'},
      {HID_KEY_INSERT, KeyCode::INSERT, 0, 0},
      {HID_KEY_HOME, KeyCode::HOME, 0, 0},
      {HID_KEY_PAGEUP, KeyCode::PAGE_UP, 0, 0},
      {HID_KEY_DELETE, KeyCode::DELETE_FORWARD, 0, 0},
      {HID_KEY_END, KeyCode::END, 0, 0},
      {HID_KEY_PAGEDOWN, KeyCode::PAGE_DOWN, 0, 0},
      {HID_KEY_RIGHT, KeyCode::ARROW_RIGHT, 0, 0},
      {HID_KEY_LEFT, KeyCode::ARROW_LEFT, 0, 0},
      {HID_KEY_DOWN, KeyCode::ARROW_DOWN, 0, 0},
      {HID_KEY_UP, KeyCode::ARROW_UP, 0, 0},
      {HID_KEY_F1, KeyCode::F1, 0, 0},
      {HID_KEY_F2, KeyCode::F2, 0, 0},
      {HID_KEY_F3, KeyCode::F3, 0, 0},
      {HID_KEY_F4, KeyCode::F4, 0, 0},
      {HID_KEY_F5, KeyCode::F5, 0, 0},
      {HID_KEY_F6, KeyCode::F6, 0, 0},
      {HID_KEY_F7, KeyCode::F7, 0, 0},
      {HID_KEY_F8, KeyCode::F8, 0, 0},
      {HID_KEY_F9, KeyCode::F9, 0, 0},
      {HID_KEY_F10, KeyCode::F10, 0, 0},
      {HID_KEY_F11, KeyCode::F11, 0, 0},
      {HID_KEY_F12, KeyCode::F12, 0, 0},
  };

  for (const auto &expectation : key_expectations) {
    if (!expect_key_mapping(expectation, checks)) {
      ++failures;
    }
  }

  static constexpr ModifierExpectation modifier_expectations[] = {
      {HID_LEFT_CONTROL, KeyCode::LEFT_CTRL},
      {HID_LEFT_SHIFT, KeyCode::LEFT_SHIFT},
      {HID_LEFT_ALT, KeyCode::LEFT_ALT},
      {HID_LEFT_GUI, KeyCode::LEFT_GUI},
      {HID_RIGHT_CONTROL, KeyCode::RIGHT_CTRL},
      {HID_RIGHT_SHIFT, KeyCode::RIGHT_SHIFT},
      {HID_RIGHT_ALT, KeyCode::RIGHT_ALT},
      {HID_RIGHT_GUI, KeyCode::RIGHT_GUI},
  };

  for (const auto &expectation : modifier_expectations) {
    if (!expect_modifier_mapping(expectation, checks)) {
      ++failures;
    }
  }

  if (failures > 0) {
    ESP_LOGE(TAG, "HID key mapping self-test FAILED: failures=%u checks=%u",
             static_cast<unsigned>(failures), static_cast<unsigned>(checks));
    return;
  }

  ESP_LOGI(TAG, "HID key mapping self-test PASS: checks=%u",
           static_cast<unsigned>(checks));
}

const char *key_action_name(KeyAction action) {
  switch (action) {
  case KeyAction::PRESS:
    return "PRESS";
  case KeyAction::RELEASE:
    return "RELEASE";
  case KeyAction::REPEAT:
    return "REPEAT";
  default:
    return "UNKNOWN";
  }
}

const char *key_code_name(KeyCode code) {
  switch (code) {
  case KeyCode::A:
    return "A";
  case KeyCode::B:
    return "B";
  case KeyCode::C:
    return "C";
  case KeyCode::D:
    return "D";
  case KeyCode::E:
    return "E";
  case KeyCode::F:
    return "F";
  case KeyCode::G:
    return "G";
  case KeyCode::H:
    return "H";
  case KeyCode::I:
    return "I";
  case KeyCode::J:
    return "J";
  case KeyCode::K:
    return "K";
  case KeyCode::L:
    return "L";
  case KeyCode::M:
    return "M";
  case KeyCode::N:
    return "N";
  case KeyCode::O:
    return "O";
  case KeyCode::P:
    return "P";
  case KeyCode::Q:
    return "Q";
  case KeyCode::R:
    return "R";
  case KeyCode::S:
    return "S";
  case KeyCode::T:
    return "T";
  case KeyCode::U:
    return "U";
  case KeyCode::V:
    return "V";
  case KeyCode::W:
    return "W";
  case KeyCode::X:
    return "X";
  case KeyCode::Y:
    return "Y";
  case KeyCode::Z:
    return "Z";
  case KeyCode::DIGIT_1:
    return "1";
  case KeyCode::DIGIT_2:
    return "2";
  case KeyCode::DIGIT_3:
    return "3";
  case KeyCode::DIGIT_4:
    return "4";
  case KeyCode::DIGIT_5:
    return "5";
  case KeyCode::DIGIT_6:
    return "6";
  case KeyCode::DIGIT_7:
    return "7";
  case KeyCode::DIGIT_8:
    return "8";
  case KeyCode::DIGIT_9:
    return "9";
  case KeyCode::DIGIT_0:
    return "0";
  case KeyCode::ENTER:
    return "ENTER";
  case KeyCode::ESCAPE:
    return "ESCAPE";
  case KeyCode::BACKSPACE:
    return "BACKSPACE";
  case KeyCode::TAB:
    return "TAB";
  case KeyCode::SPACE:
    return "SPACE";
  case KeyCode::MINUS:
    return "MINUS";
  case KeyCode::EQUAL:
    return "EQUAL";
  case KeyCode::LEFT_BRACKET:
    return "LEFT_BRACKET";
  case KeyCode::RIGHT_BRACKET:
    return "RIGHT_BRACKET";
  case KeyCode::BACKSLASH:
    return "BACKSLASH";
  case KeyCode::NON_US_HASH:
    return "NON_US_HASH";
  case KeyCode::SEMICOLON:
    return "SEMICOLON";
  case KeyCode::QUOTE:
    return "QUOTE";
  case KeyCode::GRAVE:
    return "GRAVE";
  case KeyCode::COMMA:
    return "COMMA";
  case KeyCode::PERIOD:
    return "PERIOD";
  case KeyCode::SLASH:
    return "SLASH";
  case KeyCode::CAPS_LOCK:
    return "CAPS_LOCK";
  case KeyCode::F1:
    return "F1";
  case KeyCode::F2:
    return "F2";
  case KeyCode::F3:
    return "F3";
  case KeyCode::F4:
    return "F4";
  case KeyCode::F5:
    return "F5";
  case KeyCode::F6:
    return "F6";
  case KeyCode::F7:
    return "F7";
  case KeyCode::F8:
    return "F8";
  case KeyCode::F9:
    return "F9";
  case KeyCode::F10:
    return "F10";
  case KeyCode::F11:
    return "F11";
  case KeyCode::F12:
    return "F12";
  case KeyCode::PRINT_SCREEN:
    return "PRINT_SCREEN";
  case KeyCode::SCROLL_LOCK:
    return "SCROLL_LOCK";
  case KeyCode::PAUSE:
    return "PAUSE";
  case KeyCode::INSERT:
    return "INSERT";
  case KeyCode::HOME:
    return "HOME";
  case KeyCode::PAGE_UP:
    return "PAGE_UP";
  case KeyCode::DELETE_FORWARD:
    return "DELETE";
  case KeyCode::END:
    return "END";
  case KeyCode::PAGE_DOWN:
    return "PAGE_DOWN";
  case KeyCode::ARROW_RIGHT:
    return "ARROW_RIGHT";
  case KeyCode::ARROW_LEFT:
    return "ARROW_LEFT";
  case KeyCode::ARROW_DOWN:
    return "ARROW_DOWN";
  case KeyCode::ARROW_UP:
    return "ARROW_UP";
  case KeyCode::NUM_LOCK:
    return "NUM_LOCK";
  case KeyCode::KEYPAD_DIVIDE:
    return "KEYPAD_DIVIDE";
  case KeyCode::KEYPAD_MULTIPLY:
    return "KEYPAD_MULTIPLY";
  case KeyCode::KEYPAD_SUBTRACT:
    return "KEYPAD_SUBTRACT";
  case KeyCode::KEYPAD_ADD:
    return "KEYPAD_ADD";
  case KeyCode::KEYPAD_ENTER:
    return "KEYPAD_ENTER";
  case KeyCode::KEYPAD_1:
    return "KEYPAD_1";
  case KeyCode::KEYPAD_2:
    return "KEYPAD_2";
  case KeyCode::KEYPAD_3:
    return "KEYPAD_3";
  case KeyCode::KEYPAD_4:
    return "KEYPAD_4";
  case KeyCode::KEYPAD_5:
    return "KEYPAD_5";
  case KeyCode::KEYPAD_6:
    return "KEYPAD_6";
  case KeyCode::KEYPAD_7:
    return "KEYPAD_7";
  case KeyCode::KEYPAD_8:
    return "KEYPAD_8";
  case KeyCode::KEYPAD_9:
    return "KEYPAD_9";
  case KeyCode::KEYPAD_0:
    return "KEYPAD_0";
  case KeyCode::KEYPAD_DECIMAL:
    return "KEYPAD_DECIMAL";
  case KeyCode::LEFT_CTRL:
    return "LEFT_CTRL";
  case KeyCode::LEFT_SHIFT:
    return "LEFT_SHIFT";
  case KeyCode::LEFT_ALT:
    return "LEFT_ALT";
  case KeyCode::LEFT_GUI:
    return "LEFT_GUI";
  case KeyCode::RIGHT_CTRL:
    return "RIGHT_CTRL";
  case KeyCode::RIGHT_SHIFT:
    return "RIGHT_SHIFT";
  case KeyCode::RIGHT_ALT:
    return "RIGHT_ALT";
  case KeyCode::RIGHT_GUI:
    return "RIGHT_GUI";
  default:
    return "UNKNOWN";
  }
}

static void send_key_event(uint8_t usage, KeyAction action, uint8_t modifiers) {
  KeyEvent key_event = {};
  key_event.code = key_code_from_hid_usage(usage);
  key_event.action = action;
  key_event.modifiers = modifiers;
  key_event.printable = printable_from_hid_usage(usage, modifiers);

  AppEvent app_event = {};
  app_event.type = AppEventType::KEY_EVENT;
  app_event.key = key_event;

  if (ui_queue && xQueueSend(ui_queue, &app_event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "UI queue full; dropped key event");
  }

  char printable_buffer[2] = {};
  ESP_LOGI(TAG, "Key %s action=%s modifiers=0x%02x char=%s",
           key_code_name(key_event.code), key_action_name(action), modifiers,
           printable_for_log(key_event.printable, printable_buffer,
                             sizeof(printable_buffer)));
}

static void send_modifier_event(uint8_t modifier_bit, KeyAction action,
                                uint8_t modifiers) {
  KeyEvent key_event = {};
  key_event.code = modifier_key_code(modifier_bit);
  key_event.action = action;
  key_event.modifiers = modifiers;
  key_event.printable = 0;

  AppEvent app_event = {};
  app_event.type = AppEventType::KEY_EVENT;
  app_event.key = key_event;

  if (ui_queue && xQueueSend(ui_queue, &app_event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "UI queue full; dropped modifier event");
  }

  ESP_LOGI(TAG, "Key %s action=%s modifiers=0x%02x",
           key_code_name(key_event.code), key_action_name(action), modifiers);
}

static void send_input_status(bool connected, uint16_t vid, uint16_t pid,
                              uint8_t protocol) {
  AppEvent app_event = {};
  app_event.type = AppEventType::INPUT_STATUS;
  app_event.input_status.connected = connected;
  app_event.input_status.vid = vid;
  app_event.input_status.pid = pid;
  app_event.input_status.protocol = protocol;

  if (ui_queue && xQueueSend(ui_queue, &app_event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "UI queue full; dropped input status event");
  }
}

static void update_repeat_state_on_press(uint8_t usage, uint8_t modifiers) {
  portENTER_CRITICAL(&s_key_state_lock);
  for (auto &key : s_pressed_keys) {
    if (key.active && key.usage == usage) {
      key.modifiers = modifiers;
      portEXIT_CRITICAL(&s_key_state_lock);
      return;
    }
  }
  for (auto &key : s_pressed_keys) {
    if (!key.active) {
      key.active = true;
      key.usage = usage;
      key.modifiers = modifiers;
      key.next_repeat_ms = now_ms() + KEY_REPEAT_DELAY_MS;
      key.repeat_count = 0;
      break;
    }
  }
  portEXIT_CRITICAL(&s_key_state_lock);
}

static void update_repeat_state_on_release(uint8_t usage) {
  portENTER_CRITICAL(&s_key_state_lock);
  for (auto &key : s_pressed_keys) {
    if (key.active && key.usage == usage) {
      key = {};
      break;
    }
  }
  portEXIT_CRITICAL(&s_key_state_lock);
}

static void update_repeat_modifiers(uint8_t modifiers) {
  portENTER_CRITICAL(&s_key_state_lock);
  for (auto &key : s_pressed_keys) {
    if (key.active) {
      key.modifiers = modifiers;
    }
  }
  portEXIT_CRITICAL(&s_key_state_lock);
}

static void reset_keyboard_state() {
  portENTER_CRITICAL(&s_key_state_lock);
  memset(s_pressed_keys, 0, sizeof(s_pressed_keys));
  memset(s_prev_boot_keys, 0, sizeof(s_prev_boot_keys));
  s_prev_modifiers = 0;
  portEXIT_CRITICAL(&s_key_state_lock);
}

static void handle_modifier_changes(uint8_t modifiers) {
  const uint8_t changed = s_prev_modifiers ^ modifiers;
  if (changed == 0) {
    return;
  }

  for (uint8_t bit = 0; bit < 8; ++bit) {
    const uint8_t mask = static_cast<uint8_t>(1U << bit);
    if ((changed & mask) == 0) {
      continue;
    }
    send_modifier_event(mask, (modifiers & mask) ? KeyAction::PRESS
                                                 : KeyAction::RELEASE,
                        modifiers);
  }
  s_prev_modifiers = modifiers;
}

static void handle_keyboard_report(const uint8_t *data, size_t length) {
  if (length < sizeof(hid_keyboard_input_report_boot_t)) {
    ESP_LOGW(TAG, "Keyboard report too short: %u bytes",
             static_cast<unsigned>(length));
    return;
  }

  const auto *report =
      reinterpret_cast<const hid_keyboard_input_report_boot_t *>(data);
  const uint8_t modifiers = report->modifier.val;

  handle_modifier_changes(modifiers);
  update_repeat_modifiers(modifiers);

  for (int i = 0; i < HID_KEYBOARD_KEY_MAX; ++i) {
    const uint8_t prev_usage = s_prev_boot_keys[i];
    if (prev_usage > HID_KEY_ERROR_UNDEFINED &&
        !key_found(report->key, prev_usage)) {
      send_key_event(prev_usage, KeyAction::RELEASE, modifiers);
      update_repeat_state_on_release(prev_usage);
    }

    const uint8_t current_usage = report->key[i];
    if (current_usage > HID_KEY_ERROR_UNDEFINED &&
        !key_found(s_prev_boot_keys, current_usage)) {
      send_key_event(current_usage, KeyAction::PRESS, modifiers);
      update_repeat_state_on_press(current_usage, modifiers);
    }
  }

  memcpy(s_prev_boot_keys, report->key, HID_KEYBOARD_KEY_MAX);
}

static void hid_interface_callback(hid_host_device_handle_t handle,
                                   const hid_host_interface_event_t event,
                                   void *arg) {
  (void)arg;

  hid_host_dev_params_t params = {};
  if (hid_host_device_get_params(handle, &params) != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read HID device params");
    return;
  }

  switch (event) {
  case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
    uint8_t data[HID_REPORT_BUFFER_SIZE] = {};
    size_t data_length = 0;
    const esp_err_t err = hid_host_device_get_raw_input_report_data(
        handle, data, sizeof(data), &data_length);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to read HID input report: %s",
               esp_err_to_name(err));
      return;
    }
    if (params.proto == HID_PROTOCOL_KEYBOARD) {
      handle_keyboard_report(data, data_length);
    }
    break;
  }
  case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
    ++s_keyboard_disconnect_count;
    ESP_LOGI(TAG, "HID keyboard disconnected count=%u",
             static_cast<unsigned>(s_keyboard_disconnect_count));
    reset_keyboard_state();
    send_input_status(false, 0, 0, params.proto);
    ESP_ERROR_CHECK(hid_host_device_close(handle));
    break;
  case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
    ESP_LOGW(TAG, "HID transfer error");
    break;
  default:
    ESP_LOGW(TAG, "Unhandled HID interface event: %d", event);
    break;
  }
}

static void handle_hid_device_event(hid_host_device_handle_t handle,
                                    hid_host_driver_event_t event,
                                    void *arg) {
  (void)arg;

  if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
    return;
  }

  hid_host_dev_params_t params = {};
  ESP_ERROR_CHECK(hid_host_device_get_params(handle, &params));

  hid_host_dev_info_t device_info = {};
  const esp_err_t info_err = hid_host_get_device_info(handle, &device_info);
  const uint16_t vid = (info_err == ESP_OK) ? device_info.VID : 0;
  const uint16_t pid = (info_err == ESP_OK) ? device_info.PID : 0;

  ESP_LOGI(TAG, "HID interface connected addr=%u iface=%u proto=%u VID=0x%04x PID=0x%04x",
           params.addr, params.iface_num, params.proto, vid, pid);

  if (params.proto != HID_PROTOCOL_KEYBOARD) {
    ESP_LOGI(TAG, "Ignoring non-keyboard HID interface");
    return;
  }

  ++s_keyboard_connect_count;
  ESP_LOGI(TAG, "HID keyboard connect count=%u",
           static_cast<unsigned>(s_keyboard_connect_count));

  hid_host_device_config_t device_config = {};
  device_config.callback = hid_interface_callback;

  ESP_ERROR_CHECK(hid_host_device_open(handle, &device_config));
  if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
    ESP_ERROR_CHECK(
        hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT));
    ESP_ERROR_CHECK(hid_class_request_set_idle(handle, 0, 0));
  } else {
    ESP_LOGI(TAG, "Keyboard is not boot subclass; parsing boot-compatible reports");
  }
  ESP_ERROR_CHECK(hid_host_device_start(handle));

  reset_keyboard_state();
  send_input_status(true, vid, pid, params.proto);
}

static void hid_driver_callback(hid_host_device_handle_t handle,
                                const hid_host_driver_event_t event,
                                void *arg) {
  if (!s_hid_event_queue) {
    return;
  }

  InputEvent input_event = {};
  input_event.group = InputEventGroup::HID_HOST;
  input_event.handle = handle;
  input_event.event = event;
  input_event.arg = arg;
  xQueueSend(s_hid_event_queue, &input_event, 0);
}

static void usb_lib_task(void *arg) {
  usb_host_config_t host_config = {};
  host_config.skip_phy_setup = false;
  host_config.intr_flags = ESP_INTR_FLAG_LOWMED;

  ESP_ERROR_CHECK(usb_host_install(&host_config));
  xTaskNotifyGive(static_cast<TaskHandle_t>(arg));

  while (true) {
    uint32_t event_flags = 0;
    const esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "USB host event handling failed: %s", esp_err_to_name(err));
      continue;
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_LOGI(TAG, "USB host has no clients");
      ESP_ERROR_CHECK(usb_host_device_free_all());
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      ESP_LOGI(TAG, "USB host freed all devices");
    }
  }
}

static void input_task(void *arg) {
  (void)arg;
  InputEvent input_event = {};

  while (true) {
    if (xQueueReceive(s_hid_event_queue, &input_event, portMAX_DELAY) !=
        pdTRUE) {
      continue;
    }
    if (input_event.group == InputEventGroup::HID_HOST) {
      handle_hid_device_event(input_event.handle, input_event.event,
                              input_event.arg);
    }
  }
}

static void key_repeat_task(void *arg) {
  (void)arg;

  while (true) {
    const uint32_t now = now_ms();
    struct RepeatEvent {
      uint8_t usage;
      uint8_t modifiers;
      uint32_t repeat_count;
    };
    RepeatEvent repeats[HID_KEYBOARD_KEY_MAX] = {};
    size_t repeat_count = 0;

    portENTER_CRITICAL(&s_key_state_lock);
    for (auto &key : s_pressed_keys) {
      if (key.active && now >= key.next_repeat_ms) {
        repeats[repeat_count].usage = key.usage;
        repeats[repeat_count].modifiers = key.modifiers;
        repeats[repeat_count].repeat_count = key.repeat_count + 1;
        ++repeat_count;
        key.next_repeat_ms = now + KEY_REPEAT_INTERVAL_MS;
        ++key.repeat_count;
      }
    }
    portEXIT_CRITICAL(&s_key_state_lock);

    for (size_t i = 0; i < repeat_count; ++i) {
      if (repeats[i].repeat_count == 1 ||
          repeats[i].repeat_count % 100 == 0) {
        ESP_LOGI(TAG, "Key repeat progress key=%s repeats=%u",
                 key_code_name(key_code_from_hid_usage(repeats[i].usage)),
                 static_cast<unsigned>(repeats[i].repeat_count));
      }
      send_key_event(repeats[i].usage, KeyAction::REPEAT,
                     repeats[i].modifiers);
    }

    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

void app_input_init() {
  if (s_initialized) {
    return;
  }

  run_key_mapping_self_test();

  s_hid_event_queue = xQueueCreate(10, sizeof(InputEvent));
  ESP_ERROR_CHECK(s_hid_event_queue ? ESP_OK : ESP_ERR_NO_MEM);

  ESP_ERROR_CHECK(bsp_feature_enable(BSP_FEATURE_USB, true));

  BaseType_t task_created =
      xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096,
                              xTaskGetCurrentTaskHandle(), 3, nullptr, 0);
  ESP_ERROR_CHECK(task_created == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);
  ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1000));

  hid_host_driver_config_t hid_config = {};
  hid_config.create_background_task = true;
  hid_config.task_priority = 5;
  hid_config.stack_size = 4096;
  hid_config.core_id = 0;
  hid_config.callback = hid_driver_callback;

  ESP_ERROR_CHECK(hid_host_install(&hid_config));

  task_created = xTaskCreate(input_task, "input_task", 4096, nullptr, 4, nullptr);
  ESP_ERROR_CHECK(task_created == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);
  task_created =
      xTaskCreate(key_repeat_task, "key_repeat", 3072, nullptr, 4, nullptr);
  ESP_ERROR_CHECK(task_created == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM);

  s_initialized = true;
  ESP_LOGI(TAG, "USB keyboard input initialized");
}

#pragma once

#include "key_event.h"
#include <string>
#include <vector>

enum class TextInputEventType {
  COMMIT_TEXT,
  COMPOSING_TEXT,
  DELETE_BACKWARD,
  COMMAND
};

struct TextInputEvent {
  TextInputEventType type;
  std::string text;
  KeyCode command_code;
  uint8_t modifiers;
};

enum class InputMode {
  ENGLISH,
  KOREAN
};

enum class ImeState {
  IDLE,
  CHO_ONLY,
  CHO_JUNG,
  CHO_JUNG_JONG,
  JUNG_ONLY
};

class TextInputComposer {
public:
  TextInputComposer();
  ~TextInputComposer() = default;

  std::vector<TextInputEvent> handle_key_event(const KeyEvent &event);

  InputMode get_input_mode() const { return mode_; }
  void set_input_mode(InputMode mode);
  void toggle_input_mode();

  std::string get_composing_text() const;
  bool is_composing() const { return state_ != ImeState::IDLE; }
  void clear_composition();

private:
  std::vector<TextInputEvent> handle_korean_input(const KeyEvent &event);
  std::vector<TextInputEvent> handle_english_input(const KeyEvent &event);

  std::vector<TextInputEvent> commit_composition();
  std::vector<TextInputEvent> update_composing();

  ImeState state_ = ImeState::IDLE;
  InputMode mode_ = InputMode::ENGLISH;

  int cho_idx_ = -1;
  int jung_idx_ = -1;
  int jong_idx_ = -1;
  uint32_t compat_code_ = 0;
};

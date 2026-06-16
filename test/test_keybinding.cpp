#include "keybinding.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

static KeyEvent key(KeyCode code, char printable = 0, uint8_t modifiers = 0) {
  return {code, KeyAction::PRESS, modifiers, printable};
}

static void type(EditorCore &editor, TextInputComposer &composer, const std::string &text) {
  for (char c : text) {
    KeyCode code = KeyCode::A;
    if (c == ' ') {
      code = KeyCode::SPACE;
    } else if (c == '\n') {
      code = KeyCode::ENTER;
    }
    keybinding_dispatch(editor, composer, key(code, c));
  }
}

static void test_english_typing_navigation_and_undo() {
  EditorCore editor;
  TextInputComposer composer;

  type(editor, composer, "hello\nworld");
  assert(editor.text() == "hello\nworld");
  assert(editor.cursor().position == editor.length());

  keybinding_dispatch(editor, composer, key(KeyCode::ARROW_LEFT));
  assert(editor.cursor().position == editor.length() - 1);
  keybinding_dispatch(editor, composer, key(KeyCode::BACKSPACE));
  assert(editor.text() == "hello\nword");

  keybinding_dispatch(editor, composer, key(KeyCode::Z, 0, KEY_MOD_LEFT_CTRL));
  assert(editor.text() == "hello\nworld");
  keybinding_dispatch(editor, composer, key(KeyCode::Y, 0, KEY_MOD_LEFT_CTRL));
  assert(editor.text() == "hello\nword");
}

static void test_korean_input_commit_and_backspace() {
  EditorCore editor;
  TextInputComposer composer;

  auto result = keybinding_dispatch(editor, composer, key(KeyCode::SPACE, 0, KEY_MOD_RIGHT_CTRL));
  assert(result.input_mode_changed);
  assert(composer.get_input_mode() == InputMode::KOREAN);

  type(editor, composer, "dkssud");
  assert(editor.text() == "안");
  assert(composer.get_composing_text() == "녕");

  keybinding_dispatch(editor, composer, key(KeyCode::SPACE, ' '));
  assert(editor.text() == "안녕 ");
  assert(!composer.is_composing());

  keybinding_dispatch(editor, composer, key(KeyCode::BACKSPACE));
  assert(editor.text() == "안녕");
}

static void test_shortcut_placeholders_and_delete_forward() {
  EditorCore editor("abc");
  TextInputComposer composer;
  editor.set_cursor(1);

  keybinding_dispatch(editor, composer, key(KeyCode::DELETE_FORWARD));
  assert(editor.text() == "ac");

  auto save = keybinding_dispatch(editor, composer, key(KeyCode::S, 0, KEY_MOD_LEFT_CTRL));
  assert(save.toast_changed);
  assert(save.toast.find("Memory only") != std::string::npos);

  auto esc = keybinding_dispatch(editor, composer, key(KeyCode::ESCAPE));
  assert(esc.toast_changed);
  assert(esc.toast.find("Menu") != std::string::npos);

  auto snapshot = keybinding_dispatch(editor, composer,
                                      key(KeyCode::P, 0, KEY_MOD_LEFT_CTRL | KEY_MOD_LEFT_SHIFT));
  assert(snapshot.snapshot_requested);
  assert(snapshot.toast_changed);
  assert(snapshot.toast.find("Snapshot") != std::string::npos);
}

static void test_long_continuous_input() {
  EditorCore editor;
  TextInputComposer composer;
  const auto start = std::chrono::steady_clock::now();
  long long max_dispatch_us = 0;
  for (int i = 0; i < 5000; ++i) {
    const auto dispatch_start = std::chrono::steady_clock::now();
    keybinding_dispatch(editor, composer, key(KeyCode::A, 'a'));
    const auto dispatch_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - dispatch_start).count();
    if (dispatch_us > max_dispatch_us) {
      max_dispatch_us = dispatch_us;
    }
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();
  assert(editor.length() == 5000);
  assert(editor.text().front() == 'a');
  assert(editor.text().back() == 'a');
  assert(max_dispatch_us < 20000);
  assert(elapsed < 10000);
}

int main() {
  test_english_typing_navigation_and_undo();
  test_korean_input_commit_and_backspace();
  test_shortcut_placeholders_and_delete_forward();
  test_long_continuous_input();
  std::cout << "Minimal writing screen keybinding tests passed" << std::endl;
  return 0;
}

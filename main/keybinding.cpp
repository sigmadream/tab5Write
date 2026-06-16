#include "keybinding.h"

static bool has_ctrl(uint8_t modifiers) {
  return (modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL)) != 0;
}

static bool has_shift(uint8_t modifiers) {
  return (modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0;
}

static void apply_text_input_event(EditorCore &editor, TextInputComposer &composer,
                                   const TextInputEvent &event,
                                   KeybindingResult &result) {
  (void)composer;
  switch (event.type) {
  case TextInputEventType::COMMIT_TEXT:
    if (!event.text.empty()) {
      editor.insert_text(event.text);
      result.document_changed = true;
      result.cursor_changed = true;
    }
    break;
  case TextInputEventType::COMPOSING_TEXT:
    result.composing_changed = true;
    break;
  case TextInputEventType::DELETE_BACKWARD:
    editor.delete_backward();
    result.document_changed = true;
    result.cursor_changed = true;
    break;
  case TextInputEventType::COMMAND:
    switch (event.command_code) {
    case KeyCode::ARROW_LEFT:
      editor.move_left((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::ARROW_RIGHT:
      editor.move_right((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::ARROW_UP:
      editor.move_up((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::ARROW_DOWN:
      editor.move_down((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::HOME:
      editor.move_line_start((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::END:
      editor.move_line_end((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::PAGE_UP:
      editor.move_page_up((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::PAGE_DOWN:
      editor.move_page_down((event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT)) != 0);
      result.cursor_changed = true;
      break;
    case KeyCode::DELETE_FORWARD:
      editor.delete_forward();
      result.document_changed = true;
      result.cursor_changed = true;
      break;
    case KeyCode::ENTER:
      editor.insert_text("\n");
      result.document_changed = true;
      result.cursor_changed = true;
      break;
    case KeyCode::TAB:
      editor.insert_text("\t");
      result.document_changed = true;
      result.cursor_changed = true;
      break;
    case KeyCode::SPACE:
      editor.insert_text(" ");
      result.document_changed = true;
      result.cursor_changed = true;
      break;
    case KeyCode::ESCAPE:
      result.toast = "Menu coming in Phase 9";
      result.toast_changed = true;
      break;
    default:
      break;
    }
    break;
  }
}

static void flush_composition(EditorCore &editor, TextInputComposer &composer,
                              KeybindingResult &result) {
  auto events = composer.flush_composition();
  for (const auto &event : events) {
    apply_text_input_event(editor, composer, event, result);
  }
  if (!events.empty()) {
    result.composing_changed = true;
  }
}

KeybindingResult keybinding_dispatch(EditorCore &editor,
                                      TextInputComposer &composer,
                                      const KeyEvent &event) {
  KeybindingResult result;
  if (event.action != KeyAction::PRESS && event.action != KeyAction::REPEAT) {
    return result;
  }

  const bool ctrl = has_ctrl(event.modifiers);
  const bool shift = has_shift(event.modifiers);

  if (ctrl) {
    switch (event.code) {
    case KeyCode::SPACE: {
      const InputMode before = composer.get_input_mode();
      auto events = composer.handle_key_event(event);
      for (const auto &text_event : events) {
        apply_text_input_event(editor, composer, text_event, result);
      }
      result.input_mode_changed = before != composer.get_input_mode();
      result.composing_changed = true;
      result.toast = composer.get_input_mode() == InputMode::KOREAN ? "Korean input" : "English input";
      result.toast_changed = true;
      return result;
    }
    case KeyCode::Z:
      flush_composition(editor, composer, result);
      if (editor.can_undo()) {
        editor.undo();
        result.document_changed = true;
        result.cursor_changed = true;
      }
      return result;
    case KeyCode::Y:
      flush_composition(editor, composer, result);
      if (editor.can_redo()) {
        editor.redo();
        result.document_changed = true;
        result.cursor_changed = true;
      }
      return result;
    case KeyCode::S:
      flush_composition(editor, composer, result);
      result.toast = "Memory only — storage starts in Phase 6";
      result.toast_changed = true;
      return result;
    case KeyCode::F:
      flush_composition(editor, composer, result);
      result.toast = "Find coming in Phase 9";
      result.toast_changed = true;
      return result;
    case KeyCode::P:
      if (shift) {
        flush_composition(editor, composer, result);
        result.snapshot_requested = true;
        result.toast = "Snapshot streaming over serial";
        result.toast_changed = true;
        return result;
      }
      break;
    case KeyCode::HOME:
      flush_composition(editor, composer, result);
      editor.move_doc_start(shift);
      result.cursor_changed = true;
      return result;
    case KeyCode::END:
      flush_composition(editor, composer, result);
      editor.move_doc_end(shift);
      result.cursor_changed = true;
      return result;
    case KeyCode::ARROW_LEFT:
      flush_composition(editor, composer, result);
      editor.move_word_left(shift);
      result.cursor_changed = true;
      return result;
    case KeyCode::ARROW_RIGHT:
      flush_composition(editor, composer, result);
      editor.move_word_right(shift);
      result.cursor_changed = true;
      return result;
    default:
      break;
    }
  }

  if (event.code == KeyCode::DELETE_FORWARD) {
    flush_composition(editor, composer, result);
    editor.delete_forward();
    result.document_changed = true;
    result.cursor_changed = true;
    return result;
  }

  const InputMode before_mode = composer.get_input_mode();
  auto events = composer.handle_key_event(event);
  for (const auto &text_event : events) {
    apply_text_input_event(editor, composer, text_event, result);
  }
  result.input_mode_changed = before_mode != composer.get_input_mode();
  if (event.code == KeyCode::ESCAPE && composer.is_composing()) {
    result.composing_changed = true;
  }
  return result;
}

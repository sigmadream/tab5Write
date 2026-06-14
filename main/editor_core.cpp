#include "editor_core.h"

#include <algorithm>
#include <cctype>

EditorCore::EditorCore() : document_(), undo_stack_(1000) {
  refresh_cursor_cache();
  selection_.clear(0);
}

EditorCore::EditorCore(std::string_view initial_text) : document_(initial_text), undo_stack_(1000) {
  refresh_cursor_cache();
  selection_.clear(0);
}

void EditorCore::set_text(std::string_view text) {
  document_.reset(text);
  undo_stack_.clear();
  apply_cursor_position(0, false);
}

void EditorCore::refresh_cursor_cache() {
  cursor_.position = std::min(cursor_.position, document_.length());
  cursor_.line = document_.line_for_position(cursor_.position);
  cursor_.column = document_.column_for_position(cursor_.position);
}

void EditorCore::apply_cursor_position(size_t position, bool extend_selection) {
  cursor_.position = std::min(position, document_.length());
  refresh_cursor_cache();
  selection_.move_cursor(cursor_.position, extend_selection);
  preferred_column_ = cursor_.column;
}

void EditorCore::set_cursor(size_t position, bool extend_selection) {
  apply_cursor_position(position, extend_selection);
}

bool EditorCore::should_merge_typing(std::string_view inserted_text) const {
  if (inserted_text.empty() || utf8_codepoint_count(inserted_text) != 1) {
    return false;
  }
  const unsigned char c = static_cast<unsigned char>(inserted_text.front());
  return !std::isspace(c);
}

void EditorCore::insert_text(std::string_view inserted_text) {
  if (inserted_text.empty()) {
    return;
  }

  Cursor before = cursor_;
  if (selection_.has_selection()) {
    delete_selection();
    before = cursor_;
  }

  const size_t insert_position = cursor_.position;
  document_.insert(insert_position, inserted_text);
  apply_cursor_position(insert_position + utf8_codepoint_count(inserted_text), false);

  undo_stack_.push({EditCommandType::INSERT, insert_position, std::string(inserted_text), before, cursor_},
                   should_merge_typing(inserted_text));
}

void EditorCore::delete_backward(size_t count) {
  if (selection_.has_selection()) {
    delete_selection();
    return;
  }
  if (count == 0 || cursor_.position == 0) {
    return;
  }
  count = std::min(count, cursor_.position);
  const size_t delete_position = cursor_.position - count;
  const std::string deleted = document_.text(delete_position, count);
  Cursor before = cursor_;
  document_.erase(delete_position, count);
  apply_cursor_position(delete_position, false);
  undo_stack_.push({EditCommandType::DELETE, delete_position, deleted, before, cursor_});
}

void EditorCore::delete_forward(size_t count) {
  if (selection_.has_selection()) {
    delete_selection();
    return;
  }
  if (count == 0 || cursor_.position >= document_.length()) {
    return;
  }
  count = std::min(count, document_.length() - cursor_.position);
  const std::string deleted = document_.text(cursor_.position, count);
  Cursor before = cursor_;
  document_.erase(cursor_.position, count);
  refresh_cursor_cache();
  selection_.clear(cursor_.position);
  undo_stack_.push({EditCommandType::DELETE, cursor_.position, deleted, before, cursor_});
}

void EditorCore::delete_selection() {
  if (!selection_.has_selection()) {
    return;
  }
  Cursor before = cursor_;
  const size_t delete_position = selection_.start();
  const std::string deleted = selection_.delete_selection(document_);
  cursor_.position = delete_position;
  refresh_cursor_cache();
  preferred_column_ = cursor_.column;
  undo_stack_.push({EditCommandType::DELETE, delete_position, deleted, before, cursor_});
}

void EditorCore::clear_selection() {
  selection_.clear(cursor_.position);
}

void EditorCore::move_left(bool extend_selection) {
  apply_cursor_position(document_.previous_codepoint_position(cursor_.position), extend_selection);
}

void EditorCore::move_right(bool extend_selection) {
  apply_cursor_position(document_.next_codepoint_position(cursor_.position), extend_selection);
}

void EditorCore::move_up(bool extend_selection) {
  const size_t line = document_.line_for_position(cursor_.position);
  const size_t column = preferred_column_;
  if (line == 0) {
    apply_cursor_position(0, extend_selection);
  } else {
    apply_cursor_position(document_.position_for_line_column(line - 1, column), extend_selection);
  }
  preferred_column_ = column;
}

void EditorCore::move_down(bool extend_selection) {
  const size_t line = document_.line_for_position(cursor_.position);
  const size_t column = preferred_column_;
  const size_t last_line = document_.line_count() - 1;
  if (line >= last_line) {
    apply_cursor_position(document_.length(), extend_selection);
  } else {
    apply_cursor_position(document_.position_for_line_column(line + 1, column), extend_selection);
  }
  preferred_column_ = column;
}

void EditorCore::move_word_left(bool extend_selection) {
  apply_cursor_position(document_.previous_word_position(cursor_.position), extend_selection);
}

void EditorCore::move_word_right(bool extend_selection) {
  apply_cursor_position(document_.next_word_position(cursor_.position), extend_selection);
}

void EditorCore::move_line_start(bool extend_selection) {
  apply_cursor_position(document_.line_start_position(cursor_.position), extend_selection);
}

void EditorCore::move_line_end(bool extend_selection) {
  apply_cursor_position(document_.line_end_position(cursor_.position), extend_selection);
}

void EditorCore::move_doc_start(bool extend_selection) {
  apply_cursor_position(0, extend_selection);
}

void EditorCore::move_doc_end(bool extend_selection) {
  apply_cursor_position(document_.length(), extend_selection);
}

void EditorCore::move_page_up(bool extend_selection) {
  const size_t line = document_.line_for_position(cursor_.position);
  const size_t target_line = line > viewport_lines_ ? line - viewport_lines_ : 0;
  apply_cursor_position(document_.position_for_line_column(target_line, preferred_column_), extend_selection);
}

void EditorCore::move_page_down(bool extend_selection) {
  const size_t line = document_.line_for_position(cursor_.position);
  const size_t target_line = std::min(line + viewport_lines_, document_.line_count() - 1);
  apply_cursor_position(document_.position_for_line_column(target_line, preferred_column_), extend_selection);
}

std::optional<size_t> EditorCore::find(std::string_view query, size_t start_position,
                                       FindDirection direction,
                                       bool case_sensitive) const {
  return document_.find(query, start_position, direction, case_sensitive);
}

std::vector<size_t> EditorCore::find_all(std::string_view query, bool case_sensitive) const {
  return document_.find_all(query, case_sensitive);
}

void EditorCore::apply_inverse_command(const EditCommand &command) {
  if (command.type == EditCommandType::INSERT) {
    document_.erase(command.position, utf8_codepoint_count(command.text));
    cursor_ = command.cursor_before;
  } else {
    document_.insert(command.position, command.text);
    cursor_ = command.cursor_before;
  }
  refresh_cursor_cache();
  selection_.clear(cursor_.position);
}

void EditorCore::apply_command(const EditCommand &command) {
  if (command.type == EditCommandType::INSERT) {
    document_.insert(command.position, command.text);
  } else {
    document_.erase(command.position, utf8_codepoint_count(command.text));
  }
  cursor_ = command.cursor_after;
  refresh_cursor_cache();
  selection_.clear(cursor_.position);
}

void EditorCore::undo() {
  auto group = undo_stack_.undo();
  for (auto it = group.rbegin(); it != group.rend(); ++it) {
    apply_inverse_command(*it);
  }
}

void EditorCore::redo() {
  auto group = undo_stack_.redo();
  for (const auto &command : group) {
    apply_command(command);
  }
}

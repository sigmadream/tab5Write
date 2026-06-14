#pragma once

#include "cursor.h"
#include "piece_table.h"
#include "selection.h"
#include "undo_stack.h"

#include <optional>
#include <stddef.h>
#include <string>
#include <string_view>
#include <vector>

class EditorCore {
public:
  EditorCore();
  explicit EditorCore(std::string_view initial_text);

  const PieceTable &document() const { return document_; }
  PieceTable &document() { return document_; }
  const Cursor &cursor() const { return cursor_; }
  const Selection &selection() const { return selection_; }

  std::string text() const { return document_.text(); }
  size_t length() const { return document_.length(); }
  size_t line_count() const { return document_.line_count(); }
  size_t word_count() const { return document_.word_count(); }

  void set_text(std::string_view text);
  void set_cursor(size_t position, bool extend_selection = false);

  void insert_text(std::string_view text);
  void delete_backward(size_t count = 1);
  void delete_forward(size_t count = 1);

  void move_left(bool extend_selection = false);
  void move_right(bool extend_selection = false);
  void move_up(bool extend_selection = false);
  void move_down(bool extend_selection = false);
  void move_word_left(bool extend_selection = false);
  void move_word_right(bool extend_selection = false);
  void move_line_start(bool extend_selection = false);
  void move_line_end(bool extend_selection = false);
  void move_doc_start(bool extend_selection = false);
  void move_doc_end(bool extend_selection = false);
  void move_page_up(bool extend_selection = false);
  void move_page_down(bool extend_selection = false);

  bool has_selection() const { return selection_.has_selection(); }
  std::string selected_text() const { return selection_.selected_text(document_); }
  void delete_selection();
  void clear_selection();

  std::optional<size_t> find(std::string_view query, size_t start_position = 0,
                             FindDirection direction = FindDirection::FORWARD,
                             bool case_sensitive = true) const;
  std::vector<size_t> find_all(std::string_view query, bool case_sensitive = true) const;

  DocSnapshot snapshot() const { return document_.snapshot(); }

  bool can_undo() const { return undo_stack_.can_undo(); }
  bool can_redo() const { return undo_stack_.can_redo(); }
  void undo();
  void redo();
  size_t undo_group_count() const { return undo_stack_.undo_group_count(); }
  size_t redo_group_count() const { return undo_stack_.redo_group_count(); }

  void set_viewport_lines(size_t viewport_lines) { viewport_lines_ = viewport_lines > 0 ? viewport_lines : 1; }

private:
  PieceTable document_;
  Cursor cursor_;
  Selection selection_;
  UndoStack undo_stack_;
  size_t preferred_column_ = 0;
  size_t viewport_lines_ = 20;

  void refresh_cursor_cache();
  void apply_cursor_position(size_t position, bool extend_selection);
  void apply_command(const EditCommand &command);
  void apply_inverse_command(const EditCommand &command);
  bool should_merge_typing(std::string_view text) const;
};

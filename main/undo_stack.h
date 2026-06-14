#pragma once

#include "cursor.h"

#include <stddef.h>
#include <string>
#include <vector>

enum class EditCommandType {
  INSERT,
  DELETE,
};

struct EditCommand {
  EditCommandType type;
  size_t position;
  std::string text;
  Cursor cursor_before;
  Cursor cursor_after;
};

class UndoStack {
public:
  explicit UndoStack(size_t max_groups = 1000) : max_groups_(max_groups) {}

  void push(const EditCommand &command, bool merge_with_previous = false);
  bool can_undo() const { return !undo_stack_.empty(); }
  bool can_redo() const { return !redo_stack_.empty(); }
  std::vector<EditCommand> undo();
  std::vector<EditCommand> redo();
  void clear();
  size_t undo_group_count() const { return undo_stack_.size(); }
  size_t redo_group_count() const { return redo_stack_.size(); }

private:
  size_t max_groups_;
  std::vector<std::vector<EditCommand>> undo_stack_;
  std::vector<std::vector<EditCommand>> redo_stack_;

  bool can_merge_insert(const EditCommand &command) const;
};

#include "undo_stack.h"

#include <algorithm>

static size_t utf8_count_local(const std::string &text) {
  size_t count = 0;
  for (unsigned char c : text) {
    if ((c & 0xC0) != 0x80) {
      ++count;
    }
  }
  return count;
}

bool UndoStack::can_merge_insert(const EditCommand &command) const {
  if (undo_stack_.empty() || undo_stack_.back().empty()) {
    return false;
  }
  const EditCommand &last = undo_stack_.back().back();
  return last.type == EditCommandType::INSERT && command.type == EditCommandType::INSERT &&
         last.position + utf8_count_local(last.text) == command.position &&
         last.cursor_after.position == command.cursor_before.position;
}

void UndoStack::push(const EditCommand &command, bool merge_with_previous) {
  redo_stack_.clear();

  if (merge_with_previous && can_merge_insert(command)) {
    undo_stack_.back().push_back(command);
  } else {
    undo_stack_.push_back({command});
    if (undo_stack_.size() > max_groups_) {
      undo_stack_.erase(undo_stack_.begin());
    }
  }
}

std::vector<EditCommand> UndoStack::undo() {
  if (undo_stack_.empty()) {
    return {};
  }
  std::vector<EditCommand> group = undo_stack_.back();
  undo_stack_.pop_back();
  redo_stack_.push_back(group);
  return group;
}

std::vector<EditCommand> UndoStack::redo() {
  if (redo_stack_.empty()) {
    return {};
  }
  std::vector<EditCommand> group = redo_stack_.back();
  redo_stack_.pop_back();
  undo_stack_.push_back(group);
  return group;
}

void UndoStack::clear() {
  undo_stack_.clear();
  redo_stack_.clear();
}

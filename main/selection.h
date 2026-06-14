#pragma once

#include "piece_table.h"

#include <algorithm>
#include <stddef.h>
#include <string>

struct Selection {
  size_t anchor = 0;
  size_t cursor = 0;

  bool has_selection() const { return anchor != cursor; }
  size_t start() const { return std::min(anchor, cursor); }
  size_t end() const { return std::max(anchor, cursor); }
  size_t length() const { return end() - start(); }
  void clear(size_t position) {
    anchor = position;
    cursor = position;
  }
  void move_cursor(size_t position, bool extend) {
    if (!extend) {
      anchor = position;
    }
    cursor = position;
  }
  std::string selected_text(const PieceTable &document) const {
    return has_selection() ? document.text(start(), length()) : std::string();
  }
  std::string delete_selection(PieceTable &document) {
    std::string deleted = selected_text(document);
    if (has_selection()) {
      const size_t new_position = start();
      document.erase(new_position, length());
      clear(new_position);
    }
    return deleted;
  }
};

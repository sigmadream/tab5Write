#pragma once

#include <iosfwd>
#include <memory>
#include <optional>
#include <stddef.h>
#include <string>
#include <string_view>
#include <vector>

enum class PieceSource {
  ORIGINAL,
  ADD,
};

struct Piece {
  PieceSource source;
  size_t start;  // UTF-8 byte offset in the source buffer
  size_t length; // UTF-8 byte length
};

struct DocSnapshot {
  std::vector<Piece> pieces;
  std::shared_ptr<const std::string> original_buffer;
  std::shared_ptr<const std::string> add_buffer;
};

enum class FindDirection {
  FORWARD,
  BACKWARD,
};

class PieceTable {
public:
  PieceTable();
  explicit PieceTable(std::string_view original_text);

  void reset(std::string_view original_text = "");

  void insert(size_t position, std::string_view text);
  void erase(size_t position, size_t length);

  std::string text() const;
  std::string text(size_t start, size_t length) const;

  size_t length() const;
  size_t byte_length() const;
  size_t line_count() const;
  size_t piece_count() const { return pieces_.size(); }

  void compact();
  void set_compact_threshold(size_t threshold) { compact_threshold_ = threshold; }

  std::optional<size_t> find(std::string_view query, size_t start_position = 0,
                             FindDirection direction = FindDirection::FORWARD,
                             bool case_sensitive = true) const;
  std::vector<size_t> find_all(std::string_view query,
                               bool case_sensitive = true) const;

  DocSnapshot snapshot() const;
  static void serialize(const DocSnapshot &snapshot, std::ostream &output);
  static std::string serialize(const DocSnapshot &snapshot);

  size_t word_count() const;

  size_t line_for_position(size_t position) const;
  size_t column_for_position(size_t position) const;
  size_t position_for_line_column(size_t line, size_t column) const;
  size_t line_start_position(size_t position) const;
  size_t line_end_position(size_t position) const;
  size_t next_codepoint_position(size_t position) const;
  size_t previous_codepoint_position(size_t position) const;
  size_t next_word_position(size_t position) const;
  size_t previous_word_position(size_t position) const;

private:
  std::shared_ptr<std::string> original_buffer_;
  std::shared_ptr<std::string> add_buffer_;
  std::vector<Piece> pieces_;
  size_t compact_threshold_ = 10000;
  mutable bool word_count_cache_valid_ = false;
  mutable size_t cached_word_count_ = 0;

  const std::string &buffer_for(PieceSource source) const;
  std::string &buffer_for(PieceSource source);
  void split_at(size_t char_position);
  size_t piece_index_at_boundary(size_t char_position) const;
  void compact_if_needed();
  void invalidate_word_count_cache();
  void update_word_count_for_insert(size_t position, std::string_view inserted_text, const std::string &before_text);
  void update_word_count_for_erase(size_t position, size_t erased_length, const std::string &before_text);
};

size_t utf8_codepoint_count(std::string_view text);
size_t utf8_byte_offset_for_codepoints(std::string_view text, size_t codepoints);
size_t utf8_next_byte_offset(std::string_view text, size_t byte_offset);
size_t utf8_previous_byte_offset(std::string_view text, size_t byte_offset);
size_t utf8_char_index_for_byte_offset(std::string_view text, size_t byte_offset);

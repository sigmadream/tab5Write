#include "piece_table.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <sstream>

static bool is_utf8_continuation(unsigned char c) {
  return (c & 0xC0) == 0x80;
}

size_t utf8_codepoint_count(std::string_view text) {
  size_t count = 0;
  for (unsigned char c : text) {
    if (!is_utf8_continuation(c)) {
      ++count;
    }
  }
  return count;
}

size_t utf8_byte_offset_for_codepoints(std::string_view text, size_t codepoints) {
  if (codepoints == 0) {
    return 0;
  }

  size_t seen = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (!is_utf8_continuation(c)) {
      if (seen == codepoints) {
        return i;
      }
      ++seen;
    }
  }
  return text.size();
}

size_t utf8_next_byte_offset(std::string_view text, size_t byte_offset) {
  if (byte_offset >= text.size()) {
    return text.size();
  }
  ++byte_offset;
  while (byte_offset < text.size() && is_utf8_continuation(static_cast<unsigned char>(text[byte_offset]))) {
    ++byte_offset;
  }
  return byte_offset;
}

size_t utf8_previous_byte_offset(std::string_view text, size_t byte_offset) {
  if (byte_offset == 0 || text.empty()) {
    return 0;
  }
  byte_offset = std::min(byte_offset, text.size()) - 1;
  while (byte_offset > 0 && is_utf8_continuation(static_cast<unsigned char>(text[byte_offset]))) {
    --byte_offset;
  }
  return byte_offset;
}

size_t utf8_char_index_for_byte_offset(std::string_view text, size_t byte_offset) {
  byte_offset = std::min(byte_offset, text.size());
  return utf8_codepoint_count(text.substr(0, byte_offset));
}

static std::string ascii_fold(std::string_view input) {
  std::string folded;
  folded.reserve(input.size());
  for (unsigned char c : input) {
    folded.push_back(static_cast<char>(std::tolower(c)));
  }
  return folded;
}

static bool is_space_codepoint_at(std::string_view text, size_t byte_offset) {
  if (byte_offset >= text.size()) {
    return true;
  }
  unsigned char c = static_cast<unsigned char>(text[byte_offset]);
  return c < 0x80 && std::isspace(c);
}

static size_t count_words_in_bytes(std::string_view text) {
  size_t words = 0;
  bool in_word = false;
  for (size_t byte = 0; byte < text.size(); byte = utf8_next_byte_offset(text, byte)) {
    if (is_space_codepoint_at(text, byte)) {
      in_word = false;
    } else if (!in_word) {
      ++words;
      in_word = true;
    }
  }
  return words;
}

static size_t previous_word_boundary(std::string_view text, size_t char_position) {
  char_position = std::min(char_position, utf8_codepoint_count(text));
  size_t byte = utf8_byte_offset_for_codepoints(text, char_position);
  size_t chars = char_position;
  while (byte > 0) {
    const size_t prev = utf8_previous_byte_offset(text, byte);
    if (is_space_codepoint_at(text, prev)) {
      break;
    }
    byte = prev;
    --chars;
  }
  return chars;
}

static size_t next_word_boundary(std::string_view text, size_t char_position) {
  const size_t total = utf8_codepoint_count(text);
  char_position = std::min(char_position, total);
  size_t byte = utf8_byte_offset_for_codepoints(text, char_position);
  size_t chars = char_position;
  while (byte < text.size()) {
    if (is_space_codepoint_at(text, byte)) {
      break;
    }
    byte = utf8_next_byte_offset(text, byte);
    ++chars;
  }
  return chars;
}

PieceTable::PieceTable()
    : original_buffer_(std::make_shared<std::string>()),
      add_buffer_(std::make_shared<std::string>()) {}

PieceTable::PieceTable(std::string_view original_text) : PieceTable() {
  reset(original_text);
}

void PieceTable::reset(std::string_view original_text) {
  original_buffer_ = std::make_shared<std::string>(original_text);
  add_buffer_ = std::make_shared<std::string>();
  pieces_.clear();
  invalidate_word_count_cache();
  if (!original_buffer_->empty()) {
    pieces_.push_back({PieceSource::ORIGINAL, 0, original_buffer_->size()});
  }
}

const std::string &PieceTable::buffer_for(PieceSource source) const {
  return source == PieceSource::ORIGINAL ? *original_buffer_ : *add_buffer_;
}

std::string &PieceTable::buffer_for(PieceSource source) {
  return source == PieceSource::ORIGINAL ? *original_buffer_ : *add_buffer_;
}

void PieceTable::invalidate_word_count_cache() {
  word_count_cache_valid_ = false;
  cached_word_count_ = 0;
}

void PieceTable::update_word_count_for_insert(size_t position, std::string_view inserted_text,
                                              const std::string &before_text) {
  if (!word_count_cache_valid_) {
    return;
  }

  const size_t inserted_chars = utf8_codepoint_count(inserted_text);
  const size_t before_total = utf8_codepoint_count(before_text);
  const size_t affected_start = previous_word_boundary(before_text, std::min(position, before_total));
  const size_t affected_end = next_word_boundary(before_text, std::min(position, before_total));
  const size_t before_start_byte = utf8_byte_offset_for_codepoints(before_text, affected_start);
  const size_t before_end_byte = utf8_byte_offset_for_codepoints(before_text, affected_end);
  const size_t before_words = count_words_in_bytes(
      std::string_view(before_text).substr(before_start_byte, before_end_byte - before_start_byte));

  const std::string after_text = text();
  const size_t after_probe = std::min(affected_end + inserted_chars, utf8_codepoint_count(after_text));
  const size_t after_end = next_word_boundary(after_text, after_probe);
  const size_t after_words = count_words_in_bytes(text(affected_start, after_end - affected_start));

  cached_word_count_ = cached_word_count_ - before_words + after_words;
}

void PieceTable::update_word_count_for_erase(size_t position, size_t erased_length,
                                             const std::string &before_text) {
  if (!word_count_cache_valid_) {
    return;
  }

  const size_t before_total = utf8_codepoint_count(before_text);
  const size_t erase_end = std::min(before_total, position + erased_length);
  const size_t affected_start = previous_word_boundary(before_text, std::min(position, before_total));
  const size_t affected_end = next_word_boundary(before_text, erase_end);
  const size_t before_words = count_words_in_bytes(
      std::string_view(before_text).substr(
          utf8_byte_offset_for_codepoints(before_text, affected_start),
          utf8_byte_offset_for_codepoints(before_text, affected_end) -
              utf8_byte_offset_for_codepoints(before_text, affected_start)));

  const std::string after_text = text();
  const size_t after_end = next_word_boundary(after_text, affected_start);
  const size_t after_words = count_words_in_bytes(text(affected_start, after_end - affected_start));

  cached_word_count_ = cached_word_count_ - before_words + after_words;
}

size_t PieceTable::length() const {
  size_t total = 0;
  for (const auto &piece : pieces_) {
    const auto &buffer = buffer_for(piece.source);
    total += utf8_codepoint_count(std::string_view(buffer).substr(piece.start, piece.length));
  }
  return total;
}

size_t PieceTable::byte_length() const {
  size_t total = 0;
  for (const auto &piece : pieces_) {
    total += piece.length;
  }
  return total;
}

std::string PieceTable::text() const {
  std::string result;
  result.reserve(byte_length());
  for (const auto &piece : pieces_) {
    const auto &buffer = buffer_for(piece.source);
    result.append(buffer, piece.start, piece.length);
  }
  return result;
}

std::string PieceTable::text(size_t start, size_t requested_length) const {
  const std::string all = text();
  const size_t total_chars = utf8_codepoint_count(all);
  start = std::min(start, total_chars);
  requested_length = std::min(requested_length, total_chars - start);
  const size_t start_byte = utf8_byte_offset_for_codepoints(all, start);
  const size_t end_byte = utf8_byte_offset_for_codepoints(std::string_view(all).substr(start_byte), requested_length) + start_byte;
  return all.substr(start_byte, end_byte - start_byte);
}

size_t PieceTable::line_count() const {
  size_t lines = 1;
  for (const auto &piece : pieces_) {
    const auto &buffer = buffer_for(piece.source);
    auto view = std::string_view(buffer).substr(piece.start, piece.length);
    lines += static_cast<size_t>(std::count(view.begin(), view.end(), '\n'));
  }
  return lines;
}

void PieceTable::split_at(size_t char_position) {
  if (char_position == 0 || char_position >= length()) {
    return;
  }

  size_t cursor = 0;
  for (size_t i = 0; i < pieces_.size(); ++i) {
    const auto &piece = pieces_[i];
    const auto &buffer = buffer_for(piece.source);
    const auto piece_text = std::string_view(buffer).substr(piece.start, piece.length);
    const size_t piece_chars = utf8_codepoint_count(piece_text);

    if (char_position == cursor) {
      return;
    }
    if (char_position > cursor && char_position < cursor + piece_chars) {
      const size_t left_chars = char_position - cursor;
      const size_t left_bytes = utf8_byte_offset_for_codepoints(piece_text, left_chars);
      Piece left{piece.source, piece.start, left_bytes};
      Piece right{piece.source, piece.start + left_bytes, piece.length - left_bytes};
      pieces_[i] = left;
      if (right.length > 0) {
        pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(i + 1), right);
      }
      return;
    }
    cursor += piece_chars;
  }
}

size_t PieceTable::piece_index_at_boundary(size_t char_position) const {
  size_t cursor = 0;
  for (size_t i = 0; i < pieces_.size(); ++i) {
    if (cursor == char_position) {
      return i;
    }
    const auto &piece = pieces_[i];
    const auto &buffer = buffer_for(piece.source);
    cursor += utf8_codepoint_count(std::string_view(buffer).substr(piece.start, piece.length));
  }
  return pieces_.size();
}

void PieceTable::insert(size_t position, std::string_view inserted_text) {
  if (inserted_text.empty()) {
    return;
  }

  position = std::min(position, length());
  const std::string before_text = word_count_cache_valid_ ? text() : std::string();
  split_at(position);

  const size_t add_start = add_buffer_->size();
  add_buffer_->append(inserted_text.data(), inserted_text.size());
  Piece new_piece{PieceSource::ADD, add_start, inserted_text.size()};
  const size_t index = piece_index_at_boundary(position);
  pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(index), new_piece);
  update_word_count_for_insert(position, inserted_text, before_text);
  compact_if_needed();
}

void PieceTable::erase(size_t position, size_t erase_length) {
  const size_t total = length();
  if (erase_length == 0 || position >= total) {
    return;
  }

  const size_t end = std::min(total, position + erase_length);
  const size_t actual_erased = end - position;
  const std::string before_text = word_count_cache_valid_ ? text() : std::string();
  split_at(end);
  split_at(position);

  size_t cursor = 0;
  std::vector<Piece> kept;
  kept.reserve(pieces_.size());
  for (const auto &piece : pieces_) {
    const auto &buffer = buffer_for(piece.source);
    const size_t piece_chars = utf8_codepoint_count(std::string_view(buffer).substr(piece.start, piece.length));
    const size_t piece_end = cursor + piece_chars;
    if (!(cursor >= position && piece_end <= end)) {
      kept.push_back(piece);
    }
    cursor = piece_end;
  }
  pieces_.swap(kept);
  update_word_count_for_erase(position, actual_erased, before_text);
  compact_if_needed();
}

void PieceTable::compact_if_needed() {
  if (compact_threshold_ > 0 && pieces_.size() > compact_threshold_) {
    compact();
  }
}

void PieceTable::compact() {
  const std::string flattened = text();
  if (word_count_cache_valid_) {
    cached_word_count_ = count_words_in_bytes(flattened);
  }
  original_buffer_ = std::make_shared<std::string>(flattened);
  add_buffer_ = std::make_shared<std::string>();
  pieces_.clear();
  if (!original_buffer_->empty()) {
    pieces_.push_back({PieceSource::ORIGINAL, 0, original_buffer_->size()});
  }
}

std::optional<size_t> PieceTable::find(std::string_view query, size_t start_position,
                                       FindDirection direction,
                                       bool case_sensitive) const {
  if (query.empty()) {
    return std::nullopt;
  }

  const std::string haystack = case_sensitive ? text() : ascii_fold(text());
  const std::string needle = case_sensitive ? std::string(query) : ascii_fold(query);
  const size_t total_chars = utf8_codepoint_count(haystack);
  start_position = std::min(start_position, total_chars);
  const size_t start_byte = utf8_byte_offset_for_codepoints(haystack, start_position);

  size_t found = std::string::npos;
  if (direction == FindDirection::FORWARD) {
    found = haystack.find(needle, start_byte);
  } else {
    if (start_byte == 0) {
      found = std::string::npos;
    } else {
      found = haystack.rfind(needle, start_byte - 1);
    }
  }

  if (found == std::string::npos) {
    return std::nullopt;
  }
  return utf8_char_index_for_byte_offset(haystack, found);
}

std::vector<size_t> PieceTable::find_all(std::string_view query, bool case_sensitive) const {
  std::vector<size_t> matches;
  if (query.empty()) {
    return matches;
  }

  const std::string haystack = case_sensitive ? text() : ascii_fold(text());
  const std::string needle = case_sensitive ? std::string(query) : ascii_fold(query);

  size_t byte_pos = 0;
  while ((byte_pos = haystack.find(needle, byte_pos)) != std::string::npos) {
    matches.push_back(utf8_char_index_for_byte_offset(haystack, byte_pos));
    byte_pos += std::max<size_t>(needle.size(), 1);
  }
  return matches;
}

DocSnapshot PieceTable::snapshot() const {
  return {pieces_, original_buffer_, add_buffer_};
}

void PieceTable::serialize(const DocSnapshot &snapshot, std::ostream &output) {
  for (const auto &piece : snapshot.pieces) {
    const auto &buffer = piece.source == PieceSource::ORIGINAL ? *snapshot.original_buffer : *snapshot.add_buffer;
    output.write(buffer.data() + static_cast<std::ptrdiff_t>(piece.start), static_cast<std::streamsize>(piece.length));
  }
}

std::string PieceTable::serialize(const DocSnapshot &snapshot) {
  std::ostringstream output;
  serialize(snapshot, output);
  return output.str();
}

size_t PieceTable::word_count() const {
  if (!word_count_cache_valid_) {
    cached_word_count_ = count_words_in_bytes(text());
    word_count_cache_valid_ = true;
  }
  return cached_word_count_;
}

size_t PieceTable::line_for_position(size_t position) const {
  const std::string all = text();
  position = std::min(position, utf8_codepoint_count(all));
  const size_t end_byte = utf8_byte_offset_for_codepoints(all, position);
  return static_cast<size_t>(std::count(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(end_byte), '\n'));
}

size_t PieceTable::column_for_position(size_t position) const {
  const std::string all = text();
  position = std::min(position, utf8_codepoint_count(all));
  size_t current_line_start = 0;
  size_t char_index = 0;
  for (size_t byte = 0; byte < all.size() && char_index < position; byte = utf8_next_byte_offset(all, byte), ++char_index) {
    if (all[byte] == '\n') {
      current_line_start = char_index + 1;
    }
  }
  return position - current_line_start;
}

size_t PieceTable::position_for_line_column(size_t line, size_t column) const {
  const std::string all = text();
  size_t current_line = 0;
  size_t current_column = 0;
  size_t char_index = 0;

  for (size_t byte = 0; byte < all.size(); byte = utf8_next_byte_offset(all, byte), ++char_index) {
    if (current_line == line && current_column == column) {
      return char_index;
    }
    if (all[byte] == '\n') {
      if (current_line == line) {
        return char_index;
      }
      ++current_line;
      current_column = 0;
    } else {
      ++current_column;
    }
  }

  return char_index;
}

size_t PieceTable::line_start_position(size_t position) const {
  position = std::min(position, length());
  return position_for_line_column(line_for_position(position), 0);
}

size_t PieceTable::line_end_position(size_t position) const {
  const size_t line = line_for_position(position);
  const std::string all = text();
  size_t current_line = 0;
  size_t char_index = 0;
  for (size_t byte = 0; byte < all.size(); byte = utf8_next_byte_offset(all, byte), ++char_index) {
    if (current_line == line && all[byte] == '\n') {
      return char_index;
    }
    if (all[byte] == '\n') {
      ++current_line;
    }
  }
  return char_index;
}

size_t PieceTable::next_codepoint_position(size_t position) const {
  return std::min(position + 1, length());
}

size_t PieceTable::previous_codepoint_position(size_t position) const {
  return position == 0 ? 0 : position - 1;
}

size_t PieceTable::next_word_position(size_t position) const {
  const std::string all = text();
  const size_t total = utf8_codepoint_count(all);
  position = std::min(position, total);
  size_t byte = utf8_byte_offset_for_codepoints(all, position);
  size_t chars = position;

  while (byte < all.size() && std::isspace(static_cast<unsigned char>(all[byte]))) {
    byte = utf8_next_byte_offset(all, byte);
    ++chars;
  }
  while (byte < all.size() && !std::isspace(static_cast<unsigned char>(all[byte]))) {
    byte = utf8_next_byte_offset(all, byte);
    ++chars;
  }
  return chars;
}

size_t PieceTable::previous_word_position(size_t position) const {
  const std::string all = text();
  position = std::min(position, utf8_codepoint_count(all));
  size_t byte = utf8_byte_offset_for_codepoints(all, position);
  size_t chars = position;

  while (byte > 0) {
    const size_t prev = utf8_previous_byte_offset(all, byte);
    if (!std::isspace(static_cast<unsigned char>(all[prev]))) {
      break;
    }
    byte = prev;
    --chars;
  }
  while (byte > 0) {
    const size_t prev = utf8_previous_byte_offset(all, byte);
    if (std::isspace(static_cast<unsigned char>(all[prev]))) {
      break;
    }
    byte = prev;
    --chars;
  }
  return chars;
}

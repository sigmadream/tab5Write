#include "editor_core.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

static void test_piece_table_basics() {
  PieceTable doc;
  doc.insert(0, "hello world");
  assert(doc.text() == "hello world");
  assert(doc.length() == 11);

  doc.insert(5, ", beautiful");
  assert(doc.text() == "hello, beautiful world");

  doc.erase(doc.length() - 5, 5);
  assert(doc.text() == "hello, beautiful ");
  doc.erase(doc.length() - 1, 1);
  assert(doc.text() == "hello, beautiful");

  const size_t lines_before = doc.line_count();
  doc.insert(5, "\n");
  assert(doc.line_count() == lines_before + 1);
  doc.erase(5, 1);
  assert(doc.line_count() == lines_before);
}

static void test_undo_redo_and_redo_clear() {
  EditorCore editor;
  editor.insert_text("hello");
  editor.insert_text(" ");
  editor.insert_text("world");
  assert(editor.text() == "hello world");
  assert(editor.undo_group_count() == 3); // whitespace separates typing groups

  editor.undo();
  assert(editor.text() == "hello ");
  assert(editor.can_redo());

  editor.redo();
  assert(editor.text() == "hello world");

  editor.undo();
  assert(editor.text() == "hello ");
  editor.insert_text("TABWRITE");
  assert(editor.text() == "hello TABWRITE");
  assert(!editor.can_redo());
}

static void test_find_snapshot_cursor_selection() {
  EditorCore editor("hello world\nhello tabwrite\nlast line");

  auto first = editor.find("hello", 0, FindDirection::FORWARD);
  assert(first.has_value() && *first == 0);
  auto next = editor.find("hello", 1, FindDirection::FORWARD);
  assert(next.has_value() && *next == 12);
  auto prev = editor.find("hello", editor.length(), FindDirection::BACKWARD);
  assert(prev.has_value() && *prev == 12);
  auto all = editor.find_all("hello");
  assert(all.size() == 2 && all[0] == 0 && all[1] == 12);

  auto snapshot = editor.snapshot();
  assert(PieceTable::serialize(snapshot) == editor.text());
  editor.insert_text("prefix ");
  assert(PieceTable::serialize(snapshot) == "hello world\nhello tabwrite\nlast line");

  EditorCore jumps("hello world");
  jumps.move_word_right();
  assert(jumps.cursor().position == 5);
  jumps.move_word_right();
  assert(jumps.cursor().position == 11);

  EditorCore selection("hello");
  selection.move_right(true);
  selection.move_right(true);
  selection.move_right(true);
  assert(selection.has_selection());
  assert(selection.selected_text() == "hel");
  selection.delete_selection();
  assert(selection.text() == "lo");
}

static void test_utf8_and_word_count() {
  EditorCore editor;
  editor.insert_text("한글 abc");
  assert(editor.length() == 6); // 한, 글, space, a, b, c
  editor.delete_backward(1);
  assert(editor.text() == "한글 ab");
  editor.move_doc_start();
  editor.move_right();
  editor.delete_forward(1);
  assert(editor.text() == "한 ab");
  assert(editor.word_count() == 2);

  PieceTable doc("hello world");
  assert(doc.word_count() == 2);
  doc.insert(5, ",");
  assert(doc.word_count() == 2);
  doc.insert(6, " tabwrite");
  assert(doc.word_count() == 3);
  doc.erase(6, 9);
  assert(doc.word_count() == 2);
}

static void test_compact_and_performance() {
  PieceTable doc;
  doc.set_compact_threshold(10000);
  std::string seed(10000, 'a');
  doc.insert(0, seed);

  auto start = std::chrono::steady_clock::now();
  doc.insert(5000, "b");
  doc.erase(5000, 1);
  auto end = std::chrono::steady_clock::now();
  auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  assert(doc.text() == seed);
  assert(elapsed_us < 1000);

  PieceTable compact_doc;
  compact_doc.set_compact_threshold(10000);
  for (int i = 0; i < 10001; ++i) {
    compact_doc.insert(compact_doc.length(), "x");
  }
  assert(compact_doc.text() == std::string(10001, 'x'));
  assert(compact_doc.piece_count() == 1);
}

int main() {
  test_piece_table_basics();
  test_undo_redo_and_redo_clear();
  test_find_snapshot_cursor_selection();
  test_utf8_and_word_count();
  test_compact_and_performance();
  std::cout << "Editor Core Phase 4 host tests passed" << std::endl;
  return 0;
}

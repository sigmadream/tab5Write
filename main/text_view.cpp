#include "text_view.h"

#include "piece_table.h"
#include "theme.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

LV_FONT_DECLARE(x12y12pxMaruMinyaHangul_36);

namespace {

constexpr int32_t kInsetLeft = 28;
constexpr int32_t kInsetRight = 28;
constexpr int32_t kInsetTop = 24;
constexpr int32_t kInsetBottom = 24;
constexpr int32_t kLineGap = 6;
constexpr int32_t kCursorWidth = 3;
constexpr uint32_t kBlinkPeriodMs = 500;

struct TextViewLine {
  std::string text;
  size_t start_cp = 0;
  size_t end_cp = 0;
};

struct TextViewState {
  std::string document_text;
  std::string composing_text;
  std::string display_text_cache;
  std::vector<TextViewLine> line_cache;
  size_t cursor_position = 0;
  size_t visual_cursor_position = 0;
  size_t composing_start_position = 0;
  size_t composing_end_position = 0;
  int32_t scroll_line = 0;
  int32_t cached_width = -1;
  bool cache_dirty = true;
  bool cursor_visible = true;
  lv_timer_t *blink_timer = nullptr;
};

const lv_font_t *editor_font() {
  static lv_font_t font = lv_font_montserrat_20;
  static bool initialized = false;
  if (!initialized) {
    font.fallback = &x12y12pxMaruMinyaHangul_36;
    initialized = true;
  }
  return &font;
}

int32_t line_height() {
  const lv_font_t *font = editor_font();
  int32_t height = lv_font_get_line_height(font);
  if (font->fallback != nullptr) {
    height = std::max(height, lv_font_get_line_height(font->fallback));
  }
  return height + kLineGap;
}

TextViewState *state_for(lv_obj_t *obj) {
  return static_cast<TextViewState *>(lv_obj_get_user_data(obj));
}

int32_t content_width_for(lv_obj_t *obj) {
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  const int32_t width = lv_area_get_width(&coords) - kInsetLeft - kInsetRight;
  return std::max<int32_t>(1, width);
}

int32_t content_height_for(lv_obj_t *obj) {
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  const int32_t height = lv_area_get_height(&coords) - kInsetTop - kInsetBottom;
  return std::max<int32_t>(1, height);
}

int32_t measure_text_width(std::string_view text) {
  if (text.empty()) {
    return 0;
  }
  lv_point_t size{};
  std::string stable(text);
  lv_text_get_size(&size, stable.c_str(), editor_font(), 0, kLineGap, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}

int32_t wrap_cell_width() {
  static int32_t width = 0;
  if (width == 0) {
    width = std::max<int32_t>(measure_text_width("M"), measure_text_width("가"));
    width = std::max<int32_t>(width, 1);
  }
  return width;
}

std::string utf8_slice_by_codepoints(std::string_view text, size_t start_cp, size_t end_cp) {
  if (end_cp <= start_cp) {
    return "";
  }
  const size_t start_byte = utf8_byte_offset_for_codepoints(text, start_cp);
  const size_t end_byte = utf8_byte_offset_for_codepoints(text, end_cp);
  return std::string(text.substr(start_byte, end_byte - start_byte));
}

void push_line(TextViewState *state, std::string &line, size_t start_cp, size_t end_cp) {
  state->line_cache.push_back({line, start_cp, end_cp});
  line.clear();
}

void build_display_text(TextViewState *state) {
  const size_t document_len = utf8_codepoint_count(state->document_text);
  state->cursor_position = std::min(state->cursor_position, document_len);
  state->composing_start_position = state->cursor_position;
  const size_t cursor_byte = utf8_byte_offset_for_codepoints(state->document_text, state->cursor_position);

  state->display_text_cache.clear();
  state->display_text_cache.reserve(state->document_text.size() + state->composing_text.size());
  state->display_text_cache.append(state->document_text, 0, cursor_byte);
  state->display_text_cache += state->composing_text;
  state->display_text_cache.append(state->document_text, cursor_byte, std::string::npos);

  const size_t composing_len = utf8_codepoint_count(state->composing_text);
  state->composing_end_position = state->composing_start_position + composing_len;
  state->visual_cursor_position = state->composing_end_position;
}

void rebuild_cache(lv_obj_t *obj) {
  TextViewState *state = state_for(obj);
  if (state == nullptr) {
    return;
  }

  const int32_t max_width = content_width_for(obj);
  if (!state->cache_dirty && state->cached_width == max_width) {
    return;
  }

  build_display_text(state);
  state->line_cache.clear();
  state->cached_width = max_width;

  std::string line;
  size_t line_start = 0;
  size_t cp_index = 0;
  const int32_t max_columns = std::max<int32_t>(1, max_width / wrap_cell_width());
  int32_t current_columns = 0;

  for (size_t byte = 0; byte < state->display_text_cache.size();) {
    const size_t next_byte = utf8_next_byte_offset(state->display_text_cache, byte);
    const std::string glyph = state->display_text_cache.substr(byte, next_byte - byte);

    if (glyph == "\n") {
      push_line(state, line, line_start, cp_index);
      ++cp_index;
      line_start = cp_index;
      current_columns = 0;
      byte = next_byte;
      continue;
    }

    const int32_t glyph_columns = glyph == "\t" ? 2 : 1;
    if (!line.empty() && current_columns + glyph_columns > max_columns) {
      push_line(state, line, line_start, cp_index);
      line_start = cp_index;
      current_columns = 0;
    }

    line += glyph;
    current_columns += glyph_columns;
    ++cp_index;
    byte = next_byte;
  }

  push_line(state, line, line_start, cp_index);
  state->cache_dirty = false;
}

int32_t visible_line_count_for(lv_obj_t *obj) {
  return std::max<int32_t>(1, content_height_for(obj) / line_height());
}

int32_t cursor_line_index(TextViewState *state) {
  for (size_t i = 0; i < state->line_cache.size(); ++i) {
    const auto &line = state->line_cache[i];
    if (state->visual_cursor_position >= line.start_cp && state->visual_cursor_position <= line.end_cp) {
      return static_cast<int32_t>(i);
    }
  }
  return state->line_cache.empty() ? 0 : static_cast<int32_t>(state->line_cache.size() - 1);
}

void ensure_cursor_visible(lv_obj_t *obj) {
  TextViewState *state = state_for(obj);
  if (state == nullptr) {
    return;
  }
  rebuild_cache(obj);
  const int32_t visible_lines = visible_line_count_for(obj);
  const int32_t cursor_line = cursor_line_index(state);
  state->scroll_line = std::max<int32_t>(0, cursor_line - visible_lines / 2);
}

int32_t x_for_position_in_line(const TextViewLine &line, size_t position) {
  position = std::min(std::max(position, line.start_cp), line.end_cp);
  return measure_text_width(utf8_slice_by_codepoints(line.text, 0, position - line.start_cp));
}

void draw_text_line(lv_layer_t *layer, const TextViewLine &line, const lv_area_t &area) {
  lv_draw_label_dsc_t label_dsc;
  lv_draw_label_dsc_init(&label_dsc);
  label_dsc.font = editor_font();
  label_dsc.color = THEME_TEXT_PRIMARY;
  label_dsc.text = line.text.c_str();
  label_dsc.text_static = 1;
  label_dsc.opa = LV_OPA_COVER;
  lv_draw_label(layer, &label_dsc, &area);
}

void draw_rect(lv_layer_t *layer, const lv_area_t &area, lv_color_t color, lv_opa_t opa) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = color;
  dsc.bg_opa = opa;
  lv_draw_rect(layer, &dsc, &area);
}

void draw_event(lv_event_t *event) {
  lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(event));
  TextViewState *state = state_for(obj);
  if (state == nullptr) {
    return;
  }
  rebuild_cache(obj);

  lv_layer_t *layer = lv_event_get_layer(event);
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  const int32_t x0 = coords.x1 + kInsetLeft;
  const int32_t y0 = coords.y1 + kInsetTop;
  const int32_t width = content_width_for(obj);
  const int32_t lh = line_height();
  const int32_t visible_lines = visible_line_count_for(obj);

  const int32_t first_line = std::max<int32_t>(0, state->scroll_line);
  const int32_t last_line = std::min<int32_t>(static_cast<int32_t>(state->line_cache.size()),
                                              first_line + visible_lines + 1);

  for (int32_t i = first_line; i < last_line; ++i) {
    const int32_t y = y0 + (i - first_line) * lh;
    lv_area_t line_area{x0, y, x0 + width, y + lh - 1};
    draw_text_line(layer, state->line_cache[static_cast<size_t>(i)], line_area);

    const auto &line = state->line_cache[static_cast<size_t>(i)];
    const size_t highlight_start = std::max(line.start_cp, state->composing_start_position);
    const size_t highlight_end = std::min(line.end_cp, state->composing_end_position);
    if (highlight_end > highlight_start) {
      const int32_t x1 = x0 + x_for_position_in_line(line, highlight_start);
      const int32_t x2 = x0 + x_for_position_in_line(line, highlight_end);
      lv_area_t underline{x1, y + lh - 4, std::max<int32_t>(x1 + 2, x2), y + lh - 2};
      draw_rect(layer, underline, THEME_ACCENT, LV_OPA_COVER);
    }
  }

  if (state->cursor_visible) {
    const int32_t cursor_line = cursor_line_index(state);
    if (cursor_line >= first_line && cursor_line < last_line) {
      const auto &line = state->line_cache[static_cast<size_t>(cursor_line)];
      const int32_t cursor_x = x0 + x_for_position_in_line(line, state->visual_cursor_position);
      const int32_t cursor_y = y0 + (cursor_line - first_line) * lh;
      lv_area_t cursor_area{cursor_x, cursor_y + 2, cursor_x + kCursorWidth - 1, cursor_y + lh - 4};
      draw_rect(layer, cursor_area, THEME_ACCENT, LV_OPA_COVER);
    }
  }
}

void blink_timer_cb(lv_timer_t *timer) {
  lv_obj_t *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
  TextViewState *state = state_for(obj);
  if (state == nullptr) {
    return;
  }
  state->cursor_visible = !state->cursor_visible;
  lv_obj_invalidate(obj);
}

void text_view_event_cb(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(event));
  TextViewState *state = state_for(obj);

  if (code == LV_EVENT_DRAW_MAIN) {
    draw_event(event);
  } else if (code == LV_EVENT_SIZE_CHANGED && state != nullptr) {
    state->cache_dirty = true;
    ensure_cursor_visible(obj);
  } else if (code == LV_EVENT_DELETE && state != nullptr) {
    if (state->blink_timer != nullptr) {
      lv_timer_delete(state->blink_timer);
      state->blink_timer = nullptr;
    }
    delete state;
    lv_obj_set_user_data(obj, nullptr);
  }
}

} // namespace

lv_obj_t *text_view_create(lv_obj_t *parent) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(obj, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);

  auto *state = new TextViewState();
  lv_obj_set_user_data(obj, state);
  lv_obj_add_event_cb(obj, text_view_event_cb, LV_EVENT_ALL, nullptr);
  state->blink_timer = lv_timer_create(blink_timer_cb, kBlinkPeriodMs, obj);
  return obj;
}

void text_view_set_document(lv_obj_t *view, const std::string &document_text,
                            size_t cursor_position,
                            const std::string &composing_text) {
  TextViewState *state = state_for(view);
  if (state == nullptr) {
    return;
  }
  state->document_text = document_text;
  state->cursor_position = cursor_position;
  state->composing_text = composing_text;
  state->cache_dirty = true;
  state->cursor_visible = true;
  ensure_cursor_visible(view);
  lv_obj_invalidate(view);
}

void text_view_reset_cursor_blink(lv_obj_t *view) {
  TextViewState *state = state_for(view);
  if (state == nullptr || state->blink_timer == nullptr) {
    return;
  }
  state->cursor_visible = true;
  lv_timer_reset(state->blink_timer);
  lv_obj_invalidate(view);
}

int32_t text_view_visible_line_count(lv_obj_t *view) {
  return visible_line_count_for(view);
}

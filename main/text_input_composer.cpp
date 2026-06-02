#include "text_input_composer.h"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#define LOG_I(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
#include <cstdio>
#define LOG_I(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

static const char *TAG = "TEXT_INPUT_COMPOSER";

struct Jamo {
  enum class Type { NONE, CONSONANT, VOWEL };
  Type type = Type::NONE;
  int cho_idx = -1;
  int jung_idx = -1;
  int jong_idx = -1;
  uint32_t compatibility_code = 0;
};

static Jamo get_jamo_from_char(char c) {
  Jamo j;
  switch (c) {
    // 자음 (Consonants)
    case 'q': j = {Jamo::Type::CONSONANT, 7, -1, 17, 0x3142}; break; // ㅂ
    case 'Q': j = {Jamo::Type::CONSONANT, 8, -1, -1, 0x3143}; break; // ㅃ
    case 'w': j = {Jamo::Type::CONSONANT, 12, -1, 22, 0x3148}; break; // ㅈ
    case 'W': j = {Jamo::Type::CONSONANT, 13, -1, -1, 0x3149}; break; // ㅉ
    case 'e': j = {Jamo::Type::CONSONANT, 3, -1, 7, 0x3137}; break; // ㄷ
    case 'E': j = {Jamo::Type::CONSONANT, 4, -1, -1, 0x3138}; break; // ㄸ
    case 'r': j = {Jamo::Type::CONSONANT, 0, -1, 1, 0x3131}; break; // ㄱ
    case 'R': j = {Jamo::Type::CONSONANT, 1, -1, 2, 0x3132}; break; // ㄲ
    case 't': j = {Jamo::Type::CONSONANT, 9, -1, 19, 0x3145}; break; // ㅅ
    case 'T': j = {Jamo::Type::CONSONANT, 10, -1, 20, 0x3146}; break; // ㅆ
    case 'a': j = {Jamo::Type::CONSONANT, 6, -1, 16, 0x3141}; break; // ㅁ
    case 's': j = {Jamo::Type::CONSONANT, 2, -1, 4, 0x3134}; break; // ㄴ
    case 'd': j = {Jamo::Type::CONSONANT, 11, -1, 21, 0x3147}; break; // ㅇ
    case 'f': j = {Jamo::Type::CONSONANT, 5, -1, 8, 0x3139}; break; // ㄹ
    case 'g': j = {Jamo::Type::CONSONANT, 18, -1, 27, 0x314E}; break; // ㅎ
    case 'z': j = {Jamo::Type::CONSONANT, 15, -1, 24, 0x314B}; break; // ㅋ
    case 'x': j = {Jamo::Type::CONSONANT, 16, -1, 25, 0x314C}; break; // ㅌ
    case 'c': j = {Jamo::Type::CONSONANT, 14, -1, 23, 0x314A}; break; // ㅊ
    case 'v': j = {Jamo::Type::CONSONANT, 17, -1, 26, 0x314D}; break; // ㅍ

    // caps lock 대문자 매칭
    case 'A': j = {Jamo::Type::CONSONANT, 6, -1, 16, 0x3141}; break; // ㅁ
    case 'S': j = {Jamo::Type::CONSONANT, 2, -1, 4, 0x3134}; break; // ㄴ
    case 'D': j = {Jamo::Type::CONSONANT, 11, -1, 21, 0x3147}; break; // ㅇ
    case 'F': j = {Jamo::Type::CONSONANT, 5, -1, 8, 0x3139}; break; // ㄹ
    case 'G': j = {Jamo::Type::CONSONANT, 18, -1, 27, 0x314E}; break; // ㅎ
    case 'Z': j = {Jamo::Type::CONSONANT, 15, -1, 24, 0x314B}; break; // ㅋ
    case 'X': j = {Jamo::Type::CONSONANT, 16, -1, 25, 0x314C}; break; // ㅌ
    case 'C': j = {Jamo::Type::CONSONANT, 14, -1, 23, 0x314A}; break; // ㅊ
    case 'V': j = {Jamo::Type::CONSONANT, 17, -1, 26, 0x314D}; break; // ㅍ

    // 모음 (Vowels)
    case 'y': j = {Jamo::Type::VOWEL, -1, 12, -1, 0x315B}; break; // ㅛ
    case 'u': j = {Jamo::Type::VOWEL, -1, 6, -1, 0x3155}; break; // ㅕ
    case 'i': j = {Jamo::Type::VOWEL, -1, 2, -1, 0x3151}; break; // ㅑ
    case 'o': j = {Jamo::Type::VOWEL, -1, 1, -1, 0x3150}; break; // ㅐ
    case 'O': j = {Jamo::Type::VOWEL, -1, 3, -1, 0x3152}; break; // ㅒ
    case 'p': j = {Jamo::Type::VOWEL, -1, 5, -1, 0x3154}; break; // ㅔ
    case 'P': j = {Jamo::Type::VOWEL, -1, 7, -1, 0x3156}; break; // ㅖ
    case 'h': j = {Jamo::Type::VOWEL, -1, 8, -1, 0x3157}; break; // ㅗ
    case 'j': j = {Jamo::Type::VOWEL, -1, 4, -1, 0x3153}; break; // ㅓ
    case 'k': j = {Jamo::Type::VOWEL, -1, 0, -1, 0x314F}; break; // ㅏ
    case 'l': j = {Jamo::Type::VOWEL, -1, 20, -1, 0x3163}; break; // ㅣ
    case 'b': j = {Jamo::Type::VOWEL, -1, 17, -1, 0x3160}; break; // ㅠ
    case 'n': j = {Jamo::Type::VOWEL, -1, 13, -1, 0x315C}; break; // ㅜ
    case 'm': j = {Jamo::Type::VOWEL, -1, 18, -1, 0x3161}; break; // ㅡ

    case 'Y': j = {Jamo::Type::VOWEL, -1, 12, -1, 0x315B}; break; // ㅛ
    case 'U': j = {Jamo::Type::VOWEL, -1, 6, -1, 0x3155}; break; // ㅕ
    case 'I': j = {Jamo::Type::VOWEL, -1, 2, -1, 0x3151}; break; // ㅑ
    case 'J': j = {Jamo::Type::VOWEL, -1, 4, -1, 0x3153}; break; // ㅓ
    case 'K': j = {Jamo::Type::VOWEL, -1, 0, -1, 0x314F}; break; // ㅏ
    case 'L': j = {Jamo::Type::VOWEL, -1, 20, -1, 0x3163}; break; // ㅣ
    case 'B': j = {Jamo::Type::VOWEL, -1, 17, -1, 0x3160}; break; // ㅠ
    case 'N': j = {Jamo::Type::VOWEL, -1, 13, -1, 0x315C}; break; // ㅜ
    case 'M': j = {Jamo::Type::VOWEL, -1, 18, -1, 0x3161}; break; // ㅡ

    default: break;
  }
  return j;
}

static int combine_vowels(int v1, int v2) {
  if (v1 == 8) { // ㅗ
    if (v2 == 0) return 9;   // ㅘ
    if (v2 == 1) return 10;  // ㅙ
    if (v2 == 20) return 11; // ㅚ
  } else if (v1 == 13) { // ㅜ
    if (v2 == 4) return 14;  // ㅝ
    if (v2 == 5) return 15;  // ㅞ
    if (v2 == 20) return 16; // ㅟ
  } else if (v1 == 18) { // ㅡ
    if (v2 == 20) return 19; // ㅢ
  }
  return -1;
}

static bool split_vowel(int v, int &v1, int &v2) {
  switch (v) {
    case 9:  v1 = 8; v2 = 0; return true;
    case 10: v1 = 8; v2 = 1; return true;
    case 11: v1 = 8; v2 = 20; return true;
    case 14: v1 = 13; v2 = 4; return true;
    case 15: v1 = 13; v2 = 5; return true;
    case 16: v1 = 13; v2 = 20; return true;
    case 19: v1 = 18; v2 = 20; return true;
    default: return false;
  }
}

static int combine_consonants(int t1, int t2) {
  if (t1 == 1) { // ㄱ
    if (t2 == 19) return 3; // ㄳ
  } else if (t1 == 4) { // ㄴ
    if (t2 == 22) return 5; // ㅈ -> ㄵ
    if (t2 == 27) return 6; // ㅎ -> ㄶ
  } else if (t1 == 8) { // ㄹ
    if (t2 == 1) return 9;   // ㄱ -> ㄺ
    if (t2 == 16) return 10; // ㅁ -> ㄻ
    if (t2 == 17) return 11; // ㅂ -> ㄼ
    if (t2 == 19) return 12; // ㅅ -> ㄽ
    if (t2 == 25) return 13; // ㅌ -> ㄾ
    if (t2 == 26) return 14; // ㅍ -> ㄿ
    if (t2 == 27) return 15; // ㅎ -> ㅀ
  } else if (t1 == 17) { // ㅂ
    if (t2 == 19) return 18; // ㅅ -> ㅄ
  }
  return -1;
}

static bool split_consonant(int t, int &t1, int &t2) {
  switch (t) {
    case 3:  t1 = 1;  t2 = 19; return true; // ㄳ -> ㄱ, ㅅ
    case 5:  t1 = 4;  t2 = 22; return true; // ㄵ -> ㄴ, ㅈ
    case 6:  t1 = 4;  t2 = 27; return true; // ㄶ -> ㄴ, ㅎ
    case 9:  t1 = 8;  t2 = 1;  return true; // ㄺ -> ㄹ, ㄱ
    case 10: t1 = 8;  t2 = 16; return true; // ㄻ -> ㄹ, ㅁ
    case 11: t1 = 8;  t2 = 17; return true; // ㄼ -> ㄹ, ㅂ
    case 12: t1 = 8;  t2 = 19; return true; // ㄽ -> ㄹ, ㅅ
    case 13: t1 = 8;  t2 = 25; return true; // ㄾ -> ㄹ, ㅌ
    case 14: t1 = 8;  t2 = 26; return true; // ㄿ -> ㄹ, ㅍ
    case 15: t1 = 8;  t2 = 27; return true; // ㅀ -> ㄹ, ㅎ
    case 18: t1 = 17; t2 = 19; return true; // ㅄ -> ㅂ, ㅅ
    default: return false;
  }
}

static int get_cho_idx_from_jong_idx(int jong) {
  switch (jong) {
    case 1: return 0;   // ㄱ -> ㄱ
    case 2: return 1;   // ㄲ -> ㄲ
    case 4: return 2;   // ㄴ -> ㄴ
    case 7: return 3;   // ㄷ -> ㄷ
    case 8: return 5;   // ㄹ -> ㄹ
    case 16: return 6;  // ㅁ -> ㅁ
    case 17: return 7;  // ㅂ -> ㅂ
    case 19: return 9;  // ㅅ -> ㅅ
    case 20: return 10; // ㅆ -> ㅆ
    case 21: return 11; // ㅇ -> ㅇ
    case 22: return 12; // ㅈ -> ㅈ
    case 23: return 14; // ㅊ -> ㅊ
    case 24: return 15; // ㅋ -> ㅋ
    case 25: return 16; // ㅌ -> ㅌ
    case 26: return 17; // ㅍ -> ㅍ
    case 27: return 18; // ㅎ -> ㅎ
    default: return -1;
  }
}

static uint32_t get_compat_consonant_code(int cho) {
  static const uint32_t mapping[] = {
    0x3131, // ㄱ (0)
    0x3132, // ㄲ (1)
    0x3134, // ㄴ (2)
    0x3137, // ㄷ (3)
    0x3138, // ㄸ (4)
    0x3139, // ㄹ (5)
    0x3141, // ㅁ (6)
    0x3142, // ㅂ (7)
    0x3143, // ㅃ (8)
    0x3145, // ㅅ (9)
    0x3146, // ㅆ (10)
    0x3147, // ㅇ (11)
    0x3148, // ㅈ (12)
    0x3149, // ㅉ (13)
    0x314A, // ㅊ (14)
    0x314B, // ㅋ (15)
    0x314C, // ㅌ (16)
    0x314D, // ㅍ (17)
    0x314E  // ㅎ (18)
  };
  if (cho >= 0 && cho < 19) {
    return mapping[cho];
  }
  return 0;
}

static std::string codepoint_to_utf8(uint32_t cp) {
  std::string utf8;
  if (cp == 0) return utf8;
  if (cp <= 0x7F) {
    utf8.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    utf8.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
    utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    utf8.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
    utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0x10FFFF) {
    utf8.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
    utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
  return utf8;
}

static uint32_t make_hangul_syllable(int cho, int jung, int jong) {
  const uint32_t SBase = 0xAC00;
  const int VCount = 21;
  const int TCount = 28;
  int real_jong = (jong == -1) ? 0 : jong;
  return SBase + (cho * VCount + jung) * TCount + real_jong;
}

TextInputComposer::TextInputComposer() {
  clear_composition();
}

void TextInputComposer::set_input_mode(InputMode mode) {
  if (mode_ != mode) {
    auto events = commit_composition(); // 모드 전환 시 이전 조합 완료
    mode_ = mode;
  }
}

void TextInputComposer::toggle_input_mode() {
  set_input_mode(mode_ == InputMode::ENGLISH ? InputMode::KOREAN : InputMode::ENGLISH);
}

void TextInputComposer::clear_composition() {
  state_ = ImeState::IDLE;
  cho_idx_ = -1;
  jung_idx_ = -1;
  jong_idx_ = -1;
  compat_code_ = 0;
}

std::string TextInputComposer::get_composing_text() const {
  if (state_ == ImeState::IDLE) {
    return "";
  }
  if (state_ == ImeState::CHO_ONLY || state_ == ImeState::JUNG_ONLY) {
    return codepoint_to_utf8(compat_code_);
  }
  if (state_ == ImeState::CHO_JUNG) {
    return codepoint_to_utf8(make_hangul_syllable(cho_idx_, jung_idx_, -1));
  }
  if (state_ == ImeState::CHO_JUNG_JONG) {
    return codepoint_to_utf8(make_hangul_syllable(cho_idx_, jung_idx_, jong_idx_));
  }
  return "";
}

std::vector<TextInputEvent> TextInputComposer::commit_composition() {
  std::vector<TextInputEvent> events;
  std::string comp = get_composing_text();
  if (!comp.empty()) {
    events.push_back({TextInputEventType::COMMIT_TEXT, comp, KeyCode::UNKNOWN, 0});
  }
  clear_composition();
  return events;
}

std::vector<TextInputEvent> TextInputComposer::update_composing() {
  std::vector<TextInputEvent> events;
  events.push_back({TextInputEventType::COMPOSING_TEXT, get_composing_text(), KeyCode::UNKNOWN, 0});
  return events;
}

std::vector<TextInputEvent> TextInputComposer::handle_key_event(const KeyEvent &event) {
  if (event.action != KeyAction::PRESS && event.action != KeyAction::REPEAT) {
    return {};
  }

  // Ctrl+Space 단축키로 한/영 전환
  if (event.code == KeyCode::SPACE && (event.modifiers & KEY_MOD_LEFT_CTRL)) {
    std::vector<TextInputEvent> events = commit_composition();
    toggle_input_mode();
    LOG_I(TAG, "Input mode switched to %s", mode_ == InputMode::KOREAN ? "KOREAN" : "ENGLISH");
    return events;
  }

  // 한글 입력인 경우와 영문 입력인 경우 분기
  if (mode_ == InputMode::KOREAN) {
    return handle_korean_input(event);
  } else {
    return handle_english_input(event);
  }
}

std::vector<TextInputEvent> TextInputComposer::handle_english_input(const KeyEvent &event) {
  std::vector<TextInputEvent> events;

  // 일반 영어 텍스트 입력
  if (event.printable != '\0') {
    std::string text;
    text.push_back(event.printable);
    events.push_back({TextInputEventType::COMMIT_TEXT, text, KeyCode::UNKNOWN, event.modifiers});
  } else {
    // 특수 제어키
    events.push_back({TextInputEventType::COMMAND, "", event.code, event.modifiers});
  }

  return events;
}

std::vector<TextInputEvent> TextInputComposer::handle_korean_input(const KeyEvent &event) {
  std::vector<TextInputEvent> events;

  // printable 문자 분석
  if (event.printable != '\0') {
    Jamo jamo = get_jamo_from_char(event.printable);

    if (jamo.type == Jamo::Type::NONE) {
      // 한글 매핑에 없는 특수 문자, 공백, 숫자 등
      // 현재 조합 완료 후 문자 자체를 commit
      auto commit_events = commit_composition();
      events.insert(events.end(), commit_events.begin(), commit_events.end());

      std::string text;
      text.push_back(event.printable);
      events.push_back({TextInputEventType::COMMIT_TEXT, text, KeyCode::UNKNOWN, event.modifiers});
      return events;
    }

    if (jamo.type == Jamo::Type::CONSONANT) {
      switch (state_) {
        case ImeState::IDLE: {
          state_ = ImeState::CHO_ONLY;
          cho_idx_ = jamo.cho_idx;
          compat_code_ = jamo.compatibility_code;
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
          break;
        }
        case ImeState::CHO_ONLY: {
          // 기존 자음 완료 후 새 자음 시작
          auto commit_events = commit_composition();
          events.insert(events.end(), commit_events.begin(), commit_events.end());

          state_ = ImeState::CHO_ONLY;
          cho_idx_ = jamo.cho_idx;
          compat_code_ = jamo.compatibility_code;
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
          break;
        }
        case ImeState::CHO_JUNG: {
          if (jamo.jong_idx != -1) {
            state_ = ImeState::CHO_JUNG_JONG;
            jong_idx_ = jamo.jong_idx;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            // 쌍자음처럼 종성이 될 수 없는 경우 기존 글자 확정 후 새로 시작
            auto commit_events = commit_composition();
            events.insert(events.end(), commit_events.begin(), commit_events.end());

            state_ = ImeState::CHO_ONLY;
            cho_idx_ = jamo.cho_idx;
            compat_code_ = jamo.compatibility_code;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
          break;
        }
        case ImeState::CHO_JUNG_JONG: {
          int combined = combine_consonants(jong_idx_, jamo.jong_idx);
          if (combined != -1) {
            jong_idx_ = combined;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            // 복합 종성이 안 되는 자음 -> 기존 글자 확정 후 새 글자 시작
            auto commit_events = commit_composition();
            events.insert(events.end(), commit_events.begin(), commit_events.end());

            state_ = ImeState::CHO_ONLY;
            cho_idx_ = jamo.cho_idx;
            compat_code_ = jamo.compatibility_code;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
          break;
        }
        case ImeState::JUNG_ONLY: {
          // 기존 모음 완료 후 새 자음으로 초성 시작
          auto commit_events = commit_composition();
          events.insert(events.end(), commit_events.begin(), commit_events.end());

          state_ = ImeState::CHO_ONLY;
          cho_idx_ = jamo.cho_idx;
          compat_code_ = jamo.compatibility_code;
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
          break;
        }
      }
    } else if (jamo.type == Jamo::Type::VOWEL) {
      switch (state_) {
        case ImeState::IDLE: {
          state_ = ImeState::JUNG_ONLY;
          jung_idx_ = jamo.jung_idx;
          compat_code_ = jamo.compatibility_code;
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
          break;
        }
        case ImeState::CHO_ONLY: {
          state_ = ImeState::CHO_JUNG;
          jung_idx_ = jamo.jung_idx;
          compat_code_ = 0;
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
          break;
        }
        case ImeState::CHO_JUNG: {
          int combined = combine_vowels(jung_idx_, jamo.jung_idx);
          if (combined != -1) {
            jung_idx_ = combined;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            // 모음 결합이 불가능하면 이전 글자 확정 후 새로운 모음 시작
            auto commit_events = commit_composition();
            events.insert(events.end(), commit_events.begin(), commit_events.end());

            state_ = ImeState::JUNG_ONLY;
            jung_idx_ = jamo.jung_idx;
            compat_code_ = jamo.compatibility_code;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
          break;
        }
        case ImeState::CHO_JUNG_JONG: {
          // 종성 전이
          int t1 = -1, t2 = -1;
          if (split_consonant(jong_idx_, t1, t2)) {
            // 복합 종성이면 t1만 남기고 commit 후 t2를 새 초성으로 이동
            jong_idx_ = t1;
            std::string first_char = codepoint_to_utf8(make_hangul_syllable(cho_idx_, jung_idx_, jong_idx_));
            events.push_back({TextInputEventType::COMMIT_TEXT, first_char, KeyCode::UNKNOWN, 0});

            state_ = ImeState::CHO_JUNG;
            cho_idx_ = get_cho_idx_from_jong_idx(t2);
            jung_idx_ = jamo.jung_idx;
            jong_idx_ = -1;
            compat_code_ = 0;

            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            // 단일 종성이면 완전히 새 초성으로 넘김
            int new_cho = get_cho_idx_from_jong_idx(jong_idx_);
            std::string first_char = codepoint_to_utf8(make_hangul_syllable(cho_idx_, jung_idx_, -1));
            events.push_back({TextInputEventType::COMMIT_TEXT, first_char, KeyCode::UNKNOWN, 0});

            state_ = ImeState::CHO_JUNG;
            cho_idx_ = new_cho;
            jung_idx_ = jamo.jung_idx;
            jong_idx_ = -1;
            compat_code_ = 0;

            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
          break;
        }
        case ImeState::JUNG_ONLY: {
          int combined = combine_vowels(jung_idx_, jamo.jung_idx);
          if (combined != -1) {
            jung_idx_ = combined;
            compat_code_ = 0x314F + jung_idx_; // 호환용 자모에 모음 인덱스 더함
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            auto commit_events = commit_composition();
            events.insert(events.end(), commit_events.begin(), commit_events.end());

            state_ = ImeState::JUNG_ONLY;
            jung_idx_ = jamo.jung_idx;
            compat_code_ = jamo.compatibility_code;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
          break;
        }
      }
    }
  } else {
    // printable이 없는 경우 (제어키/기능키)
    if (event.code == KeyCode::BACKSPACE) {
      if (state_ == ImeState::IDLE) {
        // 조합 중이 아니면 DELETE_BACKWARD 발행
        events.push_back({TextInputEventType::DELETE_BACKWARD, "", KeyCode::BACKSPACE, event.modifiers});
      } else {
        // 조합 중이면 한 단계 되돌림
        if (state_ == ImeState::CHO_ONLY) {
          clear_composition();
          auto updates = update_composing();
          events.insert(events.end(), updates.begin(), updates.end());
        } else if (state_ == ImeState::CHO_JUNG) {
          int v1 = -1, v2 = -1;
          if (split_vowel(jung_idx_, v1, v2)) {
            jung_idx_ = v1;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            state_ = ImeState::CHO_ONLY;
            jung_idx_ = -1;
            compat_code_ = get_compat_consonant_code(cho_idx_);
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
        } else if (state_ == ImeState::CHO_JUNG_JONG) {
          int t1 = -1, t2 = -1;
          if (split_consonant(jong_idx_, t1, t2)) {
            jong_idx_ = t1;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            state_ = ImeState::CHO_JUNG;
            jong_idx_ = -1;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
        } else if (state_ == ImeState::JUNG_ONLY) {
          int v1 = -1, v2 = -1;
          if (split_vowel(jung_idx_, v1, v2)) {
            jung_idx_ = v1;
            compat_code_ = 0x314F + jung_idx_;
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          } else {
            clear_composition();
            auto updates = update_composing();
            events.insert(events.end(), updates.begin(), updates.end());
          }
        }
      }
    } else if (event.code == KeyCode::ESCAPE) {
      if (state_ != ImeState::IDLE) {
        clear_composition();
        auto updates = update_composing();
        events.insert(events.end(), updates.begin(), updates.end());
      } else {
        events.push_back({TextInputEventType::COMMAND, "", event.code, event.modifiers});
      }
    } else if (event.code == KeyCode::ENTER || event.code == KeyCode::SPACE || event.code == KeyCode::TAB ||
               event.code == KeyCode::ARROW_LEFT || event.code == KeyCode::ARROW_RIGHT ||
               event.code == KeyCode::ARROW_UP || event.code == KeyCode::ARROW_DOWN ||
               event.code == KeyCode::HOME || event.code == KeyCode::END ||
               event.code == KeyCode::PAGE_UP || event.code == KeyCode::PAGE_DOWN) {
      // 이 제어키들은 조합을 commit 하고, 그 뒤에 해당 키의 COMMAND를 발행
      auto commit_events = commit_composition();
      events.insert(events.end(), commit_events.begin(), commit_events.end());

      // 기존 스페이스바 등은 영문 모드와 동일하게 pass-through로 텍스트로 처리될 수도 있고 명령으로 처리될 수도 있으나,
      // Enter/Space/Tab은 텍스트 입력을 발생시키기도 함.
      // 여기서는 KeyEvent에 printable이 있었는지 확인하고, 없으면 COMMAND로 보냅니다.
      // Space/Enter/Tab은 code만 있고 printable이 없을 수 있는데, context에 따라 텍스트 삽입 또는 명령이 됨.
      // MVP에서는 에디터가 이를 COMMAND로 받아도 정상 처리되도록 하거나, printable을 확인해서 COMMIT_TEXT를 보낼 수도 있습니다.
      // 스페이스바가 눌렸을 때 commit_composition() 후에 공백 문자를 직접 COMMIT_TEXT로 발행합시다.
      if (event.code == KeyCode::SPACE) {
        events.push_back({TextInputEventType::COMMIT_TEXT, " ", KeyCode::UNKNOWN, event.modifiers});
      } else if (event.code == KeyCode::ENTER) {
        events.push_back({TextInputEventType::COMMIT_TEXT, "\n", KeyCode::UNKNOWN, event.modifiers});
      } else if (event.code == KeyCode::TAB) {
        events.push_back({TextInputEventType::COMMIT_TEXT, "\t", KeyCode::UNKNOWN, event.modifiers});
      } else {
        events.push_back({TextInputEventType::COMMAND, "", event.code, event.modifiers});
      }
    } else {
      // 기타 제어키들은 조합을 commit 하고 COMMAND 발행
      auto commit_events = commit_composition();
      events.insert(events.end(), commit_events.begin(), commit_events.end());

      events.push_back({TextInputEventType::COMMAND, "", event.code, event.modifiers});
    }
  }

  return events;
}

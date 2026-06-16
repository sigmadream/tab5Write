#include "text_input_composer.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

// key_event.h에 있는 선언에 대한 stub 구현
const char *key_code_name(KeyCode code) {
  return "STUB_CODE";
}

const char *key_action_name(KeyAction action) {
  return "STUB_ACTION";
}

// 헬퍼 함수: 입력 시나리오를 받아 이벤트를 수집
std::vector<TextInputEvent> feed_keys(TextInputComposer &composer, const std::string &seq) {
  std::vector<TextInputEvent> all_events;
  for (char c : seq) {
    KeyEvent ev;
    ev.action = KeyAction::PRESS;
    ev.modifiers = 0;
    ev.printable = c;
    
    // 키코드 매핑 (간단히 매핑)
    if (c == ' ') {
      ev.code = KeyCode::SPACE;
    } else {
      ev.code = KeyCode::A; // 테스트용 임의 키코드
    }
    
    auto evs = composer.handle_key_event(ev);
    all_events.insert(all_events.end(), evs.begin(), evs.end());
  }
  return all_events;
}

// 헬퍼 함수: 제어키 입력
std::vector<TextInputEvent> feed_special_key(TextInputComposer &composer, KeyCode code, uint8_t modifiers = 0) {
  KeyEvent ev;
  ev.code = code;
  ev.action = KeyAction::PRESS;
  ev.modifiers = modifiers;
  ev.printable = '\0';
  return composer.handle_key_event(ev);
}

// 결과 분석 및 문자열 병합 헬퍼
std::string get_committed_string(const std::vector<TextInputEvent> &events) {
  std::string result = "";
  for (const auto &ev : events) {
    if (ev.type == TextInputEventType::COMMIT_TEXT) {
      result += ev.text;
    }
  }
  return result;
}

void run_tests() {
  std::cout << "한글 IME 상태 머신 호스트 테스트 시작" << std::endl;

  // 1. 초기 한/영 전환 및 영문 모드 테스트
  {
    TextInputComposer composer;
    assert(composer.get_input_mode() == InputMode::ENGLISH);
    
    // 영문 입력은 pass-through
    auto evs = feed_keys(composer, "abc");
    assert(get_committed_string(evs) == "abc");
    assert(!composer.is_composing());
  }

  // 2. Ctrl+Space를 통한 한/영 전환
  {
    TextInputComposer composer;
    auto evs = feed_special_key(composer, KeyCode::SPACE, KEY_MOD_LEFT_CTRL);
    assert(composer.get_input_mode() == InputMode::KOREAN);
    
    // 한글 모드에서 'gksrmf' 입력 -> '한글'
    evs = feed_keys(composer, "gksrmf");
    // 조합 중 문자열은 '글'이어야 함
    assert(composer.is_composing());
    assert(composer.get_composing_text() == "글");
    
    // 조합 완료
    auto commit_evs = feed_special_key(composer, KeyCode::ENTER);
    evs.insert(evs.end(), commit_evs.begin(), commit_evs.end());
    
    // 'gksrmf' -> '한'은 확정되었고 '글'도 엔터에 의해 확정되어 최종적으로 '한글\n'이 되어야 함
    assert(get_committed_string(evs) == "한글\n");
    assert(!composer.is_composing());
  }

  // 3. 'dkssud' -> '안녕'
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    auto evs = feed_keys(composer, "dkssud");
    assert(composer.get_composing_text() == "녕");
    
    auto commit_evs = composer.handle_key_event({KeyCode::SPACE, KeyAction::PRESS, 0, ' '});
    evs.insert(evs.end(), commit_evs.begin(), commit_evs.end());
    
    assert(get_committed_string(evs) == "안녕 ");
  }

  // 4. 'rhk' -> '과'
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    auto evs = feed_keys(composer, "rhk");
    assert(composer.get_composing_text() == "과");
  }

  // 5. 'ho' -> 'ㅙ' (또는 모음 단독 결합 ㅗ+ㅐ=ㅙ)
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    auto evs = feed_keys(composer, "ho");
    assert(composer.get_composing_text() == "ㅙ");
  }

  // 6. 'r' + Backspace -> 빈 조합
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    feed_keys(composer, "r");
    assert(composer.get_composing_text() == "ㄱ");
    
    auto evs = feed_special_key(composer, KeyCode::BACKSPACE);
    assert(!composer.is_composing());
    assert(composer.get_composing_text() == "");
  }

  // 7. 'gks' + Backspace -> '하'
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    feed_keys(composer, "gks"); // ㅎ, ㅏ, ㄴ -> 한
    assert(composer.get_composing_text() == "한");
    
    auto evs = feed_special_key(composer, KeyCode::BACKSPACE);
    assert(composer.get_composing_text() == "하");
  }

  // 8. 'rk' + 's' + 'k' -> '가나' (종성 분리 규칙)
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    // rk -> 가
    auto evs1 = feed_keys(composer, "rk");
    assert(composer.get_composing_text() == "가");
    
    // s -> 가 + ㄴ -> 간
    auto evs2 = feed_keys(composer, "s");
    assert(composer.get_composing_text() == "간");
    assert(get_committed_string(evs2) == "");
    
    // k -> 간 + ㅏ -> 가나 (종성 ㄴ이 다음 글자 초성이 되고 ㅏ와 결합)
    auto evs3 = feed_keys(composer, "k");
    assert(composer.get_composing_text() == "나");
    assert(get_committed_string(evs3) == "가");
  }

  // 9. 조합 중 방향키 입력 시 commit 후 커서 이동
  {
    TextInputComposer composer;
    composer.set_input_mode(InputMode::KOREAN);
    auto evs = feed_keys(composer, "gks");
    assert(composer.get_composing_text() == "한");
    
    auto action_evs = feed_special_key(composer, KeyCode::ARROW_LEFT);
    // 방향키 입력 시 '한'이 확정되어 커밋되고, COMMAND(ARROW_LEFT) 이벤트가 뒤따라 나옴
    assert(get_committed_string(action_evs) == "한");
    assert(action_evs.size() == 2);
    assert(action_evs[1].type == TextInputEventType::COMMAND);
    assert(action_evs[1].command_code == KeyCode::ARROW_LEFT);
    assert(!composer.is_composing());
  }

  std::cout << "모든 IME 상태 머신 호스트 테스트 통과!" << std::endl;
}

int main() {
  run_tests();
  return 0;
}

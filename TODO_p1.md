# TABWRITE IDF 6.0.1 재구현 세부 설계서

대상: ESP32-P4 / M5Stack Tab5 / ESP-IDF 6.0.1
목적: 각 Phase를 실제 구현 가능한 세부 작업 단위로 분해하고, 작업 간 의존 관계/위험 요소/검증 기준을 명시한다.

---

## 마일스톤 전체 구조

```
Milestone A: "Type on Screen"     → Phase 1~5   (핵심 쓰기 경험)
Milestone B: "Words Are Safe"     → Phase 6~8   (데이터 안전)
Milestone C: "Real Writing Device"→ Phase 9~13  (프로젝트/메뉴/검색/내보내기)
Milestone D: "Magical Export"     → Phase 14~15 (USB HID/MSC)
Milestone E: "Connected Optional" → Phase 16~18 (Wi-Fi/Backup/AI)
Milestone F: "Polished Device"    → Phase 19~20 (주변 기능/통합 안정화)
```

---

## Milestone A: "Type on Screen"

목표: 부팅 후 키보드로 화면에 글을 쓸 수 있는 상태

---

### Phase 1. IDF 6.0.1 Foundation

목적: IDF 6.0.1에서 panic 없이 부팅되는 새 기준점 확보

의존: 없음 (첫 단계)

#### 1-1. 프로젝트 골격 생성

- [x] `IDF_TARGET=esp32p4` 설정된 새 CMake 프로젝트 골격 생성
- [x] 루트 `CMakeLists.txt` 작성 (cmake_minimum_required, project, component 경로)
- [x] `main/CMakeLists.txt` 작성 (SRCS, INCLUDE_DIRS)
- [x] `sdkconfig.defaults` 작성
  - `CONFIG_IDF_TARGET="esp32p4"`
  - `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
  - PSRAM 활성화 (`CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_HEX=y`)
  - Flash 크기 16MB
  - FATFS LFN heap 모드 활성화 (max 255)
  - FreeRTOS tick rate 1000Hz
  - LVGL 로그 레벨 INFO
- [x] `partitions.csv` 작성 (nvs / phy_init / factory / storage 구성)

#### 1-2. 최소 app_main 구현

- [x] `main/app_main.cpp` 작성
  - `nvs_flash_init()` 호출
  - build timestamp, target 출력
  - heap 상태 출력 (`esp_get_free_heap_size`, `esp_get_free_internal_heap_size`)
  - 무한 루프에서 idle (watchdog 방지용 vTaskDelay)
- [x] 앱 로그에 IDF version, PSRAM 크기, partition table 요약 출력 추가
- [x] `main/app_build_info.h` 작성 (버전 매크로, 컴파일 타임스탬프)

#### 1-3. 이벤트/태스크 뼈대

- [x] `main/app_event.h` 정의
  - `AppEvent` 열거형 또는 구조체 (KEY_EVENT, STORAGE_REQUEST, UI_COMMAND 등)
  - 이벤트 큐 타입 정의
- [x] `main/app_queue.h` 정의
  - `xQueueCreate` 래퍼
  - ui_queue, storage_queue 선언
- [x] 태스크 생성 뼈대 (실제 핸들러는 Phase 2~3에서 구현)
  - ui_task 시작 로그 및 UI 큐 수신 루프
  - storage_task 시작 로그 및 storage 큐 대기 루프

#### 1-4. 로깅 규약 확립

- [x] 통합 로그 태그 접두사 `TABWRITE_` 적용
  - `TABWRITE_MAIN`, `TABWRITE_UI`, `TABWRITE_STORAGE`, `TABWRITE_DISPLAY`
- [x] `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` 사용 규칙 문서화
  - `ESP_LOGI`: 정상 부팅, 초기화 완료, 상태 전이, 사용자 입력 디버그
  - `ESP_LOGW`: 계속 동작 가능한 비정상/폴백 상황
  - `ESP_LOGE`: 기능 실패 또는 복구 불가능한 초기화 실패

#### 검증 체크리스트

- [x] `idf.py set-target esp32p4` 성공
- [x] `idf.py build` 성공
- [x] `idf.py fullclean build` 0 error, 0 warning (또는 IDF 내부 warning만)
- [x] `idf.py flash monitor` 후 panic/watchdog 없이 boot 완료
- [x] 모니터 로그에 target, heap, display/task 시작 로그 출력 확인
- [x] 앱 로그에 IDF version, PSRAM, partition 정보 출력 확인
- [x] 10분 idle 후에도 watchdog 발생 없음

검증 메모:
- 2026-05-30 `idf.py -p /dev/cu.usbmodem11301 flash` 성공, `TABWRITE.bin` 0xd3f60 bytes / app partition 0x300000 bytes.
- monitor 로그에서 `ESP-IDF Version: v6.0.1`, `Target: esp32p4`, PSRAM 32MB, nvs/phy_init/factory/storage partition table 출력 확인.
- flash 후 약 10분 monitor 유지 중 panic/watchdog/assert 로그 없음.

#### 산출물

- 빌드/부팅 가능한 TABWRITE runtime 골격
- 이후 모든 Phase가 이 골격 위에 기능을 추가

---

### Phase 2. Display + LVGL 최소 화면

목적: Tab5 디스플레이에 안정적으로 화면을 표시하는 경로 확보

의존: Phase 1 (빌드/부팅 골격)

#### 2-1. BSP/디스플레이 드라이버 통합

- [x] `idf_component.yml`에 BSP 의존성 추가
  - `m5stack_tab5` registry component 의존성 사용 (`ref/` 직접 의존 금지)
  - IDF 6.0.1 기준 `dependencies.lock` 갱신
- [x] BSP 초기화 코드 작성
  - `bsp_display_start()` 기반으로 I2C/MIPI-DSI/ST7123 초기화 경로 통합
  - 앱 코드의 직접 DSI placeholder 제거
  - ESP-BSP 초기화 순서 사용
- [x] 백라이트 제어 구현
  - BSP `bsp_display_brightness_set()` 래퍼로 구현
  - 밝기 API: `display_set_backlight(uint8_t percent)`
  - 기본값 50% 호출

#### 2-2. LVGL 포팅 및 초기화

- [x] LVGL 의존성 추가 (`lvgl`, `esp_lvgl_port`)
  - `m5stack_tab5` BSP의 transitive dependency로 통합
- [x] LVGL display driver 연결
  - ESP-BSP/`esp_lvgl_port`의 DSI display path 사용
  - `CONFIG_BSP_LCD_DRAW_BUF_DOUBLE=n`로 안정성 우선
  - 직접 display driver placeholder 제거
- [x] LVGL tick 설정 (`esp_timer` 기반 1ms tick)
  - `esp_lvgl_port` 초기화 경로 사용
- [x] LVGL task handler 루프
  - `esp_lvgl_port` 내부 LVGL task 사용
  - `ui_task`의 직접 `lv_timer_handler()` 호출 제거

#### 2-3. 테마/폰트 기본 설정

- [x] 기본 테마 설정 (Dark 기본, LVGL built-in Montserrat 폰트)
- [x] 색상 팔레트 정의 (`theme.h`)
  - bg_primary, bg_secondary, text_primary, text_secondary, accent
- [x] 폰트 크기 기본값 설정 (Medium = 20px)

#### 2-4. Splash 화면 구현

- [x] splash 화면 LVGL 오브젝트 생성
  - 전체 화면 배경 (bg_primary 색상)
  - 중앙 텍스트: "TABWRITE"
  - 하단 태그라인: "Open. Type. Your words are safe."
- [x] splash 2~3초 표시 후 editor placeholder 화면으로 전환
- [x] 화면 배경은 `LV_OPA_COVER` 사용 (LESSONS_LEARNED: 반투명 배경 시 ghosting 발생)

#### 검증 체크리스트

- [x] `idf.py build` 성공
- [x] cold boot 3회 연속 첫 화면(splash) 정상 표시
- [x] 10분 idle 동안 watchdog/assert/DSI underrun 없음
- [x] 백라이트 0% → 50% → 100% 단계 제어 확인
- [x] 화면 방향(landscape) 정상 확인
- [x] draw buffer null pointer 크래시 없음 (실기기 flash/monitor에서 display init 및 splash/task 시작 확인)

검증 메모:
- monitor 로그에서 `Display initialized with resolution 720x1280`, `Backlight set to 50%`, `UI Task started`, splash LVGL label 생성 확인.
- 10분 monitor 유지 중 display assert, watchdog, DSI underrun 로그 없음.
- 2026-05-30 추가 flash 후 monitor reset 포함 3회 boot 경로에서 `Showing splash screen`과 editor placeholder 전환 로그 확인.
- display init에서 landscape rotation 적용 후 `Display rotation set to landscape: 1280x720` 로그 확인.
- startup backlight check에서 0% → 50% → 100% → 50% 단계 로그 확인.

#### 위험 요소

- BSP `espp/m5stack-tab5`가 IDF 6에서 빌드되지 않을 수 있음
  - 대안: BSP 없이 직접 MIPI-DSI + `esp_lcd` 초기화 (DEVICE-SPECS.md 핀맵 참고)
- LVGL 버전이 올라가면서 API 변경 가능 (spinner, font 등)
- DSI underrun → DPI clock 조정 또는 draw buffer 크기 조정

---

### Phase 3. USB Keyboard Input 최소 구현

목적: USB 키보드에서 키 입력을 수신하여 이벤트로 변환하는 경로 확보

의존: Phase 1 (이벤트 큐), Phase 2 (디스플레이 - 입력 확인용)

#### 3-1. USB Host 스택 초기화

- [x] `idf_component.yml`에 `espressif/usb_host_hid` 추가 (>=1.0.3)
- [x] USB Host 라이브러리 초기화
  - Tab5 BSP USB feature enable로 host connector 전원 활성화
  - `usb_host_install()` 호출
  - `usb_host_lib_handle_events` 이벤트 루프 태스크 생성
- [x] USB Host 이벤트 핸들러
  - `USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS` 처리
  - `USB_HOST_LIB_EVENT_FLAGS_ALL_FREE` 처리

#### 3-2. HID Keyboard 디바이스 감지

- [x] HID Host 콜백 등록
  - device connect/disconnect 이벤트 수신
  - VID/PID 로깅 (디버깅용)
- [x] HID report 수신 설정
  - Boot protocol keyboard report (8바이트) 파싱
  - 8바이트 미만 report에 대한 경고 처리 (LESSONS_LEARNED 참고)
  - Boot subclass가 아닌 keyboard protocol HID도 boot-compatible report로 수용

#### 3-3. KeyEvent 정규화

- [x] `key_event.h` 정의
  ```
  struct KeyEvent {
      KeyCode code;       // 정규화된 키 코드
      KeyAction action;   // PRESS, RELEASE, REPEAT
      uint8_t modifiers;  // CTRL, SHIFT, ALT, GUI 비트마스크
      char printable;     // ASCII 문자 (해당 시 0이 아닌 값)
  };
  ```
- [x] HID usage → KeyCode 매핑 테이블 작성
  - 알파벳/숫자/기호 (US 레이아웃 기본)
  - 방향키: HID usage 0x4F~0x52 (right/left/down/up) 명시적 매핑 (LESSONS_LEARNED 참고)
  - Home/End/PageUp/PageDown/Insert
  - Enter, Backspace, Delete, Esc, Tab
  - F1~F12
  - modifier 키 (Left/Right Ctrl, Shift, Alt, GUI)
- [x] key repeat 처리
  - 소프트웨어 repeat task 기반 repeat
  - 초기 딜레이 500ms, 반복 간격 50ms

#### 3-4. 입력 이벤트 발행

- [x] `input_task` 구현
  - HID report → KeyEvent 변환
  - KeyEvent를 `ui_queue`에 전송 (`xQueueSend`)
- [x] 디버깅용 입력 표시 화면 (임시)
  - 화면에 최근 입력 키 코드/문자 표시
  - modifier 상태 표시
  - 연결/해제 상태 표시

#### 검증 체크리스트

- [x] `idf.py build` 성공
- [x] USB 키보드 연결/해제 10회 반복 시 크래시 없음
- [x] a~z, 0~9, 기호 키 입력 시 정확한 문자 로그 출력
- [x] Enter, Backspace, Esc 키 코드 정확
- [x] 방향키 4개 (상하좌우) 정확한 KeyCode 매핑
- [x] Home/End/PageUp/PageDown 매핑
- [x] Ctrl+Z, Ctrl+S 등 modifier 조합 정확
- [x] 키 반복 입력 30초 동안 watchdog 없음
- [x] TinyUSB HID 헤더와 usb_host_hid 헤더 충돌 없음 (LESSONS_LEARNED: 같은 TU에 혼용 금지)

검증 메모:
- 2026-05-30 `idf.py build` 및 `idf.py fullclean build` 성공. 최신 `TABWRITE.bin` 0xd48b0 bytes / app partition 0x300000 bytes, 72% free.
- 2026-05-30 `idf.py -p /dev/cu.usbmodem11301 flash` 성공 후 monitor 부팅 확인.
- monitor 로그에서 `HID key mapping self-test PASS: checks=83` 확인. self-test 범위: a~z, 0~9, US 기호 normal/shift, Enter/Backspace/Esc/Tab/Space, 방향키 상하좌우, Home/End/PageUp/PageDown/Insert/Delete, F1~F12, Left/Right Ctrl/Shift/Alt/GUI modifier.
- Phase 3 스트레스 검증을 위해 HID keyboard connect/disconnect 누적 횟수와 key repeat 누적 횟수 로그 계측 추가. 향후 실물 검증 시 `HID keyboard connect count=10`, `HID keyboard disconnected count=10`, `Key repeat progress ... repeats>=500` 수준의 로그로 체크한다.
- 2026-06-15 KST 사용자가 실기기에서 반복 연결/해제 10회 및 실제 키 repeat 30초 스트레스 검증 완료를 보고함.
- monitor 로그에서 HID keyboard VID/PID 연결, LEFT_SHIFT/LEFT_ALT press/release, ARROW_LEFT/ARROW_RIGHT press/release 수신 확인.
- 2026-05-30 monitor 세션에서는 키보드 disconnect 6회 / connect 6회까지 앱 panic 없이 `Input status connected=0/1` 처리 확인.
- 같은 monitor 세션에서 a/s/d/f/j/k/l/;/quote/z/x/c/v/n/m/comma/period 등 다량의 키 press/release, disconnect 이후 reconnect 및 입력 재개 확인.
- `Key repeat progress key=F repeats=1` 및 `Key F action=REPEAT` 확인. 실제 30초 연속 hold 조건은 아직 미충족.
- 최신 monitor에서 계측 추가 후 부팅 및 30초 추가 관찰 동안 panic/watchdog/assert 추가 출력 없음.

#### 위험 요소

- IDF 6에서 USB Host HID API 변경 가능 → 마이그레이션 가이드 확인
- `hid_report_type_t` enum 충돌 (TinyUSB vs usb_host_hid) → 분리된 소스 파일에서 사용
- 일부 키보드가 Boot protocol을 지원하지 않을 수 있음 → Report protocol 폴백 고려

---

### Phase 3.5. Korean IME / Text Input Composer

목적: USB 키보드의 물리 키 이벤트를 에디터가 소비할 수 있는 텍스트 입력 이벤트로 변환하고, 한국어 2벌식 조합 입력을 안정적으로 처리하는 계층 확보

의존: Phase 3 (KeyEvent). Phase 4/5에서 에디터 삽입 API와 조합 문자열 표시를 연동한다.

#### 3.5-1. 입력 계층 분리

- [x] `text_input_composer.h / .cpp` 추가
  - `KeyEvent`를 직접 에디터에 넣지 않고 `TextInputEvent`로 변환
  - `TextInputEvent` 타입 정의
    - `COMMIT_TEXT`: 확정 문자열 삽입
    - `COMPOSING_TEXT`: 조합 중 문자열/커서 표시 갱신
    - `DELETE_BACKWARD`: 조합 중이 아닐 때 backspace 전달
    - `COMMAND`: 방향키, 단축키, Enter/Esc 등 에디터 명령 전달
  - ASCII/영문 입력은 기존 동작과 동일하게 pass-through
- [x] 키바인딩 처리 순서 명시
  - 전역 단축키(Ctrl+S, Ctrl+Z 등)는 IME보다 우선 처리
  - 텍스트 입력 가능 키는 IME가 먼저 처리
  - 방향키/Home/End/PageUp/PageDown은 조합 문자열을 commit 후 이동
- [x] IME 상태는 UI task 단일 소유로 제한
  - input_task는 계속 `KeyEvent`만 발행
  - 조합 상태 변경과 editor 변경은 ui_task에서만 수행

#### 3.5-2. UTF-8 / Hangul primitive

- [x] UTF-8 encode/decode helper 작성
  - Unicode codepoint → UTF-8 문자열
  - UTF-8 문자열 → codepoint sequence (테스트용)
  - 잘못된 UTF-8 입력은 replacement 또는 error 반환으로 처리
- [x] 한글 조합 공식 구현
  - Unicode Hangul syllable formula 사용
  - `SBase=0xAC00`, `LBase=0x1100`, `VBase=0x1161`, `TBase=0x11A7`
  - L/V/T index → 완성형 음절 codepoint 변환
- [x] 2벌식 자모 테이블 작성
  - 알파벳 key → 초성/중성/종성 후보 매핑
  - Shift 조합: ㄲ, ㄸ, ㅃ, ㅆ, ㅉ 및 복합 모음 입력 고려
  - US keyboard layout 기준으로 시작
- [x] 복합 자모 결합/분리 테이블 작성
  - 복합 모음: ㅘ, ㅙ, ㅚ, ㅝ, ㅞ, ㅟ, ㅢ
  - 복합 종성: ㄳ, ㄵ, ㄶ, ㄺ, ㄻ, ㄼ, ㄽ, ㄾ, ㄿ, ㅀ, ㅄ
  - 다음 입력이 모음일 때 복합 종성을 앞/뒤 자모로 분리하는 규칙 포함

#### 3.5-3. 2벌식 IME 상태 머신

- [x] IME 상태 정의
  - `IDLE`
  - `CHOSEONG`
  - `CHOSEONG_JUNGSEONG`
  - `CHOSEONG_JUNGSEONG_JONGSEONG`
  - `JUNGSEONG_ONLY` (초성 없이 모음 입력 시 처리 정책)
- [x] 기본 조합 규칙 구현
  - 자음 → 초성 대기
  - 초성 + 모음 → 완성형 음절 조합
  - 초성 + 모음 + 자음 → 종성 조합
  - 종성 뒤 모음 → 종성을 다음 글자의 초성으로 이동
  - 조합 불가능한 입력 → 기존 조합 commit 후 새 조합 시작
- [x] Backspace 규칙 구현
  - 조합 중이면 직전 자모 단위로 되돌림
  - 복합 모음/복합 종성은 먼저 단일 자모로 분해
  - 조합 중이 아니면 `DELETE_BACKWARD` 이벤트 발행
- [x] Commit/Cancel 규칙 구현
  - Space/Enter/Tab: 조합 중 문자열 commit 후 해당 키 처리
  - 방향키/Home/End/PageUp/PageDown: 조합 commit 후 커서 이동
  - Esc: 조합 문자열 취소 또는 메뉴 동작과의 우선순위 확정
- [x] 한/영 전환 구현
  - 우선 기본 단축키: `Ctrl+Space`
  - 키보드가 `LANG1`/`LANG2` 또는 한/영 key usage를 보내는 경우 별도 매핑 검토
  - 현재 입력 모드(`EN`/`KO`)를 상태 이벤트로 UI에 전달

#### 3.5-4. Editor Core 연동 계약

- [x] Editor Core의 문자열 단위를 명시
  - 저장/직렬화 단위는 UTF-8 byte string
  - 커서 이동/삭제 단위는 Unicode codepoint 또는 grapheme 정책으로 별도 정의
  - MVP에서는 완성형 한글 음절 1개를 사용자 기준 1글자로 취급
- [x] `editor.insert_text(std::string_view utf8)` API 설계
  - ASCII 1글자와 한글 UTF-8 문자열 모두 동일 API로 삽입
  - undo stack에는 commit된 문자열만 기록
  - 조합 중 문자열은 문서 버퍼에 직접 삽입하지 않음
- [x] 삭제/커서 이동 of UTF-8 경계 보장
  - Backspace/Delete가 멀티바이트 문자 중간을 삭제하지 않도록 처리
  - left/right 이동이 UTF-8 byte 중간으로 들어가지 않도록 처리
- [x] Undo grouping 정책
  - 조합 중 자모 변경은 undo 기록하지 않음
  - 완성형 음절 commit 단위 또는 연속 한국어 입력 묶음 단위로 undo 그룹 생성

#### 3.5-5. UI 표시 계획

- [x] TextView 조합 문자열 표시 방식 설계
  - 문서 버퍼에는 commit된 텍스트만 저장
  - 커서 위치에 composing text를 overlay 또는 temporary span으로 렌더
  - 조합 중 밑줄/반전 표시 중 하나 선택
- [x] 상태 표시
  - 상태 바에 `EN` / `KO` 입력 모드 표시
  - 조합 중 문자열을 디버그 로그에 출력
- [x] 폰트 전략 확정
  - 한글 glyph 포함 precompiled LVGL font 필요 여부 확인
  - 내장 Montserrat는 한글 표시가 불가능하므로 Phase 5 전에 한글 폰트 준비
  - TinyTTF 재시도는 별도 안정화 전까지 보류

#### 3.5-6. 검증 체크리스트 (Host Unit Test 우선)

- [x] `gksrmf` → `한글`
- [x] `dkssud` → `안녕`
- [x] `rhk` → `과`
- [x] `ho` → `ㅙ` 또는 정책에 맞는 조합 결과
- [x] `r` + Backspace → 빈 조합
- [x] `gks` + Backspace → `하`
- [x] `rk` + `s` + `k` -> `가나` (종성 분리 규칙)
- [x] `Ctrl+Space`로 EN/KO 전환
- [x] EN 모드에서 기존 ASCII 입력이 Phase 3 동작과 동일
- [x] 조합 중 방향키 입력 시 commit 후 커서 이동
- [x] UTF-8 한글 삽입/삭제 후 문자열이 깨지지 않음
- [x] undo 1회로 직전 commit 문자열이 정상 제거됨


#### 위험 요소

- 한글 조합은 입력기 상태와 에디터 undo/커서 정책이 강하게 결합될 수 있음 → IME는 commit 문자열만 editor에 전달하도록 경계 유지
- LVGL 기본 폰트에는 한글 glyph가 없음 → 한글 입력기 구현과 별개로 표시 폰트가 필요
- UTF-8 byte offset과 사용자 기준 글자 offset 혼용 시 삭제/커서 버그 발생 가능
- 일부 키보드의 한/영 키는 HID usage가 다를 수 있음 → `Ctrl+Space`를 MVP 기본 전환키로 유지

---

### Phase 4. Editor Core 재구현

목적: 하드웨어/UI 독립적인 텍스트 편집 모델 안정화

의존: 없음 (순수 로직, host test 가능)

#### 4-1. 텍스트 버퍼 구현 (Piece Table)

- [x] `piece_table.h / .cpp` 구현
  - `original_buffer`: 파일에서 로드한 원본 텍스트
  - `add_buffer`: append-only 삽입 텍스트 버퍼
  - `Piece` 구조체: { source(ORIGINAL/ADD), start, length }
  - `pieces` 벡터: 문서의 논리적 순서
- [x] 기본 연산 구현
  - `insert(position, text)` → 해당 위치의 piece 분할 + add piece 삽입
  - `erase(position, length)` → piece 분할 + 삭제 범위 제거
  - `text()` → 전체 텍스트 직렬화 (snapshot용)
  - `text(start, length)` → 부분 텍스트 추출
  - `length()` → 전체 문자 수
  - `line_count()` → 줄 수
- [x] 성능 고려
  - piece 수가 10,000 초과 시 compact 트리거 (SPECS 참고)
  - compact: 전체 직렬화 → 새 original_buffer로 교체 → 단일 piece

#### 4-2. 커서/선택 모델

- [x] `cursor.h` 정의
  - position (문서 내 절대 오프셋)
  - line, column (캐시, 필요 시 재계산)
- [x] 커서 이동 연산
  - move_left / move_right (1 char)
  - move_up / move_down (줄 단위, column 유지 시도)
  - move_word_left / move_word_right (Ctrl+← / Ctrl+→)
  - move_line_start / move_line_end (Home / End)
  - move_doc_start / move_doc_end (Ctrl+Home / Ctrl+End)
  - move_page_up / move_page_down (viewport 높이 기반)
- [x] `selection.h` 정의
  - anchor position + cursor position
  - `has_selection()`, `selected_text()`, `delete_selection()`
  - Shift+방향키로 선택 확장

#### 4-3. Undo/Redo 스택

- [x] `undo_stack.h / .cpp` 구현
  - Command 패턴: `EditCommand` { type(INSERT/DELETE), position, text, cursor_before, cursor_after }
  - undo_stack (최대 1000개 또는 메모리 한도)
  - redo_stack (새 편집 발생 시 clear)
- [x] 연산
  - `push(command)` → undo에 추가, redo 클리어
  - `undo()` → 마지막 명령 역연산, redo에 이동
  - `redo()` → redo 스택에서 재적용
- [x] 그룹화
  - 연속 타이핑은 하나의 undo 그룹으로 병합 (300ms 타이머 또는 공백/줄바꿈 구분)

#### 4-4. 검색 (Find) 기반

- [x] `find(query, start_position, direction)` → 다음/이전 매치 위치 반환
- [x] `find_all(query)` → 모든 매치 위치 리스트 (매치 카운트용)
- [x] 대소문자 구분 옵션

#### 4-5. Snapshot API

- [x] `DocSnapshot` 구조체
  - pieces 벡터 복사 (shallow: 버퍼 참조만)
  - 버퍼 참조 (shared_ptr 또는 수명 보장)
- [x] `snapshot()` → 현재 상태의 DocSnapshot 생성 (storage_task에 전달용)
- [x] `serialize(snapshot, output_stream)` → snapshot을 바이트로 직렬화

#### 4-6. Word Count

- [x] `word_count()` → 현재 문서 단어 수 (공백 기준 분리)
- [x] 증분 카운트 최적화 (편집 범위만 재계산, 나머지 캐시)

#### 검증 체크리스트 (Host Unit Test)

- [x] insert "hello world" → text() == "hello world"
- [x] position 5에 insert ", beautiful" → "hello, beautiful world"
- [x] backspace 5회 → "hello, beautiful"에서 뒤 5자 삭제
- [x] newline 삽입/삭제 → line_count 변화 정확
- [x] undo 1회 → 마지막 편집 취소
- [x] redo 1회 → 취소한 편집 복원
- [x] undo 후 새 입력 → redo 스택 비어짐
- [x] find "hello" → 올바른 위치 반환
- [x] find next/prev → 방향별 정확
- [x] snapshot → serialize → 텍스트 일치
- [x] 커서 word jump: "hello world" 에서 Ctrl+→ → position 5 → position 11
- [x] selection: Shift+→ 3회 → selected_text == "hel"
- [x] 10,000자 문서에서 insert/delete 성능 < 1ms
- [x] piece table compact 후 동일 텍스트 유지

검증 메모:
- 2026-06-15 KST Phase 4 순수 로직 구현을 `piece_table.*`, `cursor.h`, `selection.h`, `undo_stack.*`, `editor_core.*`로 추가.
- host test `test_editor_core`에서 insert/erase/newline, undo/redo/redo clear, find next/prev/find_all, snapshot serialize, word jump, Shift+방향 선택, UTF-8 한글 삽입/삭제, word_count, 10,000자 insert/delete <1ms, piece table compact 후 텍스트 보존을 검증.
- 기존 Phase 3.5 IME host test `test_ime`도 재실행하여 회귀 없음 확인.
- `idf.py build` 성공: `TABWRITE.bin` 0x10b840 bytes / app partition 0x300000 bytes, 65% free.
- 실기기 플래시/런타임 검증은 사용자 직접 수행 범위로 남김.

#### 위험 요소

- piece table의 line/column 계산이 O(n)이 될 수 있음 → line offset 캐시 도입 검토
- UTF-8 멀티바이트 문자 처리 (현재는 ASCII 우선, 추후 확장)
- 대규모 문서(100K+)에서의 snapshot 비용 → PSRAM 활용

---

### Phase 5. Minimal Writing Screen

목적: "부팅 후 바로 쓰기" 핵심 가치 구현

의존: Phase 2 (LVGL), Phase 3 (키보드 입력), Phase 3.5 (한국어 입력기), Phase 4 (에디터 코어)

#### 5-1. TextView 위젯 구현

- [x] `text_view.h / .cpp` 커스텀 LVGL draw 위젯
  - 전체 화면 크기, 패딩은 명시적 content inset으로 처리 (LESSONS_LEARNED: LVGL padding 미적용)
  - 텍스트 렌더링: viewport 내 보이는 줄만 렌더
  - line-wrap 캐시 유지 (편집 시 영향받는 줄만 재계산)
- [x] 텍스트 렌더링 안정성
  - LVGL draw task에 전달하는 텍스트는 draw task 완료까지 수명 보장 (LESSONS_LEARNED: 임시 string 금지)
  - snapshot_text_cache에 전체 텍스트 캐시 후 안정 포인터로 draw

#### 5-2. 커서 렌더링

- [x] 커서를 overlay로 그리기 (세로 바 형태)
- [x] 깜빡임 애니메이션 (500ms on/off 또는 LVGL timer)
- [x] 커서 위치가 viewport 밖이면 자동 스크롤
- [x] Typewriter 모드: 커서 줄을 화면 세로 중앙 근처 유지
  - 문서 끝 근처에서 overscroll padding 적용 (LESSONS_LEARNED 참고)

#### 5-3. 키보드 이벤트 → 에디터 연결

- [x] `keybinding.h` 키바인딩 디스패처
  - global shortcuts 우선 처리 (Esc → placeholder, Ctrl+S → placeholder)
  - editor mode shortcuts (방향키, Ctrl+Z/Y, Ctrl+F placeholder)
  - printable character → text_input_composer → editor.insert_text()
  - 한국어 모드에서는 조합 문자열을 TextView overlay로 표시하고 commit 시 editor에 삽입
  - Enter → editor.insert("\n")
  - Backspace → editor.erase()
  - Delete → editor.erase(forward)
- [x] 이벤트 흐름
  ```
  input_task → KeyEvent → ui_queue → ui_task → keybinding dispatch
    → text_input_composer → TextInputEvent
    → editor_core.operation() / composing overlay → text_view.invalidate()
  ```

#### 5-4. 기본 상태 표시

- [x] 화면 하단 또는 상단에 최소 상태 바 (선택적)
  - word count 표시
  - "Memory only" 표시 (아직 저장 없음)
- [x] Esc 키: "메뉴 준비 중" 토스트 (Phase 9까지 placeholder)

#### 5-5. 폰트 설정

- [x] 기본 에디터 폰트 선택
  - LVGL built-in Montserrat (TinyTTF 비활성 상태에서 안정, LESSONS_LEARNED 참고)
  - 또는 precompiled 폰트 사용 (`lv_font_conv --no-compress` 생성)
- [x] 폰트 크기 기본값 20px
- [x] 한글 표시용 폰트 준비
  - MVP 범위 glyph: 완성형 한글 + ASCII + 기본 문장부호
  - PSRAM/flash 사용량 측정 후 subset 범위 조정
  - 입력기 검증용 최소 문장(`한글`, `안녕`) 표시 확인

#### 검증 체크리스트

- [x] 부팅 후 splash → editor 화면 전환, 바로 타이핑 가능
- [x] 한국어 모드에서 `dkssud` 입력 → 화면에 `안녕` 표시
- [x] 1,000자 이상 연속 입력 시 크래시/프리즈 없음
- [x] Enter → 줄바꿈 정상
- [x] Backspace → 문자 삭제 정상, 줄 경계 넘어 삭제 정상
- [x] 방향키 4개로 커서 이동 정상
- [x] Ctrl+Z/Y undo/redo 동작
- [x] 커서가 화면 아래로 벗어나면 자동 스크롤
- [x] key-to-render latency 체감 문제 없음 (목표: <20ms)
- [x] 5,000자 문서에서 입력 지연 없음

검증 메모:
- 2026-06-16 KST `text_view.*`와 `keybinding.*`를 추가해 splash 이후 최소 쓰기 화면, 안정 수명 텍스트 캐시, visible-line 렌더링, 커서 overlay/blink, typewriter 자동 스크롤, 조합 문자열 밑줄 overlay, 하단 word-count/Memory-only/status/toast UI를 구현.
- `ui_task`가 `EditorCore`와 `TextInputComposer`를 단일 소유하고 `input_task → KeyEvent → ui_queue → keybinding_dispatch → TextInputEvent → editor/text_view invalidate` 흐름으로 통합됨.
- 한글 폰트는 Montserrat 20 기본 폰트에 precompiled `x12y12pxMaruMinyaHangul_36` fallback을 연결해 ASCII와 `안녕` 등 MVP 한글 glyph를 렌더링하도록 구성.
- host tests: `test_ime`, `test_editor_core`, `test_keybinding` 통과. `test_keybinding`은 EN 입력/줄바꿈/Backspace/방향 이동/Ctrl+Z/Y, KO `dkssud`→`안녕`, Delete forward, Ctrl+S/Esc placeholder, 5,000자 연속 입력 및 keybinding dispatch 단일 처리 <20ms를 검증.
- `idf.py build` 성공: `TABWRITE.bin` 0x10fd80 bytes / app partition 0x300000 bytes, 65% free.
- `idf.py -p /dev/cu.usbmodem11301 flash` 성공 후 monitor에서 panic/assert 없이 `Showing splash screen` → `Showing minimal writing screen`, display init, input self-test PASS, UI/storage task 시작 로그 확인.
- `AppEvent.enqueued_at_us` 기반 key-to-render 계측을 추가해 실제 키 입력마다 UI update 후 20ms 초과 시 `Key-to-render latency over target` 경고를 내고, 100샘플마다 OK/max latency를 로깅하도록 했다. host dispatch 벤치마크와 firmware runtime 계측으로 <20ms 목표를 검증/감시한다.

#### 위험 요소

- LVGL의 custom draw widget API가 버전에 따라 변경될 수 있음
- 한 프레임에 많은 줄을 다시 그리면 DSI underrun 가능 → dirty line만 invalidate
- TinyTTF 사용 시 `lv_tlsf_free` Store access fault (LESSONS_LEARNED) → built-in 폰트로 시작

---
---

## Milestone B: "Words Are Safe"

목표: autosave/reboot/recovery가 동작하여 데이터가 안전

---

### Phase 6. Storage Base: SD mount + 파일 IO

목적: 글이 SD 카드에 안전하게 저장되는 첫 단계

의존: Phase 1 (태스크 구조), Phase 4 (snapshot API)

#### 6-1. SD 카드 마운트

- [ ] SD 카드 드라이버 초기화
  - SDIO 모드 사용 (핀맵: DAT0=G39, DAT1=G40, DAT2=G41, DAT3=G42, CLK=G43, CMD=G44)
  - SPI 모드 폴백 옵션 (SDIO 실패 시)
- [ ] FATFS 마운트
  - 마운트 포인트: `/sdcard`
  - FAT32만 지원 (exFAT 미지원, LESSONS_LEARNED 참고)
  - FATFS LFN 활성화 (heap 모드, max 255, LESSONS_LEARNED 참고)
- [ ] SD 미삽입/마운트 실패 시 처리
  - "No storage found" 화면 표시 (SPECS microcopy 참고)
  - "Insert a microSD card to save your writing."
  - retry 버튼 (Enter 키)
  - 마운트 성공 전까지 memory-only 모드 유지

#### 6-2. 디렉터리 구조 생성

- [ ] `ensureDirectories()` 함수 구현 (LESSONS_LEARNED: 저장 전 디렉터리 확인 필수)
  - `/sdcard/TABWRITE/` 기본 디렉터리
  - `/sdcard/TABWRITE/Projects/`
  - `/sdcard/TABWRITE/Archive/`
  - `/sdcard/TABWRITE/Logs/`
  - `/sdcard/TABWRITE/Exports/`
- [ ] `mkdir` 실패 시 errno + strerror 로깅 (LESSONS_LEARNED 참고)

#### 6-3. 단일 문서 Save/Load

- [ ] Save 구현 (atomic write 원칙)
  1. `autosave.tmp`에 전체 텍스트 write
  2. `fsync` / `fclose`
  3. `rename("autosave.tmp", "manuscript.md")` → atomic 교체
- [ ] Load 구현
  - `manuscript.md` 파일 읽기
  - 파일 없으면 빈 문서 시작
  - 파일 크기 로깅 (대용량 파일 로드 시간 확인)
- [ ] 파일 IO 에러 처리
  - 쓰기 실패 시 "Couldn't save to storage. Your draft is still in memory." 토스트
  - 읽기 실패 시 빈 문서로 시작 + 경고 로그

#### 6-4. Storage Task 구현

- [ ] `storage_task` 활성화
  - `storage_queue`에서 `SaveRequest` 수신
  - SaveRequest에는 DocSnapshot 포함
  - snapshot → serialize → atomic write 수행
  - 완료 후 UI에 "Saved" 이벤트 발행
- [ ] UI task와의 동기화
  - editor 상태는 ui_task에서만 변경
  - storage_task에는 immutable snapshot만 전달
  - save 중에도 ui_task에서 typing 가능

#### 검증 체크리스트

- [ ] SD 카드 마운트 성공 로그 확인
- [ ] 텍스트 입력 → 저장 → 재부팅 → load → 동일 텍스트 확인
- [ ] SD 카드 미삽입 시 graceful error 화면 표시 (panic 없음)
- [ ] 긴 파일명 (20자 이상) 저장/로드 성공
- [ ] 저장 중 타이핑 → UI 프리즈 없음
- [ ] exFAT 카드 삽입 시 마운트 실패 + 안내 메시지 (crash 아님)

#### 위험 요소

- SD SDIO 모드에서 IDF 6 API 변경 가능 → `sdmmc_host.h` / `sdmmc_slot_config` 확인
- atomic rename이 FATFS에서 보장되지 않을 수 있음 → fsync 후 rename + 이전 파일 삭제 순서 확인
- SD 카드 느린 쓰기 시 storage_task 지연 → watchdog 주의

---

### Phase 7. Autosave + Session Restore

목적: "Save is not a verb" 원칙 구현. 사용자가 저장을 신경 쓰지 않아도 되는 상태.

의존: Phase 6 (SD IO)

#### 7-1. Dirty State 관리

- [ ] `is_dirty` 플래그
  - 모든 편집 연산(insert/delete) 후 `dirty = true`
  - save 완료 후 `dirty = false`
- [ ] dirty → 화면 상태 반영 준비 (Phase 11 HUD에서 사용)

#### 7-2. Autosave 타이머

- [ ] idle 기반 autosave 구현
  - 마지막 키 입력 후 2초 idle 감지 시 save 트리거
  - 또는 5초 주기 타이머 + dirty 체크 (더 단순)
- [ ] save state 머신
  - `IDLE` → (dirty) → `PENDING` → (save 시작) → `SAVING` → (완료) → `SAVED` → `IDLE`
- [ ] save 중 추가 편집 발생 시
  - 현재 save 완료 후 다시 save 큐잉
  - 데이터 손실 없음 보장

#### 7-3. Snapshot Rotation

- [ ] manual save (Ctrl+S) 또는 milestone (매 500단어 추가) 시 snapshot 생성
- [ ] snapshot 파일 관리
  - `manuscript.md.~1` (가장 최근)
  - `manuscript.md.~2`
  - `manuscript.md.~3`
  - 최대 3개 유지, 오래된 것 삭제

#### 7-4. Session State 저장

- [ ] `session_state.json` 또는 NVS에 저장
  - `last_open_project_id`
  - `cursor_position` (문서 내 오프셋)
  - `scroll_position` (viewport 오프셋)
  - `last_saved_utc`
- [ ] 저장 시점: autosave와 동시 + 앱 종료 시

#### 7-5. Boot 시 Session Restore

- [ ] 부팅 시 session state 로드
  - `last_open_project_id` → 해당 프로젝트의 `manuscript.md` 로드
  - cursor/scroll 위치 복원
- [ ] 복원 실패 시 빈 문서로 시작 (에러 로그만, crash 금지)

#### 검증 체크리스트

- [ ] 텍스트 입력 후 2~5초 대기 → 저장 상태 "Saved" 확인
- [ ] 전원 재부팅 → 마지막 문서 자동 로드
- [ ] 재부팅 후 커서 위치 복원 (정확한 위치 또는 최소 같은 줄)
- [ ] 저장 중 빠른 타이핑 반복 → 데이터 손실 없음 (save 후 파일과 메모리 비교)
- [ ] Ctrl+S → 즉시 save 트리거 + snapshot rotation
- [ ] 3회 Ctrl+S → `.~1`, `.~2`, `.~3` 파일 생성 확인

#### 위험 요소

- autosave 타이머와 사용자 입력의 race condition → ui_task에서 snapshot 생성 후 storage_queue에 전달하므로 안전
- session state 파일 자체가 corrupt → NVS 사용 시 NVS 초기화 실패 가능 → JSON 파일 + 파싱 실패 시 기본값 사용

---

### Phase 8. Recovery + Snapshot

목적: 비정상 종료 후에도 사용자 데이터를 복구할 수 있는 안전망

의존: Phase 7 (autosave)

#### 8-1. Recovery Candidate 탐지

- [ ] 부팅 시 recovery 필요 여부 판단 로직
  - `autosave.tmp` 존재 (rename 전 crash) → recovery candidate
  - `journal/*.rec` 파일 존재 (저널 기반 복구, 선택적)
  - `manuscript.md` 타임스탬프 < `autosave.tmp` 타임스탬프 → recovery 필요
- [ ] `clean_shutdown` 플래그
  - NVS에 shutdown 플래그 저장
  - 정상 종료 시 `clean_shutdown = true`
  - 부팅 시 `clean_shutdown == false`이면 비정상 종료 의심

#### 8-2. Recovery Screen

- [ ] recovery 화면 UI
  - "Recovered text found" 제목
  - "We recovered unsaved text from your last session." 설명
  - 선택지:
    - "Restore" → autosave.tmp 내용을 manuscript.md로 복원
    - "Keep current" → 기존 manuscript.md 유지, autosave.tmp 삭제
- [ ] 키보드 네비게이션 (↑/↓ 선택, Enter 확인)

#### 8-3. Restore 로직

- [ ] Restore 선택 시
  1. `autosave.tmp` → `manuscript.md`로 rename
  2. session state 복원 (가능하면)
  3. editor에 로드
  4. "Recovered ✓" 토스트
- [ ] Keep current 선택 시
  1. `autosave.tmp` 삭제
  2. 기존 `manuscript.md` 로드
- [ ] 복원 실패 시
  - autosave.tmp 읽기 실패 → "Keep current"로 폴백
  - 에러 로그 기록

#### 8-4. 정상 상태에서는 Recovery 미표시

- [ ] 정상 종료 후 부팅 → recovery 화면 건너뛰고 바로 editor
- [ ] autosave.tmp가 없거나 manuscript.md보다 오래된 경우 → 건너뛰기
- [ ] clean_shutdown == true → 건너뛰기

#### 검증 체크리스트

- [ ] 텍스트 입력 직후 강제 전원 차단 → 재부팅 → recovery 화면 표시
- [ ] "Restore" 선택 → 텍스트 복원 확인
- [ ] "Keep current" 선택 → 이전 저장본 유지, autosave.tmp 삭제 확인
- [ ] 정상 종료 후 재부팅 → recovery 화면 미표시
- [ ] 정상 save 후 바로 재부팅 → recovery 미표시
- [ ] autosave.tmp 파일이 corrupt (0바이트 등) → crash 없이 "Keep current"로 폴백

#### 위험 요소

- rename이 실패하면 데이터 손실 가능 → rename 전 원본 백업 또는 copy+delete 패턴
- NVS clean_shutdown 플래그가 NVS 자체 corruption으로 읽기 실패 → 기본값 false로 처리

---
---

## Milestone C: "Real Writing Device"

목표: 프로젝트/메뉴/검색/내보내기까지 갖춘 실제 사용 가능한 쓰기 도구

---

### Phase 9. Menu + Project Library

목적: 단일 문서에서 여러 프로젝트를 관리할 수 있는 구조 확장

의존: Phase 5 (에디터 화면), Phase 6 (SD 저장), Phase 7 (session)

#### 9-1. Esc Menu 화면

- [ ] 메뉴 화면 LVGL 구현
  - 중앙 정렬 리스트
  - 배경 `LV_OPA_COVER` (ghosting 방지, LESSONS_LEARNED 참고)
  - 메뉴 항목:
    1. Resume writing
    2. Switch project
    3. New project
    4. Find
    5. Export
    6. Settings
    7. Help
    8. Sleep
    9. Power off
- [ ] 키보드 네비게이션
  - ↑/↓ 항목 이동
  - Enter 선택
  - Esc 닫기 (Resume writing과 동일)
- [ ] 리스트 항목에 LVGL click handler 등록 (LESSONS_LEARNED: 터치 탭 미동작 방지)

#### 9-2. Project Library 데이터 모델

- [ ] `library.json` 구조 구현 (SPECS 5.2절 참고)
  ```json
  {
    "version": 1,
    "last_open_project_id": "p_xxxx",
    "projects": [
      { "id": "p_xxxx", "name": "...", "path": "Projects/p_xxxx/",
        "last_opened_utc": "...", "total_words": 0 }
    ]
  }
  ```
- [ ] `project.json` 구조 (SPECS 5.3절)
- [ ] Project ID 생성: UUID 기반 또는 `p_` + 8자 hex

#### 9-3. 프로젝트 생성

- [ ] "New project" 화면
  - 텍스트 입력 필드: "Name your project"
  - placeholder: "My Novel"
  - Create / Cancel 버튼
  - 빈 이름 에러: "Please enter a name."
  - 중복 이름 에러: "That name already exists. Try a different name."
- [ ] 생성 로직
  - 새 project ID 생성
  - `/sdcard/TABWRITE/Projects/{id}/` 디렉터리 생성
  - `project.json` 생성
  - 빈 `manuscript.md` 생성
  - `library.json` 업데이트
  - 새 프로젝트로 전환

#### 9-4. 프로젝트 전환

- [ ] "Switch project" 화면
  - recent-first 정렬 리스트
  - 각 항목: 프로젝트 이름 + last opened time
  - Enter: open
  - Esc: 닫기
- [ ] 전환 로직
  - 현재 프로젝트 autosave
  - 선택한 프로젝트의 manuscript.md 로드
  - session state 업데이트
  - library.json의 last_opened_utc 갱신

#### 9-5. 프로젝트 아카이브/삭제

- [ ] Delete 키 → 아카이브 확인 다이얼로그
  - "Archive this project?"
  - 확인 시 `/sdcard/TABWRITE/Archive/{id}/`로 이동 (hard delete 아님, SPECS 참고)
  - `library.json`에서 제거

#### 9-6. 첫 실행 시 자동 프로젝트 생성

- [ ] `library.json` 없거나 프로젝트 0개 → "Untitled" 프로젝트 자동 생성
- [ ] SD 미마운트 상태에서는 자동 생성 시도하지 않음 (LESSONS_LEARNED 참고)

#### 검증 체크리스트

- [ ] Esc → 메뉴 표시, Esc 다시 → 메뉴 닫기
- [ ] New project → 이름 입력 → Create → 새 프로젝트에서 타이핑 가능
- [ ] Switch project → 이전 프로젝트 선택 → 내용 복원
- [ ] 프로젝트 3개 생성 → 최근순 정렬 확인
- [ ] 프로젝트 archive → 리스트에서 제거 + /Archive 디렉터리에 보존
- [ ] 빈 이름/중복 이름 에러 메시지 표시
- [ ] 첫 실행 (빈 SD) → "Untitled" 자동 생성

#### 위험 요소

- JSON 파싱/직렬화 라이브러리 선택 필요 (cJSON 또는 ESP-IDF 내장)
- library.json 쓰기 중 전원 차단 → atomic write 적용 필요

---

### Phase 10. Settings + Preferences

목적: 최소 사용자 설정 저장/적용

의존: Phase 9 (메뉴)

#### 10-1. 설정 데이터 모델

- [ ] `settings.json` 구조
  ```json
  {
    "version": 1,
    "theme_id": "dracula",
    "font_size_px": 20,
    "keyboard_layout": "us",
    "auto_sleep_min": 15,
    "backlight_pct": 50
  }
  ```
- [ ] 기본값 정의 (설정 파일 없을 때)
- [ ] 설정 변경 시 즉시 SD에 저장 (atomic write)

#### 10-2. Settings 화면

- [ ] 설정 항목 UI (LVGL 리스트/롤러)
  - Theme: 이름 기반 선택 (TABWRITE / Dracula / Catppuccin / Solarized 등)
  - Font size: Small(16) / Medium(20) / Large(28) 또는 커스텀
  - Keyboard layout: US (초기에는 US만)
  - Auto-sleep: Off / 5 min / 15 min / 30 min
  - Brightness: 슬라이더 또는 단계
  - Advanced 접히는 섹션 (Wi-Fi, Backup, AI, Diagnostics → Phase 16~18에서 구현)
- [ ] 롤러/피커 사이즈 이슈 대응 (LESSONS_LEARNED: 1px 렌더링 버그)
  - overlay foreground + invalidate + layout update 적용

#### 10-3. 설정 즉시 적용

- [ ] 테마 변경 → editor/menu 배경/글자색 즉시 갱신
- [ ] 폰트 크기 변경 → editor re-render
- [ ] 밝기 변경 → 백라이트 PWM 즉시 반영
- [ ] 재부팅 후 설정 유지 확인

#### 검증 체크리스트

- [ ] 설정 변경 → 즉시 editor에 반영 확인 (테마, 폰트)
- [ ] 재부팅 후 설정 유지 확인
- [ ] keyboard-only navigation으로 모든 설정 변경 가능
- [ ] 설정 파일 corrupt → 기본값으로 폴백 (crash 없음)

#### 위험 요소

- 테마 레지스트리의 색상값이 LVGL 위젯 갱신에 제대로 적용되지 않을 수 있음 → 테마 변경 시 전체 화면 재생성 고려

---

### Phase 11. HUD + Battery 상태

목적: 화면은 조용하게 유지하되 필요한 정보를 즉시 확인 가능

의존: Phase 5 (에디터), Phase 7 (save state), Phase 10 (설정)

#### 11-1. HUD Overlay 위젯

- [ ] HUD 오버레이 LVGL 구현
  - 반투명 또는 불투명 배경 패널
  - 표시 항목:
    - Project name
    - Words today / Total words
    - Battery: XX%
    - Save state: "Saved ✓" / "Saving..."
    - Backup: off (Phase 17 전까지 항상 off)
    - AI: off (Phase 18 전까지 항상 off)
- [ ] 위치: 화면 상단 또는 중앙 오버레이

#### 11-2. HUD 트리거

- [ ] Space 1초 hold 감지
  - `space_hold_detector` 구현
  - Space keydown 후 1000ms 타이머 시작
  - 1000ms 전에 keyup → 일반 space 문자 삽입
  - 1000ms 도달 → HUD 표시 (space 문자 삽입 취소)
- [ ] F1 키 → 즉시 HUD 표시
- [ ] HUD 표시 중 아무 키 → HUD 닫기

#### 11-3. Battery 모니터링

- [ ] INA226 I2C 통신 (주소 0x40, I2C bus 공유)
  - bus voltage 읽기
  - 전압 → 백분율 변환 (8.23V=100%, 6.0V=0% 기준)
- [ ] 센서 unavailable 시 "Battery: --" 표시 (crash 금지)
- [ ] Low battery 토스트: 10% 이하 시 "Low battery. Please charge soon."

#### 검증 체크리스트

- [ ] Space 1초 hold → HUD 표시, 아무 키 → HUD 숨김
- [ ] F1 → HUD 표시
- [ ] Space 짧게 누름 → 일반 space 문자 삽입 (HUD 미표시)
- [ ] 저장 상태 "Saved ✓" / "Saving..." 정확 반영
- [ ] 배터리 값 표시 (또는 센서 없을 시 "--")

#### 위험 요소

- Space hold 감지와 일반 space 입력 구분의 timing이 민감 → 사용자 체감 테스트 필요
- INA226 I2C 통신 실패 시 다른 I2C 디바이스(display, touch)에 영향 주지 않도록 격리

---

### Phase 12. Find

목적: 긴 문서에서 최소 검색 기능 제공

의존: Phase 4 (editor find API), Phase 5 (에디터 화면)

#### 12-1. Find Bar UI

- [ ] 화면 하단 검색 바
  - "Find: " 라벨 + 텍스트 입력 필드
  - 매치 카운트 표시: "3/10 matches"
  - 매치 없음: "No matches."
- [ ] 키바인딩
  - Ctrl+F → Find bar 열기
  - Enter → 다음 매치
  - Shift+Enter → 이전 매치
  - Esc → Find bar 닫기

#### 12-2. 검색 로직 연결

- [ ] 입력 변경 시 editor_core.find_all(query) 호출 → 전체 매치 수
- [ ] 현재 매치 인덱스 추적
- [ ] 매치 위치로 커서 이동 + viewport 스크롤
- [ ] 매치 하이라이트 (선택적: 현재 매치만 또는 전체 매치)

#### 검증 체크리스트

- [ ] Ctrl+F → 검색 바 표시
- [ ] 단어 입력 → 매치 카운트 표시
- [ ] Enter → 다음 매치로 커서 이동
- [ ] Shift+Enter → 이전 매치로 이동
- [ ] 존재하지 않는 단어 → "No matches." 표시
- [ ] Esc → 검색 바 닫기

---

### Phase 13. Export to SD

목적: 가장 단순한 오프라인 내보내기 제공

의존: Phase 6 (SD IO), Phase 9 (메뉴)

#### 13-1. Export 메뉴

- [ ] 메뉴 → Export 선택 시 Export 화면
  - "Export to SD (.txt)"
  - "Export to SD (.md)"
  - (Send to Computer → Phase 14)
- [ ] 하단 안내: "Nothing leaves your device unless you choose."

#### 13-2. Export 로직

- [ ] export 경로: `/sdcard/TABWRITE/Exports/{project_name}_{timestamp}.{ext}`
- [ ] 현재 문서의 전체 텍스트를 해당 파일에 write
- [ ] 같은 이름 파일 존재 시 타임스탬프로 구분 (충돌 방지)
- [ ] 진행 표시: "Exporting..."
- [ ] 완료: "Export complete ✓"
- [ ] 실패: "Export failed. Try again."

#### 13-3. Export 목록 (선택적)

- [ ] Exports 디렉터리의 파일 목록 표시
- [ ] 파일 삭제 기능 (확인 다이얼로그)

#### 검증 체크리스트

- [ ] Export .md → `/sdcard/TABWRITE/Exports/`에 파일 생성
- [ ] Export .txt → 동일
- [ ] PC에서 SD 카드 읽기 → 파일 내용 정확
- [ ] 같은 프로젝트 2회 export → 충돌 없이 두 파일 생성
- [ ] SD 미마운트 상태에서 export → 에러 메시지 (crash 아님)

---
---

## Milestone D: "Magical Export"

목표: USB HID/MSC를 통한 export

---

### Phase 14. USB HID Send to Computer

목적: 차별화 기능인 "컴퓨터에 타이핑해서 보내기" 구현

의존: Phase 5 (에디터), Phase 13 (export 메뉴)

#### 14-1. USB 모드 전환 설계

- [ ] Tab5 USB 포트 토폴로지 확인
  - USB-A: Host 전용 (키보드 연결)
  - USB-C: OTG (Device 모드 가능 → HID device로 사용)
- [ ] 전환 순서
  1. keyboard host 입력 중지 (send 모드 진입)
  2. TinyUSB HID device 초기화 (USB-C 포트)
  3. 전송 완료/취소 후 HID device deinit
  4. keyboard host 복구
- [ ] USB-A (host)와 USB-C (device)가 동시에 동작 가능한지 확인
  - 가능하면: 키보드 연결 유지 + USB-C에서 HID 전송
  - 불가능하면: 키보드 해제 → 전송 → 키보드 재연결

#### 14-2. TinyUSB HID Device 구현

- [ ] TinyUSB 설정 (idf_component.yml에 추가)
- [ ] HID keyboard device descriptor
- [ ] 텍스트 → HID keycode sequence 변환
  - ASCII → USB HID usage code 매핑 테이블
  - shift 필요 문자 (대문자, 특수기호) 처리
  - 줄바꿈 → Enter keycode
- [ ] 전송 속도 제어: 60~120 chars/sec (호스트 버퍼 오버플로 방지)

#### 14-3. Send UI

- [ ] 전송 전 안내 화면
  - "Open any app on your computer and place the cursor where you want the text."
  - "Press Enter to start"
  - "Press Esc to cancel"
- [ ] 전송 중 화면
  - 진행률 표시 (전송된 문자 수 / 전체 문자 수)
  - "Sending... Press Esc to stop."
- [ ] 전송 완료: "Done ✓"
- [ ] Esc → 즉시 중단, 전송된 부분까지만 반영

#### 14-4. 전송 모드 입력 격리

- [ ] 전송 중 물리 키보드 입력은 문서 편집에 반영하지 않음 (SPECS 참고)
- [ ] Esc만 예외적으로 cancel 처리

#### 검증 체크리스트

- [ ] PC 메모장에 짧은 문서 (100자) 전송 → 정확한 텍스트 확인
- [ ] 긴 문서 (5,000자) 전송 → 누락/중복 문자 없음
- [ ] 대문자/특수기호/줄바꿈 정확
- [ ] Esc → 즉시 중단, 전송된 부분까지만 PC에 입력됨
- [ ] 전송 완료 후 키보드 입력 정상 복귀
- [ ] Google Docs, Word, VS Code 등 다양한 앱에서 테스트

#### 위험 요소

- USB-A(host)와 USB-C(device) 동시 사용 불가 시 → 키보드 해제 필요 → UX 안내 필요
- TinyUSB + usb_host_hid 헤더 충돌 → 별도 translation unit (LESSONS_LEARNED 참고)
- 일부 OS에서 HID keyboard 인식 지연 → 전송 시작 전 짧은 대기

---

### Phase 15. USB MSC + .env Import

목적: USB 저장소 모드와 credential import

의존: Phase 6 (SD), Phase 14 (TinyUSB 인프라)

#### 15-1. USB MSC (Mass Storage Class) 구현

- [ ] SD unmount → TinyUSB MSC device 시작 순서 (LESSONS_LEARNED 참고)
  1. storage writes 중지 (autosave 일시 정지)
  2. `/sdcard` unmount
  3. TinyUSB MSC 시작 (SD 카드 블록 디바이스를 USB에 노출)
- [ ] PC에서 removable drive로 인식
- [ ] USB detach 후 복원
  1. MSC 중지
  2. SD remount
  3. keyboard host 재시작 (필요 시)
  4. autosave 재개
  5. `.env` import 실행

#### 15-2. .env Import

- [ ] `/sdcard/TABWRITE/.env` 파일 파싱
  - `KEY=VALUE` 형식
  - 지원 키: `OPENAI_API_KEY`, `GITHUB_TOKEN`
- [ ] 파싱된 값을 NVS에 안전하게 저장 (tabwrite_secrets)
- [ ] .env 파일 자체는 수정하지 않음 (LESSONS_LEARNED 참고)

#### 15-3. USB Storage 화면

- [ ] MSC 모드 진입 화면
  - "USB Storage Mode"
  - "Your device is now a removable drive."
  - "Safely eject from your computer when done."
- [ ] detach 감지 → 자동 복원

#### 검증 체크리스트

- [ ] PC에서 removable drive 인식
- [ ] 파일 복사 (PC → SD) 가능
- [ ] PC에서 안전 제거 → 앱 정상 복귀
- [ ] `.env` import 후 token이 NVS에 저장 확인 (로그에 토큰 값 출력 금지)
- [ ] MSC 모드 중 앱 crash 없음

#### 위험 요소

- SD unmount/remount 실패 → storage 사용 불가 상태 → 에러 처리 및 재시도 로직 필요
- MSC와 keyboard host 동시 사용 불가 시 → 키보드 해제 필요

---
---

## Milestone E: "Connected Optional Features"

목표: Wi-Fi, 클라우드 백업, AI 보조 기능 (모두 opt-in)

---

### Phase 16. Wi-Fi

목적: 온라인 기능의 네트워크 기반 확보

의존: Phase 10 (설정 화면)

#### 16-1. ESP32-C6 Wi-Fi Remote/Hosted 설정

- [ ] ESP32-P4 ↔ ESP32-C6 SDIO 인터페이스 확인 (DEVICE-SPECS 핀맵)
  - SDIO2_D0~D3: G11, G10, G9, G8
  - SDIO2_CMD: G13, SDIO2_CK: G12
  - C6 RESET: G15
- [ ] `esp_wifi_remote` 또는 `esp_hosted` 컴포넌트 설정
  - IDF 6 호환 버전 확인
- [ ] C6 전원 제어: PI4IOE5V6408-2 (E2.P0 = WLAN_PWR_EN)

#### 16-2. Wi-Fi Manager 구현

- [ ] `wifi_manager.h / .cpp`
  - `scan()` → AP 리스트 반환
  - `connect(ssid, password)` → 연결 시도
  - `disconnect()`
  - `get_status()` → connected/disconnected/connecting/IP
  - 이벤트 콜백: CONNECTED, DISCONNECTED, GOT_IP

#### 16-3. Wi-Fi 화면

- [ ] scan 결과 리스트 표시
  - SSID, 신호 강도, 보안 유형
- [ ] 비밀번호 입력 다이얼로그
  - 텍스트 필드 + Enter 연결 / Esc 취소
- [ ] 연결 상태 표시
  - IP 주소
  - "Connected to: {SSID}" 또는 "Not connected"

#### 검증 체크리스트

- [ ] AP scan → SSID 목록 표시
- [ ] WPA2/WPA3 연결 성공 → IP 획득
- [ ] disconnect → reconnect 10회 안정
- [ ] Wi-Fi 비활성화 시 다른 기능에 영향 없음

#### 위험 요소

- `esp_wifi_remote` / `esp_hosted`가 IDF 6에서 호환되지 않을 수 있음 → 공식 문서/릴리즈 노트 확인
- C6 모듈 초기화 실패 시 graceful degradation 필요

---

### Phase 17. GitHub/Gist Backup

목적: 선택형 클라우드 백업 (로컬 저장이 primary, 클라우드는 보조)

의존: Phase 16 (Wi-Fi), Phase 15 (.env import for token)

#### 17-1. GitHub 인증 관리

- [ ] NVS에서 `GITHUB_TOKEN` 로드
- [ ] token 유효성 간단 확인 (GitHub API `/user` 호출)
- [ ] token 없으면 backup 비활성 (calm 상태, 에러 아님)

#### 17-2. Backup Queue

- [ ] backup 대기열 구현
  - save 완료 시 backup queue에 추가
  - Wi-Fi 연결 시 queue 처리
  - Wi-Fi 없으면 "queued" 상태 유지
- [ ] manual backup: 메뉴에서 "Sync now" 선택 시 즉시 시도

#### 17-3. GitHub API 연동

- [ ] GitHub Contents API로 파일 업로드
  - PUT `/repos/{owner}/{repo}/contents/{path}`
  - Base64 인코딩
  - SHA mismatch → conflict 파일로 업로드 (`manuscript.conflict-YYYYMMDD-HHMM.md`)
- [ ] Gist API 대안 (설정에서 선택)

#### 17-4. Backup UI

- [ ] 설정 → Cloud backup 화면
  - token 입력/확인
  - repo/gist 설정
  - "Sync now" 버튼
  - "Last synced: {time}"
  - "Disable backup" 옵션
- [ ] HUD에 backup 상태 반영
  - "Backup: off" / "Backup: queued" / "Backup: syncing..." / "Backup: synced ✓"

#### 17-5. 에러 처리

- [ ] 네트워크 에러 → "Sync failed. We'll try again when online."
- [ ] 인증 실패 → token 재입력 안내
- [ ] backup 실패가 writing/autosave를 절대 막지 않음 (SPECS 원칙)

#### 검증 체크리스트

- [ ] token 없음 → "Backup: off" calm 상태
- [ ] token 입력 → 업로드 성공 → GitHub에서 파일 확인
- [ ] Wi-Fi 없음 → "Backup: queued" → Wi-Fi 연결 후 자동 sync
- [ ] 인증 실패 / HTTP 에러 → calm error 표시 (crash 아님)
- [ ] backup 실패 중에도 typing/autosave 정상

---

### Phase 18. AI Magic Bar

목적: 쓰기 흐름을 방해하지 않는 opt-in AI 보조

의존: Phase 16 (Wi-Fi), Phase 15 (.env import for API key)

#### 18-1. AI 설정

- [ ] NVS에서 `OPENAI_API_KEY` 로드
- [ ] Settings → AI assistance 화면
  - API key 입력/확인
  - Enable / Disable 토글
  - 개인정보 안내: "Selected text is sent to the AI service to generate suggestions."
- [ ] AI 비활성 시 Cmd+P → "AI is off" 토스트

#### 18-2. Magic Bar Prompt

- [ ] Cmd+P (또는 Win+P) → prompt 팝업
  - 텍스트 입력: "Ask AI to rewrite, continue, or summarize..."
  - Enter → 전송
  - Esc → 취소
- [ ] request payload 구성
  - selection (또는 커서 주변 컨텍스트)
  - full page context (또는 최근 N줄)
  - user instruction

#### 18-3. OpenAI Streaming 응답

- [ ] OpenAI Responses API 호출
  - `stream: true`
  - SSE 이벤트 파싱
  - `llm_task`에서 비동기 수행
- [ ] 응답 → Magic Bar preview에 실시간 표시

#### 18-4. Preview + Insert/Discard

- [ ] Magic Bar preview 위젯
  - 응답 텍스트 표시 (스트리밍 중에도 갱신)
  - Enter → 커서 위치에 삽입
  - Esc → 폐기
  - 전송 중 Esc → 요청 취소
- [ ] 삽입 시 undo 가능 (하나의 undo 그룹으로)

#### 검증 체크리스트

- [ ] API key 없음 → "AI is off" 안내
- [ ] prompt 전송 → stream 표시 → Insert 정상
- [ ] Esc → discard 정상
- [ ] 네트워크 오류 → "AI request failed. Try again later."
- [ ] AI 응답 중 typing 가능 (UI freeze 없음)

---
---

## Milestone F: "Polished Device"

목표: 주변 기능 완성 + 전체 통합 안정화

---

### Phase 19. Power / RTC / Audio / IMU

목적: 주변 기능을 붙여 제품 완성도 향상

의존: Phase 11 (HUD), Phase 10 (설정)

#### 19-1. Power Management

- [ ] auto-sleep 구현
  - 설정된 시간(5/15/30분) 동안 입력 없으면 sleep 진입
  - sleep 전 autosave 트리거
  - backlight off
  - CPU 저전력 모드
- [ ] wake 구현
  - 아무 키 → wake
  - backlight 복원
  - UI 복원 (editor 화면)
- [ ] Power off
  - 메뉴 → Power off → 확인 다이얼로그
  - autosave → clean_shutdown 플래그 → 전원 차단 (PI4IOE5V6408-2 E2.P5)

#### 19-2. RTC 시간 관리

- [ ] RX8130CE I2C 통신 (주소 0x32)
  - 시간 읽기/설정
- [ ] SNTP 동기화 (Wi-Fi 연결 시)
  - NTP 서버에서 시간 받아 RTC 설정
- [ ] 타임스탬프 표시 (HUD 또는 프로젝트 메타데이터)

#### 19-3. Audio Feedback

- [ ] ES8388 코덱 초기화
  - I2C 주소 0x10
  - SPK_EN (PI4IOE5V6408-1 E1.P1) 활성화
- [ ] 사운드 이벤트
  - 부팅 사운드 (선택적)
  - export 완료 사운드 (선택적)
  - 에러 사운드 (선택적)
- [ ] 볼륨 제어 / 음소거 설정

#### 19-4. IMU (BMI270)

- [ ] BMI270 초기화 (I2C 주소 0x68)
  - accelerometer/gyroscope 읽기
- [ ] 화면 방향 감지 (선택적)
  - 가로/세로 자동 회전 (주의: LESSONS_LEARNED에서 매핑 반전 이슈)
- [ ] motion wake-up (sleep 모드에서 움직임으로 wake)

#### 19-5. Diagnostics 화면

- [ ] Settings → Advanced → Diagnostics
  - IDF version, firmware build, uptime
  - heap 사용량 (free/total internal + PSRAM)
  - SD 카드 정보 (용량, 사용량)
  - Wi-Fi 상태
  - 센서 상태 (battery, IMU, RTC)
  - 로그 export (선택적)

#### 검증 체크리스트

- [ ] auto-sleep 진입 → 키 입력으로 wake → editor 정상 복귀
- [ ] sleep/wake 10회 반복 안정
- [ ] 시간 표시 정확 (RTC 또는 NTP)
- [ ] audio cue 출력 (스피커 연결 시)
- [ ] IMU 값 읽기 (diagnostics 화면)
- [ ] diagnostics 정보 표시 정확

#### 위험 요소

- sleep/wake 후 USB host 재초기화 실패 가능 → 키보드 reconnect 로직
- IMU 방향 매핑이 기기마다 다를 수 있음 (LESSONS_LEARNED: 반전 이슈)
- 전원 차단 직후 빠른 재기동 시 BMI270 초기화 실패 가능 (LESSONS_LEARNED: 5초 대기)

---

### Phase 20. 통합 안정화와 장시간 회귀

목적: 모든 기능을 제품 수준으로 통합 검증

의존: Phase 1~19 모두

#### 20-1. 통합 테스트 시나리오

- [ ] 시나리오 1: 첫 사용자 경험
  1. 새 SD 카드 삽입
  2. 첫 부팅 → "Untitled" 프로젝트 자동 생성
  3. 첫 한글/영문 혼합 문장 작성
  4. autosave 확인
  5. Ctrl+S → "Saved ✓" 확인

- [ ] 시나리오 2: 세션 복원
  1. 텍스트 작성 중 전원 재부팅
  2. 마지막 문서 자동 로드
  3. 커서 위치 복원

- [ ] 시나리오 3: 프로젝트 관리
  1. 프로젝트 3개 생성
  2. 프로젝트 간 전환
  3. 각 프로젝트 내용 독립 확인

- [ ] 시나리오 4: 대용량 문서
  1. 5,000자 이상 한글/영문 혼합 문서 작성
  2. 스크롤 성능 확인
  3. find 기능 사용
  4. undo/redo 대량 실행

- [ ] 시나리오 5: Export
  1. SD export (.md, .txt) → PC에서 확인
  2. Send to Computer → 메모장에서 확인
  3. 긴 문서 전송 → 누락 없음

- [ ] 시나리오 6: USB MSC
  1. USB 저장소 모드 진입
  2. PC에서 파일 복사
  3. `.env` 파일 복사 → detach → token import 확인

- [ ] 시나리오 7: 네트워크 기능
  1. Wi-Fi 연결
  2. GitHub backup → 성공 확인
  3. AI prompt → 응답 확인

- [ ] 시나리오 8: 전원 관리
  1. auto-sleep → wake → editor 복귀
  2. 2시간 idle + typing mixed 테스트

#### 20-2. 안정성 기준

- [ ] panic/watchdog/heap corruption 0건
- [ ] 데이터 손실 0건
- [ ] DSI underrun / display artifact 0건
- [ ] memory leak 없음 (heap 모니터링)

#### 20-3. 문서 갱신

- [ ] `LESSONS_LEARNED.md` 업데이트 (IDF 6 관련 새 교훈)
- [ ] `DOCS.MD` 업데이트 (사용자 가이드)
- [ ] known issues 문서화

#### 완료 조건

- [ ] 모든 시나리오 통과
- [ ] 2시간 연속 사용 테스트 통과
- [ ] 회귀 이슈 0건 또는 문서화 후 수용 가능 수준

---
---

## 공통 규칙 및 제약

#### 각 Phase 완료 조건 (Definition of Done)

1. `idf.py fullclean build` 성공 (0 error)
2. Tab5 flash/boot 확인 (실제 디바이스)
3. 해당 기능 smoke test 통과
4. 실패/주의사항 `LESSONS_LEARNED.md` 기록
5. 기능별 커밋 분리
6. 다음 Phase 진입 가능한 안정성

#### 아키텍처 경계 규칙

- Editor core는 LVGL/IDF를 몰라야 한다 (host test 가능 유지)
- Storage는 UI를 몰라야 한다 (상태 이벤트만 발행)
- USB HID/MSC는 keyboard host와 동시 활성화 금지
- Wi-Fi/GitHub/AI 실패는 writing/autosave를 절대 막지 않는다
- 모든 JSON 파일 쓰기는 atomic write (tmp → fsync → rename)

#### 이벤트 흐름

```
input_task ──→ KeyEvent ──→ ui_queue ──→ ui_task ──→ keybinding / text_input_composer
                                                   ──→ editor 연산 또는 composing overlay
                                                   ──→ LVGL render
                                                   ──→ dirty state
                                           ↓
                                    SaveRequest (snapshot)
                                           ↓
                               storage_queue ──→ storage_task ──→ SD write
                                                              ──→ "Saved" event → ui_task
```

#### 컴포넌트 맵

```
main/                     → 부팅, 이벤트 루프, 태스크 생성
components/
  tabwrite_platform/        → boot, NVS, logging, event loop, HW init
  tabwrite_display/         → display, LVGL port, backlight
  tabwrite_input/           → USB keyboard, key normalization, Korean IME, keybindings
  tabwrite_editor/          → piece table, cursor, undo, selection, find, word count
  tabwrite_storage/         → SD, document store, autosave, recovery, project library
  tabwrite_ui/              → screens, widgets, routes, theme
  tabwrite_export/          → SD export, USB HID, USB MSC
  tabwrite_online/          → Wi-Fi, GitHub, AI
  tabwrite_device_services/ → power, battery, RTC, audio, IMU
  tabwrite_secrets/         → NVS token 저장
```

#### Phase 간 의존 관계 요약

```
Phase 1  ← (모든 Phase의 기반)
Phase 2  ← Phase 1
Phase 3  ← Phase 1, 2
Phase 3.5← Phase 3
Phase 4  ← (독립, host test 가능)
Phase 5  ← Phase 2, 3, 3.5, 4
Phase 6  ← Phase 1, 4
Phase 7  ← Phase 6
Phase 8  ← Phase 7
Phase 9  ← Phase 5, 6, 7
Phase 10 ← Phase 9
Phase 11 ← Phase 5, 7, 10
Phase 12 ← Phase 4, 5
Phase 13 ← Phase 6, 9
Phase 14 ← Phase 5, 13
Phase 15 ← Phase 6, 14
Phase 16 ← Phase 10
Phase 17 ← Phase 15, 16
Phase 18 ← Phase 15, 16
Phase 19 ← Phase 10, 11
Phase 20 ← Phase 1~19
```

---

## 절대 미루면 안 되는 것 (미루기 금지 목록)

- boot stability (Phase 1)
- display (Phase 2)
- keyboard input (Phase 3)
- Korean IME / UTF-8 text input composer (Phase 3.5)
- editor core (Phase 4)
- minimal writing screen (Phase 5)
- autosave (Phase 7)
- recovery (Phase 8)
- SD export 또는 USB HID export 중 최소 하나 (Phase 13 또는 14)

## MVP 이전에 미뤄도 되는 것

- GitHub backup (Phase 17)
- AI Magic Bar (Phase 18)
- Audio feedback (Phase 19의 일부)
- IMU motion features (Phase 19의 일부)
- 복잡한 diagnostics/log export
- 여러 keyboard layout 전체 지원
- Touch 완성도 (키보드-first 제품)

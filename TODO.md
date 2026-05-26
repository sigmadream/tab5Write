# Scribe 기능 재구현 기반 ESP-IDF 6.0.1 포팅 계획서

작성일: 2026-05-26  
대상: Scribe / M5Stack Tab5 / ESP32-P4 / ESP-IDF 6.0.1  
전략: 기존 5.5.4 코드를 줄 단위로 고치는 포팅이 아니라, 현재 Scribe가 제공하는 기능을 IDF 6.0.1 기준으로 기능별 재구현한다. 기존 코드는 동작 명세와 참고 구현으로만 사용한다.

## 0. 포팅 전략 요약

### 목표

- "open → type → your words are safe"라는 제품 핵심 흐름을 먼저 살린다.
- 기능은 하드웨어 의존도가 낮은 순서에서 높은 순서로, 그리고 사용자 가치가 큰 순서로 붙인다.
- 각 단계는 독립적으로 빌드/플래시/기능 확인이 가능해야 한다.

### 우선순위 원칙

1. 부팅 가능한 최소 앱을 먼저 만든다.
2. 쓰기 경험: 화면, 키보드, 에디터를 가장 먼저 완성한다.
3. 데이터 안전성: 저장, autosave, recovery를 두 번째 축으로 완성한다.
4. 내보내기: SD export → USB HID Send to Computer → USB MSC 순서.
5. 부가 기능: Wi-Fi, GitHub backup, AI, audio, IMU는 핵심 안정화 후 붙인다.
6. 기존 managed component는 되도록 최신 IDF 6 호환 버전을 쓰고, local patch는 마지막 수단으로 둔다.

## 1. 기능 재구현 순서 한눈에 보기

| 순서 | 단계 | 사용자 기능 | 검증 기준 |
|---:|---|---|---|
| 1 | Foundation | IDF 6.0.1 빈 앱, 로그, 태스크 구조 | clean build/flash/boot |
| 2 | Display | 화면 켜짐, LVGL 루프, 기본 화면 | 첫 화면 안정 표시 |
| 3 | Input | USB 키보드 입력 이벤트 | 키 입력 로그/테스트 UI 반영 |
| 4 | Editor Core | 텍스트 삽입/삭제/커서/undo | host unit test + 장치 입력 |
| 5 | Minimal Writing Screen | 전체화면 에디터 | 부팅 후 바로 타이핑 가능 |
| 6 | Storage Base | SD mount, 경로 생성, 파일 read/write | 재부팅 후 파일 유지 |
| 7 | Autosave + Session | 자동 저장, 마지막 문서/커서 복원 | 전원 재부팅 후 이어쓰기 |
| 8 | Recovery | 임시파일/저널 복구 | 강제 리셋 후 복구 안내 |
| 9 | Menu + Projects | Esc 메뉴, 프로젝트 목록/생성/전환 | 키보드로 프로젝트 조작 |
| 10 | Settings + Preferences | 테마/폰트/auto-sleep/keyboard layout | 설정 저장/재부팅 유지 |
| 11 | HUD + Battery | Space hold/F1 HUD, save/battery 상태 | HUD 표시와 상태 갱신 |
| 12 | Find | 문서 검색/다음/이전 | 검색 결과 이동 |
| 13 | Export to SD | `.txt`/`.md` export | SD에 파일 생성 |
| 14 | USB HID Send | PC에 문서 타이핑 | 임의 앱에 텍스트 입력 |
| 15 | USB MSC + .env | USB 저장소 모드, `.env` import | PC 볼륨 인식/키 import |
| 16 | Wi-Fi | scan/connect/DHCP | AP 연결/IP 획득 |
| 17 | GitHub Backup | opt-in backup | 인증/업로드/오류 처리 |
| 18 | AI Magic Bar | prompt/stream/insert | 응답 preview/insert/cancel |
| 19 | Power/RTC/Audio/IMU | sleep, time, sound, sensors | 주변 기능 smoke test |
| 20 | Full Regression | 전체 통합 검증 | 장시간 사용/회귀 0 |

## 2. 단계별 상세 계획

## Phase 1. IDF 6.0.1 Foundation

### 목적

IDF 6.0.1에서 확실히 부팅되는 새 기준점을 만든다.

### 구현 범위

- `IDF_TARGET=esp32p4` 유지.
- 최소 `app_main`.
- NVS 초기화.
- build info 출력.
- heap/PSRAM/partition 로그 출력.
- event queue/task 구조의 뼈대만 생성.

### 기존 참고 파일

- `CMakeLists.txt`
- `main/app_main.cpp`
- `main/app_event.h`
- `main/app_build_info.h`
- `sdkconfig.defaults`
- `partitions.csv`

### 검증

- `idf.py fullclean build` 성공.
- `idf.py flash monitor` 후 panic 없이 boot.
- 로그에 IDF version, target, heap, PSRAM, partition 출력.

### 완료 조건
- 이후 모든 기능을 붙일 수 있는 "빈 Scribe runtime" 확보.

---

## Phase 2. Display + LVGL 최소 화면

### 목적
쓰기 장치에서 가장 먼저 필요한 화면 출력 경로를 안정화한다.

### 구현 범위
- Tab5 display 초기화.
- LVGL tick/task/flush loop.
- backlight on/off.
- 단일 splash 화면: `Scribe` / `Open. Type. Your words are safe.`
- touch는 아직 선택 사항. 우선 화면만 안정화.

### 기존 참고 파일
- `components/scribe_ui/display_driver.*`
- `components/scribe_ui/mipi_dsi_display.*`
- `components/scribe_ui/theme/theme.*`
- `components/scribe_ui/theme/fonts.cpp`
- `components/scribe_ui/theme/generated/`

### 검증
- cold boot 3회 모두 첫 화면 표시.
- 10분 idle 동안 watchdog/assert 없음.
- backlight 밝기 제어 가능.

### 완료 조건
- LVGL 기반 화면이 신뢰 가능.

---

## Phase 3. USB Keyboard Input 최소 구현

### 목적
사용자가 실제로 글을 입력할 수 있는 입력 경로를 확보한다.

### 구현 범위
- USB host install/event loop.
- HID keyboard attach/detach.
- key down/up normalize.
- printable ASCII, Enter, Backspace, Esc, 방향키, modifier 추출.
- 초기에는 텍스트 편집이 아니라 화면/로그에 최근 키 표시.

### 기존 참고 파일
- `components/scribe_input/keyboard_host.*`
- `components/scribe_input/key_event.*`
- `components/scribe_input/keymap.*`
- `components/scribe_input/keyboard_host_control.*`

### 검증
- USB 키보드 연결/해제 10회.
- 문자, Enter, Backspace, Esc, 방향키 입력 로그 확인.
- 키 반복 입력 시 watchdog 없음.

### 완료 조건
- "키보드 이벤트 버스"가 안정화됨.

---

## Phase 4. Editor Core 재구현/이식

### 목적
하드웨어와 독립적인 텍스트 모델을 먼저 안정화한다.

### 구현 범위
- piece table 또는 더 단순한 text buffer 선택.
- cursor 이동.
- insert/delete/backspace/newline.
- selection.
- undo/redo.
- find index.
- snapshot API.

### 기존 참고 파일
- `components/scribe_editor/editor_core.*`
- `components/scribe_editor/piece_table.*`
- `components/scribe_editor/undo_stack.*`
- `components/scribe_editor/selection.*`
- `test/test_editor_core.cpp`
- `test/test_piece_table.cpp`
- `test/test_undo_stack.cpp`

### 검증
- host unit test 우선.
- 입력 시나리오:
  - insert "hello"
  - backspace
  - newline
  - undo/redo
  - find next/prev
  - snapshot roundtrip

### 완료 조건
- 장치 UI와 무관하게 에디터 모델 신뢰성 확보.

---

## Phase 5. Minimal Writing Screen

### 목적
Scribe의 핵심 가치인 "부팅 후 바로 쓰기"를 구현한다.

### 구현 범위
- 전체 화면 TextView.
- keyboard event → editor core → render.
- cursor 표시.
- 기본 word count.
- Esc는 아직 menu 대신 placeholder.
- 저장은 아직 memory-only.

### 기존 참고 파일
- `components/scribe_ui/ui_screens/screen_editor.*`
- `components/scribe_ui/widgets/text_view.*`
- `components/scribe_ui/ui_app.*`
- `components/scribe_input/keybinding.*`

### 검증
- 부팅 후 키보드로 바로 글쓰기.
- 1,000자 이상 입력.
- Enter/Backspace/방향키 동작.
- key-to-render latency 체감 문제 없음.

### 완료 조건
- "타이핑 가능한 Scribe" 완성.

---

## Phase 6. Storage Base: SD mount + 파일 IO

### 목적
글이 안전하게 남는 첫 단계를 만든다.

### 구현 범위
- SD mount/unmount.
- `/sdcard/Scribe` 기본 디렉터리 생성.
- 단일 current document save/load.
- atomic write 원칙 도입: temp write → fsync/close → rename.

### 기존 참고 파일
- `components/scribe_storage/storage_manager.*`
- `partitions.csv`
- `sdkconfig.defaults`

### 검증
- SD mount 성공.
- 텍스트 저장 후 재부팅해서 load.
- SD 제거 시 graceful error.
- 긴 파일명 설정 확인.

### 완료 조건
- 단일 문서 persistence 확보.

---

## Phase 7. Autosave + Session Restore

### 목적
"Save is not a verb" 원칙을 구현한다.

### 구현 범위
- idle 기반 autosave.
- dirty state.
- save state: Saving / Saved.
- 마지막 열린 문서 ID 저장.
- cursor/scroll 위치 저장.
- boot 시 마지막 문서 자동 복원.

### 기존 참고 파일
- `components/scribe_storage/autosave.*`
- `components/scribe_storage/session_state.*`
- `components/scribe_storage/settings_store.*`
- `main/app_main.cpp` storage task 흐름

### 검증
- 입력 후 수 초 내 저장 상태가 Saved.
- 전원 재부팅 후 마지막 문서와 cursor 복원.
- 저장 중 반복 입력해도 데이터 손실 없음.

### 완료 조건
- "마지막 문장으로 돌아오기" 달성.

---

## Phase 8. Recovery + Snapshot

### 목적
비정상 종료에도 사용자가 불안하지 않도록 만든다.

### 구현 범위
- autosave temp/journal 파일.
- crash/reboot 후 recovery candidate 탐지.
- recovery screen 최소 구현.
- restore / keep current.
- snapshot rotation.

### 기존 참고 파일
- `components/scribe_storage/recovery.*`
- `components/scribe_ui/ui_screens/screen_recovery.*`
- `SPECS.md` recovery copy

### 검증
- 입력 직후 강제 reset.
- boot 후 recovery 안내 표시.
- restore 선택 시 텍스트 복원.
- 정상 종료/정상 save 후에는 recovery가 뜨지 않음.

### 완료 조건
- 데이터 안전성 MVP 완성.

---

## Phase 9. Menu + Project Library

### 목적
단일 문서에서 여러 프로젝트를 다룰 수 있게 확장한다.

### 구현 범위
- Esc menu.
- project metadata/library.
- recent-first project list.
- new project.
- switch project.
- archive/delete with confirmation.
- project names 중복 처리.

### 기존 참고 파일
- `components/scribe_storage/project_library.*`
- `components/scribe_ui/ui_screens/screen_menu.*`
- `screen_projects.*`
- `screen_new_project.*`
- `assets/strings/en.json`

### 검증
- Esc → 메뉴 → 새 프로젝트 생성.
- 프로젝트 전환.
- 최근순 정렬.
- archive 후 목록에서 제거되고 파일은 보존.

### 완료 조건
- 프로젝트 기반 writing workflow 완성.

---

## Phase 10. Settings + Preferences

### 목적
사용자 경험에 필요한 최소 설정을 저장/적용한다.

### 구현 범위
- theme light/dark.
- font size.
- keyboard layout.
- auto-sleep setting.
- brightness.
- 설정 저장/로드.

### 기존 참고 파일
- `components/scribe_ui/ui_screens/screen_settings.*`
- `components/scribe_storage/settings_store.*`
- `components/scribe_input/keymap.*`
- `components/scribe_ui/theme/fonts.cpp`

### 검증
- 설정 변경 즉시 editor에 적용.
- 재부팅 후 설정 유지.
- keyboard-only navigation.

### 완료 조건
- 최소 설정 UX 완성.

---

## Phase 11. HUD + Battery 상태

### 목적
화면은 조용하게 유지하되 필요한 상태를 즉시 보여준다.

### 구현 범위
- Hold Space 1초 또는 F1.
- project name.
- words today / total.
- battery.
- save state.
- backup/AI off 상태 표시만 우선.

### 기존 참고 파일
- `components/scribe_ui/widgets/hud_overlay.*`
- `components/scribe_input/space_hold_detector.*`
- `components/scribe_services/battery.*`
- `components/scribe_hw/tab5_ina226.*`

### 검증
- Space hold로 HUD 표시/숨김.
- F1 동작.
- 저장 상태 갱신.
- 배터리 값 표시 또는 sensor unavailable 상태 표시.

### 완료 조건
- 상태 확인 UX 완성.

---

## Phase 12. Find

### 목적
긴 문서에서 최소 검색 기능을 제공한다.

### 구현 범위
- Ctrl+F 또는 menu Find.
- 검색어 입력.
- next/previous.
- match count.
- no result 표시.

### 기존 참고 파일
- `components/scribe_ui/ui_screens/screen_find.*`
- `components/scribe_editor/editor_core.*`
- `components/scribe_input/keybinding.*`

### 검증
- 현재 문서에서 단어 검색.
- Enter next, Shift+Enter prev.
- Esc close.

### 완료 조건
- MVP 검색 기능 완성.

---

## Phase 13. Export to SD

### 목적
가장 단순하고 안정적인 내보내기부터 제공한다.

### 구현 범위
- Export menu.
- `.txt` export.
- `.md` export.
- export list/delete.
- 경로: `/sdcard/Scribe/Exports`.

### 기존 참고 파일
- `components/scribe_export/export_sd.*`
- `components/scribe_ui/ui_screens/screen_export.*`
- `components/scribe_storage/storage_manager.*`

### 검증
- 현재 문서 `.md`, `.txt` 생성.
- PC/SD 리더에서 파일 내용 확인.
- 같은 이름 충돌 처리.

### 완료 조건
- offline export 완성.

---

## Phase 14. USB HID Send to Computer

### 목적
Scribe의 차별 기능인 "컴퓨터에 타이핑해서 보내기"를 구현한다.

### 구현 범위
- TinyUSB HID device init/deinit.
- keyboard host 중지 후 device mode 전환.
- 텍스트 → HID key sequence 변환.
- 진행/취소 UI.
- 완료 후 keyboard host 복구.

### 기존 참고 파일
- `components/scribe_export/send_to_computer.*`
- `components/scribe_export/usb_hid_device.*`
- `components/scribe_input/keyboard_host_control.*`
- `components/scribe_ui/ui_screens/screen_export.*`

### 검증
- 메모장/Word/Google Docs 등 임의 앱에 짧은 문서 전송.
- 긴 문서 전송.
- Esc cancel.
- 전송 후 다시 키보드 입력 가능.

### 완료 조건
- "Magical export" MVP 완성.

---

## Phase 15. USB MSC + .env Import

### 목적
고급 사용자를 위한 파일 접근과 token import를 구현한다.

### 구현 범위
- SD unmount.
- TinyUSB MSC storage export.
- attach/detach event.
- 종료 후 SD remount.
- `/sdcard/Scribe/.env` parse.
- `OPENAI_API_KEY`, `GITHUB_TOKEN` import.

### 기존 참고 파일
- `components/scribe_export/usb_msc_device.*`
- `components/scribe_services/env_importer.*`
- `components/scribe_ui/ui_screens/screen_usb_storage.*`
- `components/scribe_ui/ui_screens/screen_advanced.*`
- `DOCS.MD` Chapter 4

### 검증
- PC에서 removable drive 인식.
- 파일 복사 가능.
- detach 후 app storage remount.
- `.env` import 후 secrets 저장.

### 완료 조건
- USB storage + credential import 완성.

---

## Phase 16. Wi-Fi

### 목적
온라인 기능의 기반만 안정화한다.

### 구현 범위
- ESP32-P4 + ESP32-C6 Wi-Fi remote/hosted 구성.
- scan.
- password entry.
- connect/disconnect.
- DHCP/IP 상태.
- Wi-Fi screen.

### 기존 참고 파일
- `components/scribe_services/wifi_manager.*`
- `components/scribe_ui/ui_screens/screen_wifi.*`
- `sdkconfig.defaults` Wi-Fi remote/hosted 설정
- `main/idf_component.yml` `esp_wifi_remote`, `esp_hosted`

### 검증
- AP scan 표시.
- WPA2/WPA3 연결.
- IP 획득.
- disconnect/reconnect 10회.

### 완료 조건
- 네트워크 기반 기능을 붙일 수 있는 상태.

---

## Phase 17. GitHub/Gist Backup

### 목적
선택형 cloud backup을 붙인다. 저장의 primary path가 되면 안 된다.

### 구현 범위
- token 저장.
- backup queue.
- manual backup.
- opportunistic sync.
- conflict/error UI.
- HUD backup state.

### 기존 참고 파일
- `components/scribe_services/github_backup.*`
- `components/scribe_secrets/secrets_nvs.*`
- `components/scribe_ui/ui_screens/screen_backup.*`
- `components/scribe_services/env_importer.*`

### 검증
- token 없음: calm error.
- token 있음: upload 성공.
- Wi-Fi 없음: queued 상태.
- 인증 실패/HTTP 실패 표시.

### 완료 조건
- cloud backup opt-in 완성.

---

## Phase 18. AI Magic Bar

### 목적
쓰기 흐름을 방해하지 않는 opt-in AI 보조 기능을 구현한다.

### 구현 범위
- AI 설정/token 저장.
- prompt popup.
- selected line + full page context payload.
- streaming response.
- Magic Bar preview.
- Enter insert / Esc discard / cancel.

### 기존 참고 파일
- `components/scribe_services/ai_assist.*`
- `components/scribe_ui/ui_screens/screen_ai.*`
- `screen_ai_prompt.*`
- `screen_magic_bar.*`
- `components/scribe_ui/ui_app.*`
- `components/scribe_secrets/secrets_nvs.*`

### 검증
- token 없음: 안내.
- prompt 전송.
- stream 표시.
- insert/discard.
- 네트워크 오류/취소 처리.

### 완료 조건
- AI는 optional helper로 안정 동작.

---

## Phase 19. Power / RTC / Audio / IMU

### 목적
주변 기능을 붙여 완성도를 높인다.

### 구현 범위
- auto-sleep.
- wake 후 backlight/UI 복원.
- RTC/SNTP sync.
- audio feedback.
- IMU polling.
- diagnostics screen.

### 기존 참고 파일
- `components/scribe_services/power_manager.*`
- `components/scribe_services/rtc_time.*`
- `components/scribe_services/audio_manager.*`
- `components/scribe_services/audio_feedback.*`
- `components/scribe_services/imu_manager.*`
- `components/sensor_bmi270/`
- `components/scribe_ui/ui_screens/screen_diagnostics.*`

### 검증
- sleep/wake 후 editor 복귀.
- 시간 표시.
- audio cue 출력.
- IMU 값 읽기.
- diagnostics 정보 표시.

### 완료 조건
- 주요 주변 기능 회귀 없음.

---

## Phase 20. 통합 안정화와 장시간 회귀

### 목적
기능별 구현을 제품 수준으로 묶는다.

### 검증 시나리오
1. 새 SD 카드에서 첫 부팅.
2. 첫 문장 작성.
3. autosave 확인.
4. 재부팅 후 마지막 문서/cursor 복원.
5. 프로젝트 3개 생성/전환.
6. 5,000자 문서 작성.
7. find 사용.
8. export `.md`.
9. Send to Computer.
10. USB MSC로 `.env` 복사 후 import.
11. Wi-Fi 연결.
12. GitHub backup.
13. AI prompt.
14. sleep/wake.
15. 2시간 idle/typing mixed test.

### 완료 조건
- panic/watchdog/heap corruption 없음.
- 데이터 손실 없음.
- known issues 문서화.
- `DOCS.MD`, `LESSONS_LEARNED.md` 업데이트.

## 3. 재구현 시 권장 아키텍처

### 레이어 분리
1. `scribe_platform`
   - boot, NVS, logging, event loop, hardware init.
2. `scribe_display`
   - display, LVGL, touch, backlight.
3. `scribe_input`
   - USB keyboard, key normalization, keybindings.
4. `scribe_editor`
   - pure text model. 가능하면 host test 가능해야 함.
5. `scribe_storage`
   - SD, document store, autosave, recovery, project library.
6. `scribe_ui`
   - screens/widgets/routes.
7. `scribe_export`
   - SD export, USB HID, USB MSC.
8. `scribe_online`
   - Wi-Fi, GitHub, AI.
9. `scribe_device_services`
   - power, battery, RTC, audio, IMU.

### 이벤트 흐름
- input task → normalized key event → app event queue.
- UI task → editor operation → dirty state.
- storage task → autosave/recovery/export IO.
- network task → Wi-Fi/backup/AI callbacks.

### 중요한 경계
- Editor core는 LVGL/IDF를 몰라야 한다.
- Storage는 UI를 몰라야 한다. 상태 이벤트만 발행한다.
- USB HID/MSC는 keyboard host와 동시에 활성화하지 않는다.
- Wi-Fi/GitHub/AI 실패는 writing/autosave를 막지 않는다.

## 4. 각 단계 공통 Definition of Done

각 Phase는 다음을 만족해야 완료로 본다.

1. `idf.py fullclean build` 성공.
2. 실제 Tab5 flash/boot 확인.
3. 해당 기능 smoke test 통과.
4. 실패/주의사항을 `LESSONS_LEARNED.md`에 기록.
5. 기능별 커밋으로 분리.
6. 다음 단계가 현재 단계에 의존해도 될 만큼 안정적임.

## 5. 먼저 버려도 되는 것 / 나중에 붙일 것

### MVP 이전에는 미뤄도 됨
- GitHub backup.
- AI Magic Bar.
- Audio feedback.
- IMU motion features.
- 복잡한 diagnostics/log export.
- 여러 keyboard layout 전체 지원.
- Touch 완성도. 키보드-first 제품이므로 초기에는 keyboard navigation 우선.

### 절대 미루면 안 됨
- boot stability.
- display.
- keyboard input.
- editor core.
- autosave.
- recovery.
- project restore.
- SD export 또는 USB HID export 중 최소 하나.

## 6. 추천 마일스톤

### Milestone A: "Type on Screen"
포함 Phase: 1~5  
결과: 부팅 후 키보드로 화면에 글을 쓸 수 있음.

### Milestone B: "Words Are Safe"
포함 Phase: 6~8  
결과: autosave/reboot/recovery가 동작함.

### Milestone C: "Real Writing Device"
포함 Phase: 9~13  
결과: 프로젝트, 메뉴, 설정, 검색, SD export까지 가능.

### Milestone D: "Magical Export"
포함 Phase: 14~15  
결과: USB HID Send to Computer와 USB MSC가 가능.

### Milestone E: "Connected Optional Features"
포함 Phase: 16~18  
결과: Wi-Fi, backup, AI가 opt-in으로 가능.

### Milestone F: "Polished Device"
포함 Phase: 19~20  
결과: 전원/오디오/IMU/장시간 안정화.

## 7. 바로 다음에 할 일

1. `feature/rebuild-idf601-foundation` 브랜치 생성.
2. 기존 코드를 직접 수정하기 전에 새 foundation 범위 확정.
3. Phase 1용 최소 앱을 만든다.
4. Phase 1 통과 후 기존 코드에서 필요한 기능을 "참고 구현"으로만 하나씩 가져온다.

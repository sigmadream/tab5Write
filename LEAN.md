# LEAN.md

TABWRITE를 ESP32-P4 / M5Stack Tab5 / ESP-IDF 6.0.1 기준으로 재구현하면서 Phase 1, Phase 2에서 확인한 오류, 수정 방법, 코드 유지보수 시 알아둘 사항을 기록한다.

---

## 공통 빌드 규칙

- `idf.py`를 실행하기 전에 반드시 `idf-venv`로 ESP-IDF 6.0.1 가상환경을 활성화한다.
  ```sh
  idf-venv
  idf.py build
  ```
- 현재 셸에서 `idf.py`가 PATH에 없으면 다음 형태를 사용한다.
  ```sh
  idf-venv
  python "$IDF_PATH/tools/idf.py" build
  ```
- ESP-BSP 관련 코드는 `ref/esp-bsp/`를 우선 참고한다.

---

## Phase 1. IDF 6.0.1 Foundation

### 발생한 오류와 수정 방법

#### 1. 앱 이름/로그 태그가 이전 코드명으로 남아 있던 문제

- 증상: 프로젝트명, 로그 태그, 빌드 정보 매크로, splash 텍스트에 `SCRIBE`, `Scribe`, `scribe`, `tabWrite` 계열 이름이 섞여 있었다.
- 수정:
  - 루트 `CMakeLists.txt`의 `project(TABWRITE)`로 변경.
  - `main/app_build_info.h` 매크로를 `TABWRITE_*`로 변경.
  - 로그 태그를 `TABWRITE_MAIN`, `TABWRITE_UI`, `TABWRITE_STORAGE`, `TABWRITE_DISPLAY`로 통일.
  - 사용자 표시 문자열도 `TABWRITE`로 통일.
- 확인:
  ```sh
  grep -RIn "SCRIBE\|Scribe\|scribe\|tabWrite" main CMakeLists.txt sdkconfig.defaults AGENTS.md TODO_p1.md
  ```
  결과가 없어야 한다.

#### 2. PSRAM 설정 불일치

- 증상: Tab5 부팅 로그에서 Hex PSRAM이 감지되는데 defaults에는 Octal PSRAM 설정이 남아 있었다.
- 수정: `sdkconfig.defaults`에서 `CONFIG_SPIRAM_MODE_HEX=y` 사용.
- 확인: 부팅 로그에 `hex_psram`, `Found 32MB PSRAM device`, `SPI SRAM memory test OK`가 출력된다.

### 알아둘 사항

- `app_main()`은 Phase 1 기준으로 다음 순서를 유지한다.
  1. `nvs_flash_init()`
  2. 빌드/타깃/heap 로그 출력
  3. 디스플레이 초기화
  4. 큐 생성
  5. UI/storage task 생성
- `ui_queue`, `storage_queue`는 Phase 3 이후 입력/저장 이벤트의 공통 통로가 된다.
- `app_build_info.h`의 `TABWRITE_BUILD_TIMESTAMP`는 컴파일 시점 문자열이므로 빌드마다 바이너리 해시가 달라질 수 있다.

---

## Phase 2. Display + LVGL 최소 화면

### 발생한 오류와 수정 방법

#### 1. 직접 LVGL/DSI placeholder 구현이 과도했던 문제

- 증상: Phase 2 코드가 직접 LEDC, LVGL port, MIPI-DSI placeholder를 섞어 관리해 실제 Tab5 BSP 경로와 어긋났다.
- 수정:
  - `main/idf_component.yml`에서 직접 `lvgl/lvgl`, `espressif/esp_lvgl_port` 의존성을 제거.
  - `m5stack_tab5` BSP를 local override로 추가.
    ```yaml
    dependencies:
      idf:
        version: ">=5.4"
      m5stack_tab5:
        version: "*"
        override_path: "../ref/esp-bsp/bsp/m5stack_tab5"
    ```
  - 앱에서는 `bsp_display_start()`를 호출하고, LVGL tick/task/display driver는 BSP와 `esp_lvgl_port`에 맡긴다.
- 효과: Phase 2 앱 코드가 실제 화면 표시와 splash 생성만 담당하도록 단순화됐다.

#### 2. `lv_timer_handler()` 직접 호출 충돌 위험

- 증상: BSP가 이미 LVGL task를 시작하는데 `ui_task`에서 `lv_timer_handler()`를 직접 호출하면 중복 처리/락 문제를 만들 수 있다.
- 수정:
  - `ui_task`의 직접 `lv_timer_handler()` 호출 제거.
  - LVGL 오브젝트 생성 시 `app_display_lock()` / `app_display_unlock()` 사용.
- 규칙: 앞으로도 LVGL API 호출은 display lock 안에서 수행한다.

#### 3. Montserrat 20 폰트 미활성화

- 증상: splash title에 `lv_font_montserrat_20`을 쓰려면 해당 폰트가 Kconfig에서 활성화되어야 한다.
- 수정: `sdkconfig.defaults`에 `CONFIG_LV_FONT_MONTSERRAT_20=y` 추가.

#### 4. BSP LEDC backlight panic

- 증상: `bsp_display_start()` 중 `bsp_display_brightness_init()`에서 아래 panic 발생.
  ```text
  Guru Meditation Error: Core 0 panic'ed (Store access fault)
  ledc_ll_set_fade_param_range ... LEDC_GAMMA_RAM...
  bsp_display_brightness_init()
  ```
- 원인: ESP32-P4 + ESP-IDF v6.0.1 환경에서 BSP의 LEDC PWM backlight 초기화가 LEDC fade/gamma RAM 경로를 건드리며 fault를 냈다. LEDC channel 1을 0으로 바꿔도 재현되어 channel 문제가 아니었다.
- 수정:
  - `ref/esp-bsp/bsp/m5stack_tab5/src/bsp_display.c`의 backlight 초기화를 LEDC PWM에서 GPIO on/off로 단순화.
  - `driver/ledc.h` 제거, `driver/gpio.h` 사용.
  - `bsp_display_brightness_set(percent)`는 `percent > 0`이면 backlight on, 아니면 off로 동작하게 변경.
- Phase 2 판단: 세밀한 밝기 PWM보다 안정적인 화면 표시가 우선이므로 on/off 제어가 적절하다.

#### 5. Double buffer 안정성 우선 설정

- 수정: `sdkconfig.defaults`에 `CONFIG_BSP_LCD_DRAW_BUF_DOUBLE=n` 설정.
- 이유: Phase 2에서는 DSI/LVGL 경로 안정화가 우선이며, double buffer는 메모리/타이밍 이슈를 늘릴 수 있다.

### 검증된 결과

- 빌드:
  ```sh
  idf-venv
  python "$IDF_PATH/tools/idf.py" build
  ```
- 플래시/모니터:
  ```sh
  idf-venv
  python "$IDF_PATH/tools/idf.py" -p /dev/cu.usbmodem21101 flash monitor
  ```
- 확인된 로그:
  - `Project name: TABWRITE`
  - `MIPI DSI PHY Powered on`
  - `Display initialized with resolution 720x1280`
  - `Setting LCD backlight: on`
  - `TABWRITE_DISPLAY: Display initialized`
  - `TABWRITE_UI: UI Task started`
  - `TABWRITE_STORAGE: Storage Task started`
- LEDC panic은 GPIO backlight 우회 후 재발하지 않았다.

### 알아둘 사항

- `main/app_display.cpp`
  - `app_display_init()`은 `bsp_display_start()`만 호출한다.
  - 실패 시 `ESP_ERROR_CHECK(ESP_FAIL)`로 즉시 중단한다.
  - `display_set_backlight(50)`은 현재 실제 50% PWM이 아니라 on/off API를 통과하는 기본 on 호출이다.
- `main/app_ui.cpp`
  - splash는 `lv_display_get_default()`가 없으면 아무것도 하지 않는다.
  - 배경은 `LV_OPA_COVER`를 사용해 ghosting을 피한다.
  - title font는 `lv_font_montserrat_20`, tagline은 `lv_font_montserrat_14`를 사용한다.
- `main/idf_component.yml`
  - BSP는 반드시 local override를 유지한다. 그렇지 않으면 registry의 원본 `m5stack_tab5`가 사용되어 LEDC panic 수정이 빠질 수 있다.
- `dependencies.lock`
  - `m5stack_tab5` source가 `type: local`, `path: ref/esp-bsp/bsp/m5stack_tab5`인지 확인한다.
- 향후 실제 밝기 단계가 필요하면 LEDC fade/gamma 경로를 피하는 다른 PWM 방식 또는 IDF/BSP 업데이트 후 재검증이 필요하다.

---

## 다음 Phase에서 지켜야 할 원칙

- Phase 3 입력 구현 전까지 display 코드는 더 확장하지 말고, splash와 lock API만 안정적으로 유지한다.
- ESP-BSP가 제공하는 초기화 순서를 우선 사용하고, 앱 코드에서 I2C/DSI/LVGL를 중복 초기화하지 않는다.
- 새 기능을 붙일 때는 먼저 `ref/esp-bsp/`의 예제와 컴포넌트 구조를 확인한다.
- 실기기 검증 로그가 없는 항목은 TODO 체크리스트에서 완료 처리하지 않는다.

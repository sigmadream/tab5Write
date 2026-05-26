## Tutorial 00

### 작업 전 준비사항

`eim`을 사용해서 `idf` 관련 개발 환경을 구성하시고, VSCode에 `esp-idf`를 설치하셔서, 좀 더 편리한 환경에서 개발을 진행하실 수 있도록 준비하세요. 그리고 `idf.py`를 실행하기 전에는 항상 `idf` 가상 환경을 활성화하세요.

```sh
$ source .espressif/tools/activate_idf_v6.0.1.sh
(venv) $ idf.py --version
ESP-IDF v6.0.1
```

### ESP-BSP 코드

Tab5 관련 코드는 [ESP-BSP](https://github.com/espressif/esp-bsp)를 참고하세요.

## Tutorial 01

> "panic 없이 부팅되는 빈 TABWRITE runtime"을 생성해 보도록 하겠습니다.

### 1-1. 프로젝트 생성

```sh
(venv) $ idf.py create-project TABWRITE
```

`TABWRITE` 폴더에 생성된 `CMakeLists.txt`는 아래와 같습니다.

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TABWRITE)
```

### 1-2. main 컴포넌트 CMake 작성

`main` 폴더 내부의 CMake 파일은 아래와 같습니다.

```cmake
idf_component_register(
    SRCS
        "app_main.cpp"
    INCLUDE_DIRS "."
)
```

---

### 1-3. sdkconfig.defaults 작성

`menuconfig`를 사용해서, 아래 설정들을 확인하고 저장하세요.

```sh
(venv) $ idf.py set-target esp32p4
(venv) $ idf.py menuconfig
```

- CONFIG_IDF_TARGET="esp32p4"
  - TAB5에서 사용하는 칩셋을 설정
- CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
  - ESP32-P4 칩이 레비전 V3보다 낮은 Rev임을 설정
- CONFIG_SPIRAM=y
  - PSRAM은 Flash와 달리 외부에 부착되어 있으므로 이를 활성화, PSRAM을 활성화하는 이유는 내부 SRAM 용량(약 768KB)의 한계를 극복하고, 고해상도 LCD 화면 구동을 위한 LVGL 프레임 버퍼 및 그래픽 리소스, 동적 메모리(Heap) 공간을 충분히 확보하기 위함
- CONFIG_SPIRAM_MODE_HEX=y
  - PSRAM 모드를 Hex(16비트 데이터 버스 대역폭)로 설정, M5Stack Tab5 하드웨어는 16비트 Hex PSRAM이 탑재되어 있으므로, 옥탈(8비트)이나 쿼드(4비트) 모드가 아닌 Hex 모드를 활성화하여 최대 통신 성능과 고속 그래픽 렌더링 대역폭을 확보
- CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
  - Flash 크기를 16MB로 설정
- CONFIG_FATFS_LFN_HEAP=y
  - FATFS에 긴 파일 이름(LFN)을 저장할 때 힙 메모리를 사용하도록 설정
- CONFIG_FATFS_MAX_LFN=255
  - 긴 파일 이름의 최대 길이를 255자로 설정
- CONFIG_FREERTOS_HZ=1000
  - FreeRTOS의 tick rate를 1000Hz로 설정, 1ms마다 tick 인터럽트가 발생하므로 정밀한 타이밍 제어가 가능
- CONFIG_PARTITION_TABLE_CUSTOM=y
  - 파티션 테이블을 커스텀으로 설정
- CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
  - 파티션 테이블 파일 이름을 "partitions.csv"로 설정

### 1-4. 파티션 테이블 작성

```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     ,        0x4000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        0x300000,
storage,  data, fat,     ,        0x500000,
```

1. nvs (Non-Volatile Storage, 16KB): 화면 밝기, Wi-Fi 접속 정보, 볼륨 설정값과 같이 기기의 전원이 꺼지더라도 계속 보존되어야 하는 가벼운 키-값(Key-Value) 형태의 사용자 메타데이터를 저장하기 위해 할당된 필수 비휘발성 영역입니다.

2. phy_init (물리 계층 전용 데이터 영역, 4KB): ESP32 시리즈 칩의 Wi-Fi, Bluetooth 등 무선 통신 주파수를 세밀하게 조율하고 초기 교정(Calibration) 파라미터를 보관하기 위한 전용 시스템 공간입니다.

3. factory (메인 앱 영역, 3MB): 기기가 동작할 때 실행되는 실제 컴파일된 펌웨어 바이너리가 탑재되는 기본 실행 영역입니다. 고해상도 LCD 그래픽 라이브러리(LVGL)와 에디터의 핵심 비즈니스 제어 로직이 포함된 TABWRITE 앱 펌웨어는 용량이 상당하므로 이를 문제없이 적재할 수 있도록 3MB 크기로 넉넉하게 설계하였습니다.

4. storage (파일 시스템 저장소 영역, 5MB): 텍스트 에디터를 통해 작성한 문서 파일(.txt 등), 로깅 내역 및 로컬에 보존해야 할 리소스 등을 영구히 저장할 수 있는 공간입니다. 이 영역은 FAT 파일 시스템(FATFS)으로 마운트하여 컴퓨터의 하드 드라이브처럼 폴더와 파일을 다루는 저장 매체 역할을 수행하게 됩니다.

전체 플래시 메모리 공간(16MB) 내에서 기본 하드웨어 통신과 설정 영역을 분리하고, 메인 프로그램 구동 영역(3MB)과 사용자 데이터 저장고(5MB)를 독립된 파티션으로 구조화하여 서로의 영역 침범 없이 기기를 안전하게 제어하고 향후 텍스트 파일 저장 기능까지 무리 없이 지원하기 위한 최적의 배치 방식입니다.

---

### 1-5. build info 작성

`main/app_build_info.h` 파일에는 앱의 버전 정보와 빌드 시간을 정의합니다.

```cpp
#pragma once

#define TABWRITE_VERSION_MAJOR 0
#define TABWRITE_VERSION_MINOR 1
#define TABWRITE_VERSION_PATCH 0

#define TABWRITE_BUILD_TIMESTAMP __DATE__ " " __TIME__

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "unknown"
#endif
```

---

### 1-6. 이벤트 타입 작성

`main/app_event.h` 파일에는 이벤트 타입을 정의합니다.

```cpp
#pragma once

#include <stdint.h>

enum class AppEventType { KEY_EVENT, STORAGE_REQUEST, UI_COMMAND };

struct AppEvent {
  AppEventType type;
  union {
    struct {
      uint32_t key_code;
      uint32_t action;
      uint32_t modifiers;
      char printable;
    } key;
    struct {
      uint32_t command;
    } ui;
    struct {
      uint32_t request_id;
    } storage;
  };
};
```

---

### 1-7. 큐 선언 작성

`main/app_queue.h` 파일에는 큐를 선언합니다.

```cpp
#pragma once

#include "app_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t ui_queue;
extern QueueHandle_t storage_queue;

void app_queue_init();
```

---

### 1-8. 최소 app_main 작성

`main/app_main.cpp`

```cpp
#include "app_build_info.h"
#include "app_queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "TABWRITE_MAIN";

QueueHandle_t ui_queue;
QueueHandle_t storage_queue;

void app_queue_init() {
  ui_queue = xQueueCreate(10, sizeof(AppEvent));
  storage_queue = xQueueCreate(10, sizeof(AppEvent));
}

void ui_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI("TABWRITE_UI", "UI Task started");

  while (1) {
    AppEvent evt;
    if (xQueueReceive(ui_queue, &evt, pdMS_TO_TICKS(10))) {
      ESP_LOGI("TABWRITE_UI", "Received UI event");
    }
  }
}

void storage_task(void *pvParameters) {
  (void)pvParameters;
  ESP_LOGI("TABWRITE_STORAGE", "Storage Task started");

  AppEvent evt;
  while (1) {
    if (xQueueReceive(storage_queue, &evt, portMAX_DELAY)) {
      ESP_LOGI("TABWRITE_STORAGE", "Received storage request");
    }
  }
}

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "TABWRITE Firmware Booting...");
  ESP_LOGI(TAG, "Build Timestamp: %s", TABWRITE_BUILD_TIMESTAMP);
  ESP_LOGI(TAG, "Target: %s", CONFIG_IDF_TARGET);
  ESP_LOGI(TAG, "Free heap size: %lu bytes",
           (unsigned long)esp_get_free_heap_size());
  ESP_LOGI(TAG, "Free internal heap: %lu bytes",
           (unsigned long)esp_get_free_internal_heap_size());

  app_queue_init();

  xTaskCreate(ui_task, "ui_task", 8192, NULL, 5, NULL);
  xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
```

---

### 1-9. Phase 1 빌드/검증

```sh
idf.py set-target esp32p4
idf.py build
idf.py flash monitor
```

---

## Tutorial 02

> Phase 2의 목적은 Tab5 디스플레이에 안정적으로 splash 화면을 표시하는 것이다.

### 2-1. BSP 의존성 추가

`main/idf_component.yml`는 기존 파일을 추가하거나, 명령어를 사용하세요.

```sh
(venv) $ idf.py add-dependency "espressif/m5stack_tab5^1.2.0~1"
```

직접 수정은 아래 파일을 참고하세요.

```yaml
dependencies:
  idf:
    version: ">=5.4"
  m5stack_tab5:
    version: "*"
```

---

### 2-2. LVGL/BSP 설정 추가

`sdkconfig.defaults`에 설정을 추가하세요.

```conf
# LVGL 설정
CONFIG_LV_FONT_FMT_TXT_LARGE=y
CONFIG_LV_USE_FONT_COMPRESSED=y
CONFIG_LV_BUILD_EXAMPLES=n
CONFIG_LV_BUILD_DEMOS=n
CONFIG_LV_USE_LOG=y
CONFIG_LV_LOG_LEVEL_INFO=y
CONFIG_LV_LOG_PRINTF=y
CONFIG_LV_FONT_MONTSERRAT_20=y

# 안정성 우선: double buffer 비활성화
CONFIG_BSP_LCD_DRAW_BUF_DOUBLE=n
```

---

### 2-3. display wrapper 작성

`main/app_display.h`에 아래 내용을 추가하세요.

```cpp
#pragma once

#include <stdint.h>

void app_display_init();
bool app_display_lock(uint32_t timeout_ms);
void app_display_unlock();
void display_set_backlight(uint8_t percent);
```

`main/app_display.cpp`는 아래와 같습니다.

```cpp
#include "app_display.h"

#include "bsp/m5stack_tab5.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "TABWRITE_DISPLAY";

void display_set_backlight(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  ESP_ERROR_CHECK(bsp_display_brightness_set(percent));
  ESP_LOGI(TAG, "Backlight set to %u%%", percent);
}

bool app_display_lock(uint32_t timeout_ms) {
  return bsp_display_lock(timeout_ms);
}

void app_display_unlock() { bsp_display_unlock(); }

void app_display_init() {
  ESP_LOGI(TAG, "Initializing Tab5 display via ESP-BSP...");

  if (bsp_display_start() == NULL) {
    ESP_LOGE(TAG, "BSP display initialization failed");
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  display_set_backlight(50);
  ESP_LOGI(TAG, "Display initialized");
}
```

- `bsp_display_start()`가 LVGL port, display driver, touch 초기화를 담당한다.
- LVGL API를 호출할 때는 `app_display_lock()`을 사용한다.

---

### 2-4. 테마 작성

`main/theme.h`에 아래 내용을 추가하세요.

```cpp
#pragma once

#include "lvgl.h"

#define THEME_BG_PRIMARY lv_color_hex(0x1E1E2E)
#define THEME_BG_SECONDARY lv_color_hex(0x282A36)
#define THEME_TEXT_PRIMARY lv_color_hex(0xF8F8F2)
#define THEME_TEXT_SECONDARY lv_color_hex(0x6272A4)
#define THEME_ACCENT lv_color_hex(0xBD93F9)
```

---

### 2-5. Splash UI 작성

`main/app_ui.h`에 아래 내용을 추가하세요.

```cpp
#pragma once

void app_ui_show_splash();
```

`main/app_ui.cpp`:

```cpp
#include "app_ui.h"
#include "lvgl.h"
#include "theme.h"

void app_ui_show_splash() {
  if (lv_display_get_default() == NULL) {
    return;
  }

  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screen, THEME_BG_PRIMARY, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "TABWRITE");
  lv_obj_set_style_text_color(title, THEME_TEXT_PRIMARY, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t *tagline = lv_label_create(screen);
  lv_label_set_text(tagline, "Open. Type. Your words are safe.");
  lv_obj_set_style_text_color(tagline, THEME_TEXT_SECONDARY, 0);
  lv_obj_set_style_text_font(tagline, &lv_font_montserrat_14, 0);
  lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 20);

  lv_scr_load(screen);
}
```

- 배경은 `LV_OPA_COVER`로 설정한다. 반투명 배경은 ghosting을 만들 수 있다.
- `lv_font_montserrat_20`을 쓰므로 `CONFIG_LV_FONT_MONTSERRAT_20=y`가 필요하다.

---

### 2-6. app_main에 display와 splash 연결

`app_main.cpp`에서는 display를 큐/task 생성 전에 초기화한다.

```cpp
#include "app_display.h"
#include "app_ui.h"

app_display_init();
app_queue_init();

xTaskCreate(ui_task, "ui_task", 8192, NULL, 5, NULL);
xTaskCreate(storage_task, "storage_task", 4096, NULL, 4, NULL);
```

`ui_task()` 시작 부분은 아래와 같습니다.

```cpp
ESP_LOGI("TABWRITE_UI", "UI Task started");

if (app_display_lock(0)) {
  app_ui_show_splash();
  app_display_unlock();
}
```

---

### 2-7. 빌드

```sh
(venv) idf.py build
(venv) idf.py flash monitor

# ...
Project name:     TABWRITE
TABWRITE_MAIN: TABWRITE Firmware Booting...
TABWRITE_DISPLAY: Initializing Tab5 display via ESP-BSP...
M5Stack Tab5: MIPI DSI PHY Powered on
M5Stack Tab5: Display initialized with resolution 720x1280
M5Stack Tab5: Setting LCD backlight: on
TABWRITE_DISPLAY: Backlight set to 50%
TABWRITE_DISPLAY: Display initialized
TABWRITE_UI: UI Task started
TABWRITE_STORAGE: Storage Task started
```

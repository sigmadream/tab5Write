# TABWRITE 화면 캡처 도구

개발 기록용으로 펌웨어의 LVGL 화면을 ESP-IDF serial console로 덤프하고 PC에서 PNG로 저장한다.

## 펌웨어 트리거

1. 펌웨어를 빌드/플래시한다.
2. PC에서 캡처 스크립트를 실행한다. 기본값은 serial로 `snapshot\n` 명령을 보내 캡처를 시작한다.
3. 또는 `--no-trigger`로 스크립트를 실행한 뒤 TABWRITE에 연결된 USB 키보드에서 `Ctrl+Shift+P`를 눌러 수동 캡처할 수 있다.
4. 펌웨어가 현재 active LVGL screen을 RGB565 snapshot으로 캡처하고 RLE16으로 압축해 serial로 전송한다.

## PC 저장

```bash
python3 tools/capture_lvgl_snapshot.py \
  --port /dev/cu.usbmodem11301 \
  --baud 921600 \
  --output-dir captures/phase5
```

수동 키보드 트리거를 쓰려면 `--no-trigger`를 추가한다. 스크립트는 third-party Python 패키지 없이 POSIX `termios`와 표준 라이브러리만 사용한다. macOS에서는 `termios`에 없는 921600 baud도 `IOSSIOSPEED`로 설정한다. 출력 파일명을 지정하려면 `--output captures/phase5/editor.png`를 사용한다.

## 프레임 형식

펌웨어는 다음 ASCII 헤더, binary payload, ASCII 종료 마커를 보낸다.

```text
TABWRITE_SNAPSHOT_BEGIN v=1 name=active_screen format=RGB565_LE encoding=rle16 width=1280 height=720 stride=2560 raw_bytes=1843200 encoded_bytes=... crc32=...
<rle16 payload>
TABWRITE_SNAPSHOT_END name=active_screen
```

수신 스크립트는 payload를 정확히 `encoded_bytes`만큼 읽고, RLE16을 raw RGB565로 복원한 뒤 CRC32를 검증하고 PNG로 변환한다.

## 주의

- `CONFIG_LV_USE_SNAPSHOT=y`가 필요하다. 새 설정은 `sdkconfig.defaults`에 기록되어 있다.
- 기존 로컬 `sdkconfig`가 있으면 `idf.py menuconfig` 또는 `sdkconfig` 재생성으로 snapshot/monitor baud 옵션을 반영해야 한다.
- `sdkconfig.defaults`는 캡처 시간을 줄이기 위해 monitor baud를 921600으로 둔다. 모니터/캡처 스크립트도 같은 baud로 열어야 한다. ESP32-P4 USB Serial/JTAG 경로에서는 실제 USB 전송이라 baud 값이 논리 설정에 가깝지만, 도구 설정은 맞춰 두는 편이 안전하다.
- 전체 1280x720 화면 raw 크기는 약 1.8MB다. RLE16으로 압축되지만 화면 내용에 따라 전송 시간이 달라진다.

# Repository Guidelines

## Project Structure & Module Organization
This is an ESP-IDF firmware project for `TABWRITE` targeting `esp32p4`. The root `CMakeLists.txt` declares the project; `sdkconfig.defaults` stores default target, PSRAM, flash, FATFS, FreeRTOS, partition, and LVGL settings. Application code lives in `main/`:

- `app_main.cpp` initializes NVS, display, queues, and FreeRTOS tasks.
- `app_display.*` contains display, LVGL port, and backlight setup.
- `app_ui.*` builds LVGL screens.
- `app_event.h`, `app_queue.h`, `theme.h`, and `app_build_info.h` hold shared types and constants.
- `main/idf_component.yml` pins ESP-IDF component dependencies, including LVGL.

Generated build output belongs in `build/` and must not be committed.

## Build, Test, and Development Commands
Run commands from the repository root after activating ESP-IDF with `idf-venv`.

- `idf-venv` — activate the ESP-IDF virtual environment before any `idf.py` command.
- `idf.py set-target esp32p4` — select the configured chip target.
- `idf.py build` — configure CMake, fetch managed components, and compile firmware.
- `idf.py flash monitor` — flash the connected device and open serial logs.
- `idf.py menuconfig` — inspect or change SDK options; persist intentional defaults in `sdkconfig.defaults`.
- `idf.py clean` — remove build artifacts when configuration changes are stale.

## Coding Style & Naming Conventions
Use C++ source files (`.cpp`) with lightweight headers (`.h`) in `main/`. Follow the existing two-space indentation and keep braces on the same line. Prefer `snake_case` for functions and variables (`app_queue_init`, `display_set_backlight`) and `UPPER_SNAKE_CASE` for macros or log tags. Keep module prefixes (`app_`, `display_`) to make ownership obvious. Use ESP-IDF error handling macros such as `ESP_ERROR_CHECK` and LVGL APIs consistently. When adding hardware, display, input, or board-support code, consult `ref/esp-bsp/` first and mirror its ESP-BSP patterns where applicable.

## Testing Guidelines
There is no dedicated test suite yet. For every change, at minimum run `idf.py build`. Add new tests under `test/` when introducing pure logic or regressions; name files `test_<module>.cpp` and keep hardware-dependent checks documented as manual verification steps. Include serial-log evidence for device behavior changes.

## Commit & Pull Request Guidelines
Git history is currently minimal (`init`, `// wip`), so prefer clearer future commits: imperative intent line, concise rationale, and relevant Lore trailers such as `Tested:` and `Not-tested:`. Pull requests should detabwrite the firmware behavior changed, list build/test results, link issues, and include screenshots or serial logs for UI/display changes.

## Security & Configuration Tips
Do not commit `sdkconfig`, secrets, local `.omx/` state, or generated binaries. Keep dependency changes in `main/idf_component.yml` and `dependencies.lock` reviewable.

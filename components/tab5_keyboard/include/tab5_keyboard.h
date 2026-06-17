#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tabwrite {

static constexpr uint8_t TAB5_KEYBOARD_DEFAULT_ADDR = 0x6D;
static constexpr gpio_num_t TAB5_KEYBOARD_DEFAULT_SDA = GPIO_NUM_0;
static constexpr gpio_num_t TAB5_KEYBOARD_DEFAULT_SCL = GPIO_NUM_1;
static constexpr gpio_num_t TAB5_KEYBOARD_DEFAULT_INT = GPIO_NUM_50;
static constexpr uint32_t TAB5_KEYBOARD_DEFAULT_I2C_HZ = 400000;
static constexpr uint32_t TAB5_KEYBOARD_DEFAULT_POLL_MS = 20;

struct Tab5KeyboardConfig {
  i2c_port_num_t port = I2C_NUM_0;
  uint8_t address = TAB5_KEYBOARD_DEFAULT_ADDR;
  gpio_num_t sda = TAB5_KEYBOARD_DEFAULT_SDA;
  gpio_num_t scl = TAB5_KEYBOARD_DEFAULT_SCL;
  gpio_num_t interrupt = TAB5_KEYBOARD_DEFAULT_INT;
  uint32_t clock_hz = TAB5_KEYBOARD_DEFAULT_I2C_HZ;
  uint32_t poll_interval_ms = TAB5_KEYBOARD_DEFAULT_POLL_MS;
  uint8_t max_consecutive_errors = 10;
};

struct Tab5KeyboardHidEvent {
  uint8_t modifier;
  uint8_t keycode;
};

using Tab5KeyboardHidCallback = void (*)(const Tab5KeyboardHidEvent &event, void *user_data);

class Tab5Keyboard {
public:
  Tab5Keyboard() = default;
  ~Tab5Keyboard();

  esp_err_t begin(const Tab5KeyboardConfig &config = {});
  void end();

  bool is_initialized() const { return initialized_; }
  esp_err_t last_error() const { return last_error_; }
  esp_err_t set_hid_callback(Tab5KeyboardHidCallback callback, void *user_data);
  esp_err_t read_version(uint8_t *version);

private:
  static void task_entry(void *arg);

  esp_err_t write_reg(uint8_t reg, uint8_t value);
  esp_err_t read_reg(uint8_t reg, uint8_t *value);
  esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len);
  esp_err_t configure_device();
  esp_err_t poll_once();
  void record_poll_result(esp_err_t err);

  Tab5KeyboardConfig config_ = {};
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  TaskHandle_t task_ = nullptr;
  Tab5KeyboardHidCallback callback_ = nullptr;
  void *callback_user_data_ = nullptr;
  esp_err_t last_error_ = ESP_OK;
  uint8_t consecutive_errors_ = 0;
  volatile bool initialized_ = false;
};

} // namespace tabwrite

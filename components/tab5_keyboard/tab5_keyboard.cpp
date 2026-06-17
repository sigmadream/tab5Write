#include "tab5_keyboard.h"

#include <algorithm>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tabwrite {
namespace {

static const char *TAG = "TAB5_KEYBOARD";

constexpr uint8_t REG_INT_CFG = 0x00;
constexpr uint8_t REG_INT_STA = 0x01;
constexpr uint8_t REG_EVENT_NUM = 0x02;
constexpr uint8_t REG_BRIGHTNESS = 0x03;
constexpr uint8_t REG_KEYBOARD_MODE = 0x10;
constexpr uint8_t REG_RGB_MODE = 0x11;
constexpr uint8_t REG_HID_EVENT = 0x30;
constexpr uint8_t REG_VERSION = 0xFE;

constexpr uint8_t MODE_HID = 1;
constexpr uint8_t RGB_MODE_BINDING = 0;
constexpr uint8_t INT_STATUS_HID = 0x02;
constexpr uint8_t INT_CFG_HID = 0x02;
constexpr uint8_t MAX_EVENTS_PER_POLL = 32;
constexpr uint8_t DEFAULT_BRIGHTNESS = 40;

} // namespace

Tab5Keyboard::~Tab5Keyboard() {
  end();
}

esp_err_t Tab5Keyboard::begin(const Tab5KeyboardConfig &config) {
  end();
  config_ = config;
  if (config_.poll_interval_ms == 0) {
    config_.poll_interval_ms = TAB5_KEYBOARD_DEFAULT_POLL_MS;
  }
  if (config_.clock_hz == 0) {
    config_.clock_hz = TAB5_KEYBOARD_DEFAULT_I2C_HZ;
  }
  if (config_.max_consecutive_errors == 0) {
    config_.max_consecutive_errors = 10;
  }
  consecutive_errors_ = 0;
  last_error_ = ESP_OK;

  i2c_master_bus_config_t bus_config = {};
  bus_config.i2c_port = config_.port;
  bus_config.sda_io_num = config_.sda;
  bus_config.scl_io_num = config_.scl;
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&bus_config, &bus_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to create I2C bus port=%d sda=%d scl=%d: %s",
             static_cast<int>(config_.port), static_cast<int>(config_.sda),
             static_cast<int>(config_.scl), esp_err_to_name(err));
    end();
    return err;
  }

  i2c_device_config_t device_config = {};
  device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device_config.device_address = config_.address;
  device_config.scl_speed_hz = config_.clock_hz;

  err = i2c_master_bus_add_device(bus_, &device_config, &device_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to add keyboard I2C device addr=0x%02x: %s",
             config_.address, esp_err_to_name(err));
    end();
    return err;
  }

  err = i2c_master_probe(bus_, config_.address, 50);
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "Tab5 Keyboard not detected at addr=0x%02x: %s",
             config_.address, esp_err_to_name(err));
    end();
    return err;
  }

  if (config_.interrupt >= 0) {
    gpio_config_t int_gpio_config = {};
    int_gpio_config.mode = GPIO_MODE_INPUT;
    int_gpio_config.pin_bit_mask = 1ULL << config_.interrupt;
    int_gpio_config.pull_up_en = GPIO_PULLUP_ENABLE;
    int_gpio_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    int_gpio_config.intr_type = GPIO_INTR_DISABLE;
    err = gpio_config(&int_gpio_config);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to configure keyboard INT pin %d: %s",
               static_cast<int>(config_.interrupt), esp_err_to_name(err));
      end();
      return err;
    }
  }

  err = configure_device();
  if (err != ESP_OK) {
    end();
    return err;
  }

  initialized_ = true;

  BaseType_t created = xTaskCreate(task_entry, "tab5_keyboard", 4096, this, 4, &task_);
  if (created != pdPASS) {
    ESP_LOGW(TAG, "Failed to create keyboard polling task");
    end();
    return ESP_ERR_NO_MEM;
  }

  uint8_t version = 0;
  if (read_version(&version) == ESP_OK) {
    ESP_LOGI(TAG, "Tab5 Keyboard initialized: addr=0x%02x fw=0x%02x mode=HID",
             config_.address, version);
  } else {
    ESP_LOGI(TAG, "Tab5 Keyboard initialized: addr=0x%02x mode=HID", config_.address);
  }
  return ESP_OK;
}

void Tab5Keyboard::end() {
  initialized_ = false;

  if (task_ != nullptr) {
    TaskHandle_t task = task_;
    task_ = nullptr;
    if (task != xTaskGetCurrentTaskHandle()) {
      vTaskDelete(task);
    }
  }

  if (device_ != nullptr) {
    i2c_master_bus_rm_device(device_);
    device_ = nullptr;
  }

  if (bus_ != nullptr) {
    i2c_del_master_bus(bus_);
    bus_ = nullptr;
  }
}

esp_err_t Tab5Keyboard::set_hid_callback(Tab5KeyboardHidCallback callback, void *user_data) {
  callback_ = callback;
  callback_user_data_ = user_data;
  return ESP_OK;
}

esp_err_t Tab5Keyboard::read_version(uint8_t *version) {
  if (version == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  return read_reg(REG_VERSION, version);
}

void Tab5Keyboard::task_entry(void *arg) {
  auto *keyboard = static_cast<Tab5Keyboard *>(arg);
  while (keyboard != nullptr && keyboard->initialized_) {
    keyboard->record_poll_result(keyboard->poll_once());
    vTaskDelay(pdMS_TO_TICKS(keyboard->config_.poll_interval_ms));
  }
  if (keyboard != nullptr && keyboard->task_ == xTaskGetCurrentTaskHandle()) {
    keyboard->task_ = nullptr;
  }
  vTaskDelete(nullptr);
}

esp_err_t Tab5Keyboard::write_reg(uint8_t reg, uint8_t value) {
  if (device_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  uint8_t payload[2] = {reg, value};
  return i2c_master_transmit(device_, payload, sizeof(payload), 50);
}

esp_err_t Tab5Keyboard::read_reg(uint8_t reg, uint8_t *value) {
  if (value == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  return read_bytes(reg, value, 1);
}

esp_err_t Tab5Keyboard::read_bytes(uint8_t reg, uint8_t *data, size_t len) {
  if (device_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  if (data == nullptr || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  return i2c_master_transmit_receive(device_, &reg, 1, data, len, 50);
}

esp_err_t Tab5Keyboard::configure_device() {
  esp_err_t err = write_reg(REG_KEYBOARD_MODE, MODE_HID);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to set HID mode: %s", esp_err_to_name(err));
    return err;
  }

  // Keep the keyboard's built-in mode/Caps indicators active and readable.
  (void)write_reg(REG_RGB_MODE, RGB_MODE_BINDING);
  (void)write_reg(REG_BRIGHTNESS, DEFAULT_BRIGHTNESS);

  // Enable HID-mode interrupt/status generation, clear stale events/status.
  (void)write_reg(REG_EVENT_NUM, 0);
  (void)write_reg(REG_INT_STA, 0);
  err = write_reg(REG_INT_CFG, INT_CFG_HID);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to configure HID interrupt bit: %s", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

esp_err_t Tab5Keyboard::poll_once() {
  uint8_t status = 0;
  esp_err_t err = read_reg(REG_INT_STA, &status);
  if (err != ESP_OK) {
    return err;
  }
  if ((status & INT_STATUS_HID) == 0) {
    return ESP_OK;
  }

  uint8_t count = 0;
  err = read_reg(REG_EVENT_NUM, &count);
  if (err != ESP_OK || count == 0) {
    (void)write_reg(REG_INT_STA, 0);
    return err;
  }

  const uint8_t events_to_read = std::min<uint8_t>(count, MAX_EVENTS_PER_POLL);
  for (uint8_t i = 0; i < events_to_read; ++i) {
    uint8_t raw[2] = {};
    err = read_bytes(REG_HID_EVENT, raw, sizeof(raw));
    if (err != ESP_OK) {
      (void)write_reg(REG_INT_STA, 0);
      return err;
    }
    if (raw[0] == 0xFF && raw[1] == 0xFF) {
      continue;
    }
    if (callback_ != nullptr) {
      Tab5KeyboardHidEvent event = {.modifier = raw[0], .keycode = raw[1]};
      callback_(event, callback_user_data_);
    }
  }

  (void)write_reg(REG_INT_STA, 0);
  return ESP_OK;
}

void Tab5Keyboard::record_poll_result(esp_err_t err) {
  last_error_ = err;
  if (err == ESP_OK) {
    consecutive_errors_ = 0;
    return;
  }

  if (consecutive_errors_ < UINT8_MAX) {
    ++consecutive_errors_;
  }

  if (consecutive_errors_ >= config_.max_consecutive_errors) {
    ESP_LOGW(TAG, "Tab5 Keyboard communication lost after %u consecutive errors: %s",
             static_cast<unsigned>(consecutive_errors_), esp_err_to_name(err));
    initialized_ = false;
  }
}

} // namespace tabwrite

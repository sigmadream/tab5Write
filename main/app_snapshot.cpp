#include "app_snapshot.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"

static const char *TAG = "TABWRITE_SNAPSHOT";

namespace {

constexpr uint32_t kCrc32Polynomial = 0xEDB88320U;


void snapshot_write(const void *data, size_t len) {
  if (len == 0) {
    return;
  }
  if (usb_serial_jtag_is_driver_installed()) {
    const uint8_t *ptr = static_cast<const uint8_t *>(data);
    size_t remaining = len;
    while (remaining > 0) {
      const int written = usb_serial_jtag_write_bytes(ptr, remaining, pdMS_TO_TICKS(1000));
      if (written <= 0) {
        break;
      }
      ptr += written;
      remaining -= static_cast<size_t>(written);
    }
    return;
  }
  fwrite(data, 1, len, stdout);
}

void snapshot_flush() {
  if (usb_serial_jtag_is_driver_installed()) {
    usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(5000));
    return;
  }
  fflush(stdout);
}

void snapshot_printf(const char *fmt, ...) {
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  const int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  if (len <= 0) {
    return;
  }
  const size_t write_len = static_cast<size_t>(len) < sizeof(buffer) ? static_cast<size_t>(len) : sizeof(buffer) - 1;
  snapshot_write(buffer, write_len);
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (kCrc32Polynomial & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

size_t rle16_encoded_size(const uint8_t *data, size_t raw_size) {
  const size_t words = raw_size / 2;
  size_t encoded = 0;
  size_t i = 0;

  while (i < words) {
    size_t run = 1;
    while (i + run < words && run < 128 &&
           data[(i + run) * 2] == data[i * 2] &&
           data[(i + run) * 2 + 1] == data[i * 2 + 1]) {
      ++run;
    }

    if (run >= 4) {
      encoded += 3;
      i += run;
      continue;
    }

    size_t literal = 0;
    while (i + literal < words && literal < 128) {
      run = 1;
      while (i + literal + run < words && run < 128 &&
             data[(i + literal + run) * 2] == data[(i + literal) * 2] &&
             data[(i + literal + run) * 2 + 1] == data[(i + literal) * 2 + 1]) {
        ++run;
      }
      if (run >= 4) {
        break;
      }
      ++literal;
    }
    encoded += 1 + literal * 2;
    i += literal;
  }

  if ((raw_size & 1U) != 0U) {
    encoded += 2;
  }
  return encoded;
}

void write_rle16(const uint8_t *data, size_t raw_size) {
  const size_t words = raw_size / 2;
  size_t i = 0;

  while (i < words) {
    size_t run = 1;
    while (i + run < words && run < 128 &&
           data[(i + run) * 2] == data[i * 2] &&
           data[(i + run) * 2 + 1] == data[i * 2 + 1]) {
      ++run;
    }

    if (run >= 4) {
      const uint8_t token = static_cast<uint8_t>(0x80U | (run - 1));
      snapshot_write(&token, 1);
      snapshot_write(data + i * 2, 2);
      i += run;
      continue;
    }

    const size_t literal_start = i;
    size_t literal = 0;
    while (i + literal < words && literal < 128) {
      run = 1;
      while (i + literal + run < words && run < 128 &&
             data[(i + literal + run) * 2] == data[(i + literal) * 2] &&
             data[(i + literal + run) * 2 + 1] == data[(i + literal) * 2 + 1]) {
        ++run;
      }
      if (run >= 4) {
        break;
      }
      ++literal;
    }

    const uint8_t token = static_cast<uint8_t>(literal - 1);
    snapshot_write(&token, 1);
    snapshot_write(data + literal_start * 2, literal * 2);
    i += literal;
  }

  if ((raw_size & 1U) != 0U) {
    const uint8_t token = 0;
    snapshot_write(&token, 1);
    snapshot_write(data + raw_size - 1, 1);
    const uint8_t pad = 0;
    snapshot_write(&pad, 1);
  }
}

} // namespace

void app_snapshot_dump_obj_over_serial(lv_obj_t *obj, const char *name) {
  if (obj == nullptr) {
    obj = lv_scr_act();
  }
  if (name == nullptr || name[0] == '\0') {
    name = "active_screen";
  }

#if LV_USE_SNAPSHOT
  ESP_LOGI(TAG, "Taking LVGL snapshot for %s", name);
  lv_obj_update_layout(obj);
  const uint32_t requested_width = static_cast<uint32_t>(lv_obj_get_width(obj));
  const uint32_t requested_height = static_cast<uint32_t>(lv_obj_get_height(obj));
  const uint32_t requested_stride = lv_draw_buf_width_to_stride(requested_width, LV_COLOR_FORMAT_RGB565);
  const size_t requested_size = static_cast<size_t>(requested_stride) * requested_height;
  uint8_t *snapshot_data = static_cast<uint8_t *>(heap_caps_malloc(requested_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (snapshot_data == nullptr) {
    ESP_LOGE(TAG, "Snapshot buffer allocation failed: %lu bytes", static_cast<unsigned long>(requested_size));
    snapshot_printf("\nTABWRITE_SNAPSHOT_ERROR reason=buffer_alloc_failed bytes=%lu name=%s\n",
                    static_cast<unsigned long>(requested_size), name);
    snapshot_flush();
    return;
  }

  lv_draw_buf_t snapshot;
  if (lv_draw_buf_init(&snapshot, requested_width, requested_height, LV_COLOR_FORMAT_RGB565,
                       requested_stride, snapshot_data, requested_size) != LV_RESULT_OK ||
      lv_snapshot_take_to_draw_buf(obj, LV_COLOR_FORMAT_RGB565, &snapshot) != LV_RESULT_OK) {
    ESP_LOGE(TAG, "Snapshot failed");
    heap_caps_free(snapshot_data);
    snapshot_printf("\nTABWRITE_SNAPSHOT_ERROR reason=snapshot_failed name=%s\n", name);
    snapshot_flush();
    return;
  }

  const uint32_t width = snapshot.header.w;
  const uint32_t height = snapshot.header.h;
  const uint32_t stride = snapshot.header.stride;
  const size_t raw_size = static_cast<size_t>(stride) * height;
  const uint32_t crc = crc32_update(0, static_cast<const uint8_t *>(snapshot.data), raw_size);
  const size_t encoded_size = rle16_encoded_size(static_cast<const uint8_t *>(snapshot.data), raw_size);

  snapshot_printf("\nTABWRITE_SNAPSHOT_BEGIN v=1 name=%s format=RGB565_LE encoding=rle16 width=%lu height=%lu stride=%lu raw_bytes=%lu encoded_bytes=%lu crc32=%08lx\n",
         name,
         static_cast<unsigned long>(width),
         static_cast<unsigned long>(height),
         static_cast<unsigned long>(stride),
         static_cast<unsigned long>(raw_size),
         static_cast<unsigned long>(encoded_size),
         static_cast<unsigned long>(crc));
  snapshot_flush();
  write_rle16(static_cast<const uint8_t *>(snapshot.data), raw_size);
  snapshot_flush();
  snapshot_printf("\nTABWRITE_SNAPSHOT_END name=%s\n", name);
  snapshot_flush();

  ESP_LOGI(TAG, "Snapshot sent: %lux%lu raw=%lu encoded=%lu crc32=%08lx",
           static_cast<unsigned long>(width),
           static_cast<unsigned long>(height),
           static_cast<unsigned long>(raw_size),
           static_cast<unsigned long>(encoded_size),
           static_cast<unsigned long>(crc));
  heap_caps_free(snapshot_data);
#else
  ESP_LOGE(TAG, "LVGL snapshot support is disabled");
  snapshot_printf("\nTABWRITE_SNAPSHOT_ERROR reason=CONFIG_LV_USE_SNAPSHOT_DISABLED name=%s\n", name);
  snapshot_flush();
#endif
}

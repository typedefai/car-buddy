#include "display/oled_display.h"

#include <cstdint>

#include <esp_err.h>
#include <esp_lcd_panel_ssd1306.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <themes/mono/lv_theme_mono.h>

#include "device.h"

namespace {

constexpr char kTag[] = "oled";
constexpr int kWidth = 128;
constexpr int kHeight = 64;
constexpr int kPageCount = kHeight / 8;

// LVGL reserves 2 x 4 bytes at the start of an I1 buffer for the palette.
constexpr size_t kPaletteSize = 8;

uint8_t s_draw_buffer[kWidth * kHeight / 8 + kPaletteSize];

// SSD1306 page format: each byte holds 8 vertical pixels.
uint8_t s_oled_buffer[kWidth * kPageCount];

}  // namespace

OledDisplay::~OledDisplay() {
  if (display_ != nullptr) {
    lv_display_delete(display_);
  }
  if (panel_handle_ != nullptr) {
    esp_lcd_panel_del(panel_handle_);
  }
  if (io_handle_ != nullptr) {
    esp_lcd_panel_io_del(io_handle_);
  }
  if (i2c_bus_ != nullptr) {
    i2c_del_master_bus(i2c_bus_);
  }
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
  }
}

void OledDisplay::initI2c() {
  i2c_master_bus_config_t bus_config = {};
  bus_config.i2c_port = -1;  // auto-select a free I2C controller
  bus_config.sda_io_num = static_cast<gpio_num_t>(OLED_I2C_SDA_PIN);
  bus_config.scl_io_num = static_cast<gpio_num_t>(OLED_I2C_SCL_PIN);
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = 1;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_));

  esp_lcd_panel_io_i2c_config_t io_config = {};
  io_config.dev_addr = OLED_I2C_ADDR;
  io_config.scl_speed_hz = 400 * 1000;
  io_config.control_phase_bytes = 1;
  io_config.dc_bit_offset = 6;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  io_config.transaction_timeout_ms = 1000;
  io_config.flags.dc_low_on_data = 0;
  io_config.flags.disable_control_phase = 0;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &io_handle_));
}

void OledDisplay::initPanel() {
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = GPIO_NUM_NC;
  panel_config.bits_per_pixel = 1;

  esp_lcd_panel_ssd1306_config_t ssd1306_config = {};
  ssd1306_config.height = static_cast<uint8_t>(kHeight);
  panel_config.vendor_config = &ssd1306_config;

  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ssd1306(io_handle_, &panel_config, &panel_handle_));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_));

  // Same orientation the previous firmware got with A1h/C8h.
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle_, true, true));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_, true));
}

bool OledDisplay::onColorTransDone(esp_lcd_panel_io_handle_t io,
                                   esp_lcd_panel_io_event_data_t* edata,
                                   void* user_ctx) {
  (void)io;
  (void)edata;
  lv_display_flush_ready(static_cast<lv_display_t*>(user_ctx));
  return false;
}

void OledDisplay::flushCallback(lv_display_t* disp, const lv_area_t* area,
                                uint8_t* px_map) {
  (void)area;  // full-refresh mode: the buffer always holds the whole frame

  auto* panel = static_cast<esp_lcd_panel_handle_t>(
      lv_display_get_user_data(disp));
  const int hor_res = lv_display_get_physical_horizontal_resolution(disp);

  // Skip the LVGL palette; I1 pixels are packed MSB-first, row-major.
  px_map += kPaletteSize;

  for (int page = 0; page < kPageCount; ++page) {
    for (int col = 0; col < kWidth; ++col) {
      uint8_t page_byte = 0;
      for (int bit = 0; bit < 8; ++bit) {
        const int y = page * 8 + bit;
        const size_t px_offset =
            static_cast<size_t>(y) * hor_res + static_cast<size_t>(col);
        const size_t byte_offset = px_offset / 8;
        const size_t bit_offset = 7 - (px_offset % 8);

        // LVGL I1: 1 = light. SSD1306: 1 = pixel on (lit).
        // Invert so the mono theme with dark_bg=false renders as white text
        // on a black OLED.
        const bool pixel_on =
            ((px_map[byte_offset] >> bit_offset) & 0x01) == 0;
        if (pixel_on) {
          page_byte |= static_cast<uint8_t>(1U << bit);
        }
      }
      s_oled_buffer[page * kWidth + col] = page_byte;
    }
  }

  const esp_err_t err = esp_lcd_panel_draw_bitmap(
      panel, 0, 0, kWidth, kHeight, s_oled_buffer);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "draw bitmap failed: %s", esp_err_to_name(err));
    lv_display_flush_ready(disp);
  }
}

void OledDisplay::init() {
  mutex_ = xSemaphoreCreateMutex();
  configASSERT(mutex_ != nullptr);

  ESP_LOGI(kTag, "I2C SDA=%d SCL=%d addr=0x%02X", OLED_I2C_SDA_PIN,
           OLED_I2C_SCL_PIN, OLED_I2C_ADDR);
  initI2c();
  initPanel();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
  });

  display_ = lv_display_create(kWidth, kHeight);
  lv_display_set_user_data(display_, panel_handle_);
  lv_display_set_color_format(display_, LV_COLOR_FORMAT_I1);
  lv_display_set_buffers(display_, s_draw_buffer, nullptr,
                         sizeof(s_draw_buffer), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display_, flushCallback);

  lv_theme_t* theme = lv_theme_mono_init(display_, false, LV_FONT_DEFAULT);
  lv_display_set_theme(display_, theme);

  esp_lcd_panel_io_callbacks_t callbacks = {};
  callbacks.on_color_trans_done = onColorTransDone;
  ESP_ERROR_CHECK(
      esp_lcd_panel_io_register_event_callbacks(io_handle_, &callbacks,
                                                display_));

  lv_obj_t* screen = lv_obj_create(nullptr);
  label_ = lv_label_create(screen);
  lv_label_set_text(label_, "");
  lv_obj_center(label_);
  lv_scr_load(screen);

  ESP_LOGI(kTag, "SSD1306 ready");
}

void OledDisplay::showText(const char* text) {
  if (mutex_ == nullptr ||
      xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return;
  }
  if (label_ != nullptr) {
    lv_label_set_text(label_, text);
    lv_obj_center(label_);
  }
  xSemaphoreGive(mutex_);
}

uint32_t OledDisplay::process() {
  if (mutex_ == nullptr ||
      xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
    return 1000;
  }
  const uint32_t next = lv_timer_handler();
  xSemaphoreGive(mutex_);
  return next;
}

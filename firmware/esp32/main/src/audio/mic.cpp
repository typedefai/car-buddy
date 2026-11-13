#include "audio/mic.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "device.h"

namespace {

constexpr char kTag[] = "mic";
constexpr size_t kReadChunkFrames = 128;

}  // namespace

Mic::~Mic() {
  if (rx_chan_ != nullptr) {
    if (started_) {
      i2s_channel_disable(rx_chan_);
    }
    i2s_del_channel(rx_chan_);
  }
}

void Mic::init() {
  if (rx_chan_ != nullptr) {
    return;
  }

  // L/R is a static strap input on the microphone, not a clock. Read it as an
  // input with a pull-down: low/left (also the safe default if floating),
  // high/right if the module is strapped to 3.3 V.
  gpio_config_t lr_cfg = {};
  lr_cfg.pin_bit_mask = 1ULL << MIC_LR_SELECT_PIN;
  lr_cfg.mode = GPIO_MODE_INPUT;
  lr_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  lr_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
  lr_cfg.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&lr_cfg));
  vTaskDelay(pdMS_TO_TICKS(5));
  const int lr_level = gpio_get_level(static_cast<gpio_num_t>(MIC_LR_SELECT_PIN));
  const i2s_std_slot_mask_t slot_mask =
      lr_level ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_chan_));

  // INMP441/ICS-43434 outputs 24-bit samples in 32-bit slots. The L/R strap
  // selects whether the module drives the left or right slot.
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = static_cast<gpio_num_t>(MIC_I2S_BCLK),
              .ws = static_cast<gpio_num_t>(MIC_I2S_WS),
              .dout = I2S_GPIO_UNUSED,
              .din = static_cast<gpio_num_t>(MIC_I2S_DATA),
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  std_cfg.slot_cfg.slot_mask = slot_mask;
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan_, &std_cfg));

  ESP_LOGI(kTag,
           "INMP441 ready: %u Hz, 32-bit slots, L/R=%d (%s), WS=%d BCLK=%d SD=%d",
           static_cast<unsigned>(kSampleRateHz), lr_level,
           slot_mask == I2S_STD_SLOT_RIGHT ? "right" : "left", MIC_I2S_WS,
           MIC_I2S_BCLK, MIC_I2S_DATA);}

void Mic::start() {
  if (rx_chan_ == nullptr || started_) {
    return;
  }
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_));
  started_ = true;
  dc_ = 0;
}

void Mic::stop() {
  if (rx_chan_ == nullptr || !started_) {
    return;
  }
  i2s_channel_disable(rx_chan_);
  started_ = false;
}

size_t Mic::read(int16_t* out, size_t max_samples, uint32_t timeout_ms) {
  if (!started_ || out == nullptr || max_samples == 0) {
    return 0;
  }

  int32_t raw[kReadChunkFrames];
  size_t total = 0;
  int32_t peak = 0;
  int64_t sum = 0;

  while (total < max_samples) {
    size_t want_frames = max_samples - total;
    if (want_frames > kReadChunkFrames) {
      want_frames = kReadChunkFrames;
    }

    size_t bytes_read = 0;
    const esp_err_t err = i2s_channel_read(
        rx_chan_, raw, want_frames * sizeof(int32_t), &bytes_read,
        pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
      ESP_LOGE(kTag, "i2s read failed: %s", esp_err_to_name(err));
      break;
    }
    if (bytes_read == 0) {
      break;
    }

    const size_t frames = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < frames; ++i) {
      // INMP441/ICS-43434: valid 24 bits are left-justified.
      const int32_t x = raw[i] >> 16;
      const int32_t magnitude = x < 0 ? -x : x;
      if (magnitude > peak) {
        peak = magnitude;
      }
      sum += x;

      // One-pole DC blocker (~40 Hz corner at 16 kHz).
      dc_ += (x - dc_) / 64;
      int32_t sample = x - dc_;
      if (sample > INT16_MAX) {
        sample = INT16_MAX;
      } else if (sample < INT16_MIN) {
        sample = INT16_MIN;
      }
      out[total + i] = static_cast<int16_t>(sample);
    }
    total += frames;
  }

  const long avg =
      total ? static_cast<long>(sum / static_cast<int64_t>(total)) : 0;
  ESP_LOGI(kTag, "recorded %u samples, peak=%ld, avg=%ld",
           static_cast<unsigned>(total), static_cast<long>(peak), avg);
  return total;
}

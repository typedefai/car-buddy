#include "audio/mic.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "device.h"

namespace {

constexpr char kTag[] = "mic";
constexpr size_t kReadChunkSamples = 256;
// INMP441 has no AGC; a modest digital boost makes playback audible.
constexpr int32_t kPcm16Gain = 4;

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

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_chan_));

  // INMP441 outputs 24-bit samples in 32-bit slots. Read 32-bit words and
  // shift them down to PCM16 in read().
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
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan_, &std_cfg));

  ESP_LOGI(kTag, "INMP441 ready: %u Hz, 32-bit slots, WS=%d BCLK=%d SD=%d",
           static_cast<unsigned>(kSampleRateHz), MIC_I2S_WS, MIC_I2S_BCLK,
           MIC_I2S_DATA);
}

void Mic::start() {
  if (rx_chan_ == nullptr || started_) {
    return;
  }
  ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_));
  started_ = true;
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

  int32_t raw[kReadChunkSamples];
  size_t total = 0;

  while (total < max_samples) {
    size_t want = max_samples - total;
    if (want > kReadChunkSamples) {
      want = kReadChunkSamples;
    }

    size_t bytes_read = 0;
    const esp_err_t err = i2s_channel_read(
        rx_chan_, raw, want * sizeof(int32_t), &bytes_read,
        pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
      ESP_LOGE(kTag, "i2s read failed: %s", esp_err_to_name(err));
      break;
    }
    if (bytes_read == 0) {
      break;
    }

    const size_t samples = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < samples; ++i) {
      // INMP441: valid 24 bits are left-justified in the 32-bit slot.
      int32_t sample = (raw[i] >> 16) * kPcm16Gain;
      if (sample > INT16_MAX) {
        sample = INT16_MAX;
      } else if (sample < INT16_MIN) {
        sample = INT16_MIN;
      }
      out[total + i] = static_cast<int16_t>(sample);
    }
    total += samples;
  }

  ESP_LOGI(kTag, "recorded %u samples", static_cast<unsigned>(total));
  return total;
}

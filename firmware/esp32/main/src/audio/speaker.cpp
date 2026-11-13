#include "audio/speaker.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "device.h"

namespace {

constexpr char kTag[] = "speaker";
constexpr int kChunkFrames = 256;
constexpr size_t kFileBufferSize = 1024;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int16_t kAmplitude = 8000;

struct WavInfo {
  uint16_t audio_format = 0;
  uint16_t channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits_per_sample = 0;
  uint32_t data_offset = 0;
  uint32_t data_size = 0;
};

uint32_t readLe32(FILE* f) {
  uint8_t b[4] = {};
  if (fread(b, 1, sizeof(b), f) != sizeof(b)) {
    return 0;
  }
  return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
         (static_cast<uint32_t>(b[2]) << 16) |
         (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readLe16(FILE* f) {
  uint8_t b[2] = {};
  if (fread(b, 1, sizeof(b), f) != sizeof(b)) {
    return 0;
  }
  return static_cast<uint16_t>(b[0] | (b[1] << 8));
}

// Parses a RIFF/WAVE file up to the "data" chunk. PCM16 is checked by the
// caller; this parser only locates the format and payload.
bool parseWavHeader(FILE* f, WavInfo* info) {
  uint8_t riff[12] = {};
  if (fread(riff, 1, sizeof(riff), f) != sizeof(riff)) {
    return false;
  }
  if (std::memcmp(riff, "RIFF", 4) != 0 ||
      std::memcmp(riff + 8, "WAVE", 4) != 0) {
    return false;
  }

  bool found_fmt = false;
  while (true) {
    char chunk_id[4] = {};
    if (fread(chunk_id, 1, sizeof(chunk_id), f) != sizeof(chunk_id)) {
      return false;
    }
    const uint32_t chunk_size = readLe32(f);

    if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
      if (chunk_size < 16) {
        return false;
      }
      info->audio_format = readLe16(f);
      info->channels = readLe16(f);
      info->sample_rate = readLe32(f);
      // Skip byte rate and block align.
      if (fseek(f, 6, SEEK_CUR) != 0) {
        return false;
      }
      info->bits_per_sample = readLe16(f);
      if (chunk_size > 16 && fseek(f, chunk_size - 16, SEEK_CUR) != 0) {
        return false;
      }
      found_fmt = true;
    } else if (std::memcmp(chunk_id, "data", 4) == 0) {
      const long offset = ftell(f);
      if (offset < 0) {
        return false;
      }
      info->data_offset = static_cast<uint32_t>(offset);
      info->data_size = chunk_size;
      return found_fmt;
    } else {
      // Unknown chunk (LIST, fact, ...), skip it plus the pad byte.
      if (fseek(f, chunk_size + (chunk_size & 1U), SEEK_CUR) != 0) {
        return false;
      }
    }
  }
}

}  // namespace

Speaker::~Speaker() {
  if (tx_chan_ != nullptr) {
    i2s_channel_disable(tx_chan_);
    i2s_del_channel(tx_chan_);
  }
}

bool Speaker::configure(uint32_t sample_rate_hz, uint16_t channels) {
  if (sample_rate_hz == 0 || (channels != 1 && channels != 2)) {
    return false;
  }
  if (tx_chan_ != nullptr && sample_rate_hz == sample_rate_hz_ &&
      channels == channels_) {
    return true;
  }

  if (tx_chan_ != nullptr) {
    i2s_channel_disable(tx_chan_);
    i2s_del_channel(tx_chan_);
    tx_chan_ = nullptr;
  }

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  // Keep the link clocked but output zeros when the playback queue is empty.
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan_, nullptr));

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_hz),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT,
          channels == 2 ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = static_cast<gpio_num_t>(SPEAKER_I2S_BCLK),
              .ws = static_cast<gpio_num_t>(SPEAKER_I2S_LRC),
              .dout = static_cast<gpio_num_t>(SPEAKER_I2S_DOUT),
              .din = I2S_GPIO_UNUSED,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan_, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan_));

  sample_rate_hz_ = sample_rate_hz;
  channels_ = channels;
  ESP_LOGI(kTag, "I2S %u Hz, 16-bit %s, BCLK=%d LRC=%d DOUT=%d",
           static_cast<unsigned>(sample_rate_hz),
           channels == 2 ? "stereo" : "mono", SPEAKER_I2S_BCLK,
           SPEAKER_I2S_LRC, SPEAKER_I2S_DOUT);
  return true;
}

void Speaker::init() {
  if (tx_chan_ != nullptr) {
    return;
  }
  configure(kDefaultSampleRateHz, 2);
}

bool Speaker::writeBytes(const void* data, size_t bytes) {
  if (tx_chan_ == nullptr || data == nullptr || bytes == 0) {
    return false;
  }

  const auto* p = static_cast<const uint8_t*>(data);
  size_t remaining = bytes;
  while (remaining > 0) {
    size_t written = 0;
    const esp_err_t err =
        i2s_channel_write(tx_chan_, p, remaining, &written, portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGE(kTag, "i2s write failed: %s", esp_err_to_name(err));
      return false;
    }
    if (written == 0) {
      ESP_LOGW(kTag, "i2s wrote 0 bytes");
      return false;
    }
    p += written;
    remaining -= written;
  }
  return true;
}

bool Speaker::writePcm16(const int16_t* interleaved_stereo,
                         size_t frame_count) {
  return writeBytes(interleaved_stereo,
                    frame_count * 2 * sizeof(int16_t));
}

bool Speaker::playPcm16(const int16_t* samples, size_t sample_count,
                        uint16_t channels, uint32_t sample_rate_hz) {
  if (samples == nullptr || sample_count == 0 ||
      !configure(sample_rate_hz, channels)) {
    return false;
  }
  if (!writeBytes(samples, sample_count * sizeof(int16_t))) {
    return false;
  }

  int16_t silence[256] = {};
  writeBytes(silence, sizeof(silence));
  return true;
}

bool Speaker::playWav(const char* path) {
  FILE* f = fopen(path, "rb");
  if (f == nullptr) {
    ESP_LOGE(kTag, "failed to open %s", path);
    return false;
  }

  WavInfo info;
  if (!parseWavHeader(f, &info)) {
    ESP_LOGE(kTag, "bad WAV header: %s", path);
    fclose(f);
    return false;
  }

  if (info.audio_format != 1 || info.bits_per_sample != 16 ||
      (info.channels != 1 && info.channels != 2)) {
    ESP_LOGE(kTag, "unsupported WAV (format=%u, bits=%u, channels=%u): %s",
             static_cast<unsigned>(info.audio_format),
             static_cast<unsigned>(info.bits_per_sample),
             static_cast<unsigned>(info.channels), path);
    fclose(f);
    return false;
  }

  ESP_LOGI(kTag, "playing %s: %u Hz, 16-bit, %u ch, %u bytes", path,
           static_cast<unsigned>(info.sample_rate),
           static_cast<unsigned>(info.channels),
           static_cast<unsigned>(info.data_size));

  if (!configure(info.sample_rate, info.channels)) {
    fclose(f);
    return false;
  }
  if (fseek(f, info.data_offset, SEEK_SET) != 0) {
    ESP_LOGE(kTag, "failed to seek to WAV data: %s", path);
    fclose(f);
    return false;
  }

  int16_t buffer[kFileBufferSize / sizeof(int16_t)];
  size_t remaining = info.data_size;
  bool ok = true;
  while (remaining > 0) {
    const size_t chunk =
        remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
    const size_t read = fread(buffer, 1, chunk, f);
    if (read == 0) {
      ESP_LOGW(kTag, "short read in %s (%u bytes left)", path,
               static_cast<unsigned>(remaining));
      ok = false;
      break;
    }

    const size_t sample_count = read / sizeof(int16_t);
    if (!writeBytes(buffer, sample_count * sizeof(int16_t))) {
      ok = false;
      break;
    }
    remaining -= read;
  }

  fclose(f);

  // Let the last samples drain, then send silence to avoid a pop.
  int16_t silence[256] = {};
  writeBytes(silence, sizeof(silence));
  return ok;
}

void Speaker::playTone(uint32_t frequency_hz, uint32_t duration_ms) {
  if (duration_ms == 0 || !configure(kDefaultSampleRateHz, 2)) {
    return;
  }

  const uint32_t total_frames = (sample_rate_hz_ * duration_ms) / 1000;
  const uint32_t fade_frames = sample_rate_hz_ / 100;  // 10 ms
  const float phase_step =
      kTwoPi * static_cast<float>(frequency_hz) / sample_rate_hz_;

  int16_t buffer[kChunkFrames * 2];
  float phase = 0.0f;
  uint32_t frame_index = 0;

  while (frame_index < total_frames) {
    size_t chunk = total_frames - frame_index;
    if (chunk > kChunkFrames) {
      chunk = kChunkFrames;
    }

    for (size_t i = 0; i < chunk; ++i) {
      const uint32_t absolute = frame_index + i;
      const uint32_t from_end = total_frames - 1 - absolute;

      float envelope = 1.0f;
      if (absolute < fade_frames) {
        envelope = absolute / static_cast<float>(fade_frames);
      }
      if (from_end < fade_frames) {
        const float fade_out = from_end / static_cast<float>(fade_frames);
        if (fade_out < envelope) {
          envelope = fade_out;
        }
      }

      const int16_t sample =
          static_cast<int16_t>(sinf(phase) * kAmplitude * envelope);
      buffer[2 * i] = sample;
      buffer[2 * i + 1] = sample;

      phase += phase_step;
      if (phase >= kTwoPi) {
        phase -= kTwoPi;
      }
    }

    if (!writePcm16(buffer, chunk)) {
      return;
    }
    frame_index += chunk;
  }

  int16_t silence[kChunkFrames * 2] = {};
  writePcm16(silence, kChunkFrames);
}

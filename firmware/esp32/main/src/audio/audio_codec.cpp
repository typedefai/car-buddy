#include "audio/audio_codec.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <driver/gpio.h>
#include <driver/i2s_tdm.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <es7210_adc.h>
#include <es8311_codec.h>

#include "device.h"

namespace {

constexpr char kTag[] = "codec";
constexpr int kSampleRate = 16000;
constexpr int kOutChannels = 2;
constexpr int kInChannels = 4;  // ES7210 TDM: MIC1, MIC3, MIC2, MIC4
constexpr int kBitsPerSample = 16;
constexpr size_t kChunkFrames = 256;
constexpr size_t kFileBufferSize = 1024;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int16_t kToneAmplitude = 8000;
constexpr bool kDumpCodecRegisters = false;  // set true for bring-up

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
      if (fseek(f, chunk_size + (chunk_size & 1U), SEEK_CUR) != 0) {
        return false;
      }
    }
  }
}

}  // namespace

AudioCodec::~AudioCodec() {
  if (out_dev_ != nullptr) {
    esp_codec_dev_close(static_cast<esp_codec_dev_handle_t>(out_dev_));
    esp_codec_dev_delete(static_cast<esp_codec_dev_handle_t>(out_dev_));
  }
  if (in_dev_ != nullptr) {
    esp_codec_dev_close(static_cast<esp_codec_dev_handle_t>(in_dev_));
    esp_codec_dev_delete(static_cast<esp_codec_dev_handle_t>(in_dev_));
  }
  if (tx_chan_ != nullptr) {
    i2s_channel_disable(tx_chan_);
    i2s_del_channel(tx_chan_);
  }
  if (rx_chan_ != nullptr) {
    i2s_channel_disable(rx_chan_);
    i2s_del_channel(rx_chan_);
  }
  if (i2c_bus_ != nullptr) {
    i2c_del_master_bus(i2c_bus_);
  }
}

bool AudioCodec::init(bool with_input) {
  if (out_dev_ != nullptr || in_dev_ != nullptr) {
    return true;
  }

  // NS4150B enable is active high. Keep it off until the codec is configured.
  gpio_config_t en_cfg = {};
  en_cfg.pin_bit_mask = 1ULL << CODEC_PA_EN;
  en_cfg.mode = GPIO_MODE_INPUT_OUTPUT;  // input path for level readback
  en_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  en_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
  en_cfg.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&en_cfg) != ESP_OK ||
      gpio_set_level(static_cast<gpio_num_t>(CODEC_PA_EN), 0) != ESP_OK) {
    ESP_LOGE(kTag, "failed to configure PA enable");
    return false;
  }

  i2c_master_bus_config_t bus_cfg = {};
  bus_cfg.i2c_port = I2C_NUM_1;
  bus_cfg.sda_io_num = static_cast<gpio_num_t>(CODEC_I2C_SDA);
  bus_cfg.scl_io_num = static_cast<gpio_num_t>(CODEC_I2C_SCL);
  bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_cfg.glitch_ignore_cnt = 7;
  bus_cfg.flags.enable_internal_pullup = 1;
  if (i2c_new_master_bus(&bus_cfg, &i2c_bus_) != ESP_OK) {
    ESP_LOGE(kTag, "failed to create codec I2C bus");
    return false;
  }

  const uint8_t probe_addrs[] = {0x18, 0x40, 0x41, 0x42, 0x43};
  for (uint8_t addr : probe_addrs) {
    if (i2c_master_probe(i2c_bus_, addr, 20) == ESP_OK) {
      ESP_LOGI(kTag, "I2C device found at 0x%02X", addr);
    }
  }

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  if (i2s_new_channel(&chan_cfg, &tx_chan_,
                      with_input ? &rx_chan_ : nullptr) != ESP_OK) {
    ESP_LOGE(kTag, "failed to create I2S channels");
    return false;
  }

  i2s_std_config_t tx_cfg = {};
  tx_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate);
  tx_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  tx_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  tx_cfg.gpio_cfg.mclk = static_cast<gpio_num_t>(CODEC_I2S_MCLK);
  tx_cfg.gpio_cfg.bclk = static_cast<gpio_num_t>(CODEC_I2S_BCLK);
  tx_cfg.gpio_cfg.ws = static_cast<gpio_num_t>(CODEC_I2S_WS);
  tx_cfg.gpio_cfg.dout = static_cast<gpio_num_t>(CODEC_I2S_DOUT);
  tx_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx_chan_, &tx_cfg) != ESP_OK) {
    ESP_LOGE(kTag, "failed to initialize I2S TX");
    return false;
  }

  if (with_input) {
    // The ES7210 always streams its four mic channels over TDM on DOUT1.
    // MIC1/MIC2 are the microphones; MIC3 is the ES8311 echo reference.
    i2s_tdm_config_t rx_cfg = {};
    rx_cfg.clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(kSampleRate);
    rx_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    rx_cfg.clk_cfg.bclk_div = 8;
    rx_cfg.slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
        static_cast<i2s_tdm_slot_mask_t>(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
                                         I2S_TDM_SLOT2 | I2S_TDM_SLOT3));
    rx_cfg.slot_cfg.total_slot = 4;
    rx_cfg.gpio_cfg.mclk = static_cast<gpio_num_t>(CODEC_I2S_MCLK);
    rx_cfg.gpio_cfg.bclk = static_cast<gpio_num_t>(CODEC_I2S_BCLK);
    rx_cfg.gpio_cfg.ws = static_cast<gpio_num_t>(CODEC_I2S_WS);
    rx_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    rx_cfg.gpio_cfg.din = static_cast<gpio_num_t>(CODEC_I2S_DIN);
    if (i2s_channel_init_tdm_mode(rx_chan_, &rx_cfg) != ESP_OK) {
      ESP_LOGE(kTag, "failed to initialize I2S RX TDM");
      return false;
    }
  }

  if (i2s_channel_enable(tx_chan_) != ESP_OK) {
    ESP_LOGE(kTag, "failed to enable I2S TX");
    return false;
  }
  if (with_input && i2s_channel_enable(rx_chan_) != ESP_OK) {
    ESP_LOGE(kTag, "failed to enable I2S RX");
    return false;
  }

  audio_codec_i2s_cfg_t i2s_cfg = {};
  i2s_cfg.port = I2S_NUM_0;
  i2s_cfg.tx_handle = tx_chan_;
  i2s_cfg.rx_handle = with_input ? rx_chan_ : nullptr;
  i2s_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
  data_if_ = const_cast<void*>(
      static_cast<const void*>(audio_codec_new_i2s_data(&i2s_cfg)));
  if (data_if_ == nullptr) {
    ESP_LOGE(kTag, "failed to create I2S data interface");
    return false;
  }

  audio_codec_i2c_cfg_t i2c_cfg = {};
  i2c_cfg.port = I2C_NUM_1;
  i2c_cfg.bus_handle = i2c_bus_;
  i2c_cfg.clock_speed_hz = 100000;
  i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;
  out_ctrl_ = const_cast<void*>(
      static_cast<const void*>(audio_codec_new_i2c_ctrl(&i2c_cfg)));
  // This module straps ES7210 AD0 high: 7-bit 0x41 -> 8-bit 0x82.
  if (with_input) {
    i2c_cfg.addr = 0x82;
    in_ctrl_ = const_cast<void*>(
        static_cast<const void*>(audio_codec_new_i2c_ctrl(&i2c_cfg)));
  }
  gpio_if_ = const_cast<void*>(
      static_cast<const void*>(audio_codec_new_gpio()));
  if (out_ctrl_ == nullptr || gpio_if_ == nullptr ||
      (with_input && in_ctrl_ == nullptr)) {
    ESP_LOGE(kTag, "failed to create codec control interfaces");
    return false;
  }

  es8311_codec_cfg_t es8311_cfg = {};
  es8311_cfg.ctrl_if =
      static_cast<const audio_codec_ctrl_if_t*>(out_ctrl_);
  es8311_cfg.gpio_if =
      static_cast<const audio_codec_gpio_if_t*>(gpio_if_);
  es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
  es8311_cfg.pa_pin = CODEC_PA_EN;
  es8311_cfg.pa_reverted = false;
  es8311_cfg.use_mclk = true;
  es8311_cfg.hw_gain.pa_voltage = 5.0;
  es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
  out_codec_if_ = const_cast<void*>(
      static_cast<const void*>(es8311_codec_new(&es8311_cfg)));

  es7210_codec_cfg_t es7210_cfg = {};
  if (with_input) {
    es7210_cfg.ctrl_if = static_cast<const audio_codec_ctrl_if_t*>(in_ctrl_);
    // All four inputs must be enabled so the ES7210 switches DOUT1 to TDM.
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 |
                              ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    in_codec_if_ = const_cast<void*>(
        static_cast<const void*>(es7210_codec_new(&es7210_cfg)));
  }
  if (out_codec_if_ == nullptr ||
      (with_input && in_codec_if_ == nullptr)) {
    ESP_LOGE(kTag, "failed to create codec drivers");
    return false;
  }

  esp_codec_dev_cfg_t dev_cfg = {};
  dev_cfg.codec_if = static_cast<const audio_codec_if_t*>(out_codec_if_);
  dev_cfg.data_if = static_cast<const audio_codec_data_if_t*>(data_if_);
  dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
  out_dev_ = esp_codec_dev_new(&dev_cfg);
  if (with_input) {
    dev_cfg.codec_if = static_cast<const audio_codec_if_t*>(in_codec_if_);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    in_dev_ = esp_codec_dev_new(&dev_cfg);
  }
  if (out_dev_ == nullptr || (with_input && in_dev_ == nullptr)) {
    ESP_LOGE(kTag, "failed to create codec devices");
    return false;
  }

  esp_codec_dev_sample_info_t fs = {};
  fs.sample_rate = kSampleRate;
  fs.channel = kOutChannels;
  fs.bits_per_sample = kBitsPerSample;
  fs.mclk_multiple = 256;

  if (esp_codec_dev_open(static_cast<esp_codec_dev_handle_t>(out_dev_), &fs) !=
      ESP_CODEC_DEV_OK) {
    ESP_LOGE(kTag, "failed to open ES8311 output");
    return false;
  }

  // The ES8311 driver also toggles this pin through its PA control. Drive it
  // high explicitly so the NS4150B is definitely enabled after open.
  const gpio_num_t pa_pin = static_cast<gpio_num_t>(CODEC_PA_EN);
  gpio_set_direction(pa_pin, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(pa_pin, 1);
  esp_rom_delay_us(10);
  ESP_LOGI(kTag, "NS4150B enable GPIO%d high, pad level=%d", CODEC_PA_EN,
           gpio_get_level(pa_pin));

  if (with_input) {
    esp_codec_dev_sample_info_t in_fs = fs;
    in_fs.channel = kInChannels;
    in_fs.channel_mask = 0x000F;  // receive all four TDM slots
    if (esp_codec_dev_open(static_cast<esp_codec_dev_handle_t>(in_dev_),
                           &in_fs) != ESP_CODEC_DEV_OK) {
      ESP_LOGE(kTag, "failed to open ES7210 input");
      return false;
    }
    if (kDumpCodecRegisters) {
      dumpInRegisters("init");
    }
  }

  esp_codec_dev_set_out_vol(static_cast<esp_codec_dev_handle_t>(out_dev_), 80);
  if (in_dev_ != nullptr) {
    // MEMS mics on this module need a healthy PGA; the ES7210 driver's own
    // default is 30 dB and the vendor examples run around this level.
    esp_codec_dev_set_in_gain(static_cast<esp_codec_dev_handle_t>(in_dev_),
                              30.0f);
    if (kDumpCodecRegisters) {
      dumpInRegisters("after-gain");
    }
  }

  ESP_LOGI(kTag,
           "ES8311 ready: %d Hz, 16-bit stereo, I2S %d/%d/%d/%d/%d, input=%s",
           kSampleRate, CODEC_I2S_MCLK, CODEC_I2S_BCLK, CODEC_I2S_WS,
           CODEC_I2S_DOUT, CODEC_I2S_DIN, with_input ? "on" : "off");
  return true;
}

size_t AudioCodec::readMono(int16_t* out, size_t max_samples) {
  if (in_dev_ == nullptr || out == nullptr || max_samples == 0) {
    return 0;
  }

  int16_t frame[kChunkFrames * kInChannels];
  static int32_t dc_state[kInChannels] = {};
  size_t total = 0;
  int32_t peaks[kInChannels] = {};
  while (total < max_samples) {
    size_t want = max_samples - total;
    if (want > kChunkFrames) {
      want = kChunkFrames;
    }
    const int bytes = static_cast<int>(want * kInChannels * sizeof(int16_t));
    const int ret = esp_codec_dev_read(
        static_cast<esp_codec_dev_handle_t>(in_dev_), frame, bytes);
    if (ret != ESP_CODEC_DEV_OK) {
      ESP_LOGE(kTag, "ES7210 read failed: %d", ret);
      break;
    }
    for (size_t i = 0; i < want; ++i) {
      for (int ch = 0; ch < kInChannels; ++ch) {
        // Remove the ES7210 DC offset; WakeNet and playback both want
        // zero-centred PCM.
        const int32_t raw = frame[i * kInChannels + ch];
        dc_state[ch] += (raw - dc_state[ch]) >> 8;
        const int32_t sample = raw - dc_state[ch];
        const int32_t magnitude = sample < 0 ? -sample : sample;
        if (magnitude > peaks[ch]) {
          peaks[ch] = magnitude;
        }
        if (ch == 0) {
          // TDM slot 0 is MIC1, the primary voice microphone.
          out[total + i] = static_cast<int16_t>(sample);
        }
      }
    }
    total += want;
  }

  static uint32_t read_calls = 0;
  ++read_calls;
  // WakeNet polls in small chunks; the codec test records one long block.
  // Log both so mic levels are visible in either mode.
  if (max_samples >= static_cast<size_t>(kSampleRate) ||
      read_calls % 32 == 0) {
    ESP_LOGI(kTag, "mic peak MIC1=%ld MIC3=%ld MIC2=%ld MIC4=%ld",
             static_cast<long>(peaks[0]), static_cast<long>(peaks[1]),
             static_cast<long>(peaks[2]), static_cast<long>(peaks[3]));
  }
  return total;
}

bool AudioCodec::writeMono(const int16_t* samples, size_t sample_count) {
  if (out_dev_ == nullptr || samples == nullptr || sample_count == 0) {
    return false;
  }

  int16_t stereo[kChunkFrames * kOutChannels];
  size_t offset = 0;
  while (offset < sample_count) {
    size_t chunk = sample_count - offset;
    if (chunk > kChunkFrames) {
      chunk = kChunkFrames;
    }
    for (size_t i = 0; i < chunk; ++i) {
      stereo[i * 2] = samples[offset + i];
      stereo[i * 2 + 1] = samples[offset + i];
    }
    const int bytes = static_cast<int>(chunk * kOutChannels * sizeof(int16_t));
    const int ret = esp_codec_dev_write(
        static_cast<esp_codec_dev_handle_t>(out_dev_), stereo, bytes);
    if (ret != ESP_CODEC_DEV_OK) {
      ESP_LOGE(kTag, "ES8311 write failed: %d", ret);
      return false;
    }
    offset += chunk;
  }
  return true;
}

bool AudioCodec::playWav(const char* path) {
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
      info.sample_rate != kSampleRate ||
      (info.channels != 1 && info.channels != 2)) {
    ESP_LOGE(kTag, "unsupported WAV: %u Hz, %u-bit, %u ch", info.sample_rate,
             info.bits_per_sample, info.channels);
    fclose(f);
    return false;
  }
  if (fseek(f, info.data_offset, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  int16_t buffer[kFileBufferSize / sizeof(int16_t)];
  size_t remaining = info.data_size;
  bool ok = true;
  while (remaining > 0) {
    size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
    const size_t read = fread(buffer, 1, chunk, f);
    if (read == 0) {
      ok = false;
      break;
    }
    if (info.channels == 1) {
      if (!writeMono(buffer, read / sizeof(int16_t))) {
        ok = false;
        break;
      }
    } else {
      const int ret = esp_codec_dev_write(
          static_cast<esp_codec_dev_handle_t>(out_dev_), buffer,
          static_cast<int>(read));
      if (ret != ESP_CODEC_DEV_OK) {
        ok = false;
        break;
      }
    }
    remaining -= read;
  }
  fclose(f);

  int16_t silence[kChunkFrames] = {};
  writeMono(silence, kChunkFrames);
  return ok;
}

bool AudioCodec::playTone(uint32_t frequency_hz, uint32_t duration_ms) {
  if (duration_ms == 0) {
    return true;
  }

  const uint32_t total_frames = kSampleRate * duration_ms / 1000;
  const uint32_t fade_frames = kSampleRate / 100;
  const float phase_step =
      kTwoPi * static_cast<float>(frequency_hz) / kSampleRate;

  int16_t buffer[kChunkFrames];
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
      buffer[i] = static_cast<int16_t>(
          std::sin(phase) * kToneAmplitude * envelope);
      phase += phase_step;
      if (phase >= kTwoPi) {
        phase -= kTwoPi;
      }
    }
    if (!writeMono(buffer, chunk)) {
      return false;
    }
    frame_index += chunk;
  }

  int16_t silence[kChunkFrames] = {};
  writeMono(silence, kChunkFrames);
  return true;
}

void AudioCodec::flushInput() {
  if (in_dev_ == nullptr) {
    return;
  }
  int16_t discard[kChunkFrames * kInChannels];
  for (int i = 0; i < 2; ++i) {
    esp_codec_dev_read(static_cast<esp_codec_dev_handle_t>(in_dev_), discard,
                       sizeof(discard));
  }
}

void AudioCodec::setPaEnabled(bool enabled) {
  const gpio_num_t pin = static_cast<gpio_num_t>(CODEC_PA_EN);
  gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(pin, enabled ? 1 : 0);
  esp_rom_delay_us(10);
  ESP_LOGI(kTag, "PA_EN GPIO%d set %d, pad level %d", CODEC_PA_EN,
           enabled ? 1 : 0, gpio_get_level(pin));
}

void AudioCodec::setOutputVolume(int volume) {
  if (out_dev_ == nullptr) {
    return;
  }
  esp_codec_dev_set_out_vol(static_cast<esp_codec_dev_handle_t>(out_dev_),
                            volume);
}

void AudioCodec::dumpRegisters(const char* label) {
  if (out_dev_ == nullptr) {
    return;
  }
  static constexpr int kRegs[] = {
      0x00, 0x01, 0x02, 0x05, 0x06, 0x09, 0x0D, 0x0E, 0x12, 0x13,
      0x14, 0x31, 0x32, 0x37, 0x44, 0x45, 0xFD, 0xFE, 0xFF,
  };
  for (int reg : kRegs) {
    int value = 0;
    const int ret = esp_codec_dev_read_reg(
        static_cast<esp_codec_dev_handle_t>(out_dev_), reg, &value);
    if (ret == ESP_CODEC_DEV_OK) {
      ESP_LOGI(kTag, "%s ES8311[0x%02X]=0x%02X", label, reg, value);
    } else {
      ESP_LOGW(kTag, "%s ES8311[0x%02X] read failed: %d", label, reg, ret);
    }
  }
}

void AudioCodec::dumpInRegisters(const char* label) {
  if (in_dev_ == nullptr) {
    return;
  }
  static constexpr int kRegs[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0B, 0x0C,
      0x11, 0x12, 0x40, 0x41, 0x43, 0x44, 0x45, 0x46, 0x4B, 0x4C,
  };
  for (int reg : kRegs) {
    int value = 0;
    const int ret = esp_codec_dev_read_reg(
        static_cast<esp_codec_dev_handle_t>(in_dev_), reg, &value);
    if (ret == ESP_CODEC_DEV_OK) {
      ESP_LOGI(kTag, "%s ES7210[0x%02X]=0x%02X", label, reg, value);
    } else {
      ESP_LOGW(kTag, "%s ES7210[0x%02X] read failed: %d", label, reg, ret);
    }
  }
}

#ifndef CAR_BUDDY_AUDIO_CODEC_H_
#define CAR_BUDDY_AUDIO_CODEC_H_

#include <cstddef>
#include <cstdint>

#include <driver/i2c_master.h>
#include <driver/i2s_std.h>

class AudioCodec {
 public:
  AudioCodec() = default;
  ~AudioCodec();

  AudioCodec(const AudioCodec&) = delete;
  AudioCodec& operator=(const AudioCodec&) = delete;

  bool init();

  // Reads mono PCM16 samples from ES7210 (MIC1/left channel).
  size_t readMono(int16_t* out, size_t max_samples);

  // Writes mono PCM16 samples to ES8311 (duplicated to both I2S slots).
  bool writeMono(const int16_t* samples, size_t sample_count);

  // Plays a 16 kHz, 16-bit PCM WAV file from SPIFFS.
  bool playWav(const char* path);

  // Plays a short test tone through ES8311.
  bool playTone(uint32_t frequency_hz, uint32_t duration_ms);

  // Discards queued mic data (used after speaker playback).
  void flushInput();

 private:
  i2c_master_bus_handle_t i2c_bus_ = nullptr;
  i2s_chan_handle_t tx_chan_ = nullptr;
  i2s_chan_handle_t rx_chan_ = nullptr;

  void* data_if_ = nullptr;
  void* out_ctrl_ = nullptr;
  void* in_ctrl_ = nullptr;
  void* gpio_if_ = nullptr;
  void* out_codec_if_ = nullptr;
  void* in_codec_if_ = nullptr;
  void* out_dev_ = nullptr;
  void* in_dev_ = nullptr;
};

#endif

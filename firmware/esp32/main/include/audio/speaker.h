#ifndef CAR_BUDDY_SPEAKER_H_
#define CAR_BUDDY_SPEAKER_H_

#include <cstddef>
#include <cstdint>

#include <driver/i2s_std.h>

class Speaker {
 public:
  Speaker() = default;
  ~Speaker();

  Speaker(const Speaker&) = delete;
  Speaker& operator=(const Speaker&) = delete;

  // Initializes the MAX98357A I2S TX channel (16 kHz, 16-bit stereo).
  void init();

  // 0.0 = mute, 1.0 = full scale. Applies to playWav()/playTone().
  void setVolume(float volume);

  // Plays a PCM16 WAV file (mono or stereo). Reconfigures I2S to match it.
  bool playWav(const char* path);

  // Plays a sine tone. Blocks until the tone has been written to I2S.
  void playTone(uint32_t frequency_hz, uint32_t duration_ms);

  // Writes interleaved stereo PCM16 frames at the current sample rate.
  bool writePcm16(const int16_t* interleaved_stereo, size_t frame_count);

 private:
  bool configure(uint32_t sample_rate_hz, uint16_t channels);
  bool writeBytes(const void* data, size_t bytes);

  static constexpr uint32_t kDefaultSampleRateHz = 16000;
  i2s_chan_handle_t tx_chan_ = nullptr;
  uint32_t sample_rate_hz_ = 0;
  uint16_t channels_ = 0;
  float volume_ = 1.0f;
};

#endif

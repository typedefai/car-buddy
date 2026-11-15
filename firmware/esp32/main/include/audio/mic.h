#ifndef CAR_BUDDY_MIC_H_
#define CAR_BUDDY_MIC_H_

#include <cstddef>
#include <cstdint>

#include <driver/i2s_std.h>

class Mic {
 public:
  static constexpr uint32_t kSampleRateHz = 16000;

  Mic() = default;
  ~Mic();

  Mic(const Mic&) = delete;
  Mic& operator=(const Mic&) = delete;

  // Creates the I2S RX channel. The channel stays disabled until start().
  void init();
  void start();
  void stop();

  // Reads processed mono PCM16 samples (DC-blocked, fixed 4x gain).
  size_t read(int16_t* out, size_t max_samples, uint32_t timeout_ms);

  // Reads raw mono PCM16 samples for WakeNet (unity gain, no DC blocking).
  size_t readRaw(int16_t* out, size_t max_samples, uint32_t timeout_ms);

 private:
  size_t readInternal(int16_t* out, size_t max_samples, uint32_t timeout_ms,
                      bool processed);

  i2s_chan_handle_t rx_chan_ = nullptr;
  bool started_ = false;
  int32_t dc_ = 0;
};

#endif

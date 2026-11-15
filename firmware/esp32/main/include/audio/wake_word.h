#ifndef CAR_BUDDY_WAKE_WORD_H_
#define CAR_BUDDY_WAKE_WORD_H_

#include <cstddef>
#include <cstdint>

class WakeWord {
 public:
  WakeWord() = default;
  ~WakeWord();

  WakeWord(const WakeWord&) = delete;
  WakeWord& operator=(const WakeWord&) = delete;

  // Loads the "Computer" model from the ESP-SR model partition.
  bool init();

  // Consumes exactly chunk_size() samples. Returns true on detection.
  bool detect(const int16_t* samples);

  int chunk_size() const { return chunk_size_; }
  int sample_rate() const { return sample_rate_; }
  const char* model_name() const { return model_name_; }

 private:
  void* models_ = nullptr;
  const void* iface_ = nullptr;
  void* model_data_ = nullptr;
  int chunk_size_ = 0;
  int sample_rate_ = 0;
  char model_name_[64] = {};
};

#endif

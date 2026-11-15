#ifndef CAR_BUDDY_RGB_LED_H_
#define CAR_BUDDY_RGB_LED_H_

#include <cstdint>

#include <driver/gpio.h>
#include <esp_timer.h>
#include <led_strip.h>

class RgbLed {
 public:
  explicit RgbLed(gpio_num_t pin);
  ~RgbLed();

  RgbLed(const RgbLed&) = delete;
  RgbLed& operator=(const RgbLed&) = delete;

  void init();

  // Starts a smooth breathing animation for the given color and period.
  void breathe(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms);

  void off();

 private:
  static void timerCallback(void* arg);
  void apply(uint8_t r, uint8_t g, uint8_t b);

  static constexpr uint32_t kStepMs = 20;
  gpio_num_t pin_;
  led_strip_handle_t strip_ = nullptr;
  esp_timer_handle_t timer_ = nullptr;
  uint8_t r_ = 0;
  uint8_t g_ = 0;
  uint8_t b_ = 0;
  uint32_t period_ms_ = 3000;
  float phase_ = 0.0f;
};

#endif

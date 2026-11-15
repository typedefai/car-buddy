#include "led/rgb_led.h"

#include <cmath>

#include <esp_err.h>
#include <esp_log.h>

namespace {
constexpr char kTag[] = "led";
constexpr float kTwoPi = 6.28318530717958647692f;
}

RgbLed::RgbLed(gpio_num_t pin) : pin_(pin) {}

RgbLed::~RgbLed() {
  if (timer_ != nullptr) {
    esp_timer_stop(timer_);
    esp_timer_delete(timer_);
  }
  if (strip_ != nullptr) {
    led_strip_del(strip_);
  }
}

void RgbLed::init() {
  if (strip_ != nullptr) {
    return;
  }

  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = pin_;
  strip_config.max_leds = 1;
  strip_config.led_model = LED_MODEL_WS2812;
  strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.resolution_hz = 10 * 1000 * 1000;
  rmt_config.flags.with_dma = true;

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_));
  ESP_ERROR_CHECK(led_strip_clear(strip_));

  esp_timer_create_args_t timer_args = {};
  timer_args.callback = timerCallback;
  timer_args.arg = this;
  timer_args.name = "rgb_led";
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_));

  ESP_LOGI(kTag, "WS2812 ready on GPIO%d", pin_);
}

void RgbLed::apply(uint8_t r, uint8_t g, uint8_t b) {
  if (strip_ == nullptr) {
    return;
  }
  led_strip_set_pixel(strip_, 0, r, g, b);
  led_strip_refresh(strip_);
}

void RgbLed::breathe(uint8_t r, uint8_t g, uint8_t b, uint32_t period_ms) {
  if (strip_ == nullptr || timer_ == nullptr) {
    return;
  }
  r_ = r;
  g_ = g;
  b_ = b;
  period_ms_ = period_ms < kStepMs * 2 ? kStepMs * 2 : period_ms;
  phase_ = 0.0f;
  apply(0, 0, 0);

  esp_timer_stop(timer_);
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(timer_, kStepMs * 1000));
}

void RgbLed::timerCallback(void* arg) {
  auto* self = static_cast<RgbLed*>(arg);
  const float step = kTwoPi * kStepMs / self->period_ms_;
  self->phase_ += step;
  if (self->phase_ >= kTwoPi) {
    self->phase_ -= kTwoPi;
  }

  const float level = (1.0f - std::cos(self->phase_)) * 0.5f;
  self->apply(static_cast<uint8_t>(self->r_ * level),
              static_cast<uint8_t>(self->g_ * level),
              static_cast<uint8_t>(self->b_ * level));
}

void RgbLed::off() {
  if (timer_ != nullptr) {
    esp_timer_stop(timer_);
  }
  apply(0, 0, 0);
}

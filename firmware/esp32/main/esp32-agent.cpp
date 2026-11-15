#include <lvgl.h>

#include <cstdint>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "audio/audio_codec.h"
#include "audio/wake_word.h"
#include "device.h"
#include "display/oled_display.h"
#include "led/rgb_led.h"
#include "storage.h"

namespace {

constexpr char kTag[] = "app";
constexpr char kBootWav[] = "/spiffs/Vgramoa.wav";
constexpr uint32_t kRecordDurationMs = 3000;
constexpr uint32_t kLvglTaskStackSize = 8192;
constexpr uint32_t kAudioTaskStackSize = 12288;
constexpr UBaseType_t kLvglTaskPriority = 2;
constexpr UBaseType_t kAudioTaskPriority = 5;

struct AudioContext {
  OledDisplay* display;
  AudioCodec* codec;
  WakeWord* wake;
};

void lvglTask(void* arg) {
  auto* display = static_cast<OledDisplay*>(arg);
  while (true) {
    uint32_t next = display->process();
    if (next == LV_NO_TIMER_READY) {
      next = 1000;
    } else if (next == 0) {
      next = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(next));
  }
}

void audioTask(void* arg) {
  auto* ctx = static_cast<AudioContext*>(arg);

  // Play the greeting before enabling the microphone so it cannot wake itself.
  if (!ctx->codec->playWav(kBootWav)) {
    ESP_LOGW(kTag, "boot WAV unavailable, playing test tone");
    ctx->codec->playTone(440, 180);
  }
  vTaskDelay(pdMS_TO_TICKS(200));

  if (!ctx->wake->init()) {
    ESP_LOGE(kTag, "wake word init failed");
    ctx->display->showText("No wake model");
    vTaskDelete(nullptr);
    return;
  }

  const int chunk = ctx->wake->chunk_size();
  if (chunk <= 0 || ctx->wake->sample_rate() != 16000) {
    ESP_LOGE(kTag, "wake word format mismatch");
    ctx->display->showText("Wake format error");
    vTaskDelete(nullptr);
    return;
  }

  auto* frame = static_cast<int16_t*>(heap_caps_malloc(
      chunk * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  constexpr size_t kRecordSamples = 16000 * kRecordDurationMs / 1000;
  auto* record = static_cast<int16_t*>(heap_caps_malloc(
      kRecordSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (record == nullptr) {
    record = static_cast<int16_t*>(heap_caps_malloc(
        kRecordSamples * sizeof(int16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (frame == nullptr || record == nullptr) {
    ESP_LOGE(kTag, "audio buffer allocation failed");
    ctx->display->showText("No audio memory");
    vTaskDelete(nullptr);
    return;
  }

  ctx->display->showText("Say Computer");
  ESP_LOGI(kTag, "listening for \"Computer\", chunk=%d", chunk);

  while (true) {
    const size_t got = ctx->codec->readMono(frame, chunk);
    if (got != static_cast<size_t>(chunk)) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (!ctx->wake->detect(frame)) {
      continue;
    }

    ESP_LOGI(kTag, "Computer detected");
    ctx->display->showText("Yes?");
    ctx->codec->playTone(880, 120);
    ctx->codec->flushInput();

    ctx->display->showText("Listening...");
    size_t recorded = 0;
    while (recorded < kRecordSamples) {
      const size_t remaining = kRecordSamples - recorded;
      const size_t want =
          remaining < static_cast<size_t>(chunk) ? remaining : chunk;
      const size_t n = ctx->codec->readMono(record + recorded, want);
      if (n == 0) {
        break;
      }
      recorded += n;
    }

    if (recorded > 0) {
      ctx->display->showText("Playing...");
      vTaskDelay(pdMS_TO_TICKS(100));
      ESP_LOGI(kTag, "playing back %u samples",
               static_cast<unsigned>(recorded));
      if (!ctx->codec->writeMono(record, recorded)) {
        ESP_LOGE(kTag, "playback failed");
      }
      ESP_LOGI(kTag, "playback finished");
    }

    ctx->codec->flushInput();
    ctx->display->showText("Say Computer");
  }
}

}  // namespace

extern "C" void app_main(void) {
  static OledDisplay display;
  static AudioCodec codec;
  static WakeWord wake;
  static RgbLed led(static_cast<gpio_num_t>(RGB_LED_GPIO));
  static AudioContext audio_ctx{&display, &codec, &wake};

  display.init();
  display.showText("Hello, World!");

  mountSpiffs();

  if (!codec.init()) {
    ESP_LOGE(kTag, "audio codec initialization failed");
    display.showText("Codec failed");
    return;
  }

  led.init();
  led.breathe(0, 64, 160, 3000);

  ESP_LOGI(kTag, "OLED + codec + LED ready");

  const BaseType_t lvgl_created =
      xTaskCreate(lvglTask, "lvgl", kLvglTaskStackSize, &display,
                  kLvglTaskPriority, nullptr);
  const BaseType_t audio_created =
      xTaskCreate(audioTask, "audio_wake", kAudioTaskStackSize, &audio_ctx,
                  kAudioTaskPriority, nullptr);

  configASSERT(lvgl_created == pdPASS);
  configASSERT(audio_created == pdPASS);
}

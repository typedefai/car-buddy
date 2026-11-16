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
#include "link/stm32_link.h"
#include "storage.h"

namespace {

constexpr char kTag[] = "app";
constexpr char kBootWav[] = "/spiffs/Vgramoa.wav";
constexpr uint32_t kRecordDurationMs = 3000;
constexpr size_t kRecordSamples = 16000 * kRecordDurationMs / 1000;
constexpr uint32_t kLoopPauseMs = 500;
constexpr uint32_t kLvglTaskStackSize = 8192;
constexpr uint32_t kAudioTaskStackSize = 12288;
constexpr UBaseType_t kLvglTaskPriority = 2;
constexpr UBaseType_t kAudioTaskPriority = 5;

// Normal operation: WakeNet "Xiao Ai Tong Xue" plus the local record/playback
// reply.
// Set `kWakeWordEnabled = false` and `kSpeakerTestOnly = true` to bring up the
// speaker alone, or `false`/`false` to run the mic record/playback loop.
constexpr bool kWakeWordEnabled = true;
constexpr bool kSpeakerTestOnly = false;
// Keep the ES7210 open so the duplex pairing widens the ES8311 TX slots to
// 32 bits, matching the vendor reference configuration.
constexpr bool kEnableMic = true;

struct AudioContext {
  OledDisplay* display;
  AudioCodec* codec;
  WakeWord* wake;
};

int32_t peakAbs(const int16_t* samples, size_t count) {
  int32_t peak = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t value = samples[i];
    const int32_t magnitude = value < 0 ? -value : value;
    if (magnitude > peak) {
      peak = magnitude;
    }
  }
  return peak;
}

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

void runSpeakerTest(AudioContext* ctx) {
  ESP_LOGI(kTag, "speaker test: EN held HIGH, continuous 1 kHz");
  ctx->codec->setOutputVolume(100);
  ctx->codec->dumpRegisters("init");
  ctx->codec->setPaEnabled(true);
  ctx->display->showText("SPK ON 1kHz");
  while (true) {
    ctx->codec->playTone(1000, 1000);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void audioTask(void* arg) {
  auto* ctx = static_cast<AudioContext*>(arg);

  // Play the greeting before enabling the microphone so it cannot wake itself.
  ESP_LOGI(kTag, "playing startup WAV: %s", kBootWav);
  if (!ctx->codec->playWav(kBootWav)) {
    ESP_LOGW(kTag, "boot WAV unavailable, playing test tone");
    ctx->codec->playTone(440, 180);
  }
  ESP_LOGI(kTag, "startup WAV done");
  vTaskDelay(pdMS_TO_TICKS(300));

  if (!kWakeWordEnabled && kSpeakerTestOnly) {
    runSpeakerTest(ctx);
  }

  auto* record = static_cast<int16_t*>(heap_caps_malloc(
      kRecordSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (record == nullptr) {
    record = static_cast<int16_t*>(heap_caps_malloc(
        kRecordSamples * sizeof(int16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (record == nullptr) {
    ESP_LOGE(kTag, "audio buffer allocation failed");
    ctx->display->showText("No audio memory");
    vTaskDelete(nullptr);
    return;
  }

  if (!kWakeWordEnabled) {
    ESP_LOGI(kTag, "codec test mode: record %u ms, then play back",
             static_cast<unsigned>(kRecordDurationMs));
    while (true) {
      ctx->display->showText("Speak now...");
      ctx->codec->flushInput();

      const size_t recorded = ctx->codec->readMono(record, kRecordSamples);
      if (recorded == 0) {
        ESP_LOGE(kTag, "nothing recorded");
        vTaskDelay(pdMS_TO_TICKS(kLoopPauseMs));
        continue;
      }

      ESP_LOGI(kTag, "recorded %u samples, peak=%ld",
               static_cast<unsigned>(recorded),
               static_cast<long>(peakAbs(record, recorded)));

      ctx->display->showText("Playing...");
      if (!ctx->codec->writeMono(record, recorded)) {
        ESP_LOGE(kTag, "playback failed");
      }
      ctx->codec->flushInput();
      vTaskDelay(pdMS_TO_TICKS(kLoopPauseMs));
    }
  }

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
  if (frame == nullptr) {
    ESP_LOGE(kTag, "audio buffer allocation failed");
    ctx->display->showText("No audio memory");
    vTaskDelete(nullptr);
    return;
  }

  ctx->display->showText("Say Xiao Ai");
  ESP_LOGI(kTag, "listening for \"xiaoaitongxue\", chunk=%d", chunk);

  // Drop any startup-WAV audio still queued in the ES7210 path.
  ctx->codec->flushInput();

  while (true) {
    const size_t got = ctx->codec->readMono(frame, chunk);
    if (got != static_cast<size_t>(chunk)) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (!ctx->wake->detect(frame)) {
      continue;
    }

    ESP_LOGI(kTag, "xiaoaitongxue detected");
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
    ctx->display->showText("Say Xiao Ai");
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

  Stm32Link_Start();

  if (!codec.init(kEnableMic)) {
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

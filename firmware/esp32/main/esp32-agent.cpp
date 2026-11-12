#include <lvgl.h>

#include <cstdlib>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "audio/mic.h"
#include "audio/speaker.h"
#include "display/oled_display.h"
#include "storage.h"

namespace {

constexpr char kTag[] = "app";
constexpr char kBootWav[] = "/spiffs/Vgramoa.wav";
constexpr float kBootVolume = 0.4f;  // 40% of full scale, about -8 dB
constexpr uint32_t kRecordDurationMs = 3000;
constexpr uint32_t kLvglTaskStackSize = 8192;
constexpr uint32_t kAudioTaskStackSize = 8192;
constexpr UBaseType_t kLvglTaskPriority = 2;
constexpr UBaseType_t kAudioTaskPriority = 3;

struct AudioDemoContext {
  OledDisplay* display;
  Speaker* speaker;
  Mic* mic;
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

// Boot greeting, then record three seconds and play the captured audio back.
// The OLED shows the current state so the demo is visible without a monitor.
void audioDemoTask(void* arg) {
  auto* ctx = static_cast<AudioDemoContext*>(arg);

  if (!ctx->speaker->playWav(kBootWav)) {
    ESP_LOGW(kTag, "boot WAV unavailable, playing test tone");
    ctx->speaker->playTone(440, 180);
  }
  vTaskDelay(pdMS_TO_TICKS(200));

  constexpr size_t kRecordSamples =
      Mic::kSampleRateHz * kRecordDurationMs / 1000;
  auto* record = static_cast<int16_t*>(heap_caps_malloc(
      kRecordSamples * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (record == nullptr) {
    ESP_LOGE(kTag, "record buffer allocation failed");
    ctx->display->showText("No memory");
    vTaskDelete(nullptr);
    return;
  }

  ctx->display->showText("Recording...");
  vTaskDelay(pdMS_TO_TICKS(100));

  ctx->mic->start();
  const size_t samples = ctx->mic->read(record, kRecordSamples, 1000);
  ctx->mic->stop();

  if (samples == 0) {
    ESP_LOGE(kTag, "no audio captured");
    ctx->display->showText("Mic failed");
    free(record);
    vTaskDelete(nullptr);
    return;
  }

  ctx->display->showText("Speaking...");
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_LOGI(kTag, "playing back %u samples", static_cast<unsigned>(samples));

  if (!ctx->speaker->playPcm16(record, samples, 1, Mic::kSampleRateHz)) {
    ESP_LOGE(kTag, "playback failed");
  }
  ESP_LOGI(kTag, "playback finished");

  free(record);
  ctx->display->showText("Hello, World!");
  vTaskDelete(nullptr);
}

}  // namespace

extern "C" void app_main(void) {
  static OledDisplay display;
  static Speaker speaker;
  static Mic mic;
  static AudioDemoContext audio_ctx{&display, &speaker, &mic};

  display.init();
  display.showText("Hello, World!");

  mountSpiffs();

  speaker.init();
  speaker.setVolume(kBootVolume);
  mic.init();

  ESP_LOGI(kTag, "OLED + speaker + mic ready");

  const BaseType_t lvgl_created =
      xTaskCreate(lvglTask, "lvgl", kLvglTaskStackSize, &display,
                  kLvglTaskPriority, nullptr);
  const BaseType_t audio_created =
      xTaskCreate(audioDemoTask, "audio_demo", kAudioTaskStackSize, &audio_ctx,
                  kAudioTaskPriority, nullptr);

  configASSERT(lvgl_created == pdPASS);
  configASSERT(audio_created == pdPASS);
}

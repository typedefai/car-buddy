#include <lvgl.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "audio/speaker.h"
#include "display/oled_display.h"
#include "storage.h"

namespace {

constexpr char kTag[] = "app";
constexpr char kBootWav[] = "/spiffs/Vgramoa.wav";
constexpr float kBootVolume = 0.4f;  // 40% of full scale, about -8 dB
constexpr uint32_t kLvglTaskStackSize = 8192;
constexpr uint32_t kSpeakerTaskStackSize = 4096;
constexpr UBaseType_t kLvglTaskPriority = 2;
constexpr UBaseType_t kSpeakerTaskPriority = 3;

void lvglTask(void*) {
  while (true) {
    uint32_t next = lv_timer_handler();
    if (next == LV_NO_TIMER_READY) {
      next = 1000;
    } else if (next == 0) {
      next = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(next));
  }
}

// Plays the boot greeting from SPIFFS. Falls back to a short tone if the
// asset is missing.
void speakerTestTask(void* arg) {
  auto* speaker = static_cast<Speaker*>(arg);
  if (!speaker->playWav(kBootWav)) {
    ESP_LOGW(kTag, "boot WAV unavailable, playing test tone");
    speaker->playTone(440, 180);
  }
  vTaskDelete(nullptr);
}

}  // namespace

extern "C" void app_main(void) {
  static OledDisplay display;
  display.init();
  display.showText("Hello, World!");

  mountSpiffs();

  static Speaker speaker;
  speaker.init();
  speaker.setVolume(kBootVolume);

  ESP_LOGI(kTag, "OLED hello world running");

  const BaseType_t lvgl_created =
      xTaskCreate(lvglTask, "lvgl", kLvglTaskStackSize, nullptr,
                  kLvglTaskPriority, nullptr);
  const BaseType_t speaker_created =
      xTaskCreate(speakerTestTask, "speaker_test", kSpeakerTaskStackSize,
                  &speaker, kSpeakerTaskPriority, nullptr);

  configASSERT(lvgl_created == pdPASS);
  configASSERT(speaker_created == pdPASS);
}

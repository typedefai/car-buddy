#include <lvgl.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display/oled_display.h"

namespace {

constexpr char kTag[] = "app";
constexpr uint32_t kLvglTaskStackSize = 8192;
constexpr UBaseType_t kLvglTaskPriority = 2;

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

}  // namespace

extern "C" void app_main(void) {
  static OledDisplay display;
  display.init();
  display.showText("Hello, World!");
  ESP_LOGI(kTag, "OLED hello world running");

  const BaseType_t created =
      xTaskCreate(lvglTask, "lvgl", kLvglTaskStackSize, nullptr,
                  kLvglTaskPriority, nullptr);
  configASSERT(created == pdPASS);
}

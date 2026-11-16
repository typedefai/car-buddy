#include "link/drive_console.h"

#include <algorithm>
#include <cstdio>

#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "link/stm32_link.h"

namespace {

constexpr char kTag[] = "console";
constexpr uart_port_t kConsoleUart = UART_NUM_0;
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 2;

void PrintHelp() {
  std::printf("\r\ncar-buddy drive console\r\n");
  std::printf("  w/f : forward      x/b : reverse\r\n");
  std::printf("  a   : turn left    d   : turn right\r\n");
  std::printf("  s   : stop         e   : e-stop latch\r\n");
  std::printf("  c   : clear e-stop +/- : speed step\r\n");
  std::printf("  h/? : help\r\n");
}

void ConsoleTask(void* arg) {
  (void)arg;
  int16_t speed = 300;
  int16_t throttle = 0;
  int16_t steering = 0;
  bool enable = false;
  bool estop = false;
  char c = 0;

  while (true) {
    if (uart_read_bytes(kConsoleUart, &c, 1, portMAX_DELAY) != 1) {
      continue;
    }

    switch (c) {
      case 'w':
      case 'f':
        throttle = speed;
        steering = 0;
        enable = true;
        estop = false;
        break;
      case 'x':
      case 'b':
        throttle = static_cast<int16_t>(-speed);
        steering = 0;
        enable = true;
        estop = false;
        break;
      case 'a':
        throttle = 0;
        steering = static_cast<int16_t>(-speed);
        enable = true;
        estop = false;
        break;
      case 'd':
        throttle = 0;
        steering = speed;
        enable = true;
        estop = false;
        break;
      case 's':
        throttle = 0;
        steering = 0;
        enable = false;
        break;
      case '+':
        speed = static_cast<int16_t>(std::min<int>(1000, speed + 100));
        break;
      case '-':
        speed = static_cast<int16_t>(std::max<int>(100, speed - 100));
        break;
      case 'e':
        throttle = 0;
        steering = 0;
        enable = false;
        estop = true;
        break;
      case 'c':
        throttle = 0;
        steering = 0;
        enable = false;
        estop = false;
        break;
      case 'h':
      case '?':
        PrintHelp();
        continue;
      default:
        continue;
    }

    Stm32Link_SetCommand(throttle, steering, enable, estop);
    std::printf(
        "cmd throttle=%d steering=%d enable=%d estop=%d step=%d\r\n",
        throttle, steering, enable ? 1 : 0, estop ? 1 : 0, speed);
    std::fflush(stdout);
  }
}

}  // namespace

void DriveConsole_Start() {
  uart_config_t config = {};
  config.baud_rate = 115200;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;

  uart_param_config(kConsoleUart, &config);
  const esp_err_t err =
      uart_driver_install(kConsoleUart, 512, 0, 0, nullptr, 0);
  if (err != ESP_OK) {
    ESP_LOGW(kTag, "UART0 driver install returned %s", esp_err_to_name(err));
  }

  xTaskCreate(ConsoleTask, "drive_console", kTaskStack, nullptr, kTaskPriority,
              nullptr);
  PrintHelp();
}

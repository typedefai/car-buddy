#include "link/stm32_link.h"

#include <cstring>

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "car_protocol.h"
#include "device.h"

namespace {

constexpr char kTag[] = "stm32";
constexpr uart_port_t kUart = UART_NUM_1;
constexpr int kTxPin = UART_STM32_TX;
constexpr int kRxPin = UART_STM32_RX;
constexpr int kBaud = 115200;
constexpr uint32_t kHeartbeatPeriodMs = 100;
constexpr uint32_t kSetpointPeriodMs = 50;
constexpr uint32_t kLinkTimeoutMs = 500;
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 4;

car_parser_t g_parser;
uint16_t g_tx_seq = 0;
volatile uint32_t g_last_rx_ms = 0;
volatile bool g_alive = false;
Stm32Telemetry g_telemetry = {};
portMUX_TYPE g_command_mux = portMUX_INITIALIZER_UNLOCKED;
Stm32Command g_command = {0, 0, false, false};

Stm32Command CopyCommand() {
  portENTER_CRITICAL(&g_command_mux);
  const Stm32Command command = g_command;
  portEXIT_CRITICAL(&g_command_mux);
  return command;
}

void StopCommand() {
  portENTER_CRITICAL(&g_command_mux);
  g_command.throttle = 0;
  g_command.steering = 0;
  g_command.enable = false;
  portEXIT_CRITICAL(&g_command_mux);
}

bool SendFrame(uint8_t type, const void* payload, uint8_t payload_len) {
  uint8_t buffer[CAR_PROTOCOL_MAX_FRAME];
  const uint32_t now_ms =
      static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  const size_t len = car_protocol_encode(type, g_tx_seq++, now_ms, payload,
                                         payload_len, buffer, sizeof(buffer));
  if (len == 0) {
    return false;
  }
  return uart_write_bytes(kUart, buffer, len) == static_cast<int>(len);
}

void HandleFrame(const car_frame_t& frame) {
  switch (frame.type) {
    case CAR_MSG_HEARTBEAT:
      ESP_LOGD(kTag, "heartbeat seq=%u", frame.seq);
      break;
    case CAR_MSG_TELEMETRY:
      if (frame.payload_len == sizeof(car_telemetry_t)) {
        car_telemetry_t telemetry;
        std::memcpy(&telemetry, frame.payload, sizeof(telemetry));
        g_telemetry.uptime_ms = telemetry.uptime_ms;
        g_telemetry.left_speed_mm_s = telemetry.left_speed_mm_s;
        g_telemetry.right_speed_mm_s = telemetry.right_speed_mm_s;
        g_telemetry.battery_mv = telemetry.battery_mv;
        g_telemetry.state = telemetry.state;
        g_telemetry.fault = telemetry.fault;
        ESP_LOGI(kTag,
                 "telemetry uptime=%lu state=%u battery=%umV",
                 static_cast<unsigned long>(telemetry.uptime_ms),
                 telemetry.state, telemetry.battery_mv);
      }
      break;
    default:
      ESP_LOGW(kTag, "unhandled frame type=%u len=%u", frame.type,
               frame.payload_len);
      break;
  }
}

void LinkTask(void* arg) {
  (void)arg;
  uint8_t buffer[64];
  uint32_t last_heartbeat_ms = 0;
  uint32_t last_setpoint_ms = 0;

  while (true) {
    const int read = uart_read_bytes(kUart, buffer, sizeof(buffer),
                                     pdMS_TO_TICKS(20));
    if (read > 0) {
      for (int i = 0; i < read; ++i) {
        car_frame_t frame;
        if (car_parser_push(&g_parser, buffer[i], &frame)) {
          g_last_rx_ms =
              static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
          g_alive = true;
          HandleFrame(frame);
        }
      }
    }

    const uint32_t now_ms =
        static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);

    if (now_ms - last_setpoint_ms >= kSetpointPeriodMs) {
      last_setpoint_ms = now_ms;
      const Stm32Command command = CopyCommand();
      car_setpoint_t setpoint = {};
      setpoint.throttle = command.throttle;
      setpoint.steering = command.steering;
      if (command.enable) {
        setpoint.flags |= CAR_SETPOINT_ENABLE;
      }
      if (command.estop) {
        setpoint.flags |= CAR_SETPOINT_ESTOP;
      }
      SendFrame(CAR_MSG_SETPOINT, &setpoint, sizeof(setpoint));
    }

    if (now_ms - last_heartbeat_ms >= kHeartbeatPeriodMs) {
      last_heartbeat_ms = now_ms;
      SendFrame(CAR_MSG_HEARTBEAT, nullptr, 0);
    }

    if (g_alive && now_ms - g_last_rx_ms > kLinkTimeoutMs) {
      g_alive = false;
      StopCommand();
      ESP_LOGW(kTag, "link lost; motion stopped");
    }
  }
}

}  // namespace

void Stm32Link_Start() {
  car_parser_init(&g_parser);

  uart_config_t config = {};
  config.baud_rate = kBaud;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;

  ESP_ERROR_CHECK(uart_param_config(kUart, &config));
  ESP_ERROR_CHECK(uart_set_pin(kUart, kTxPin, kRxPin, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(kUart, 2048, 2048, 0, nullptr, 0));

  xTaskCreate(LinkTask, "stm32_link", kTaskStack, nullptr, kTaskPriority,
              nullptr);
  ESP_LOGI(kTag, "UART1 ready: TX=%d RX=%d %d baud", kTxPin, kRxPin, kBaud);
}

bool Stm32Link_IsAlive() {
  const uint32_t now_ms =
      static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
  return g_alive && (now_ms - g_last_rx_ms <= kLinkTimeoutMs);
}

Stm32Telemetry Stm32Link_Telemetry() { return g_telemetry; }

void Stm32Link_SetCommand(int16_t throttle, int16_t steering, bool enable,
                          bool estop) {
  portENTER_CRITICAL(&g_command_mux);
  g_command.throttle = throttle;
  g_command.steering = steering;
  g_command.enable = enable;
  g_command.estop = estop;
  portEXIT_CRITICAL(&g_command_mux);
}

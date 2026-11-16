#include "app.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
#include "stm32f1xx_hal.h"
#include "usart.h"
#include "uart_link.h"

#include "board_config.h"
#include "motor.h"

// 1 = bring-up test only: blink PC13 and print "Hello, World!" on USART1.
// 0 = ESP32 UART link, motor control, heartbeat, telemetry, safe-stop.
#define BRINGUP_BLINK_ONLY 0

#define HEARTBEAT_PERIOD_MS 50u
#define TELEMETRY_PERIOD_MS 500u
#define LINK_TIMEOUT_MS 300u

#define LED_GPIO_PORT GPIOC
#define LED_GPIO_PIN GPIO_PIN_13

static app_state_t g_state = APP_STATE_BOOT;
static car_setpoint_t g_setpoint;

#if !BRINGUP_BLINK_ONLY
static uint16_t g_tx_seq = 0;
static uint32_t g_last_rx_ms = 0;
static uint32_t g_last_heartbeat_ms = 0;
static uint32_t g_last_telemetry_ms = 0;
static uint32_t g_last_control_ms = 0;
static bool g_link_alive = false;
#endif

#if BRINGUP_BLINK_ONLY
static uint32_t g_last_blink_ms = 0;
static uint32_t g_last_hello_ms = 0;
static uint32_t g_hello_count = 0;

static void led_init(void) {
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = LED_GPIO_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &gpio);
  // Most F103 boards wire the PC13 LED active-low; start with it off.
  HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
}

static void hello_send(const char *line, uint16_t len) {
  HAL_UART_Transmit(&huart1, (const uint8_t *)line, len, 100);
}

static void bringup_init(void) {
  led_init();
  g_last_blink_ms = HAL_GetTick();
  g_last_hello_ms = HAL_GetTick();
  static const char kHello[] = "Hello, World!\r\n";
  hello_send(kHello, sizeof(kHello) - 1u);
}

static void bringup_loop(void) {
  const uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - g_last_blink_ms) >= 500u) {
    g_last_blink_ms = now;
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
  }

  if ((uint32_t)(now - g_last_hello_ms) >= 1000u) {
    g_last_hello_ms = now;
    char line[48];
    const int len = snprintf(line, sizeof(line), "Hello, World! %lu\r\n",
                             (unsigned long)g_hello_count++);
    if (len > 0) {
      hello_send(line, (uint16_t)len);
    }
  }
}
#else
static int16_t clamp_permille(int16_t value) {
  if (value > 1000) {
    return 1000;
  }
  if (value < -1000) {
    return -1000;
  }
  return value;
}

static void apply_setpoint(const car_frame_t *frame) {
  if (frame->payload_len != sizeof(car_setpoint_t)) {
    return;
  }

  car_setpoint_t next;
  memcpy(&next, frame->payload, sizeof(next));
  next.throttle = clamp_permille(next.throttle);
  next.steering = clamp_permille(next.steering);

  if ((next.flags & CAR_SETPOINT_ESTOP) != 0u) {
    g_state = APP_STATE_ESTOP;
    next.throttle = 0;
    next.steering = 0;
  } else if ((next.flags & CAR_SETPOINT_ENABLE) != 0u) {
    g_state = APP_STATE_READY;
  } else {
    g_state = APP_STATE_IDLE;
    next.throttle = 0;
    next.steering = 0;
  }
  g_setpoint = next;

  if (g_state == APP_STATE_READY) {
    const int16_t left =
        clamp_permille((int16_t)(next.throttle + next.steering));
    const int16_t right =
        clamp_permille((int16_t)(next.throttle - next.steering));
    Motor_SetTargets(left, right);
  } else {
    Motor_SetTargets(0, 0);
    if (g_state == APP_STATE_ESTOP) {
      Motor_Stop();
    }
  }
}

static void stop_motion(void) {
  g_setpoint.throttle = 0;
  g_setpoint.steering = 0;
  g_setpoint.flags = 0;
  if (g_state != APP_STATE_ESTOP) {
    g_state = APP_STATE_IDLE;
  }
  Motor_Stop();
}
#endif

void App_Init(void) {
  memset(&g_setpoint, 0, sizeof(g_setpoint));
  g_state = APP_STATE_IDLE;

#if BRINGUP_BLINK_ONLY
  bringup_init();
#else
  Motor_Init();
  UartLink_Init();
  g_last_rx_ms = HAL_GetTick() - (LINK_TIMEOUT_MS + 1u);
  g_last_heartbeat_ms = HAL_GetTick();
  g_last_telemetry_ms = HAL_GetTick();
  g_last_control_ms = HAL_GetTick();
  g_link_alive = false;
#endif
}

void App_Loop(void) {
#if BRINGUP_BLINK_ONLY
  bringup_loop();
#else
  const uint32_t now = HAL_GetTick();

  car_frame_t frame;
  while (UartLink_ReadFrame(&frame)) {
    g_last_rx_ms = now;
    g_link_alive = true;

    switch (frame.type) {
      case CAR_MSG_HEARTBEAT:
        break;
      case CAR_MSG_SETPOINT:
        apply_setpoint(&frame);
        break;
      default:
        break;
    }
  }

  if ((uint32_t)(now - g_last_rx_ms) > LINK_TIMEOUT_MS) {
    if (g_link_alive) {
      g_link_alive = false;
      stop_motion();
    }
  }

  if ((uint32_t)(now - g_last_control_ms) >= MOTOR_CONTROL_PERIOD_MS) {
    const uint32_t dt_ms = now - g_last_control_ms;
    g_last_control_ms = now;
    Motor_Update(dt_ms);
  }

  if (Motor_HasFault()) {
    g_state = APP_STATE_ESTOP;
    Motor_Stop();
  }

  if ((uint32_t)(now - g_last_heartbeat_ms) >= HEARTBEAT_PERIOD_MS) {
    g_last_heartbeat_ms = now;
    UartLink_SendFrame(CAR_MSG_HEARTBEAT, g_tx_seq++, now, NULL, 0);
  }

  if ((uint32_t)(now - g_last_telemetry_ms) >= TELEMETRY_PERIOD_MS) {
    g_last_telemetry_ms = now;
    car_telemetry_t telemetry = {
        .uptime_ms = now,
        .left_speed_mm_s = Motor_LeftSpeedMmS(),
        .right_speed_mm_s = Motor_RightSpeedMmS(),
        .battery_mv = 0,
        .state = (uint8_t)g_state,
        .fault = Motor_HasFault() ? 1u : 0u,
    };
    UartLink_SendFrame(CAR_MSG_TELEMETRY, g_tx_seq++, now, &telemetry,
                       sizeof(telemetry));
  }

  Motor_RefreshWatchdog();
#endif
}

app_state_t App_GetState(void) { return g_state; }

bool App_LinkAlive(void) {
#if BRINGUP_BLINK_ONLY
  return false;
#else
  return g_link_alive;
#endif
}

const car_setpoint_t *App_GetSetpoint(void) { return &g_setpoint; }

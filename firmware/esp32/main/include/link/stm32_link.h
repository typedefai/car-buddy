#ifndef CAR_BUDDY_STM32_LINK_H_
#define CAR_BUDDY_STM32_LINK_H_

#include <cstdint>

// UART1 link to the STM32: ESP32 TX GPIO14 -> STM32 PA3 (USART2_RX),
// ESP32 RX GPIO21 <- STM32 PA2 (USART2_TX), common ground.
void Stm32Link_Start();

// True when a valid frame arrived within the last 500 ms.
bool Stm32Link_IsAlive();

// Last parsed telemetry, or zeroed before the first frame.
struct Stm32Telemetry {
  uint32_t uptime_ms;
  int16_t left_speed_mm_s;
  int16_t right_speed_mm_s;
  uint16_t battery_mv;
  uint8_t state;
  uint8_t fault;
};

Stm32Telemetry Stm32Link_Telemetry();

#endif  // CAR_BUDDY_STM32_LINK_H_

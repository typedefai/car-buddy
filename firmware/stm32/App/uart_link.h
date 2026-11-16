#ifndef CAR_BUDDY_UART_LINK_H_
#define CAR_BUDDY_UART_LINK_H_

#include <stdbool.h>
#include <stdint.h>

#include "car_protocol.h"

// USART2 is the ESP32 link: PA2 = TX, PA3 = RX, 115200 8N1.
void UartLink_Init(void);

// Pops bytes from the RX ring and returns one complete, valid frame.
bool UartLink_ReadFrame(car_frame_t *out);

// Queues one frame for DMA transmission. Returns false if TX is still busy.
bool UartLink_SendFrame(uint8_t type, uint16_t seq, uint32_t timestamp_ms,
                        const void *payload, uint8_t payload_len);

// Number of bytes dropped because the RX ring was full (diagnostics).
uint32_t UartLink_RxDrops(void);

#endif  // CAR_BUDDY_UART_LINK_H_

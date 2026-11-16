#include "uart_link.h"

#include <stddef.h>
#include <string.h>

#include "main.h"
#include "stm32f1xx_hal.h"
#include "usart.h"

#define RING_SIZE 256u
#define DMA_BUFFER_SIZE 64u
#define TX_BUFFER_SIZE 64u

static uint8_t g_ring[RING_SIZE];
static volatile uint16_t g_head = 0;
static volatile uint16_t g_tail = 0;
static volatile uint32_t g_drops = 0;

static uint8_t g_rx_dma[DMA_BUFFER_SIZE];
static uint8_t g_tx_buffer[TX_BUFFER_SIZE];
static volatile bool g_tx_busy = false;

static DMA_HandleTypeDef g_hdma_usart2_rx;
static DMA_HandleTypeDef g_hdma_usart2_tx;
static car_parser_t g_parser;

static void ring_write(const uint8_t *data, uint16_t len) {
  for (uint16_t i = 0; i < len; ++i) {
    const uint16_t next = (uint16_t)((g_head + 1u) & (RING_SIZE - 1u));
    if (next == g_tail) {
      ++g_drops;
      continue;
    }
    g_ring[g_head] = data[i];
    g_head = next;
  }
}

static bool ring_pop(uint8_t *byte) {
  if (g_tail == g_head) {
    return false;
  }
  *byte = g_ring[g_tail];
  g_tail = (uint16_t)((g_tail + 1u) & (RING_SIZE - 1u));
  return true;
}

static void dma_init(void) {
  __HAL_RCC_DMA1_CLK_ENABLE();

  g_hdma_usart2_rx.Instance = DMA1_Channel6;
  g_hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  g_hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  g_hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
  g_hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  g_hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  g_hdma_usart2_rx.Init.Mode = DMA_NORMAL;
  g_hdma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&g_hdma_usart2_rx);
  __HAL_LINKDMA(&huart2, hdmarx, g_hdma_usart2_rx);

  g_hdma_usart2_tx.Instance = DMA1_Channel7;
  g_hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  g_hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  g_hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
  g_hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  g_hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  g_hdma_usart2_tx.Init.Mode = DMA_NORMAL;
  g_hdma_usart2_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
  HAL_DMA_Init(&g_hdma_usart2_tx);
  __HAL_LINKDMA(&huart2, hdmatx, g_hdma_usart2_tx);

  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}

void UartLink_Init(void) {
  dma_init();
  car_parser_init(&g_parser);
  g_head = 0;
  g_tail = 0;
  g_drops = 0;
  g_tx_busy = false;
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_rx_dma, DMA_BUFFER_SIZE);
}

bool UartLink_ReadFrame(car_frame_t *out) {
  if (out == NULL) {
    return false;
  }
  uint8_t byte = 0;
  while (ring_pop(&byte)) {
    if (car_parser_push(&g_parser, byte, out)) {
      return true;
    }
  }
  return false;
}

bool UartLink_SendFrame(uint8_t type, uint16_t seq, uint32_t timestamp_ms,
                        const void *payload, uint8_t payload_len) {
  if (g_tx_busy) {
    return false;
  }
  const size_t len =
      car_protocol_encode(type, seq, timestamp_ms, payload, payload_len,
                          g_tx_buffer, sizeof(g_tx_buffer));
  if (len == 0) {
    return false;
  }
  g_tx_busy = true;
  if (HAL_UART_Transmit_DMA(&huart2, g_tx_buffer, (uint16_t)len) != HAL_OK) {
    g_tx_busy = false;
    return false;
  }
  return true;
}

uint32_t UartLink_RxDrops(void) { return g_drops; }

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
  if (huart->Instance != USART2) {
    return;
  }
  // In normal DMA mode a half-transfer event does not end the transfer.
  // Only copy on transfer-complete or IDLE, then restart reception.
  if (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_HT) {
    return;
  }
  ring_write(g_rx_dma, size);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_rx_dma, DMA_BUFFER_SIZE);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    g_tx_busy = false;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    g_tx_busy = false;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_rx_dma, DMA_BUFFER_SIZE);
  }
}

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }

void DMA1_Channel6_IRQHandler(void) { HAL_DMA_IRQHandler(&g_hdma_usart2_rx); }

void DMA1_Channel7_IRQHandler(void) { HAL_DMA_IRQHandler(&g_hdma_usart2_tx); }

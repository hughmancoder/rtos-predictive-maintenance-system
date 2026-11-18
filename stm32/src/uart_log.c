#include "uart_log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "stm32f4xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  char buffer[96];
} log_msg_t;

static UART_HandleTypeDef huart2;
static QueueHandle_t log_queue = NULL;

void log_init(void) {
  /* Enable clocks for GPIOA and USART2 peripheral */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();

  /* Hand PA2 (TX) and PA3 (RX) over to USART2 (AF7) */
  GPIO_InitTypeDef gpio = {
      .Pin = GPIO_PIN_2 | GPIO_PIN_3,
      .Mode = GPIO_MODE_AF_PP,
      .Pull = GPIO_PULLUP,
      .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
      .Alternate = GPIO_AF7_USART2,
  };

  HAL_GPIO_Init(GPIOA, &gpio);

  /*  Configure USART2: 115200 baud, 8 data bits, no parity, 1 stop bit */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    for (;;) {
    }
  }

  /* Create the FreeRTOS queue (16 slots) rfer to section 5.3 for api in
   * documentation
   */
  log_queue = xQueueCreate(16, sizeof(log_msg_t));
  configASSERT(log_queue != NULL);
}

void log_printf(const char *fmt, ...) {
  if (log_queue == NULL) {
    return;
  }

  log_msg_t msg;
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg.buffer, sizeof(msg.buffer), fmt, args);
  va_end(args);

  // push log queue with 0 timeout
  xQueueSendToBack(log_queue, &msg, 0);
}

void uart_log_task(void *arg) {
  (void)arg;
  log_msg_t msg;
  for (;;) {
    if (xQueueReceive(log_queue, &msg, portMAX_DELAY) == pdTRUE) {
      HAL_UART_Transmit(&huart2, (uint8_t *)msg.buffer,
                        (uint16_t)strlen(msg.buffer), 50);
    }
  }
}
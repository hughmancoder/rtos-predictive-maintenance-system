#include "i2s_mic.h"
#include "FreeRTOS.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#include "uart_log.h"
#include <stdint.h>

/*
 * INMP441 I2S2 Microphone Driver Template
 *
 * Wiring (STM32F401CCU6 Black Pill):
 *   PB10 -> I2S2_CK (SCK)
 *   PB12 -> I2S2_WS (WS)
 *   PB15 -> I2S2_SD (SD)
 *   L/R  -> GND (Left channel)
 *   VDD  -> 3V3
 *   GND  -> GND
 */

#define MIC_FRAMES                                                             \
  256U /* 256 stereo frames per half-buffer (16 ms @ 16 kHz)                   \
        */

static I2S_HandleTypeDef hi2s2;
static DMA_HandleTypeDef hdma_spi2_rx;
static TaskHandle_t mic_task_handle = NULL;

/* Ping-pong buffer: 2 halves * MIC_FRAMES * 2 channels (L+R) * 2 half-words
 * (32-bit slot) */
static uint16_t i2s_dma_buf[2U * MIC_FRAMES * 4U];

void i2s_mic_init(void) {
  /* Enable GPIOB, SPI2 (I2S2), and DMA1 clocks */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {
      .Pin = GPIO_PIN_10 | GPIO_PIN_15 | GPIO_PIN_12,
      .Mode =
          GPIO_MODE_AF_PP, // push pull as i2s actively drives high and lower
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_HIGH,
      .Alternate = GPIO_AF5_SPI2, // Routes PB10, PB12, and PB15 to the
                                  // SPI2/I2S2 peripheral
  };
  HAL_GPIO_Init(GPIOB, &gpio);

  /* Configure PLLI2S clock for target audio sample rate
   */
  RCC_PeriphCLKInitTypeDef periph_clk = {0};
  periph_clk.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  periph_clk.PLLI2S.PLLI2SN = 256; // multiplier to give 256 Mhz
  periph_clk.PLLI2S.PLLI2SR =
      5; // divider to give 256 / 5 = 51.2 Mhz, which is target frequency for
         // i2s microphone at 16kHz sample rate with 64 bits per frame
  if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
    for (;;) {
    }
  }

  hdma_spi2_rx.Instance = DMA1_Stream3;
  hdma_spi2_rx.Init.Channel = DMA_CHANNEL_0;
  hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_spi2_rx.Init.Mode = DMA_CIRCULAR; // used for i2s audio
  hdma_spi2_rx.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK) {
    for (;;) {
    }
  }
  __HAL_LINKDMA(&hi2s2, hdmarx, hdma_spi2_rx);

  /* Initialize hi2s2 (I2S_MODE_MASTER_RX, I2S_DATAFORMAT_24B, etc.) */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_16K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK) {
    for (;;) {
    }
  }

  /* Priority >= 5 is required so xTaskNotifyFromISR is safe under FreeRTOS.*/
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_spi2_rx); }
static void notify_half(I2S_HandleTypeDef *hi2s, uint32_t half_flag) {
  if (hi2s->Instance != SPI2 || mic_task_handle == NULL) {
    return;
  }
  BaseType_t hpw = pdFALSE;
  xTaskNotifyFromISR(mic_task_handle, half_flag, eSetBits, &hpw);
  portYIELD_FROM_ISR(hpw);
}
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
  notify_half(hi2s, 1U << 0); /* first half ready */
}
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
  notify_half(hi2s, 1U << 1); /* second half ready */
}

void i2s_mic_task(void *arg) {
  (void)arg;
  mic_task_handle = xTaskGetCurrentTaskHandle();

  // size: 2 halves (ping pong double buffer) * frames * 2 channels (l/r)
  if (HAL_I2S_Receive_DMA(&hi2s2, i2s_dma_buf, 2U * MIC_FRAMES * 2U) !=
      HAL_OK) {
    log_printf("I2S2 DMA start failed\r\n");
    vTaskDelete(NULL);
  }

  for (;;) {
    uint32_t flags = 0;
    // 0xFFFFFFFFU — Clear all bits on exit so we don't process the same half
    // twice.
    xTaskNotifyWait(0, 0xFFFFFFFFU, &flags, portMAX_DELAY);

    const uint16_t *half =
        (flags & (1U << 0)) ? &i2s_dma_buf[0] : &i2s_dma_buf[MIC_FRAMES * 4U];

    /* Extract Left channel (L/R = GND): each frame is [L_hi, L_lo, R_hi,
     * R_lo] */
    for (uint32_t i = 0; i < MIC_FRAMES; i++) {
      uint16_t l_hi = half[i * 4U + 0U];
      uint16_t l_lo = half[i * 4U + 1U];
      int32_t sample24 = (int32_t)(((uint32_t)l_hi << 16) | l_lo) >> 8;

      (void)sample24;
    }
  }
}
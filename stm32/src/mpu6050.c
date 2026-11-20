#include "mpu6050.h"
#include "FreeRTOS.h"
#include "i2c1_bus.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#include "uart_log.h"
#include <stdint.h>

// Based on MPU-6050 Register Map and Datasheet from InvenSense
#define MPU6050_ADDR 0x68 /* AD0=GND -> 0x68, AD0=3V3 -> 0x69 */
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1 0x6B
#define REG_WHO_AM_I 0x75

#define MPU_NOTIFY_INDEX 1U /* Index 0 is reserved for i2c1_bus */
#define MPU_SAMPLE_HZ 1000U // 1000 samples

static TIM_HandleTypeDef htim2;
static volatile TaskHandle_t mpu_task_handle = NULL;

// big endian 16 from each axis. I2C registers are only 8 bits wide so a
// conversion is needed from high byte p[0] and then low byte p[1].
static int16_t be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

void mpu6050_init(void) {
  __HAL_RCC_TIM2_CLK_ENABLE();

  RCC_ClkInitTypeDef clk_config;
  uint32_t flash_latency;
  HAL_RCC_GetClockConfig(&clk_config, &flash_latency);
  uint32_t tim_clock = HAL_RCC_GetPCLK1Freq();
  if (clk_config.APB1CLKDivider != RCC_HCLK_DIV1) {
    tim_clock *=
        2U; /* STM32F4 doubles timer clock when APB1 prescaler != 1 (84 MHz) */
  }
  htim2.Instance = TIM2;
  htim2.Init.Prescaler =
      (tim_clock / 1000000U) - 1U; /* 84 MHz / 84 = 1 MHz tick */
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period =
      (1000000U / MPU_SAMPLE_HZ) - 1U; /* 1 MHz / 1000 = 1 kHz */
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    for (;;) {
    }
  }

  HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&htim2); }

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance != TIM2 || mpu_task_handle == NULL) {
    return;
  }
  BaseType_t hpw = pdFALSE;
  vTaskNotifyGiveIndexedFromISR(mpu_task_handle, MPU_NOTIFY_INDEX, &hpw);
  portYIELD_FROM_ISR(hpw);
}

void mpu6050_task(void *arg) {
  (void)arg;
  uint8_t who = 0;

  /* Give sensor 100 ms after power-up, then check WHO_AM_I (expect 0x68) */
  vTaskDelay(pdMS_TO_TICKS(100));
  if (!i2c1_read_regs(MPU6050_ADDR, REG_WHO_AM_I, &who, 1)) {
    log_printf("MPU6050: I2C read failed\r\n");
  } else {
    log_printf("MPU6050 WHO_AM_I = 0x%02X\r\n", who);
  }

  /* Wake from sleep (PWR_MGMT_1 = 0x00), DLPF = 184 Hz (1 kHz), div = 0 */
  i2c1_write_reg(MPU6050_ADDR, REG_PWR_MGMT_1, 0x00);
  i2c1_write_reg(MPU6050_ADDR, REG_CONFIG, 0x01);
  i2c1_write_reg(MPU6050_ADDR, REG_SMPLRT_DIV, 0x00);

  mpu_task_handle = xTaskGetCurrentTaskHandle();
  HAL_TIM_Base_Start_IT(&htim2);

  uint8_t raw[6];
  uint32_t sample_count = 0;
  uint32_t fail_count = 0;

  for (;;) {
    ulTaskNotifyTakeIndexed(MPU_NOTIFY_INDEX, pdTRUE, portMAX_DELAY);

    if (i2c1_read_regs(MPU6050_ADDR, REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
      int16_t ax = be16(&raw[0]);
      int16_t ay = be16(&raw[2]);
      int16_t az = be16(&raw[4]);
      sample_count++;

      /* Log once per second (1000 samples) so UART doesn't overflow */
      if ((sample_count % MPU_SAMPLE_HZ) == 0U) {
        log_printf("mpu 1kHz (%lu ok, %lu err): %d, %d, %d\r\n",
                   (unsigned long)sample_count, (unsigned long)fail_count, ax,
                   ay, az);
      }
    } else {
      fail_count++;
    }
  }
}
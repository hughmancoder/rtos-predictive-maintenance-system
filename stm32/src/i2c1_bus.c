#include "i2c1_bus.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include "task.h"

#define I2C1_TIMEOUT_MS 20U

static I2C_HandleTypeDef hi2c1;
static DMA_HandleTypeDef hdma_i2c1_rx;

static SemaphoreHandle_t i2c1_mutex = NULL;
static volatile TaskHandle_t caller_task = NULL;
static volatile bool i2c_ok = false;

void i2c1_bus_init(void) {
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* PB6 = I2C1_SCL, PB7 = I2C1_SDA (open-drain, external 4.7 kOhm pull-ups) */
  GPIO_InitTypeDef gpio = {
      .Pin = GPIO_PIN_6 | GPIO_PIN_7,
      .Mode = GPIO_MODE_AF_OD,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
      .Alternate = GPIO_AF4_I2C1,
  };
  HAL_GPIO_Init(GPIOB, &gpio);

  /* DMA1 Stream 0 Channel 1 -> I2C1_RX */
  hdma_i2c1_rx.Instance = DMA1_Stream0;
  hdma_i2c1_rx.Init.Channel = DMA_CHANNEL_1;
  hdma_i2c1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_i2c1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_i2c1_rx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_i2c1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_i2c1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_i2c1_rx.Init.Mode = DMA_NORMAL;
  hdma_i2c1_rx.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_i2c1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_i2c1_rx) != HAL_OK) {
    for (;;) {
    }
  }
  __HAL_LINKDMA(&hi2c1, hdmarx, hdma_i2c1_rx);

  /* I2C1 @ 400 kHz Fast Mode */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    for (;;) {
    }
  }

  /* All ISRs calling FreeRTOS FromISR APIs must have priority >= 5 */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  HAL_NVIC_SetPriority(I2C1_EV_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
  HAL_NVIC_SetPriority(I2C1_ER_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);

  // setup mutex since multiple sensors share the same 2 i2c wires
  i2c1_mutex = xSemaphoreCreateMutex();
  configASSERT(i2c1_mutex != NULL);
}

static bool bus_lock(void) {
  if (i2c1_mutex == NULL) {
    return false;
  }
  if (xSemaphoreTake(i2c1_mutex, pdMS_TO_TICKS(I2C1_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }
  ulTaskNotifyTake(pdTRUE, 0); /* clear any stale notification */
  caller_task = xTaskGetCurrentTaskHandle();
  i2c_ok = false;
  return true;
}

static bool bus_wait_and_unlock(HAL_StatusTypeDef start_status) {
  bool ok = false;
  if (start_status == HAL_OK) {
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(I2C1_TIMEOUT_MS)) > 0) {
      ok = i2c_ok;
    }
  }
  caller_task = NULL;
  xSemaphoreGive(i2c1_mutex);
  return ok;
}

bool i2c1_write(uint8_t dev_addr, const uint8_t *data, uint16_t len) {
  if (!bus_lock()) {
    return false;
  }
  HAL_StatusTypeDef st = HAL_I2C_Master_Transmit_IT(
      &hi2c1, (uint16_t)(dev_addr << 1), (uint8_t *)data, len);
  return bus_wait_and_unlock(st);
}

bool i2c1_read(uint8_t dev_addr, uint8_t *data, uint16_t len) {
  if (!bus_lock()) {
    return false;
  }
  HAL_StatusTypeDef st =
      HAL_I2C_Master_Receive_IT(&hi2c1, (uint16_t)(dev_addr << 1), data, len);
  return bus_wait_and_unlock(st);
}

bool i2c1_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val) {
  if (!bus_lock()) {
    return false;
  }
  HAL_StatusTypeDef st = HAL_I2C_Mem_Write_IT(
      &hi2c1, (uint16_t)(dev_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, &val, 1);
  return bus_wait_and_unlock(st);
}

bool i2c1_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint16_t len) {
  if (!bus_lock()) {
    return false;
  }
  HAL_StatusTypeDef st = HAL_I2C_Mem_Read_DMA(
      &hi2c1, (uint16_t)(dev_addr << 1), reg, I2C_MEMADD_SIZE_8BIT, buf, len);
  return bus_wait_and_unlock(st);
}

/* --- Interrupt handlers & HAL completion callbacks --- */

void DMA1_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_i2c1_rx); }

void I2C1_EV_IRQHandler(void) { HAL_I2C_EV_IRQHandler(&hi2c1); }

void I2C1_ER_IRQHandler(void) { HAL_I2C_ER_IRQHandler(&hi2c1); }

static void notify_caller(I2C_HandleTypeDef *hi2c, bool ok) {
  if (hi2c->Instance != I2C1 || caller_task == NULL) {
    return;
  }
  i2c_ok = ok;
  TaskHandle_t t = caller_task;
  caller_task = NULL;
  BaseType_t hpw = pdFALSE;
  vTaskNotifyGiveFromISR(t, &hpw);
  portYIELD_FROM_ISR(hpw);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  notify_caller(hi2c, true);
}
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  notify_caller(hi2c, true);
}
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  notify_caller(hi2c, true);
}
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  notify_caller(hi2c, true);
}
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  notify_caller(hi2c, false);
}
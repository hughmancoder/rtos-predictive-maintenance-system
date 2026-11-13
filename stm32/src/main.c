/*
 * STM32F401CCU6 (Black Pill) — FreeRTOS base firmware.
 * Clock: 25 MHz HSE -> PLL -> 84 MHz SYSCLK (48 MHz for USB).
 * HAL timebase: TIM11 (1 kHz), leaving SysTick exclusively for FreeRTOS.
 */
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define LED_PORT GPIOC
#define LED_PIN  GPIO_PIN_13

static TIM_HandleTypeDef htim11;

static void SystemClock_Config(void);
static void LED_Init(void);
static void led_task(void *arg);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    BaseType_t ok = xTaskCreate(led_task, "led", 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    configASSERT(ok == pdPASS);

    vTaskStartScheduler();

    /* Should never reach here unless heap is too small for idle/timer tasks. */
    for (;;) {}
}

static void led_task(void *arg)
{
    (void)arg;
    for (;;) {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void LED_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET); /* off */

    GPIO_InitTypeDef gpio = {
        .Pin   = LED_PIN,
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(LED_PORT, &gpio);
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;            /* 25 MHz / 25  = 1 MHz   */
    osc.PLL.PLLN       = 336;           /* 1 MHz * 336  = 336 MHz */
    osc.PLL.PLLP       = RCC_PLLP_DIV4; /* 336 / 4      = 84 MHz  */
    osc.PLL.PLLQ       = 7;             /* 336 / 7      = 48 MHz  */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        for (;;) {}
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;  /* 42 MHz (max) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;  /* 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        for (;;) {}
    }
}

/* --- HAL 1 ms timebase on TIM11 (keeps SysTick free for FreeRTOS) --- */

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clk_config;
    uint32_t           flash_latency;
    uint32_t           tim_clock;

    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, TickPriority, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);

    __HAL_RCC_TIM11_CLK_ENABLE();

    HAL_RCC_GetClockConfig(&clk_config, &flash_latency);
    /* TIM11 is on APB2; timer clock equals APB2 clock when APB2 prescaler is 1. */
    tim_clock = HAL_RCC_GetPCLK2Freq();
    if (clk_config.APB2CLKDivider != RCC_HCLK_DIV1) {
        tim_clock *= 2U;
    }

    htim11.Instance               = TIM11;
    htim11.Init.Period            = (1000000U / 1000U) - 1U; /* 1 kHz */
    htim11.Init.Prescaler         = (tim_clock / 1000000U) - 1U; /* 1 MHz */
    htim11.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim11.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim11.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim11) == HAL_OK) {
        return HAL_TIM_Base_Start_IT(&htim11);
    }
    return HAL_ERROR;
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim11, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim11, TIM_IT_UPDATE);
}

void TIM1_TRG_COM_TIM11_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim11, TIM_FLAG_UPDATE) != RESET &&
        __HAL_TIM_GET_IT_SOURCE(&htim11, TIM_IT_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&htim11, TIM_IT_UPDATE);
        HAL_IncTick();
    }
}

/* --- FreeRTOS error hooks --- */

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

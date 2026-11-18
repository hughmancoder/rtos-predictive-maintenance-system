# Microcontroller based predictive-maintenance system

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-2563EB?style=for-the-badge)
![STM32](https://img.shields.io/badge/STM32F401-03234B?style=for-the-badge&logo=stmicroelectronics&logoColor=white)

An STM32 running FreeRTOS samples vibration, sound and environmental data.
An ESP32-S3 displays fan health (`STABLE` / `FAULTY`) on a live TFT dashboard.

`Work in progress`
## Pinout

Reference diagrams: [Black Pill](docs/blackpill-pinout.png) ([PDF](docs/blackpill-pin-layout.pdf), from WeAct Studio) · [ESP32-S3-DevKitC-1](docs/esp32s3-devkitc1-pinout.jpg) (from Espressif, board v1.1)

### STM32F401CCU6 (Black Pill)

![Black Pill pinout](docs/blackpill-pinout.png)

| Pin | Function | Connection |
| --- | --- | --- |
| PC13 | GPIO out | On-board LED (active low) |
| PA2 / PA3 | USART2 TX / RX | USB-UART |
| PA9 / PA10 | USART1 TX / RX | ESP32 |
| PB6 / PB7 | I2C1 SCL / SDA | IMU + environmental sensors |
| PB0 | EXTI | IMU interrupt |
| PB10 / PB12 / PB15 | I2S2 CK / WS / SD | I2S microphone |

## STM32 FreeRTOS Tasks

| Task | Function | Priority | Stack Size in Words | Period | Role |
| --- | --- | --- | --- | --- | --- |
| `mpu` | `mpu6050_task` | `tskIDLE_PRIORITY + 2 (Highest)` | 256 | 500 ms | Samples MPU6050 accelerometer over shared `I2C1` |
| `elog` | `uart_log_task` | `tskIDLE_PRIORITY + 1` | 256 | Event-driven | Flushes queued log messages over `USART2` |
| `led` | `led_task` | `tskIDLE_PRIORITY + 1` | 128 | 500 ms | Blinks `PC13` heartbeat LED and logs, tick count |

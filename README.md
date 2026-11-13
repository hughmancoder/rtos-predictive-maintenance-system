# Microcontroller based predictive-maintenance system

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-2563EB?style=for-the-badge)
![STM32](https://img.shields.io/badge/STM32F401-03234B?style=for-the-badge&logo=stmicroelectronics&logoColor=white)
![ESP32-S3](https://img.shields.io/badge/ESP32--S3-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-E7352C?style=for-the-badge&logo=espressif&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-FF7F00?style=for-the-badge&logoColor=white)
![Peripherals](https://img.shields.io/badge/I2C%20%C2%B7%20SPI%20%C2%B7%20I2S%20%C2%B7%20UART%20%C2%B7%20DMA-374151?style=for-the-badge)

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

### ESP32-S3 N16R8

![ESP32-S3-DevKitC-1 pinout](docs/esp32s3-devkitc1-pinout.jpg)

| Pin | Function | Connection |
| --- | --- | --- |
| 48 | RMT | On-board RGB LED (GPIO38 on DevKitC-1 v1.1) |
| 17 / 18 | UART1 TX / RX | STM32 |
| 12 / 11 / 13 | SPI2 SCLK / MOSI / MISO | TFT display |
| 10 / 9 / 8 | CS / DC / RST | TFT display |
| 14 | GPIO / PWM | TFT display backlight |

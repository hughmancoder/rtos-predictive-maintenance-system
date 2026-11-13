# STM32F401CCU6 (Black Pill) firmware

A PlatformIO project using the `stm32cube` framework (ST's HAL).

## Build, flash, monitor

```bash
cd stm32
pio run                    # build
pio run -t upload          # flash over ST-LINK (SWD)
pio device monitor         # USART2 log, 115200 (from Stage 1)
pio debug --interface=gdb  # or press F5 in VS Code with the PlatformIO extension
```

## Wiring the Nucleo's ST-LINK to the Black Pill

1. On the Nucleo, remove both **CN2** jumpers. This disconnects the ST-LINK from the Nucleo's own STM32.
2. Wire the 6-pin **CN4** (SWD) header to the Black Pill's 4-pin SWD header:

| Nucleo CN4 | Black Pill |
| --- | --- |
| pin 2 SWCLK | SCK (PA14) |
| pin 3 GND | GND |
| pin 4 SWDIO | DIO (PA13) |
| pin 1 VDD_TARGET | 3V3 (lets the ST-LINK sense the target voltage) |

Power the Black Pill from its own USB-C.

**No ST-LINK?** Flash over USB instead: set `upload_protocol = dfu` in `platformio.ini`. Hold **BOOT0**, tap **NRST**, release BOOT0, then run `pio run -t upload`.

## Notes

- `platformio.ini` pins GCC 14. The platform's default GCC 7.2 only runs on Intel Macs.
- The clock is set in `SystemClock_Config()` in `src/main.c`: 25 MHz HSE → 84 MHz.
- Optional: use CubeMX (installed) to sanity-check the clock and pin settings. Copy the generated init code into `src/`; don't commit a whole CubeIDE project.
- FreeRTOS (Stage 1): add the FreeRTOS-Kernel source under `lib/`, using the `ARM_CM4F` port.

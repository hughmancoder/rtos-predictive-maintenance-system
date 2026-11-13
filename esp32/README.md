# ESP32-S3 (N16R8) firmware

A PlatformIO project using the `espidf` framework. It keeps the standard ESP-IDF layout (`main/`, `CMakeLists.txt`), so `idf.py` also works if you install ESP-IDF separately.

## Build, flash, monitor

```bash
cd esp32
pio run                   # build (the first build downloads ESP-IDF, so it's slow)
pio run -t upload         # flash
pio device monitor        # serial log; exit with Ctrl+C
pio run -t menuconfig     # ESP-IDF settings
```

Plug in using the USB-C port labelled **UART**. If upload can't connect, hold **BOOT**, tap **RESET**, release BOOT and retry.

## Notes

- 16 MB flash is set in `platformio.ini`. Octal PSRAM is turned on in `sdkconfig.defaults`, and PlatformIO writes the resolved settings to `sdkconfig.esp32s3`.
- PlatformIO's `espressif32` platform currently ships **ESP-IDF 6.1**. Components such as `led_strip` (3.x, which supports IDF 6) are declared in `main/idf_component.yml` and downloaded on the first build.
- The default partition table gives the app 1 MB. You'll switch to a custom partition table in Stage 6 for OTA.
- If the on-board RGB LED doesn't blink, change `BLINK_GPIO` in `main/main.c` from 48 to 38.

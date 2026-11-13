/*
 * Stage 0 bring-up: blink the on-board RGB LED on the ESP32-S3.
 *
 * The DevKitC-1 style boards use a WS2812 addressable LED, not a plain LED,
 * so a GPIO toggle won't light it. It sits on GPIO48 on most N16R8 boards
 * (GPIO38 on DevKitC-1 v1.1). Change BLINK_GPIO if yours doesn't light up.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "led_strip.h"

#define BLINK_GPIO   48
#define BLINK_MS     500

static const char *TAG = "blink";

void app_main(void)
{
    ESP_LOGI(TAG, "PSRAM: %u bytes", (unsigned)esp_psram_get_size());

    led_strip_handle_t led;
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &led));

    bool on = false;
    for (;;) {
        on = !on;
        if (on) {
            led_strip_set_pixel(led, 0, 0, 16, 0);
            led_strip_refresh(led);
        } else {
            led_strip_clear(led);
        }
        ESP_LOGI(TAG, "LED %s", on ? "on" : "off");
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}

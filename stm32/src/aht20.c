#include "env_sensors.h"
#include "FreeRTOS.h"
#include "i2c1_bus.h"
#include "task.h"
#include "uart_log.h"

#define AHT20_ADDR          0x38U
#define AHT20_CMD_INIT      0xBEU
#define AHT20_CMD_MEASURE   0xACU
#define AHT20_STATUS_BUSY   0x80U
#define AHT20_STATUS_CALIB  0x08U

bool aht20_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(40));

    uint8_t status = 0;
    if (!i2c1_read(AHT20_ADDR, &status, 1)) {
        return false;
    }

    if ((status & AHT20_STATUS_CALIB) == 0U) {
        const uint8_t cmd[3] = {AHT20_CMD_INIT, 0x08U, 0x00U};
        if (!i2c1_write(AHT20_ADDR, cmd, sizeof(cmd))) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

bool aht20_read(int32_t *temp_x100, uint32_t *humidity_x100)
{
    const uint8_t cmd[3] = {AHT20_CMD_MEASURE, 0x33U, 0x00U};
    if (!i2c1_write(AHT20_ADDR, cmd, sizeof(cmd))) {
        return false;
    }

    /* Release the I2C bus during the 80 ms conversion so MPU6050 isn't blocked. */
    vTaskDelay(pdMS_TO_TICKS(80));

    uint8_t buf[7];
    if (!i2c1_read(AHT20_ADDR, buf, sizeof(buf))) {
        return false;
    }
    if ((buf[0] & AHT20_STATUS_BUSY) != 0U) {
        return false;
    }

    uint32_t raw_hum = ((uint32_t)buf[1] << 12) |
                       ((uint32_t)buf[2] << 4) |
                       ((uint32_t)buf[3] >> 4);

    uint32_t raw_tmp = (((uint32_t)buf[3] & 0x0FU) << 16) |
                       ((uint32_t)buf[4] << 8) |
                       (uint32_t)buf[5];

    if (humidity_x100 != NULL) {
        *humidity_x100 = (uint32_t)(((uint64_t)raw_hum * 10000ULL) >> 20);
    }
    if (temp_x100 != NULL) {
        *temp_x100 = (int32_t)(((int64_t)raw_tmp * 20000LL) >> 20) - 5000;
    }
    return true;
}

void env_task(void *arg)
{
    (void)arg;

    bool aht_ok = aht20_init();
    log_printf("env init: AHT20=%s\r\n", aht_ok ? "ok" : "fail");

    for (;;) {
        int32_t  t_aht = 0;
        uint32_t rh    = 0;

        if (aht_ok && aht20_read(&t_aht, &rh)) {
            int32_t frac = t_aht % 100;
            if (frac < 0) {
                frac = -frac;
            }
            log_printf("env: T=%ld.%02ldC RH=%lu.%02lu%%\r\n",
                       (long)(t_aht / 100), (long)frac,
                       (unsigned long)(rh / 100), (unsigned long)(rh % 100));
        } else {
            log_printf("env read: AHT20 failed\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

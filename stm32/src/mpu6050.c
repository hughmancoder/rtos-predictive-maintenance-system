#include "mpu6050.h"
#include "FreeRTOS.h"
#include "i2c1_bus.h"
#include "task.h"
#include "uart_log.h"
#include <stdint.h>

// Based on MPU-6050 Register Map and Datasheet from InvenSense
#define MPU6050_ADDR 0x68 /* AD0=GND -> 0x68, AD0=3V3 -> 0x69 */
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B
#define REG_WHO_AM_I 0x75

// big endian 16 from each axis. I2C registers are only 8 bits wide so a
// conversion is needed from high byte p[0] and then low byte p[1].
static int16_t be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

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

  /* Wake from sleep (PWR_MGMT_1 = 0x00) */
  i2c1_write_reg(MPU6050_ADDR, REG_PWR_MGMT_1, 0x00);

  uint8_t raw[14];
  for (;;) {
    if (i2c1_read_regs(MPU6050_ADDR, REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
      int16_t ax = be16(&raw[0]);
      int16_t ay = be16(&raw[2]);
      int16_t az = be16(&raw[4]);
      log_printf("accel: %d, %d, %d\r\n", ax, ay, az);
    } else {
      log_printf("MPU6050: burst read failed\r\n");
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
#ifndef I2C1_BUS_H
#define I2C1_BUS_H

#include <stdbool.h>
#include <stdint.h>

void i2c1_bus_init(void);
bool i2c1_write(uint8_t dev_addr, const uint8_t *data, uint16_t len);
bool i2c1_read(uint8_t dev_addr, uint8_t *data, uint16_t len);
bool i2c1_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val);
bool i2c1_read_regs(uint8_t dev_addr, uint8_t reg, uint8_t *buf, uint16_t len);

#endif /* I2C1_BUS_H */

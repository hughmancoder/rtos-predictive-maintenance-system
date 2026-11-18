#ifndef ENV_SENSORS_H
#define ENV_SENSORS_H

#include <stdbool.h>
#include <stdint.h>

bool aht20_init(void);
bool aht20_read(int32_t *temp_x100, uint32_t *humidity_x100);

void env_task(void *arg);

#endif /* ENV_SENSORS_H */

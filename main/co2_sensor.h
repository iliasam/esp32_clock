
#ifndef __CO2_SENSOR_H__
#define __CO2_SENSOR_H__

#include <stdint.h>
#include <stdbool.h>

#include <esp_err.h>

void co2_init(void);
void co2_single_request(void);
void co2_sensor_handling(void);

uint8_t co2_is_sensor_ok(void);
uint16_t co2_last_value(void);

uint16_t *co2_get_1min_data(void);
uint16_t *co2_get_10min_data(void);

#endif /* __MAX7219_H__ */

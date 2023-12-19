
#ifndef __LIGHT_SENSOR_H__
#define __LIGHT_SENSOR_H__

#include <stdint.h>
#include <stdbool.h>

#include <esp_err.h>


void light_sensor_init(void);
void light_sensor_handling(void);
uint16_t  light_sensor_get_volt(void);
float light_lensor_get_lux(uint32_t volt_mv);
void dimming_set_common_brightness(float percent);
float light_lensor_get_cur_lux(void);
void dimming_auto_controlling(void);
void dimming_change_auto_controlling(bool is_enabled);
bool light_sensor_is_night(void);
void light_sensor_reset_night_counter(void);

#endif /* __LIGHT_SENSOR_H__ */

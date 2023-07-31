#ifndef _NVS_FUNCTIONS_H
#define _NVS_FUNCTIONS_H

//6 hours
#define NVS_DEVICE_TIME_CNT_PERIOD_S    (6 * 60 * 60)

void nvs_init(void);
uint32_t nvs_get_work_count(void);
void nvs_set_work_count(uint32_t new_count);
void nvs_device_time_counter_handling(void);

#endif
#ifndef _TIME_UPDATE_H
#define _TIME_UPDATE_H

void time_update_init(void);
void time_update_handler(void);
void initialize_sntp(void);
uint8_t time_update_is_ok(void);
time_t get_startup_time(void);

#endif
#ifndef _PCF2129_RTC_DRIVER_H
#define _PCF2129_RTC_DRIVER_H

#include "i2c_driver.h"
#include <time.h>
#include <sys/time.h>

#define PCF_I2C_RTC_ADDR            0x51

#define PCF_SECONDS_REG             0x03

typedef enum
{
    RTC_NO_CONNECTION = 0,
    RTC_NOT_CORRECT,
    RTC_CORRECT,
} rtc_state_t;

rtc_state_t rtc_driver_read_time(i2c_port_t i2c_num, struct tm *time_p);
void rtc_driver_write_time(i2c_port_t i2c_num, struct tm *time_p);


#endif //_PCF2129_RTC_DRIVER_H
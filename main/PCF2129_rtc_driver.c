
#include "PCF2129_rtc_driver.h"
#include "esp_log.h"

rtc_state_t rtc_state = RTC_NO_CONNECTION;

rtc_state_t rtc_driver_read_time(i2c_port_t i2c_num, struct tm *time_p)
{
    uint8_t tmp_data[7];
    esp_err_t res = i2c_driver_read_slave_reg(
        i2c_num, PCF_I2C_RTC_ADDR,  PCF_SECONDS_REG, tmp_data, 7);
    if (res != ESP_OK)
    {
        rtc_state = RTC_NO_CONNECTION;
        return rtc_state;
    }

    /*
    for (uint8_t i = 0; i < 7; i++)
        printf ("%d; ", tmp_data[i]);
    printf ("\r\n");
    */

    rtc_state = RTC_CORRECT;
    if (tmp_data[0] & (1 << 7))
        rtc_state = RTC_NOT_CORRECT;

    tmp_data[0] &= 0x7f;
    time_p->tm_sec = (tmp_data[0] >> 4) * 10 + (tmp_data[0] & 0xf);

    tmp_data[1] &= 0x7f;
    time_p->tm_min = (tmp_data[1] >> 4) * 10 + (tmp_data[1] & 0xf);

    tmp_data[2] &= 0x3f;
    time_p->tm_hour = (tmp_data[2] >> 4) * 10 + (tmp_data[2] & 0xf);

    tmp_data[3] &= 0x3f;
    time_p->tm_mday = (tmp_data[3] >> 4) * 10 + (tmp_data[3] & 0xf);
    if (time_p->tm_mday == 0)
        rtc_state = RTC_NOT_CORRECT;

    time_p->tm_wday = tmp_data[4];

    tmp_data[5] &= 0x1f;
    time_p->tm_mon = ((tmp_data[5] >> 4) * 10 + (tmp_data[5] & 0xf)) - 1;

    uint8_t year = (tmp_data[6] >> 4) * 10 + (tmp_data[6] & 0xf);//0-99
    if (year < 22)
        rtc_state = RTC_NOT_CORRECT;

    time_p->tm_year = year + 100; //tm start is 1900
    //printf ("RTC state: %d\r\n", rtc_state);
    return rtc_state;
}

void rtc_driver_write_time(i2c_port_t i2c_num, struct tm *time_p)
{
    uint8_t tmp_data[8];
    tmp_data[0] = ((time_p->tm_sec / 10) << 4) + (time_p->tm_sec % 10);
    tmp_data[1] = ((time_p->tm_min / 10) << 4) + (time_p->tm_min % 10);
    tmp_data[2] = ((time_p->tm_hour / 10) << 4) + (time_p->tm_hour % 10);
    tmp_data[3] = ((time_p->tm_mday / 10) << 4) + (time_p->tm_mday % 10);
    tmp_data[4] = time_p->tm_wday;
    uint8_t month = time_p->tm_mon + 1;
    tmp_data[5] = ((month / 10) << 4) + (month % 10);
    int year = time_p->tm_year - 100;
    tmp_data[6] = ((year / 10) << 4) + (year % 10);
    i2c_master_write_slave_reg(
        i2c_num, PCF_I2C_RTC_ADDR,  PCF_SECONDS_REG, tmp_data, 7);
}
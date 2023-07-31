#include "clock_config.h"
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "PCF2129_rtc_driver.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_event.h"
#include <sys/types.h>

#include "time_update.h"

static const char *TAG2 = "TIME_UPDATE";

#define SNTP_RETRY_COUNT    8

#define SNTP_TOTAL_TRIES_COUNT    9

typedef enum
{
    STARTUP_TIMESTAMP_NO = 0,
    STARTUP_TIMESTAMP_RTC,
    STARTUP_TIMESTAMP_SNTP,
} startup_timestamp_state_t;


typedef struct 
{
    uint8_t     system_time_ok;
    rtc_state_t last_rtc_state;
    uint8_t     wait_sntp_flag;
    time_t      last_sntp_update_timestamp;
    
    uint8_t     sntp_total_tries;
    time_t      sntp_long_fail_timestamp;
    bool        sntp_long_fail_flag; //This flag is set if lot of tries failed

    time_t      startup_timestamp;
    startup_timestamp_state_t startup_time_state;
} time_update_state_t;

uint8_t system_time_actual_flag = 0;
time_update_state_t time_update_state;

void time_update_handler_rtc(void);
void time_update_handler_sntp(void);
void time_update_get_from_sntp(void);
void time_sync_notification_cb(struct timeval *tv);
void initialize_sntp(void);
esp_err_t time_update_sntp_try(void);

//***************************************************************************

/// @brief Return if system time is OK
/// @param  
/// @return 1 if time is OK
uint8_t time_update_is_ok(void)
{
    return time_update_state.system_time_ok;
}

void time_update_init(void)
{
    printf("Size of time_t: %i byte\n", sizeof(time_t));
}

/// @brief Called from the task, controlling keeping system time correct 
/// @param  
void time_update_handler(void)
{
    time_update_handler_rtc();
    time_update_handler_sntp();
}

/// @brief Update system time using external RTC
/// @param  
void time_update_handler_rtc(void)
{
    if (time_update_state.system_time_ok != 0)
        return;

    struct tm timeinfo;
    time_update_state.last_rtc_state = rtc_driver_read_time(I2C_PORT_NUM, &timeinfo);
    if (time_update_state.last_rtc_state != RTC_CORRECT)
    {
        ESP_LOGI(TAG2, "RTC fail, state: %d", time_update_state.last_rtc_state);
        return;
    }    
    
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG2, "Ext. RTC time: %s", strftime_buf);

    time_t time_bin = mktime(&timeinfo);
    struct timeval now = { .tv_sec = time_bin };
    settimeofday(&now, NULL);

    //System time is syncronized from external RTC, but it can be not accurate
    time_update_state.system_time_ok = 1;

    if (time_update_state.startup_time_state == STARTUP_TIMESTAMP_NO)
    {
        time_update_state.startup_timestamp = time_bin;
        time_update_state.startup_time_state = STARTUP_TIMESTAMP_RTC;
    }
}

/// @brief Update system time using SNTP - if it is needed
/// @param  
void time_update_handler_sntp(void)
{
    time_t now_binary = time(NULL);

    if (time_update_state.sntp_long_fail_flag)
    {
         time_t fail_diff_s = now_binary - time_update_state.sntp_long_fail_timestamp;
         if (fail_diff_s > SNTP_LONG_FAIL_UPDATE_TIME_S)
         {
            time_update_state.sntp_long_fail_flag = false;//allow new tries
         }
         else
         {
            return;//update is not allowed now
         }
    }


    if (time_update_state.system_time_ok == 0)
    {
        //System time is not actual
        time_update_get_from_sntp();
    }
    else
    {
        //time is actual, periodic update here
        time_t diff_s = now_binary - time_update_state.last_sntp_update_timestamp;
        if ((diff_s > SNTP_UPDATE_TIME_S) || 
            (diff_s < 0) ||
            (time_update_state.last_sntp_update_timestamp == 0))
        {
            time_update_get_from_sntp();
        }  
        return;
    }
}

void time_update_get_from_sntp(void)
{
    /**
     * NTP server address could be aquired via DHCP,
     * see LWIP_DHCP_GET_NTP_SRV menuconfig option
     */
#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif

    if (example_is_wifi_enabled()) //this code is simple so it is expecting that wifi is off
        return;

    ESP_LOGI(TAG2, "Need to get time from SNTP");
    // This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
    esp_err_t res = example_connect();
    if (res != ESP_OK)
    {
        ESP_LOGI(TAG2, "Error - can't connect to WIFI");
        ESP_LOGI(TAG2, "Force Disconnect WIFI");
        ESP_ERROR_CHECK(example_disconnect());
        return;
    }

    time_update_state.wait_sntp_flag = 1;
    esp_err_t sntp_res;

    for (uint8_t try = 0; try < 3; try++)
    {
        sntp_res = time_update_sntp_try();
        if (sntp_res == ESP_OK)
            break;
    }
    
    ESP_LOGI(TAG2, "Disconnect WIFI");
    ESP_ERROR_CHECK(example_disconnect());

    if (time_update_state.wait_sntp_flag != 0)
    {
        ESP_LOGI(TAG2, "Error - can't update time from SNTP");
        if (time_update_state.sntp_total_tries >= SNTP_TOTAL_TRIES_COUNT)
        {
            ESP_LOGI(TAG2, "Too much attempts, stop it for a while");
            time_update_state.sntp_total_tries = 0;
            time_update_state.sntp_long_fail_timestamp = time(NULL);
            time_update_state.sntp_long_fail_flag = true;
        }
        return;
    }

    //Print current time, save it to the RTC
    // wait for time to be set
    time_t now_binary = 0;
    struct tm timeinfo = { 0 };
    now_binary = time(NULL);
    localtime_r(&now_binary, &timeinfo);
    time_update_state.last_sntp_update_timestamp = now_binary;
    rtc_driver_write_time(I2C_PORT_NUM, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG2, "SNTP time: %s", strftime_buf);

    if (time_update_state.startup_time_state != STARTUP_TIMESTAMP_SNTP)
    {
        time_update_state.startup_timestamp = now_binary;
        time_update_state.startup_time_state = STARTUP_TIMESTAMP_SNTP;
    }
    
    time_update_state.sntp_total_tries = 0;
    time_update_state.system_time_ok = 1;
}

esp_err_t time_update_sntp_try(void)
{
    time_update_state.sntp_total_tries++;
    ESP_LOGI(TAG2, "SNTP common connect try: %d", time_update_state.sntp_total_tries);
    initialize_sntp();
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < SNTP_RETRY_COUNT) 
    {
        ESP_LOGI(TAG2, "Waiting for SNTP... (%d/%d)", retry, SNTP_RETRY_COUNT);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    sntp_stop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (time_update_state.wait_sntp_flag != 0)
    {
        ESP_LOGI(TAG2, "Error - can't update time from SNTP");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG2, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, SNTP_SERVER);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG2, "Notification of a time synchronization event");
    time_update_state.wait_sntp_flag = 0;
}

time_t get_startup_time(void)
{
    if (time_update_state.startup_time_state == STARTUP_TIMESTAMP_NO)
    {
        time_t now_binary = time(NULL);
        return now_binary;//return current time
    }
    
    return time_update_state.startup_timestamp;
}

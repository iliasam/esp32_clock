#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"

#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "i2c_driver.h"
#include "PCF2129_rtc_driver.h"
#include "max7219.h"
#include "time_update.h"
#include "clock_display.h"
#include "co2_sensor.h"
#include "display_drawing.h"
#include "mavlink_handling.h"
#include "weather_update.h"
#include "keys_controlling.h"
#include "light_sensor.h"
#include "nvs_functions.h"

#include "clock_config.h"


/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
//RTC_DATA_ATTR static int boot_count = 0;


void time_weather_task(void *pvParameter);
void clock_disp_task(void *pvParameter);
void sensors_update_task(void *pvParameter);
void keys_update_task(void *pvParameter);

void time_sync_notification_cb(struct timeval *tv);

//**************************************************************************



void app_main(void)
{
    i2c_driver_init(I2C_PORT_NUM);
    nvs_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    

    setenv("TZ", CLOCK_TIMEZONE, 1);
    tzset();

    time_update_init();
    clock_display_init();
    co2_init();
    display_drawing_init();
    keys_init();
    light_sensor_init();

    mavlink_handling_init();
    
    //********************************************************

    xTaskCreate(&time_weather_task, "time_weath_task", 8000, NULL, 5, NULL);
    xTaskCreate(&clock_disp_task, "clock_task", 4000, NULL, 4, NULL);
    xTaskCreate(&sensors_update_task, "sensors_task", 2500, NULL, 3, NULL);
    xTaskCreate(&keys_update_task, "keys_task", 1024, NULL, 2, NULL);

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

void time_weather_task(void *pvParameter)
{
    while (1)
    {
        time_update_handler();
        weather_update_handler();
        nvs_device_time_counter_handling();//do not need low period
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void clock_disp_task(void *pvParameter)
{
    while (1)
    {
        light_sensor_handling();
        key_handling_execution();
        clock_display_handler();
        display_drawing_handler();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void sensors_update_task(void *pvParameter)
{
    while (1)
    {
        co2_sensor_handling();
        mavlink_requests_handling();

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void keys_update_task(void *pvParameter)
{
    while (1)
    {
        key_handling();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}




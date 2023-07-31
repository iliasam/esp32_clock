#include <stdio.h>
#include "clock_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>
#include "time_update.h"
#include "nvs_functions.h"

nvs_handle_t my_handle;
time_t nvs_time_counter_timestamp = 0;


void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
     {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    else 
    {
        printf("NVS handle created OK!\n");
    }

    
}

/// @brief Return number of working cycles
/// @param  
/// @return 
uint32_t nvs_get_work_count(void)
{
    uint32_t work_counter = 0;
    esp_err_t err = nvs_get_u32(my_handle, "work_counter", &work_counter);
    switch (err) 
    {
        case ESP_OK:
            //printf("NVS read OK\n");
        break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            printf("Write zero value\n");
            nvs_set_work_count(0);
            break;
        default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
    return work_counter;
}

void nvs_set_work_count(uint32_t new_count)
{
        esp_err_t err = nvs_set_u32(my_handle, "work_counter", new_count);
        printf((err != ESP_OK) ? "NVS Failed!\n" : "NVS Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // but this is not guaranteed.
        printf("Committing updates in NVS ... \n");
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "NVS Failed!\n" : "NVS Done\n");
}

void nvs_device_time_counter_handling(void)
{
    if (time_update_is_ok() == false)
        return;

    time_t now_binary = time(NULL);
    if (nvs_time_counter_timestamp == 0)
    {
        nvs_time_counter_timestamp = now_binary;
        return;
    }

    time_t diff_s = now_binary - nvs_time_counter_timestamp;
    if (diff_s > NVS_DEVICE_TIME_CNT_PERIOD_S)
    {
        uint32_t tmp = nvs_get_work_count();
        printf("Current time period: %lu \n", tmp);
        tmp++;
        nvs_set_work_count(tmp);
        nvs_time_counter_timestamp = now_binary;
    }
}

 
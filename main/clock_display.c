//LED display is contolled from here

#include "clock_config.h"
#include "max7219.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "time_update.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "mode_controlling.h"
#include "co2_sensor.h"

#include "mavlink_handling.h"
extern mavlink_handling_state_t mavlink_handling_state;

#include "clock_display.h"

#define MAX_TRY_CNT      5

typedef struct 
{
    uint8_t             is_init_flag;
    max7219_t           dev;
    bool                change_brightness;
    uint8_t             target_brightness;
} clock_display_state_t;

static const char *TAG_DISP = "clock_disp";

clock_display_state_t clock_display_state = {.is_init_flag = 0, .change_brightness = false, .target_brightness = 15,};

void clock_display_time(void);
void clock_display_co2(void);

//*************************************************************************************


void clock_display_init(void)
{
    spi_bus_config_t cfg = 
    {
       .mosi_io_num = LCD_PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = LCD_PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_NAME, &cfg, 1));

    clock_display_state.dev.cascade_size = 1;
    clock_display_state.dev.digits = 0;
    clock_display_state.dev.mirrored = true;
    clock_display_state.dev.bcd = false;

    uint8_t counter = 0;
    while ((max7219_init_desc(&clock_display_state.dev, SPI_HOST_NAME, 1e6, LCD_PIN_NUM_CS) != ESP_OK) && 
            (counter < MAX_TRY_CNT)) 
    {
        ESP_LOGE(TAG_DISP, "cannot max7219_init_desc");
        vTaskDelay(pdMS_TO_TICKS(1000));
        counter++;
    }
    if (counter == MAX_TRY_CNT)
    {
        ESP_LOGE(TAG_DISP, "MAX7219 init failed!");
        return;
    }

    counter = 0;
    while ((max7219_init(&clock_display_state.dev) != ESP_OK) &&
            (counter < MAX_TRY_CNT))
    {
        ESP_LOGE(TAG_DISP, "cannot max7219_init");
        vTaskDelay(pdMS_TO_TICKS(1000));
        counter++;
    }
    if (counter == MAX_TRY_CNT)
    {
        ESP_LOGE(TAG_DISP, "MAX7219 init failed!");
        return;
    }

    max7219_clear(&clock_display_state.dev);
    max7219_set_brightness(&clock_display_state.dev, 4);
    static char test_str[5] = "INIT";
    test_str[4] = 0;
    max7219_draw_text_7seg(&clock_display_state.dev, 0, (const char *)test_str);
    vTaskDelay(300 / portTICK_PERIOD_MS);

    clock_display_state.is_init_flag = 1;
}

/// @brief Redraw information at the LED display, based on current mode
/// @param  
void clock_display_handler(void)
{
    if (clock_display_state.is_init_flag == 0)
        return;

    menu_mode_t menu_mode = mode_get_curr_value();
    switch (menu_mode)
    {
        case MENU_MODE_BASIC:
        case MENU_SELECTOR:
        case MENU_MODE_SUN_INFO:
            clock_display_time();
            break;

        case MENU_MODE_CO2_1MIN:
        case MENU_MODE_CO2_10MIN:
            clock_display_co2();
            break;
    
        default:
            break;
    }

    if (clock_display_state.change_brightness)
    {
        clock_display_state.change_brightness = false;
        max7219_set_brightness(&clock_display_state.dev, clock_display_state.target_brightness);
    }
}

/// @brief Draw CO2 value at the LED display
/// @param  
void clock_display_co2(void)
{
    char tmp_buf[8];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    uint16_t co2_value = co2_last_value();

    if (co2_value < 1000)
    {
        snprintf(tmp_buf, 7, " %u ", co2_value);
    }
    else
    {
        snprintf(tmp_buf, 7, "%u ", co2_value);
    }
    tmp_buf[4] = 0;//termination
    max7219_draw_text_7seg(&clock_display_state.dev, 0, (const char *)tmp_buf);
}

/// @brief Display current time
/// @param  
void clock_display_time(void)
{
    time_t now_binary;
    struct tm timeinfo;
    char strtime_buf[64];
    memset(strtime_buf, 0, sizeof(strtime_buf));

    if (time_update_is_ok())
    {
        now_binary = time(NULL);
        localtime_r(&now_binary, &timeinfo);
        if (now_binary & 1)
            strftime(strtime_buf, sizeof(strtime_buf), "%H.%M", &timeinfo);//with point
        else
            strftime(strtime_buf, sizeof(strtime_buf), "%H%M", &timeinfo);
    }
    else
    {
        strtime_buf[0] = '-';
        strtime_buf[1] = '-';
        strtime_buf[2] = '-';
        strtime_buf[3] = '-';
        strtime_buf[4] = 0;//termination
    }

    max7219_draw_text_7seg(&clock_display_state.dev, 0, (const char *)strtime_buf);
}

void clock_display_change_brightness(uint8_t new_value)
{
    if (new_value > 15)
        new_value = 15;
    clock_display_state.change_brightness = true;
    clock_display_state.target_brightness = new_value;
}

uint8_t clock_display_get_brightness_code(void)
{
    return clock_display_state.target_brightness;
}
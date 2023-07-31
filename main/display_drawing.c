//LED display is contolled from here

#include "clock_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "time_update.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "co2_sensor.h"
#include "display_functions.h"
#include "oled_display.h"
#include "images.h"
#include "mavlink_handling.h"
#include "protocol_examples_common.h"
#include "weather_json_parse.h"
#include "mode_controlling.h"
#include "menu_selector.h"
#include "weather_update.h"
#include "light_sensor.h"


#include "display_drawing.h"

typedef struct 
{
    uint8_t             is_init_flag;
    bool                change_brightness;
    uint8_t             target_brightness;
} display_display_state_t;

#define VFD_BASIC_LEFT_CENTER_X     32 //in pixels

#define VFD_CO2_MAX_HEIGHT          (64 - 11)

display_display_state_t display_display_state = {.is_init_flag = 0, .target_brightness = 255,};

const char* month_strings[] = 
{
    "ЯНВАРЯ",
    "ФЕВРАЛЯ",
    "МАРТА",
    "АПРЕЛЯ",
    "МАЯ",
    "ИЮНЯ",
    "ИЮЛЯ",
    "АВГУСТА",
    "СЕНТЯБРЯ",
    "ОКТЯБРЯ",
    "НОЯБРЯ",
    "ДЕКАБРЯ",
};

const char* week_strings[] = 
{
    "ВОСКР",
    "ПОНЕД",
    "ВТОРНИК",
    "СРЕДА",
    "ЧЕТВЕРГ",
    "ПЯТНИЦА",
    "СУББОТА",
};

extern mavlink_handling_state_t mavlink_handling_state;
extern weather_values_t weather_values;



void display_draw_basic_mode_date(void);
void display_draw_basic_mode_co2(void);
void display_draw_basic_temperatures(void);
void display_draw_basic_wifi(void);
void display_draw_weather_condition_icon(uint16_t x, uint16_t y, weather_condition_t state);

void display_draw_basic_mode(void);
void display_draw_co2_1min_mode(void);
void display_draw_co2_10min_mode(void);
void display_draw_not_implemented_mode(void);
void display_draw_co2_bar(uint16_t *data);
void display_draw_co2_time_markers(uint8_t period);
void display_draw_internet_weather(uint8_t is_actual);
void display_draw_new_year_mode(int32_t year);
bool display_need_disable(void);
int32_t display_is_new_year_time(void);


//*************************************************************************************

/// @brief Init drawing and HW
/// @param  
void display_drawing_init(void)
{
    oled_init();
    display_draw_utf8_string("INIT DISPLAY", VFD_BASIC_LEFT_CENTER_X, 0, FONT_SIZE_11, 64);
    display_update();
}

/// @brief Redraw information at the VDF display, based on current mode
/// @param  
void display_drawing_handler(void)
{
    if (display_display_state.change_brightness)
    {
        display_display_state.change_brightness = false;
        oled_set_brightness(display_display_state.target_brightness);
    }

    //positive if it is NY time
    int32_t ny_year = display_is_new_year_time();

    if (display_need_disable() && (ny_year < 0))
    {
        //Full dark
        display_clear_framebuffer();
        display_update();
        return;
    }

    menu_mode_t menu_mode = mode_get_curr_value();
    switch (menu_mode)
    {
        case MENU_MODE_BASIC:
            if (ny_year > 0)
            {
                display_draw_new_year_mode(ny_year);
                return;
            }
            display_draw_basic_mode();
            break;

        case MENU_MODE_CO2_1MIN:
            display_draw_co2_1min_mode();
            break;

        case MENU_MODE_CO2_10MIN:
            display_draw_co2_10min_mode();
            break;

        case MENU_SELECTOR:
            menu_selector_draw();
            break;
    
        default:
            display_draw_not_implemented_mode();
            break;
    }
}

bool display_need_disable(void)
{
    if (light_sensor_is_night() == false)
        return false;

    //light sensor is dark if we get here

    struct tm timeinfo;
    if (time_update_is_ok())
    {
        time_t now_binary = time(NULL);
        localtime_r(&now_binary, &timeinfo);
        if ((timeinfo.tm_hour > 21) || (timeinfo.tm_hour < 9))//night
            return true;
        else
            return false;
    }
    else
    {
        return false;
    }
}

/// @brief Return year if it is new year time
/// @param  
/// @return -1 if it is not NY time, or year otherwise
int32_t display_is_new_year_time(void)
{
    struct tm timeinfo;
    if (time_update_is_ok())
    {
        time_t now_binary = time(NULL);
        localtime_r(&now_binary, &timeinfo);
        if ((timeinfo.tm_hour == 0) && (timeinfo.tm_mon == 0) && (timeinfo.tm_mday == 1) && (timeinfo.tm_year > 2000))//1 jan
            return timeinfo.tm_year;
        else
            return -1;
    }
    else
    {
        return -1;
    }
}

void display_draw_co2_1min_mode(void)
{
    display_clear_framebuffer();
    display_draw_utf8_string("CO2: 1 МИН", 15, 0, FONT_SIZE_8, 0);
    display_draw_co2_bar(co2_get_1min_data());
    display_draw_co2_time_markers(10);//every 10 min
    display_update();
}

void display_draw_co2_10min_mode(void)
{
    display_clear_framebuffer();
    display_draw_utf8_string("CO2: 10 МИН", 15, 0, FONT_SIZE_8, 0);
    display_draw_co2_bar(co2_get_10min_data());
    display_draw_co2_time_markers(6);//every 1 hour
    display_update();
}

void display_draw_co2_bar(uint16_t *data)
{
  uint8_t i;
  uint32_t tmp_val = 0;
  for (i = 0; i < 128; i++) //i - x position
  {
    
    if (data[i] < 401)
    {
      tmp_val = 0;
      if (data[i] == 5) 
        display_draw_vertical_line_dotted(
            127 - i, DISPLAY_HEIGHT - 2, DISPLAY_HEIGHT- VFD_CO2_MAX_HEIGHT);
    }
    else if (data[i] < 2000) 
        tmp_val = (data[i] - 400) * VFD_CO2_MAX_HEIGHT / (2000 - 400);
    else 
        tmp_val = VFD_CO2_MAX_HEIGHT;//max value

    if (tmp_val > VFD_CO2_MAX_HEIGHT)
        tmp_val = VFD_CO2_MAX_HEIGHT;

    display_draw_vertical_line(127 - i, DISPLAY_HEIGHT - 2, DISPLAY_HEIGHT - tmp_val - 2);
  }
}

/// @brief Draw grid at the lowest line
/// @param period - in pixels
void display_draw_co2_time_markers(uint8_t period)
{
  for (uint8_t i = 0; i < 128; i = i + period) 
    display_set_pixel(127 - i, DISPLAY_HEIGHT - 1);//lowest line
}

void display_draw_not_implemented_mode(void)
{
    display_clear_framebuffer();
    display_draw_utf8_string("ERROR!", 0, 0, FONT_SIZE_11, 0);
    display_draw_utf8_string("NOT IMPLEMENTED!", 0, 15, FONT_SIZE_11, 0);
    display_update();
}

// ********************************************************************************

/// @brief Draw basic information - date, CO2
/// @param  
void display_draw_basic_mode(void)
{
    display_clear_framebuffer();
    display_draw_basic_mode_date();
    display_draw_basic_mode_co2();
    display_draw_basic_temperatures();
    display_draw_basic_wifi();

    display_update();
}

void display_draw_basic_wifi(void)
{
    if (example_is_wifi_enabled())
    {
        draw_image((uint8_t*)wifi_icon, 0, 0);
    }
}

void display_draw_basic_temperatures(void)
{
    // DRAW RADIO WEATHER
    char str_buf[20];
    if (mavlink_handling_state.data_actual)
    {
        snprintf(str_buf, 64, "%i\xAC", mavlink_handling_state.measured_temp1_deg); //0xac = 172dec = deg sumbol
        display_draw_utf8_string(str_buf, 81, 48, FONT_SIZE_11, LCD_CENTER_X_FLAG);

        snprintf(str_buf, 64, "%iC", mavlink_handling_state.measured_temp2_deg);
        display_draw_utf8_string(str_buf, 105, 48+4, FONT_SIZE_8, 0);
    }
    else
    {
        display_draw_utf8_string("ERR. RADIO!", 65, 48, FONT_SIZE_8, 0);
    }

    //DRAW YANDEX WEATHER
    /*
    weather_values.is_actual_flag = 1;
    weather_values.now_temp_deg = -10;
    weather_values.forecast1_temp_deg = -10;
    weather_values.forecast2_temp_deg = -10;
    weather_values.forecast1_state = WEATHER_COND_STRONG_RAIN;
    weather_values.forecast2_state = WEATHER_COND_SNOW;
    */
    
    if (weather_values.is_actual_flag)
    {
        display_draw_internet_weather(1);
    }
    else
    {
        if (weather_update_is_too_old() == false)
        {
            //Not really actual but not old
            display_draw_internet_weather(0);
        }
        else
        {
            if (weather_update_is_updating())//network is busy
            {
                display_draw_string("UPDATING", VFD_BASIC_LEFT_CENTER_X - 3, 20, FONT_SIZE_11, LCD_CENTER_X_FLAG);
                display_draw_string("WEATHER", VFD_BASIC_LEFT_CENTER_X - 3, (20 + 14), FONT_SIZE_11, LCD_CENTER_X_FLAG);
            }
        }
    }
}

void display_draw_internet_weather(uint8_t is_actual)
{
    char str_buf[64];
    //NOW temperature
    int32_t abs_now_t_deg = abs(weather_values.now_temp_deg);
    snprintf(str_buf, 64, "%li", abs_now_t_deg);
    uint16_t width = display_draw_string(str_buf, VFD_BASIC_LEFT_CENTER_X-4, 0, FONT_SIZE_32, LCD_CENTER_X_FLAG);
    display_draw_string("\xAC", VFD_BASIC_LEFT_CENTER_X-4 + width/2, 17, FONT_SIZE_11, 0);//0xac = 172dec = deg sumbol

    snprintf(str_buf, 64, "%li", weather_values.forecast1_temp_deg);
    display_draw_utf8_string(str_buf, 15, 33, FONT_SIZE_11, LCD_CENTER_X_FLAG);
    
    snprintf(str_buf, 64, "%li", weather_values.forecast2_temp_deg);
    display_draw_utf8_string(str_buf, 49, 33, FONT_SIZE_11, LCD_CENTER_X_FLAG);
    
    if (weather_values.forecast1_temp_deg > -9)
        display_draw_weather_condition_icon(7, 49, weather_values.forecast1_state);
    else
        display_draw_weather_condition_icon(7+3, 49, weather_values.forecast1_state);
    
    if (weather_values.forecast2_temp_deg > -9)
        display_draw_weather_condition_icon(41, 49, weather_values.forecast2_state);
    else
        display_draw_weather_condition_icon(41+3, 49, weather_values.forecast2_state);

    if (is_actual == 0)
        display_draw_utf8_string("!", VFD_BASIC_LEFT_CENTER_X, 45, FONT_SIZE_11, 0);
}

void display_draw_weather_condition_icon(uint16_t x, uint16_t y, weather_condition_t state)
{
    const uint8_t *image;
    switch (state)
    {
    case WEATHER_COND_ERROR:
        return;

    case WEATHER_COND_CLEAR:
        image = sun_icon;
        break;

    case WEATHER_COND_CLOUDY:
        image = cloud_icon;
        break;

    case WEATHER_COND_LIGHT_RAIN:
        image = rain1_icon;
        break;

    case WEATHER_COND_MED_RAIN:
        image = rain2_icon;
        break;

    case WEATHER_COND_STRONG_RAIN:
        image = rain3_icon;
        break;

    case WEATHER_COND_HAIL:
        image = hail_icon;
        break;

    case WEATHER_COND_SNOW:
        image = snow_icon;
        break;
    
    default:
        return;
    }

    draw_image((uint8_t*)image, x, y);
}

void display_draw_basic_mode_co2(void)
{
    char str_buf[64];
    display_draw_utf8_string("CO2:", 66, 30+4, FONT_SIZE_8, 0);
    if (co2_is_sensor_ok())
    {
        uint16_t co2_value = co2_last_value();
        snprintf(str_buf, 64, "%u", co2_value);
    }
    else
    {
        snprintf(str_buf, 64, "ERR!");
    }
    display_draw_utf8_string(str_buf, 90, 30, FONT_SIZE_11, 0);
}

void display_draw_basic_mode_date(void)
{
    struct tm timeinfo;
    char strtime_buf[64];
    if (time_update_is_ok())
    {
        time_t now_binary = time(NULL);
        localtime_r(&now_binary, &timeinfo);
        if (timeinfo.tm_mon > 11)
            timeinfo.tm_mon = 11;
        if (timeinfo.tm_wday > 6)
            timeinfo.tm_wday = 6;
        //Day-Month
        snprintf(strtime_buf, 64, "%u %s", timeinfo.tm_mday, (char*)month_strings[timeinfo.tm_mon]);
        display_draw_utf8_string(strtime_buf, 91, 1, FONT_SIZE_11, LCD_CENTER_X_FLAG);

        //Week
        display_draw_utf8_string(
            (char*)week_strings[timeinfo.tm_wday], 91, 15, FONT_SIZE_11, LCD_CENTER_X_FLAG);
    }
    else
    {
        display_draw_utf8_string("ВРЕМЯ", 67, 1, FONT_SIZE_11, 0);
        display_draw_utf8_string("НЕИЗВ!", 67, 15 , FONT_SIZE_11, 0);
    }
}

void display_draw_new_year_mode(int32_t year)
{
    char strtime_buf[64];
    display_clear_framebuffer();
    display_draw_utf8_string("С НОВЫМ ГОДОМ!", 64, 0, FONT_SIZE_11, LCD_CENTER_X_FLAG);

    snprintf(strtime_buf, 64, "%li", year);
    display_draw_utf8_string(strtime_buf, 64, 15, FONT_SIZE_32, LCD_CENTER_X_FLAG);
    display_update();
}

void display_change_brightness(uint8_t new_value)
{
    display_display_state.change_brightness = true;
    display_display_state.target_brightness = new_value;
}

uint8_t display_get_brightness_code(void)
{
    return display_display_state.target_brightness;
}

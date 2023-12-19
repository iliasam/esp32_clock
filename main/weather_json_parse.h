#ifndef _WEATHER_JSON_PARSE_H
#define _WEATHER_JSON_PARSE_H

#include <time.h>

typedef enum
{
    WEATHER_COND_ERROR = 0,//wrong parsing
    WEATHER_COND_CLEAR,//ясно
    WEATHER_COND_CLOUDY,//облака
    WEATHER_COND_LIGHT_RAIN,
    WEATHER_COND_MED_RAIN,
    WEATHER_COND_STRONG_RAIN,
    WEATHER_COND_HAIL,//град
    WEATHER_COND_SNOW,
} weather_condition_t;

typedef struct
{
    int32_t now_temp_deg;
    int32_t forecast1_temp_deg;//6h
    int32_t forecast2_temp_deg;//12h

    struct tm sunrise_time;
    struct tm sunset_time;

    weather_condition_t forecast1_state;
    weather_condition_t forecast2_state;

    /// @brief Data parsed good flag
    uint8_t is_actual_flag;
    /// @brief Do not changed during parsing
    int32_t last_parse_result;
    /// @brief Do not changed during parsing
    uint32_t last_bytes_rx;
    time_t good_parse_timestamp;

} weather_values_t;

int32_t parse_weather_json(char *data);

#endif
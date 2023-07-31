#include "clock_config.h"
#include "string.h"
#include "stdio.h"
#include <time.h>

#include "weather_json_parse.h"

weather_values_t weather_values;

//***************************************************************************


char *find_json_name(char *source_text, char *name_text);
int32_t get_json_integer(char **source_text);
char *get_json_string_value(char *source_text);
weather_condition_t parse_json_condition(char *source_text);
char *sstrstr(char *source_text, char *needle, size_t length);

int32_t parse_weather_json(char *data)
{
    char *find_res_p;
    //time_t tmp_timestamp = weather_values.good_parse_timestamp;//save
    //memset(&weather_values, 0, sizeof(weather_values));
    weather_values.is_actual_flag = 0;
    //weather_values.good_parse_timestamp = tmp_timestamp;//restore

    char *forecast_condition1_p = NULL;
    char *forecast_condition2_p = NULL;

    find_res_p = find_json_name(data, "temp");
    if (find_res_p == NULL)
        return -1;
    weather_values.now_temp_deg = get_json_integer(&find_res_p);
    data = find_res_p;

    // ***** search in "parts" [0]
    find_res_p = find_json_name(data, "temp_avg");
    if (find_res_p == NULL)
        return -2;
    weather_values.forecast1_temp_deg = get_json_integer(&find_res_p);
    data = find_res_p;

    find_res_p = find_json_name(data, "condition");
    find_res_p = get_json_string_value(find_res_p);
    if (find_res_p == NULL)
        return -3;
    forecast_condition1_p = find_res_p;

    // ***** search in "parts" [1]
    find_res_p = find_json_name(data, "temp_avg");
    if (find_res_p == NULL)
        return -4;
    weather_values.forecast2_temp_deg = get_json_integer(&find_res_p);
    data = find_res_p;

    find_res_p = find_json_name(data, "condition");
    find_res_p = get_json_string_value(find_res_p);
    if (find_res_p == NULL)
        return -5;
    forecast_condition2_p = find_res_p;

    weather_values.forecast1_state = parse_json_condition(forecast_condition1_p);
    weather_values.forecast2_state = parse_json_condition(forecast_condition2_p);

    if ((weather_values.forecast1_state == WEATHER_COND_ERROR) && 
        (weather_values.forecast2_state == WEATHER_COND_ERROR))
    {
        return -6;
    }

    weather_values.is_actual_flag = 1;
    weather_values.good_parse_timestamp  = time(NULL);
    return 1;
}

weather_condition_t parse_json_condition(char *source_text)
{
    //Condition starts from text, ends with '"'
    char *end_str = strchr(source_text, '"');
    if (end_str == NULL)
        return WEATHER_COND_ERROR;

    uint16_t size = (uint16_t)(end_str - source_text);

    if (sstrstr(source_text, "clear", size) != NULL)
        return WEATHER_COND_CLEAR;

    if (sstrstr(source_text, "partly-cloudy", size) != NULL)
        return WEATHER_COND_CLEAR;

    if (sstrstr(source_text, "cloudy", size) != NULL)
        return WEATHER_COND_CLOUDY;

    if (sstrstr(source_text, "overcast", size) != NULL)
        return WEATHER_COND_CLOUDY;

    if (sstrstr(source_text, "drizzle", size) != NULL)
        return WEATHER_COND_LIGHT_RAIN;

    if (sstrstr(source_text, "light-rain", size) != NULL)
        return WEATHER_COND_LIGHT_RAIN;

    if (sstrstr(source_text, "moderate-rain", size) != NULL)
        return WEATHER_COND_MED_RAIN;

    if (sstrstr(source_text, "heavy-rain", size) != NULL)
        return WEATHER_COND_STRONG_RAIN;

    if (sstrstr(source_text, "showers", size) != NULL)
        return WEATHER_COND_STRONG_RAIN;

    if (sstrstr(source_text, "rain", size) != NULL)
        return WEATHER_COND_MED_RAIN;

    if (sstrstr(source_text, "snow", size) != NULL)
        return WEATHER_COND_SNOW;

    if (sstrstr(source_text, "hail", size) != NULL)
        return WEATHER_COND_HAIL;

    if (sstrstr(source_text, "thunderstorm", size) != NULL)
        return WEATHER_COND_STRONG_RAIN;

    return WEATHER_COND_ERROR;
}


/// @brief Find position of a substring, return beginning
/// @param source_text 
/// @param name_text 
/// @return 
char *find_json_name(char *source_text, char *name_text)
{
    char tmp_str[32];
    char *find_res_p = NULL;
    int length = sprintf(tmp_str, "\"%s\"", name_text); //generate "name" with quotation marks
    tmp_str[length] = 0;//terminate
    find_res_p = strstr(source_text, tmp_str);
    return find_res_p;
}

char *sstrstr(char *source_text, char *needle, size_t length)
{
    size_t needle_length = strlen(needle);
    size_t i;
    for (i = 0; i < length; i++) 
    {
        if (i + needle_length > length) 
        {
            return NULL;
        }
        if (strncmp(&source_text[i], needle, needle_length) == 0) 
        {
            return &source_text[i];
        }
    }
    return NULL;
}


/// @brief Extract integer value and move pointer to the position of number start
/// @param source_text_p 
/// @return Parsed ingeger value
int32_t get_json_integer(char **source_text_p)
{
    char *find_res_p = strchr(*source_text_p, ':');
    if (find_res_p == NULL)
        return 0;

    find_res_p++;//skip ":"

    while ((*find_res_p) == ' ') //skip "_"
    {
        find_res_p++;
    }
    *source_text_p = find_res_p;//move pointer

    int res = atoi(find_res_p);
    return res;
}

// Return pointer to the string value starts with '"'
// "source_text" is pointing to the name before the value
//like "condition":"light-snow" - name:strig
char *get_json_string_value(char *source_text)
{
    char *find_res_p = strchr(source_text, ':');
    if (find_res_p == NULL)
        return NULL;

    find_res_p = strchr(find_res_p, '"');
    if (find_res_p == NULL)
        return NULL;
    return (find_res_p + 1);
}

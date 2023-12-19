#include "clock_config.h"
#include <string.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_intr_alloc.h"
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include "light_sensor.h"
#include "clock_display.h"
#include "display_drawing.h"


//*********************************************************************
#define LIGHT_SENS_ADC_RESOLUTION       ADC_WIDTH_BIT_12
#define LIGHT_SENS_ADC_ATTEN            ADC_ATTEN_DB_11
#define LIGHT_SENS_ADC                  ADC_UNIT_1
#define DEFAULT_VREF                    1100

#define LIGHT_SENS_MV_REF               3300.0f
//In KOhm
#define LIGHT_SENS_LOWER_RES            4.7f

#define DIMMING_LED_MAX_CODE            15
#define DIMMING_OLED_MAX_CODE           255
//Difference in brightnes
#define DIMMING_OLED_TO_LED_DIFF        6.4f

#define LIGHT_SENS_FILTER_COEF          0.8f
#define LIGHT_SENS_SLOW_FILTER_COEF     0.9f

// 5min; 10 is 1s/100ms
#define LIGHT_SENS_NIGHT_DETECT_TIME    (5 * 60 * 10)

static esp_adc_cal_characteristics_t *adc_chars;
int32_t light_sensor_raw = 0;
uint32_t light_sensor_volt_mv = 0;
float light_sensor_volt_mv_filtered = 0.0f;
float light_sensor_volt_mv_slow_filtered = 0.0f;
float light_sensor_lux = 0.0f;

/// Counting up if it is dark
uint32_t light_sensor_night_counter = 0;

bool dimming_auto_enabled = true;

//Used for hysteresis calculation
//[0]=0; [1] = 3.33; [2] = 10.0; [15] = 96.6
//So when val>[n], n level will be selected
float led_switch_levels[DIMMING_LED_MAX_CODE + 1];

//*******************************************************

static void light_sensor_check_efuse(void);
uint32_t correct_adc(uint32_t adc_value_mv);

void light_sensor_process_hight_light(void);


//*********************************************************************

static void light_sensor_check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) 
    {
        printf("eFuse Two Point: Supported\n");
    } 
    else 
    {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) 
    {
        printf("eFuse Vref: Supported\n");
    } 
    else 
    {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) 
    {
        printf("eFuse Two Point: Supported\n");
    } 
    else 
    {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

void light_sensor_init(void)
{
    //Fill hysteresis table "led_switch_levels". 
    float tmp_percent = 0.0f;
    uint8_t led_usage_prev = 0;
    while (tmp_percent < 100.0f)
    {
        float led_usage_f = tmp_percent * DIMMING_LED_MAX_CODE / 100.0f;
        uint8_t led_usage = (uint8_t)roundf(led_usage_f);
        if (led_usage_prev != led_usage)
        {
            led_switch_levels[led_usage_prev + 1] = tmp_percent;
            led_usage_prev = led_usage;
        }
        tmp_percent += 0.01f;
    }
    led_switch_levels[0] = 0.0f;

    light_sensor_check_efuse();
    adc1_config_width(LIGHT_SENS_ADC_RESOLUTION);
    adc1_config_channel_atten(LIGHT_SENS_ADC_CH, LIGHT_SENS_ADC_ATTEN);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        LIGHT_SENS_ADC, LIGHT_SENS_ADC_ATTEN, LIGHT_SENS_ADC_RESOLUTION, DEFAULT_VREF, adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

//called every 100 ms
void light_sensor_handling(void)
{
    light_sensor_raw = adc1_get_raw((adc1_channel_t)LIGHT_SENS_ADC_CH);
    light_sensor_volt_mv = correct_adc(esp_adc_cal_raw_to_voltage(light_sensor_raw, adc_chars));

    light_sensor_volt_mv_filtered = (1.0f - LIGHT_SENS_FILTER_COEF) * light_sensor_volt_mv + 
            LIGHT_SENS_FILTER_COEF * light_sensor_volt_mv_filtered;

    light_sensor_volt_mv_slow_filtered = (1.0f - LIGHT_SENS_SLOW_FILTER_COEF) * light_sensor_volt_mv + 
            LIGHT_SENS_SLOW_FILTER_COEF * light_sensor_volt_mv_slow_filtered;

    light_sensor_lux = light_lensor_get_lux(light_sensor_get_volt());
    //printf("Raw: %d\tVoltage: %dmV\n", light_sensor_raw, voltage);

    if (dimming_auto_enabled)
        dimming_auto_controlling();

    light_sensor_process_hight_light();
}

//called every 100 ms
void light_sensor_process_hight_light(void)
{
    // This value is slower
    uint32_t light_sensor_lux_filt = (uint32_t)light_lensor_get_lux((uint32_t)light_sensor_volt_mv_slow_filtered);
    bool strong_light_flag = (light_sensor_lux_filt > 3);

    if (strong_light_flag)
        light_sensor_reset_night_counter();
    else
        light_sensor_night_counter++;
}

void light_sensor_reset_night_counter(void)
{
    light_sensor_night_counter = 0;
}

bool light_sensor_is_night(void)
{
    return (light_sensor_night_counter > LIGHT_SENS_NIGHT_DETECT_TIME);
}

/// @brief Calculate ADC compensation
/// @param adc_value_mv - measured ADC voltage
/// @return Voltage in mv
uint32_t correct_adc(uint32_t adc_value_mv)
{
    int32_t compensation = (int32_t)(0.01f * adc_value_mv + 33.0f);
    if (adc_value_mv > 2500)
    {
         compensation = (int32_t)(-0.0715f * adc_value_mv + 238.0f);
    }
    return adc_value_mv - compensation;
}

/// @brief Return filtered voltage og light sensor
/// @param  
/// @return Voltage in mv
uint16_t light_sensor_get_volt(void)
{
    if (light_sensor_volt_mv_filtered < 0.0f)
        return 0;
    if (light_sensor_volt_mv_filtered > LIGHT_SENS_MV_REF)
        return (uint16_t)LIGHT_SENS_MV_REF;
    return (uint16_t)light_sensor_volt_mv_filtered;
}

float light_lensor_get_cur_lux(void)
{
    return light_sensor_lux;
}

float light_lensor_get_lux(uint32_t volt_mv)
{
    if ((volt_mv < 10) || (volt_mv > LIGHT_SENS_MV_REF))
        return 0.0f;

    float resistance_kohm = 
        LIGHT_SENS_LOWER_RES * (LIGHT_SENS_MV_REF - (float)volt_mv) / (float)volt_mv;
    if (resistance_kohm < 0.0f)
        return 0.0f;

    float lux = 2747.738f * powf(resistance_kohm, -1.50915f);
    if (lux < 0.0f)
        return 0.0f;

    if (lux > 30000)
        return 30000.0f;

    return lux;
}

/// @brief Set brightness of both displays
/// @param percent - brightness percent
void dimming_set_common_brightness(float percent)
{
    if (percent > 100.0f)
        percent = 100.0f;
    if (percent < 0.0f)
        percent = 0.0f;
    
    float led_usage_f = percent * DIMMING_LED_MAX_CODE / 100.0f;

    float oled_usage_f = percent * DIMMING_OLED_MAX_CODE / 100.0f * DIMMING_OLED_TO_LED_DIFF;
    if (oled_usage_f > (float)DIMMING_OLED_MAX_CODE)
        oled_usage_f = DIMMING_OLED_MAX_CODE;
    if (oled_usage_f < 10.0f)
        oled_usage_f = 10.f;
    
     clock_display_change_brightness((uint8_t)roundf(led_usage_f));
     display_change_brightness((uint8_t)roundf(oled_usage_f));
}

/// @brief Calculate target display brightness based on light sensor value and set it
/// @param  
void dimming_auto_controlling(void)
{
    float target_percent_f = 0.0f;
    
    if (light_sensor_lux < 100.0f)
    {
        //A kind of hysteresis
        if (light_sensor_lux < 2.1f)
            target_percent_f = 0;
        else if (light_sensor_lux > 3.0f)
            target_percent_f = 2.9f * logf(light_sensor_lux) + 1.6f;
        else
            return;
    }
    else
    {
        target_percent_f = 0.0085859f * light_sensor_lux + 14.14f;
    }

    if (light_sensor_lux > 5.0f)
    {
        uint8_t led_value = DIMMING_LED_MAX_CODE;
        for (int i = 1; i <= DIMMING_LED_MAX_CODE; i++)
        {
            float diff = fabs(target_percent_f - led_switch_levels[i]);
            if (diff < 0.4f)
            {
                return;//not stable
            }
        }
    }

    dimming_set_common_brightness(target_percent_f);
}

/// @brief Change auto dimming mode
/// @param is_enabled 
void dimming_change_auto_controlling(bool is_enabled)
{
    dimming_auto_enabled = is_enabled;
}
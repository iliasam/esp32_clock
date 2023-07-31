#ifndef _CLOCK_CONFIG_H
#define _CLOCK_CONFIG_H

#define I2C_PORT_NUM			I2C_NUM_1        /*!< I2C port number for master dev */
#define I2C_SCL_IO				22               /*!< gpio number for I2C master clock */
#define I2C_SDA_IO				21               /*!< gpio number for I2C master data  */
#define I2C_FREQ_HZ				100000           /*!< I2C master clock frequency */

#define SNTP_SERVER             "pool.ntp.org"
#define SNTP_UPDATE_TIME_S      (8 * 60 * 60)
#define SNTP_LONG_FAIL_UPDATE_TIME_S      (10 * 60) //10min

#define SPI_HOST_NAME           VSPI_HOST
#define G_DISP_SPI_HOST_NAME    HSPI_HOST

#define LCD_PIN_NUM_MOSI        23
#define LCD_PIN_NUM_CLK         18
#define LCD_PIN_NUM_CS          5

#define G_DISP_PIN_NUM_MOSI     26
#define G_DISP_PIN_NUM_CLK      25
#define G_DISP_PIN_NUM_CS       27
#define G_DISP_PIN_NUM_CD       2
#define G_DISP_PIN_NUM_RES      13//TCK


#define CO2_SENSOR_UART_NUM     UART_NUM_1
#define CO2_SENSOR_TX_PIN       GPIO_NUM_33
#define CO2_SENSOR_RX_PIN       GPIO_NUM_32

#define RADIO_UPDATE_PERIOD_S    (10 * 60)

#define RADIO_UART_NUM          UART_NUM_2
#define RADIO_TX_PIN            GPIO_NUM_17
#define RADIO_RX_PIN            GPIO_NUM_16
#define RADIO_PIN_NUM_SET       GPIO_NUM_4
#define RADIO_UART_BAUD         9600

#define CLOCK_TIMEZONE          "MSK-3"

#define WEATHER_UPDATE_PERIOD_S (40 * 60)
#define WEATHER_UPDATE_PERIOD_HIGHT_S   (60 * 60) //1hour

//down
#define BUTTON1_PIN             GPIO_NUM_19 //P10
//up
#define BUTTON2_PIN             GPIO_NUM_35 //P12

#define LIGHT_SENS_ADC_PIN      GPIO_NUM_34 //P11 - ADC1_CH6 
#define LIGHT_SENS_ADC_CH       ADC_CHANNEL_6

//need #include "freertos/task.h"
#define GET_TIME_MS             (xTaskGetTickCount() * portTICK_PERIOD_MS)

#define START_TIMER(x, duration)  (x = (GET_TIME_MS + duration))
#define TIMER_ELAPSED(x)  ((GET_TIME_MS > x) ? 1 : 0)

#endif
#include "clock_config.h"
#include "co2_sensor.h"
#include <string.h>
#include <esp_log.h>
#include "driver/uart.h"
#include <driver/gpio.h>
#include <soc/uart_struct.h>
#include <soc/uart_reg.h>
#include "esp_intr_alloc.h"
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//static const char *TAG = "co2_sensor";

#define CO2_SENSOR_BAUD             (9600)
#define CO2_SENSOR_UART_BUF_SIZE    (256)

#define CO2_SENSOR_REQ_SIZE         (9)
#define CO2_POINTS_CNT              (128)

//Period of sending requests to the sensor
#define CO2_UPDATE_PERIOD_S         (10)

#define CO2_SAVING_1M_PERIOD_S      (60)

typedef enum
{
    CO2_SENSOR_FAIL = 0,
    CO2_SENSOR_WRONG_DATA,
    CO2_SENSOR_OK,

} co2_sensor_state_t;

typedef struct 
{
    co2_sensor_state_t state;
    uint16_t last_co2_value;

    uint16_t fifo_1m[CO2_POINTS_CNT];
    uint16_t fifo_10m[CO2_POINTS_CNT];
    uint8_t min_counter;

    time_t sensor_req_timestamp;
    time_t saving_timestamp;
} co2_sensor_data_t;


//*********************************************************************

uint8_t co2_sensor_rxbuf[CO2_SENSOR_UART_BUF_SIZE];

void co2_uart_initialize(void);
//static intr_handle_t handle_console;

co2_sensor_data_t co2_sensor_data;

//*********************************************************************

void co2_fifo_1m_add_val(uint16_t value);
void co2_fifo_10m_add_val(uint16_t value);
void co2_fifo_saving(void);

//*********************************************************************


uint16_t *co2_get_1min_data(void)
{
    return co2_sensor_data.fifo_1m;
}

uint16_t *co2_get_10min_data(void)
{
    return co2_sensor_data.fifo_10m;
}


/*
static void IRAM_ATTR uart_intr_handle(void *arg)
{
    volatile uart_dev_t *uart = arg;
    uint16_t rx_fifo_len, status;
    uint16_t i = 0;
  
    status = uart->int_st.val; // read UART interrupt Status
    rx_fifo_len = uart->status.rxfifo_cnt; // read number of bytes in UART buffer
  
    while(rx_fifo_len){
        co2_sensor_rxbuf[i++] = uart->fifo.rw_byte; // read all bytes
        rx_fifo_len--;
    }
  
    // after reading bytes from buffer clear UART interrupt status
    uart_clear_intr_status(CO2_SENSOR_UART_NUM, UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR);
}
*/

void co2_init(void)
{
    memset(&co2_sensor_data, 0, sizeof(co2_sensor_data));
    //Set default values
    for (uint8_t i = 0; i < CO2_POINTS_CNT; i++)
    {
        co2_sensor_data.fifo_1m[i] = 400;
        co2_sensor_data.fifo_10m[i] = 400;
    }
    co2_sensor_data.state = CO2_SENSOR_FAIL;
    co2_uart_initialize();

    co2_fifo_1m_add_val(5);
    co2_fifo_10m_add_val(5);
}

void co2_uart_initialize(void)
{
    uart_config_t uart_config = {
        .baud_rate = CO2_SENSOR_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(CO2_SENSOR_UART_NUM, CO2_SENSOR_UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CO2_SENSOR_UART_NUM, &uart_config));
    
    ESP_ERROR_CHECK(uart_set_pin(
        CO2_SENSOR_UART_NUM, CO2_SENSOR_TX_PIN, CO2_SENSOR_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

/*
    ESP_ERROR_CHECK(uart_isr_free(CO2_SENSOR_UART_NUM));

    // register new UART subroutine
	ESP_ERROR_CHECK(uart_isr_register(
        CO2_SENSOR_UART_NUM, uart_intr_handle, NULL, ESP_INTR_FLAG_IRAM, &handle_console));

    // enable RX interrupt
	ESP_ERROR_CHECK(uart_enable_rx_intr(CO2_SENSOR_UART_NUM));
    ESP_ERROR_CHECK(uart_set_rx_timeout(CO2_SENSOR_UART_NUM, 20));
    */
}

void co2_sensor_handling(void)
{
    time_t now_binary = time(NULL);
    time_t diff_s = now_binary - co2_sensor_data.sensor_req_timestamp;
    if (diff_s > CO2_UPDATE_PERIOD_S)
    {
        co2_single_request();
        co2_fifo_saving();
    }
}

void co2_single_request(void)
{
    int length = 0;

    ESP_ERROR_CHECK(uart_flush(CO2_SENSOR_UART_NUM));
    uint8_t tx_array[CO2_SENSOR_REQ_SIZE] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
    uart_write_bytes(CO2_SENSOR_UART_NUM, (const char*)tx_array, sizeof(tx_array));
    
    //ESP_ERROR_CHECK(uart_wait_tx_done(CO2_SENSOR_UART_NUM, 100));
    vTaskDelay(30 / portTICK_PERIOD_MS);
    uint8_t tmp_data[64];
    
    length = uart_read_bytes(CO2_SENSOR_UART_NUM, tmp_data, 64, 200 / portTICK_PERIOD_MS);
    if (length != CO2_SENSOR_REQ_SIZE)
    {
        co2_sensor_data.last_co2_value = 30; //error - wrong length
        co2_sensor_data.state = CO2_SENSOR_FAIL;
    }
    else
    {
        if (tmp_data[1] == 0x86)
        {
            co2_sensor_data.last_co2_value = (uint16_t)tmp_data[2] * 256 + (uint16_t)tmp_data[3];
            if (co2_sensor_data.last_co2_value > 5000)
            {
                co2_sensor_data.last_co2_value = 5100; //error - too hight co2
                co2_sensor_data.state = CO2_SENSOR_FAIL;
            }
            else
            {
                co2_sensor_data.state = CO2_SENSOR_OK;
            }
        }
        else
        {
            co2_sensor_data.state = CO2_SENSOR_FAIL;
            co2_sensor_data.last_co2_value = 10; //error - wrong data
        }
    }
    co2_sensor_data.sensor_req_timestamp = time(NULL);
    
    //ESP_LOGI(TAG, "State: %d", co2_sensor_data.state);
    //ESP_LOGI(TAG, "CO2: %d", co2_sensor_data.last_co2_value);
}

/// @brief Save tota to the FIFO
/// @param  
void co2_fifo_saving(void)
{
    time_t now_binary = time(NULL);
    time_t diff_s = now_binary - co2_sensor_data.saving_timestamp;
    if (diff_s >= CO2_SAVING_1M_PERIOD_S)
    {
        co2_sensor_data.saving_timestamp = now_binary;
        co2_fifo_1m_add_val(co2_sensor_data.last_co2_value);

        co2_sensor_data.min_counter++;
        if (co2_sensor_data.min_counter >= 10)
        {
            co2_sensor_data.min_counter = 0;
            co2_fifo_10m_add_val(co2_sensor_data.last_co2_value);
        }
    }
}

/// @brief Add value to fifo - 1 min period
/// @param value - CO2 value
void co2_fifo_1m_add_val(uint16_t value)
{
  for (uint8_t i = 127; i > 0; i--)
    co2_sensor_data.fifo_1m[i] = co2_sensor_data.fifo_1m[i-1];
  co2_sensor_data.fifo_1m[0] = value;
}

/// @brief Add value to fifo - 10 min period
/// @param value - CO2 value
void co2_fifo_10m_add_val(uint16_t value)
{
  for (uint8_t i = 127; i > 0; i--)
    co2_sensor_data.fifo_10m[i] = co2_sensor_data.fifo_10m[i-1];
  co2_sensor_data.fifo_10m[0] = value;
}

uint8_t co2_is_sensor_ok(void)
{
    if (co2_sensor_data.state == CO2_SENSOR_OK)
        return 1;
    else
        return 0;
}

uint16_t co2_last_value(void)
{
    uint16_t res = co2_sensor_data.last_co2_value;
    if (res > 5000)
        res = 5000;
    return res;
}
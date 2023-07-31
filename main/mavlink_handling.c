#include "clock_config.h"
#include "mavlink_handling.h"
#include "driver/uart.h"
#include <driver/gpio.h>
#include <soc/uart_struct.h>
#include <soc/uart_reg.h>
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "mavlink.h"
#include "mode_controlling.h"


// Defines ********************************************************************
#define MAVLINK_ACK_VALUE               1
#define MAVLINK_DRIVE_ID                10 //system id

#define MAVLINK_DRIVE_COMP_CURTAINS_ID  1  //Component

#define MAVLINT_TX_MAX_TRY_CNT          5

#define RADIO_UART_BUF_SIZE             (256)

#define RADIO_REQ_TEMP                  (1)
#define RADIO_REQ_DRIVE_STATE           (2)

// Variables ******************************************************************

mavlink_message_t tmp_tx_mav_msg;

mavlink_status_t mavlink_status; //contains state machine
mavlink_message_t mavlink_msg;

uint8_t temperatures_recieved_flag = 0;

mavlink_handling_state_t mavlink_handling_state;

// Functions ******************************************************************
void mavlink_parse_curtains_cmd(mavlink_message_t* msg);
void mavlink_send_ack_msg(mavlink_mav_command_ack_t *command_ack);
void mavlink_send_message(mavlink_message_t *msg);
void mavlink_parse_lamp_cmd(mavlink_message_t* msg);
void mavlink_parse_data_request(mavlink_message_t* msg);
void mavlink_parse_temperatures(mavlink_message_t* msg);
void mavlink_handling_uart_init(void);
uint8_t mavlink_handling_try_get_info(uint8_t data_code);
void mavlink_request_drive_state(void);
void mavlink_parse_drive_state(mavlink_message_t* msg);

//****************************************************************************

void mavlink_handling_init(void)
{
  memset(&mavlink_handling_state, 0, sizeof(mavlink_handling_state));
  mavlink_handling_uart_init();
  mavlink_handling_state.last_update_timestamp = 0;
}

void mavlink_requests_handling(void)
{
  time_t now_binary = time(NULL);
  time_t diff = now_binary - mavlink_handling_state.last_update_timestamp;

  /// Send requests for drive state
  bool drive_state_req = (mode_get_curr_submenu() == MENU_SUBITEM_DRIVE_BRIGHTNESS);

  if ((diff >= 0) && (diff < RADIO_UPDATE_PERIOD_S) && !drive_state_req)
  {
    return;
  }

  ESP_LOGI("RADIO", "Try to send requests");
  uint8_t res = 0;
  for (uint8_t try_cnt = 0; try_cnt < MAVLINT_TX_MAX_TRY_CNT; try_cnt++)
  {
    uint8_t data_code = RADIO_REQ_TEMP;
    if (drive_state_req)
      data_code = RADIO_REQ_DRIVE_STATE;
    res = mavlink_handling_try_get_info(data_code);
    if (res)
      break;
  }
  if (res)
    mavlink_handling_state.data_actual = 1;
  else
    mavlink_handling_state.data_actual = 0;

  ESP_LOGI("RADIO", "Try to send requests done");
  
  now_binary = time(NULL);
  mavlink_handling_state.last_update_timestamp = now_binary;
}

/// @brief Try to send request and listen for RX data
/// @param  
/// @return 0 if operation failled
uint8_t mavlink_handling_try_get_info(uint8_t data_code)
{
  ESP_LOGI("RADIO", "Try to send request N");
  ESP_ERROR_CHECK(uart_flush(RADIO_UART_NUM));
  if (data_code == RADIO_REQ_TEMP)
    mavlink_request_temperatures();
  else if (data_code == RADIO_REQ_DRIVE_STATE)
    mavlink_request_drive_state();
  else
    return 0;
  vTaskDelay(300 / portTICK_PERIOD_MS);

  uint8_t tmp_data[64];
  int length = uart_read_bytes(RADIO_UART_NUM, tmp_data, 64, 50 / portTICK_PERIOD_MS);
  if (length <= 0)
    return 0;

  //ESP_LOGI("RADIO", "UART rx: %d", length);

  for (uint8_t i = 0; i < length; i++)
    mavlink_parse_byte(tmp_data[i]);

  if (mavlink_handling_state.temperatures_rx_flag)
  {
    ESP_LOGI("RADIO", "Mavlink Answer received");
    return 1;
  } 

  return 0;
}

void mavlink_handling_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = RADIO_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(RADIO_UART_NUM, RADIO_UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RADIO_UART_NUM, &uart_config));
    
    ESP_ERROR_CHECK(uart_set_pin(
        RADIO_UART_NUM, RADIO_TX_PIN, RADIO_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  gpio_set_direction(RADIO_PIN_NUM_SET, GPIO_MODE_OUTPUT);
  gpio_set_level(RADIO_PIN_NUM_SET, 1);//bridge mode
}


void mavlink_parse_byte(uint8_t value)
{
  if (mavlink_parse_char(0, value, &mavlink_msg, &mavlink_status))
  {
    if (mavlink_msg.msgid == MAVLINK_MSG_ID_MAV_TEMPERATURE_VALUES)
    {
      mavlink_parse_temperatures(&mavlink_msg);
      mavlink_handling_state.temperatures_rx_flag = 1;
    }
    else if (mavlink_msg.msgid == MAVLINK_MSG_ID_MAV_CURTAINS_DRIVE_STATE)
    {
      mavlink_parse_drive_state(&mavlink_msg);
      mavlink_handling_state.temperatures_rx_flag = 1;
    }
  }
}

//Parce mavlink packet
void mavlink_parse_temperatures(mavlink_message_t* msg)
{
  mavlink_mav_temperature_values_t tmp_msg;
  mavlink_msg_mav_temperature_values_decode(msg, &tmp_msg);

  mavlink_handling_state.measured_temp1_deg = (int16_t)tmp_msg.temp2;
  mavlink_handling_state.measured_temp2_deg = (int16_t)tmp_msg.temp3;
}

//Parce mavlink packet
void mavlink_parse_drive_state(mavlink_message_t* msg)
{
  mavlink_mav_curtains_drive_state_t tmp_msg;
  mavlink_msg_mav_curtains_drive_state_decode(msg, &tmp_msg);

  mavlink_handling_state.measured_light = tmp_msg.brightness;
  ESP_LOGI("RADIO", "LIGHT: %d", mavlink_handling_state.measured_light);
}

//Send request to drive: drive should send temperatures
void mavlink_request_temperatures(void)
{
  mavlink_handling_state.temperatures_rx_flag = 0;
  mavlink_mav_data_request_t mav_request;
  mav_request.data_type = TEMPERATURE;
  mav_request.tar_system_id = MAVLINK_DRIVE_ID;
  mav_request.tar_component_id = MAVLINK_DRIVE_COMP_CURTAINS_ID;

  mavlink_msg_mav_data_request_encode(
            1, //system id
            MAVLINK_DRIVE_COMP_CURTAINS_ID, //component id
            &tmp_tx_mav_msg,
            &mav_request);

    mavlink_send_message(&tmp_tx_mav_msg);
}

//Send request to drive: drive should send its state
void mavlink_request_drive_state(void)
{
  mavlink_handling_state.temperatures_rx_flag = 0;
  mavlink_mav_data_request_t mav_request;
  mav_request.data_type = DRIVE_STATE;
  mav_request.tar_system_id = MAVLINK_DRIVE_ID;
  mav_request.tar_component_id = MAVLINK_DRIVE_COMP_CURTAINS_ID;

  mavlink_msg_mav_data_request_encode(
            1, //system id
            MAVLINK_DRIVE_COMP_CURTAINS_ID, //component id
            &tmp_tx_mav_msg,
            &mav_request);

    mavlink_send_message(&tmp_tx_mav_msg);
}

//Test command
void mavlink_request_beep(void)
{
  mavlink_mav_curtains_cmd_t mav_cmd;
  mav_cmd.command = MAV_BEEP;

  mavlink_msg_mav_curtains_cmd_encode(
            1, //system id
            MAVLINK_DRIVE_COMP_CURTAINS_ID, //component id
            &tmp_tx_mav_msg,
            &mav_cmd);

    mavlink_send_message(&tmp_tx_mav_msg);
}

// Send confirmation
void mavlink_send_ack(uint8_t msg_id)
{
  mavlink_mav_command_ack_t cmd_ack;
  cmd_ack.cmd_id = msg_id;
  cmd_ack.result = MAVLINK_ACK_VALUE;
  mavlink_send_ack_msg(&cmd_ack);
}

// Send ACK, lower level
void mavlink_send_ack_msg(mavlink_mav_command_ack_t *command_ack)
{
    mavlink_msg_mav_command_ack_encode(
            MAVLINK_DRIVE_ID,
            MAVLINK_DRIVE_COMP_CURTAINS_ID,
            &tmp_tx_mav_msg,
            command_ack);

    mavlink_send_message(&tmp_tx_mav_msg);
}

// Send mavlink to UART
void mavlink_send_message(mavlink_message_t *msg)
{
    if (msg == NULL)
        return;
            
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t size = mavlink_msg_to_send_buffer(buffer, msg);
    if (size > MAVLINK_MAX_PACKET_LEN)
        return;
    
    uart_write_bytes(RADIO_UART_NUM, (const char*)buffer, size);
}


uint8_t mavlink_get_meas_light(void)
{
  return mavlink_handling_state.measured_light;
}
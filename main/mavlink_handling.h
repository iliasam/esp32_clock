#ifndef MAVLINK_HANDLING_H
#define MAVLINK_HANDLING_H

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

// Defines ********************************************************************
typedef struct 
{
  time_t    last_update_timestamp;
  uint8_t   temperatures_rx_flag;
  uint8_t   data_actual;

  int16_t   measured_temp1_deg; //temperature1
  int16_t   measured_temp2_deg;

  uint8_t   measured_light;
} mavlink_handling_state_t;

// Functions ******************************************************************
void mavlink_handling_init(void);

void mavlink_parse_byte(uint8_t value);
void mavlink_send_ack(uint8_t msg_id);

void mavlink_request_temperatures(void);
void mavlink_request_beep(void);
void mavlink_requests_handling(void);
uint8_t mavlink_get_meas_light(void);

#endif

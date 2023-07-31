/* Includes ------------------------------------------------------------------*/
#include "keys_controlling.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "light_sensor.h"

#include "mode_controlling.h"
#include "freertos/queue.h"


/* Private typedef -----------------------------------------------------------*/
typedef enum
{
  KEY_UPPER_PRESSED = 1,
  KEY_UPPER_HOLD,
  KEY_LOWER_PRESSED,
  KEY_LOWER_HOLD,
} key_event_type_t;

/* Private define ------------------------------------------------------------*/
//Time in ms
#define KEY_HOLD_TIME_MS          900

//Time in ms
#define KEY_PRESSED_TIME_MS       50

//Time in ms
#define KEY_RELEASE_TIME_MS       50

//Time in ms
#define KEYS_STARTUP_DELAY_MS     500

#define KEYS_QUEUE_SIZE           10

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
key_item_t key_down;
key_item_t key_up;
QueueHandle_t key_queue;


/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void keys_init(void)
{
  key_down.pin_name = BUTTON1_PIN;
  keys_functons_init_hardware(&key_down);
  
  key_up.pin_name = BUTTON2_PIN;
  keys_functons_init_hardware(&key_up);

  key_queue = xQueueCreate(KEYS_QUEUE_SIZE, sizeof(uint8_t));
}

void key_handling(void)
{
  keys_functons_update_key_state(&key_down);
  keys_functons_update_key_state(&key_up);
  //ESP_LOGI("KEY", "key2: %d", key_down.state);

  if ((key_down.prev_state == KEY_PRESSED) && 
      (key_down.state == KEY_WAIT_FOR_RELEASE))
  {
    uint8_t tmp_val = KEY_LOWER_PRESSED;
    xQueueSend(key_queue, &tmp_val, 0);
  }
  
  if ((key_up.prev_state == KEY_PRESSED) && 
      (key_up.state == KEY_WAIT_FOR_RELEASE))
  {
    uint8_t tmp_val = KEY_UPPER_PRESSED;
    xQueueSend(key_queue, &tmp_val, 0);
  }
  
  if ((key_up.prev_state == KEY_PRESSED) && 
      (key_up.state == KEY_HOLD))
  {
    uint8_t tmp_val = KEY_UPPER_HOLD;
    xQueueSend(key_queue, &tmp_val, 0);
  }
  
  if ((key_down.prev_state == KEY_PRESSED) && 
      (key_down.state == KEY_HOLD))
  {
    uint8_t tmp_val = KEY_LOWER_HOLD;
    xQueueSend(key_queue, &tmp_val, 0);
  }
}

//Heavy functions can be called here
void key_handling_execution(void)
{
  uint8_t tmp_val;
  while (xQueueReceive(key_queue, &tmp_val, 0) == pdTRUE)
  {
    switch ((key_event_type_t)tmp_val)
    {
      case KEY_LOWER_PRESSED:
        light_sensor_reset_night_counter();
        menu_lower_button_pressed();
        break;

      case KEY_LOWER_HOLD:
        light_sensor_reset_night_counter();
        menu_lower_button_hold();
        break;

      case KEY_UPPER_PRESSED:
        light_sensor_reset_night_counter();
        menu_upper_button_pressed();
        break;

      case KEY_UPPER_HOLD:
        light_sensor_reset_night_counter();
        menu_upper_button_hold();
        break;

      default:
        break;
    }
  }
}

//*****************************************************************************

// Initialize single key pin
void keys_functons_init_hardware(key_item_t* key_item)
{
  if (key_item == NULL)
    return;
  
  gpio_set_direction(key_item->pin_name, GPIO_MODE_INPUT);
  
  key_item->state = KEY_RELEASED;
}

void keys_functons_update_key_state(key_item_t* key_item)
{
  key_item->prev_state = key_item->state;
  
  if (gpio_get_level(key_item->pin_name) == 0)
    key_item->current_state = 1;
  else
    key_item->current_state = 0;
  
  if ((key_item->state == KEY_RELEASED) && (key_item->current_state != 0))
  {
    //key presed now
    key_item->state = KEY_PRESSED_WAIT;
    key_item->key_timestamp = GET_TIME_MS;
    return;
  }
  
  if (key_item->state == KEY_PRESSED_WAIT)
  {
    uint32_t delta_time = GET_TIME_MS - key_item->key_timestamp;
    if (delta_time > KEY_PRESSED_TIME_MS)
    {
      if (key_item->current_state != 0)
        key_item->state = KEY_PRESSED;
      else
        key_item->state = KEY_RELEASED;
    }
    return;
  }
  
  if ((key_item->state == KEY_PRESSED) || (key_item->state == KEY_HOLD))
  {
    // key not pressed
    if (key_item->current_state == 0)
    {
      key_item->state = KEY_WAIT_FOR_RELEASE;// key is locked here
      key_item->key_timestamp = GET_TIME_MS;
      return;
    }
  }
  
  if (key_item->state == KEY_WAIT_FOR_RELEASE)
  {
    uint32_t delta_time = GET_TIME_MS - key_item->key_timestamp;
    if (delta_time > KEY_RELEASE_TIME_MS)
    {
      key_item->state = KEY_RELEASED;
      return;
    }
  }
  
  if ((key_item->state == KEY_PRESSED) && (key_item->current_state != 0))
  {
    //key still presed now
    uint32_t delta_time = GET_TIME_MS - key_item->key_timestamp;
    if (delta_time > KEY_HOLD_TIME_MS)
    {
      key_item->state = KEY_HOLD;
      return;
    }
  }
}

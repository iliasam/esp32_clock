
/* Includes ------------------------------------------------------------------*/
#include "config.h"
#include <time.h>
#include <sys/time.h>
#include "stdio.h"
#include "stdint.h"
#include "mode_controlling.h"
#include "display_functions.h"
#include "display_drawing.h"
#include "clock_display.h"
#include "clock_display.h"
#include "mavlink_handling.h"
#include "menu_selector.h"
#include "weather_update.h"
#include "light_sensor.h"
#include "weather_json_parse.h"
#include "time_update.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "nvs_functions.h"


/* Private typedef -----------------------------------------------------------*/


/* Private variables ---------------------------------------------------------*/
extern mavlink_handling_state_t mavlink_handling_state;
extern weather_values_t weather_values;

menu_selector_item_t menu_selector_items[] = 
{
  {1, MENU_SUBITEM_EXIT, "ВЫХОД"},
  {2, MENU_SUBITEM_INFO, "ИНФО"},
  {3, MENU_SUBITEM_SYSTEM_BRIGHTNESS, "ЯРКОСТЬ ЭКРАНА"},
  {4, MENU_SUBITEM_DRIVE_BRIGHTNESS, "ОСВЕЩ. БАЛКОНА"},
  {5, MENU_SUBITEM_LIGHT_SENSOR, "ДАТЧИК ОСВЕЩ."},
  {0, MENU_SUBITEM_NULL, ""}, //null item
};

menu_selector_enum menu_selector_selected = MENU_SUBITEM_EXIT;

//Setted to 1 when submenu is active
uint8_t menu_selector_submenu_flag = 0;

//Data must be saved to nvram after exiting subitem
uint8_t menu_selector_value_changed_flag = 0;

int8_t brightness_common = -5;

uint8_t menu_selector_info_page = 0;

char menu_selector_tmp_buf[64];


/* Private function prototypes -----------------------------------------------*/
void menu_selector_draw_items(void);
void menu_selector_draw_items_handle_button(void);
void menu_selector_handle_button_hold_in_main_menu(void);
void menu_selector_draw_subitems(void);
void menu_selector_exit_subitem(void);
void menu_selector_draw_info_menu(void);
void menu_selector_subitem_upper_button(void);
void menu_selector_subitem_lower_button(void);
void menu_selector_draw_brightness_menu(void);
void menu_selector_draw_drive_brightness_menu(void);
void menu_selector_draw_light_sensor_brightness_menu(void);
void menu_selector_draw_clock_info(void);


/* Private functions ---------------------------------------------------------*/

void menu_selector_upper_button_pressed(void)
{
  if (menu_selector_submenu_flag == 0)
  {
    menu_selector_draw_items_handle_button();
  }
  else
  {
    menu_selector_subitem_upper_button();
  }
}

void menu_selector_upper_button_hold(void)
{
  if (menu_selector_submenu_flag == 0)
  {
    menu_selector_handle_button_hold_in_main_menu();
  }
}

void menu_selector_lower_button_pressed(void)
{
  menu_selector_subitem_lower_button();
}

//exit event
void menu_selector_lower_button_hold(void)
{
  if (menu_selector_submenu_flag == 0)
  {
    menu_main_switch_to_next_mode();//leave menu selector mode
  }
  else
  {
    menu_selector_exit_subitem();
  }
}

/// @brief Draw menu at the Display, called periodically
/// @param  
void menu_selector_draw(void)
{
  display_clear_framebuffer();
  
  if (menu_selector_submenu_flag == 0)
  {  
    menu_selector_draw_items();
  }
  else
  {
    menu_selector_draw_subitems();
  }
  
  display_update();
}

uint8_t menu_selector_submenu_active(void)
{
  return menu_selector_submenu_flag;
}


// MENU SELECTOR ###########################################################

//written for small number of elements
void menu_selector_draw_items(void)
{
  uint8_t i = 0;
  uint8_t y_pos = 10;//start in pixels
  
  display_draw_utf8_string("  ВЫБОР РЕЖИМА", 0, 0, FONT_SIZE_8, 0);
  while (menu_selector_items[i].item_number > 0)
  {
     //shifted right for displaying cursor
    display_draw_utf8_string((char*)menu_selector_items[i].name, 8, y_pos, FONT_SIZE_8, 0);
    
    //draw cursor
    if (menu_selector_items[i].item_type == menu_selector_selected)
      display_draw_utf8_string(">", 0, y_pos, FONT_SIZE_8, 0);
    else
      display_draw_utf8_string(" ", 0, y_pos, FONT_SIZE_8, 0);
    
    y_pos += FONT_SIZE_8 + 1;
    i++;
  }
}

// Handle pressing upper button in main menu selector
void menu_selector_draw_items_handle_button(void)
{
  menu_selector_selected++;
  if (menu_selector_selected == MENU_SUBITEM_NULL)
    menu_selector_selected = (menu_selector_enum)0;
}

// Handle holding upper button in main menu selector
void menu_selector_handle_button_hold_in_main_menu(void)
{
  if (menu_selector_selected == MENU_SUBITEM_EXIT)
  {
    menu_main_switch_to_next_mode();
  }
  else
  {
    //Enter to submenu
    menu_selector_submenu_flag = 1;
    menu_selector_value_changed_flag = 0;
    menu_selector_draw_subitems();
  }
}

// SUBITEMS ###########################################################

void menu_selector_draw_subitems(void)
{
  switch (menu_selector_selected)
  {
    case MENU_SUBITEM_INFO:
      menu_selector_draw_info_menu();
      break;

    case MENU_SUBITEM_SYSTEM_BRIGHTNESS:
      menu_selector_draw_brightness_menu();
      break;

    case MENU_SUBITEM_DRIVE_BRIGHTNESS:
      menu_selector_draw_drive_brightness_menu();
      break;

    case MENU_SUBITEM_LIGHT_SENSOR:
      menu_selector_draw_light_sensor_brightness_menu();
      break;

    default: break;
  }
}

void menu_selector_exit_subitem(void)
{
  menu_selector_submenu_flag = 0;
  menu_main_switch_to_next_mode();//leave menu selector mode
  if (menu_selector_value_changed_flag != 0)
  {
    //nvram_save_current_settings();
  }
}

//Handle pressing upper button in subitem mode
void menu_selector_subitem_upper_button(void)
{
  switch (menu_selector_selected)
  {     
    
    case MENU_SUBITEM_SYSTEM_BRIGHTNESS:
      brightness_common+=5;
      if (brightness_common >= 100)
      {  
        brightness_common = 100;
      }
      menu_selector_value_changed_flag = 1;
      dimming_change_auto_controlling(brightness_common < 0);
      dimming_set_common_brightness((float)brightness_common);
      break;

    case MENU_SUBITEM_INFO:
      menu_selector_info_page++;
      if (menu_selector_info_page > 1)
        menu_selector_info_page = 0;
      break;

    /*
    case MENU_SUBITEM_CALIBRATE:
      menu_selector_subitem_adc_config_button_pressed(1);
      break;
    */
    default: break;
  }
  
}

//Handle pressing upper button in subitem mode
void menu_selector_subitem_lower_button(void)
{
  switch (menu_selector_selected)
  {
    case MENU_SUBITEM_SYSTEM_BRIGHTNESS:
      if (brightness_common >= 0)
      {
        brightness_common-= 5;
      }
      menu_selector_value_changed_flag = 1;
      dimming_change_auto_controlling(brightness_common < 0);
      dimming_set_common_brightness((float)brightness_common);
      break;

    default: break;
  }
}


//*****************************************************************************

void menu_selector_draw_info_menu(void)
{
  char tmp_buf[32];
  if (menu_selector_info_page == 0)
  {
    menu_selector_draw_clock_info();
  }
  else if (menu_selector_info_page == 1)
  {
    display_draw_utf8_string("ПОГОДА ПО СЕТИ", 0, 0, FONT_SIZE_8, 0);

    struct tm timeinfo;
    localtime_r(&weather_values.good_parse_timestamp, &timeinfo);
    strftime(tmp_buf, sizeof(tmp_buf), "%H:%M", &timeinfo);
    display_draw_utf8_string("Пос. обнов: ", 0, 10, FONT_SIZE_8, 0);
    display_draw_utf8_string(tmp_buf, 72, 10, FONT_SIZE_8, 0);

    sprintf(tmp_buf, "Рез. парсинга: %li", weather_values.last_parse_result);
    display_draw_utf8_string(tmp_buf, 0, 20, FONT_SIZE_8, 0);

    sprintf(tmp_buf, "Получено: %lu байт", weather_values.last_bytes_rx);
    display_draw_utf8_string(tmp_buf, 0, 30, FONT_SIZE_8, 0);
  }
}

void menu_selector_draw_clock_info(void)
{
    display_draw_utf8_string("ИНФОРМАЦИЯ О ЧАСАХ", 0, 0, FONT_SIZE_8, 0);

    time_t now_binary = time(NULL);
    time_t startup = get_startup_time();
    time_t diff_s = now_binary - startup;
    float diff_days = (float)diff_s / 86400.0f;

    memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
    snprintf(menu_selector_tmp_buf, sizeof(menu_selector_tmp_buf), "ОТ ВКЛЮЧ.: %.1f дн", diff_days);
    display_draw_utf8_string(menu_selector_tmp_buf, 0, 12, FONT_SIZE_8, 0);

    //Working time counter
    uint32_t time_count = nvs_get_work_count();//in periods
    uint32_t work_time_s = time_count * NVS_DEVICE_TIME_CNT_PERIOD_S;
    float work_days = (float)work_time_s / 86400.0f;

    memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
    snprintf(menu_selector_tmp_buf, sizeof(menu_selector_tmp_buf), "ВРЕМ. РАБОТЫ: %.1f дн", work_days);
    display_draw_utf8_string(menu_selector_tmp_buf, 0, 14 + 12, FONT_SIZE_8, 0);

    display_draw_utf8_string("ILIASAM 06/2023", 0, 52, FONT_SIZE_8, 0);
}

void menu_selector_draw_brightness_menu(void)
{
  display_draw_utf8_string("ЯРКОСТЬ", 0, 0, FONT_SIZE_11, 0);
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));

  if (brightness_common >= 0)
    snprintf(menu_selector_tmp_buf, sizeof(menu_selector_tmp_buf), "УРОВЕНЬ: %i", brightness_common);
  else
    snprintf(menu_selector_tmp_buf, sizeof(menu_selector_tmp_buf), "УРОВЕНЬ: АВТО");

  display_draw_utf8_string(menu_selector_tmp_buf, 0, 14, FONT_SIZE_8, 0);

  uint8_t tmp_val = display_get_brightness_code();
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
  snprintf(menu_selector_tmp_buf, 32, "УРОВЕНЬ OLED: %d", tmp_val);
  display_draw_utf8_string(menu_selector_tmp_buf, 0, 14+13, FONT_SIZE_8, 0);

  tmp_val = clock_display_get_brightness_code();
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
  snprintf(menu_selector_tmp_buf, 32, "УРОВЕНЬ LED: %d", tmp_val);
  display_draw_utf8_string(menu_selector_tmp_buf, 0, 14+13*2, FONT_SIZE_8, 0);
  
  uint32_t lux = light_lensor_get_cur_lux();
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
  snprintf(menu_selector_tmp_buf, 40, "Illumin: %lu lux    ", lux);
  display_draw_utf8_string(menu_selector_tmp_buf, 0, 14+13*3, FONT_SIZE_8, 0);
}

void menu_selector_draw_drive_brightness_menu(void)
{
  display_draw_utf8_string("ЯРКОСТЬ БАЛКОНА", 0, 0, FONT_SIZE_11, 0);

  char tmp_buf[32];
  memset(tmp_buf, 0, sizeof(tmp_buf));
  uint8_t tmp = mavlink_get_meas_light();
  snprintf(tmp_buf, 32, "УРОВЕНЬ: %i", tmp);

  display_draw_utf8_string(tmp_buf, 0, 20, FONT_SIZE_8, 0);
}

void menu_selector_draw_light_sensor_brightness_menu(void)
{
  display_draw_utf8_string("ЯРКОСТЬ ВН. ДАТЧИКА", 0, 0, FONT_SIZE_11, 0);
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
  uint16_t tmp_volt_mv = light_sensor_get_volt();
  snprintf(menu_selector_tmp_buf, 40, "VOLT: %d мВ   ", tmp_volt_mv);
  display_draw_utf8_string(menu_selector_tmp_buf, 0, 20, FONT_SIZE_8, 0);
  
  uint32_t lux = light_lensor_get_cur_lux();
  memset(menu_selector_tmp_buf, 0, sizeof(menu_selector_tmp_buf));
  snprintf(menu_selector_tmp_buf, 40, "Illumin: %lu lux    ", lux);
  display_draw_utf8_string(menu_selector_tmp_buf, 0, 40, FONT_SIZE_8, 0);
}


menu_selector_enum menu_selector_get_submenu(void)
{
  return menu_selector_selected;
}


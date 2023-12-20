/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MODE_CONTROLLING_H
#define __MODE_CONTROLLING_H

/* Includes ------------------------------------------------------------------*/
#include "clock_config.h"
#include "menu_selector.h"

/* Exported types ------------------------------------------------------------*/


typedef enum
{
  MENU_MODE_BASIC = 0,
  MENU_MODE_CO2_1MIN,
  MENU_MODE_CO2_10MIN,
  MENU_MODE_SUN_INFO,
  MENU_SELECTOR,
  MENU_MODE_COUNT,//LAST!
} menu_mode_t;



/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void menu_main_switch_to_next_mode(void);
void menu_main_switch_to_start(void);
void menu_main_init(void);

void menu_lower_button_pressed(void);
void menu_upper_button_pressed(void);
void menu_upper_button_hold(void);
void menu_lower_button_hold(void);

menu_mode_t mode_get_curr_value(void);

menu_selector_enum mode_get_curr_submenu(void);


#endif /* __MENU_CONTROLLING_H */


//Drawing of modes are called from here

/* Includes ------------------------------------------------------------------*/
#include "mode_controlling.h"
#include "display_functions.h"
#include "string.h"
#include "stdio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
menu_mode_t main_menu_mode = MENU_MODE_BASIC;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void menu_main_init(void)
{
  
}

void menu_lower_button_pressed(void)
{
  if (menu_selector_submenu_active())
  {
    menu_selector_lower_button_pressed();
    return;
  }
  
  //Current mode must be checked here
  //and based on it we need to switch to a next menu or execute command
  menu_main_switch_to_next_mode();
}

void menu_lower_button_hold(void)
{
  switch (main_menu_mode)
  {
    case MENU_SELECTOR:
      menu_selector_lower_button_hold();
      break;
    
    default: break;
  }
}

void menu_upper_button_pressed(void)
{
  switch (main_menu_mode)
  {
    case MENU_SELECTOR:
      menu_selector_upper_button_pressed();
      break;
    
    default: break;
  }
}

void menu_upper_button_hold(void)
{
  switch (main_menu_mode)
  {
    case MENU_SELECTOR:
      menu_selector_upper_button_hold();
      break;
    
    default: break;
  }
}

// Switch main mode to a next mode
void menu_main_switch_to_next_mode(void)
{
  main_menu_mode++;
  if (main_menu_mode >= MENU_MODE_COUNT)
    main_menu_mode = (menu_mode_t)0;
}

/// @brief Return current mode
/// @return Main menu code, like - clock/CO2/menu
menu_mode_t mode_get_curr_value()
{
  return main_menu_mode;
}

menu_selector_enum mode_get_curr_submenu(void)
{
  if (main_menu_mode != MENU_SELECTOR)
    return MENU_SUBITEM_NULL;

  return menu_selector_get_submenu();
}


#ifndef _CLOCK_DISPLAY_H
#define _CLOCK_DISPLAY_H

void clock_display_init(void);
void clock_display_handler(void);
void clock_display_change_brightness(uint8_t new_value);
uint8_t clock_display_get_brightness_code(void);

#endif
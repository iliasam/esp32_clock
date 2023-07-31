#ifndef _DISPLAY_DRAWING_H
#define _DISPLAY_DRAWING_H

void display_drawing_init(void);
void display_drawing_handler(void);
void display_change_brightness(uint8_t new_value);
uint8_t display_get_brightness_code(void);

#endif
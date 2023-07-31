#ifndef __DISPLAY_FUNCTIONS_H
#define __DISPLAY_FUNCTIONS_H

#include "stdint.h"
#include "string.h"
#include "oled_display.h"


#define DISPLAY_WIDTH           G_DISP_WIDTH
#define DISPLAY_HEIGHT          G_DISP_HEIGHT

#define FONT_SIZE_6             6//3*5
#define FONT_SIZE_6_WIDTH       4//3*5

#define FONT_SIZE_8             8//5*7
#define FONT_SIZE_8_WIDTH       6//5*7

#define FONT_SIZE_11            12//7*11
#define FONT_SIZE_11_WIDTH      8//7*11

#define FONT_SIZE_22            22
#define FONT_SIZE_22_WIDTH      16

#define FONT_SIZE_32            32
#define FONT_SIZE_32_WIDTH      20//24

#define LCD_NEW_LINE_FLAG       1//jump to new line
#define LCD_INVERTED_FLAG       2//inverted draw
#define LCD_CENTER_X_FLAG       4//Center around x value

void display_full_clear(void);
void display_clear_framebuffer(void);
void display_update(void);
void display_set_pixel(uint16_t x, uint16_t y);
void display_reset_pixel(uint16_t x, uint16_t y);

void display_draw_char(uint8_t chr, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags);
void display_set_cursor_pos(uint16_t x, uint16_t y);
uint16_t display_draw_string(char *s, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags);
uint16_t display_draw_utf8_string(char *s, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags);
uint16_t get_font_width(uint8_t font);
void draw_caption_bar(uint8_t height);
void draw_image(uint8_t *image_data, uint16_t x, uint16_t y);

void display_draw_line(uint16_t y);
void display_clear_line(uint16_t y);
void display_draw_vertical_line(uint16_t x, uint16_t y1, uint16_t y2);
void display_draw_vertical_line_dotted(uint16_t x, uint16_t y1, uint16_t y2);

void display_draw_test(void);

#endif


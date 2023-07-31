//Special framebuffer wrapper used for basic operations - like text drawing

/* Includes ------------------------------------------------------------------*/
#include "display_functions.h"
#include "fonts.h"

/* Private variables ---------------------------------------------------------*/
uint16_t display_cursor_text_x = 0;
uint16_t display_cursor_text_y = 0;
uint8_t display_framebuffer[DISPLAY_WIDTH*DISPLAY_HEIGHT / 8];

volatile uint32_t display_update_time_ms = 0;

//extern volatile uint32_t ms_tick;

/* Private function prototypes -----------------------------------------------*/
void display_draw_char_size8(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags);
void display_draw_char_size6(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags);
void display_draw_char_size11(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags);
void display_draw_char_size22(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags);
void display_draw_char_size32(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags);
void utf8_to_cp1251(uint8_t *in_str, uint8_t *out_str, uint8_t max_out_len, uint16_t len);

/* Private functions ---------------------------------------------------------*/

void display_draw_test(void)
{
  display_framebuffer[0] = 0xFF;
  display_framebuffer[7] = 0xAA;

  display_framebuffer[8] = 1 + 2 + 4 + 8;
  display_framebuffer[15] = 0xFF;

}

void display_set_pixel(uint16_t x, uint16_t y)
{
  y = DISPLAY_HEIGHT - 1 - y ;
  uint16_t loc_x = DISPLAY_WIDTH - 1 - x;
  if ((loc_x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT))
    return;

  uint16_t loc_y = y / 8;//8 - size of display line
  uint32_t byte_pos = loc_y * DISPLAY_WIDTH + loc_x;
  uint8_t byte_val = 1 << ((y % 8));
  display_framebuffer[byte_pos]|= byte_val;
}

void display_reset_pixel(uint16_t x, uint16_t y)
{
  y = DISPLAY_HEIGHT - 1 - y ;
  uint16_t loc_x = DISPLAY_WIDTH - 1 - x;
  if ((loc_x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT))
    return;

  uint16_t loc_y = y / 8;//8 - size of display line
  uint32_t byte_pos = loc_y * DISPLAY_WIDTH + loc_x;
  uint8_t byte_val = 1 << ((y % 8));
  display_framebuffer[byte_pos]&= ~byte_val;
}

void display_clear_framebuffer(void)
{
  memset(display_framebuffer, 0, sizeof(display_framebuffer));
  display_cursor_text_x = 0;
  display_cursor_text_y = 0;
}

void display_full_clear(void)
{
  display_clear_framebuffer();
  oled_full_clear();
}

void display_set_cursor_pos(uint16_t x, uint16_t y)
{
  display_cursor_text_x = x;
  display_cursor_text_y = y;
}

/// @brief Send framebuffer to HW
/// @param  
void display_update(void)
{
  //uint32_t start_time = ms_tick;
  oled_send_full_framebuffer(display_framebuffer);
  //display_update_time_ms = ms_tick - start_time;
}

uint16_t display_draw_utf8_string(char *s, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags)
{
  char tmp_buf[64];
  memset(tmp_buf, 0, sizeof(tmp_buf));
  utf8_to_cp1251((uint8_t *)s, (uint8_t *)tmp_buf, 64, 64);
  return display_draw_string(tmp_buf, x, y, font_size, flags);
}

//x, y - in pixel
//return string width
//String end is 0x00 char
uint16_t display_draw_string(char *s, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags)
{
  uint16_t font_width = get_font_width(font_size);
  uint8_t chr_pos = 0;
  char chr = *s;

  if (flags & LCD_CENTER_X_FLAG)
  {
    uint8_t length =  strlen(s);
    uint16_t str_width = length * font_width;
    int16_t start_x = (int16_t)x - (str_width / 2);
    if (start_x < 0)
      start_x = 0;
    x = (uint16_t)start_x;
  }
  
  while (chr && (chr_pos < 50)) 
  {
    display_draw_char(chr, x + chr_pos * font_width, y, font_size, flags);
    chr_pos++;
    chr = s[chr_pos];
  }
  display_cursor_text_x = x + chr_pos * font_width;
  display_cursor_text_y = y;
  
  return chr_pos*font_width;
}

//x - size in pixel
//y - in pixel
void display_draw_char(uint8_t chr, uint16_t x, uint16_t y, uint8_t font_size, uint8_t flags)
{
  switch (font_size)
  {
    case FONT_SIZE_8:
    {
      display_draw_char_size8(chr, x, y, flags);
      break;
    }
    case FONT_SIZE_6:
    {
      display_draw_char_size6(chr, x, y, flags);
      break;
    }
    case FONT_SIZE_11:
    {
      display_draw_char_size11(chr, x, y, flags);
      break;
    }
    case FONT_SIZE_22:
    {
      display_draw_char_size22(chr, x, y, flags);
      break;
    }
    case FONT_SIZE_32:
    {
      display_draw_char_size32(chr, x, y, flags);
      break;
    }
  }
}

//x, y - size in pixel
void display_draw_char_size8(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags)
{
  uint16_t x_pos, y_pos;
  
  //decoding symbol
  if (chr >= 32 && chr <= '~')
  {
    chr = chr - 32;
  } 
  else
  {
    if (chr >= 192)
      chr = chr - 97;
    else
    {
        return;
    }
      
  }
  
  for (x_pos = 0; x_pos < (FONT_SIZE_8_WIDTH); x_pos++)
  {
    for (y_pos = 0; y_pos < FONT_SIZE_8; y_pos++)
    {
      uint8_t pixel = display_font_size8[chr][x_pos] & (1<<y_pos);
      if (x_pos == (FONT_SIZE_8_WIDTH-1))
        pixel = 0;
      
      if (flags & LCD_INVERTED_FLAG) 
      {
        if (pixel) pixel = 0; else pixel = 1;
      } 
      
      if (pixel) 
        display_set_pixel(x_start + x_pos, y_start + y_pos);
      else
        display_reset_pixel(x_start + x_pos, y_start + y_pos);
    }
  }
}

void display_draw_char_size6(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags)
{
  uint16_t x_pos, y_pos;
  
  for (x_pos = 0; x_pos < (FONT_SIZE_6_WIDTH); x_pos++)
  {
    for (y_pos = 0; y_pos < FONT_SIZE_6; y_pos++)
    {
      uint8_t pixel = display_font_size6[chr][y_pos] & (1<<(3-x_pos));
      
      if (flags & LCD_INVERTED_FLAG) 
      { 
        if (pixel) pixel = 0; else pixel = 1;
      }
      
      if (pixel) 
        display_set_pixel(x_start + x_pos, y_start + y_pos);
      else
        display_reset_pixel(x_start + x_pos, y_start + y_pos);
    }
  }
}

//x, y - size in pixel
void display_draw_char_size11(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags)
{
  uint16_t x_pos, y_pos;
  
  //decoding symbol
  if (chr >= 32)
  {
    chr = chr - 32;
  }
  
  for (x_pos = 0; x_pos < (FONT_SIZE_11_WIDTH); x_pos++)
  {
    for (y_pos = 0; y_pos < (FONT_SIZE_11); y_pos++)
    {
      if (display_font_size11[chr][y_pos] & (1<<(x_pos))) 
        display_set_pixel(x_start + x_pos, y_start + y_pos);
      else
        display_reset_pixel(x_start + x_pos, y_start + y_pos);
    }
  }
}

//x, y - size in pixel
void display_draw_char_size22(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags)
{
  uint16_t x_pos, y_pos;
  
  //decoding symbol
  if (chr >= 32 && chr <= 128)
  {
    chr = chr - 32;
  }
  else
  {
    return;
  }
  
  uint16_t start = chr * FONT_SIZE_22 * 2;
  
  for (y_pos = 0; y_pos < (FONT_SIZE_22 - 1); y_pos++)
  {
    uint16_t line_num = start + y_pos*2;
    uint16_t hor_line = (uint16_t)display_font_size22[line_num + 1] | ((uint16_t)display_font_size22[line_num] << 8);
    //uint16_t hor_line = ((uint16_t*)display_font_size22)[start + y_pos];
    for (x_pos = 0; x_pos < (FONT_SIZE_22_WIDTH - 1); x_pos++)
    {
      if (hor_line & (1 << (FONT_SIZE_22_WIDTH - x_pos - 1)))
        display_set_pixel(x_start + x_pos, y_start + y_pos);
      else
        display_reset_pixel(x_start + x_pos, y_start + y_pos);
    }
  }
}

//x, y - size in pixel
void display_draw_char_size32(uint8_t chr, uint16_t x_start, uint16_t y_start, uint8_t flags)
{
  uint16_t x_pos, y_pos;
  
  //decoding symbol
  if (chr >= 48 && chr <= 58)
  {
    chr = chr - 48;
  }
  else
  {
    return;
  }
  
  for (x_pos = 0; x_pos < FONT_SIZE_32_WIDTH; x_pos++)
  {
    uint8_t int_pos = x_pos + 2;
    uint32_t vert_line = 0;
    vert_line |= (display_font_size32[chr][int_pos] << 0);
    vert_line |= (display_font_size32[chr][int_pos + FONT32_LINE_LENGTH] << 8);
    vert_line |= (display_font_size32[chr][int_pos + FONT32_LINE_LENGTH*2] << 16);
    vert_line |= (display_font_size32[chr][int_pos + FONT32_LINE_LENGTH*3] << 24);

    for (y_pos = 0; y_pos < (FONT_SIZE_32); y_pos++)
    {
      if (vert_line & (1 << y_pos))
        display_set_pixel(x_start + x_pos, y_start + y_pos);
      else
        display_reset_pixel(x_start + x_pos, y_start + y_pos);
    }
  }
}

//Draw black bar
void draw_caption_bar(uint8_t height)
{
  uint16_t x_pos, y_pos;
  for (x_pos = 0; x_pos <= DISPLAY_WIDTH; x_pos++)
  {
    for (y_pos = 0; y_pos < height; y_pos++)
    {
        display_set_pixel(x_pos, y_pos);
    }
  }
}

//Horizontal line
void display_draw_line(uint16_t y)
{
  uint16_t x_pos;
  for (x_pos = 0; x_pos <= DISPLAY_WIDTH; x_pos++)
  {
    display_set_pixel(x_pos, y);
  }
}

//Horizontal line
void display_clear_line(uint16_t y)
{
  uint16_t x_pos;
  for (x_pos = 0; x_pos <= DISPLAY_WIDTH; x_pos++)
  {
    display_reset_pixel(x_pos, y);
  }
}

void display_draw_vertical_line(uint16_t x, uint16_t y1, uint16_t y2)
{
  //y1 must be less than y2
  if (y1 > y2)
  {
    uint16_t tmp = y1;
    y1 = y2;
    y2 = tmp;
  }
  
  for (uint16_t y = y1; y <= y2; y++)
  {
    display_set_pixel(x, y);
  }
}

void display_draw_vertical_line_dotted(uint16_t x, uint16_t y1, uint16_t y2)
{
  //y1 must be less than y2
  if (y1 > y2)
  {
    uint16_t tmp = y1;
    y1 = y2;
    y2 = tmp;
  }
  
  for (uint16_t y = y1; y <= y2; y++)
  {
    if ((y & 2) ==0) 
      display_set_pixel(x, y);
  }
}



uint16_t get_font_width(uint8_t font)
{
  switch (font)
  {
    case FONT_SIZE_6:  return FONT_SIZE_6_WIDTH;
    case FONT_SIZE_8:  return FONT_SIZE_8_WIDTH;
    case FONT_SIZE_11: return FONT_SIZE_11_WIDTH;
    case FONT_SIZE_22: return FONT_SIZE_22_WIDTH;
    case FONT_SIZE_32: return FONT_SIZE_32_WIDTH;
    default: return 5;
  }
}



//width must divide to 8!!
void draw_image(uint8_t *image_data, uint16_t x, uint16_t y)
{
  uint16_t width = image_data[0] + (uint16_t)image_data[1] * 256;
  uint16_t height = image_data[2] + (uint16_t)image_data[3] * 256;
  uint16_t bytes_in_line =  width / 8;
  
  for (uint16_t y_pos = 0; y_pos < height; y_pos++) 
  {
    for (uint16_t x_pos = 0; x_pos < width; x_pos++)
    {
      int32_t byte_offset = y_pos * bytes_in_line + (x_pos / 8) + 4;
      if (image_data[byte_offset] & (1 << (7 - (x_pos % 8)))) 
        display_set_pixel(x + x_pos, y + y_pos);
      else
        display_reset_pixel(x + x_pos, y + y_pos);
    }
  }
}

void utf8_to_cp1251(uint8_t *in_str, uint8_t *out_str, uint8_t max_out_len, uint16_t len)
{
  uint16_t i = 0;
  uint16_t j = 0;
  uint8_t char1 = 0;
  uint8_t char2 = 0;
          
	while(in_str[i] && (j <= max_out_len) && (in_str[i] > 0))
	{
		if ((i + 1) < len)
		{
			char1 = in_str[i] & 0xFF;
			char2 = in_str[i+1] & 0xFF;
			if ((char1 == 0xD0) && (char2 == 0x81))
			{
				out_str[j] = 168;
				i++;
			}
			else if ((char1 == 0xD1) && (char2 == 0x91))
			{
				out_str[j] = 184;
				i++;
			}
			else if ((char1 == 0xD0) && (char2 >= 0x90) && (char2 <= 0xBF))
			{
				out_str[j] = char2 + 48;
				i++;
			}
			else if ((char1 == 0xD1) && (char2 >= 0x80) && (char2 <= 0x8F))
			{
				out_str[j] = char2 + 112;
				i++;
			}
			else 
      {
        out_str[j] = in_str[i];
      }
		}
		else
    {
      out_str[j] = in_str[i];
    }
    i++;
    j++;
	}
}
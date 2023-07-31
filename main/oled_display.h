//WEX012864ELPP3N10000 OLED display driver

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#define G_DISP_WIDTH       128
#define G_DISP_HEIGHT      64

#include <stdint.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_err.h>

void oled_init(void);
void oled_full_clear(void);
void oled_send_full_framebuffer(uint8_t *data);
void oled_set_brightness(uint8_t code);

#endif /* OLED_DISPLAY_H */

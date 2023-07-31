/*
 * Copyright (c) 2019 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ESP-IDF driver for MAX7219/MAX7221
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2017 Ruslan V. Uss <unclerus@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */
#include "max7219.h"
#include <string.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "max7219";

#define ALL_CHIPS           0xff
#define ALL_DIGITS          8

#define REG_DIGIT_0         (1 << 8)
#define REG_DECODE_MODE     (9 << 8)
#define REG_INTENSITY       (10 << 8)
#define REG_SCAN_LIMIT      (11 << 8)
#define REG_SHUTDOWN        (12 << 8)
#define REG_DISPLAY_TEST    (15 << 8)

static const uint8_t font_7seg[] = {
    /*  ' '   !     "     #     $     %     &     '     (     )     */
        0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x02, 0x4e, 0x78,
    /*  *     +     ,     -     .     /     0     1     2     3     */
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7e, 0x30, 0x6d, 0x79,
    /*  4     5     6     7     8     9     :     ;     <     =     */
        0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b, 0x00, 0x00, 0x0d, 0x09,
    /*  >     ?     @     A     B     C     D     E     F     G     */
        0x19, 0x65, 0x00, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47, 0x5e,
    /*  H     I     J     K     L     M     N     O     P     Q     */
        0x37, 0x06, 0x38, 0x57, 0x0e, 0x76, 0x15, 0x1d, 0x67, 0x73,
    /*  R     S     T     U     V     W     X     Y     Z     [     */
        0x05, 0x5b, 0x0f, 0x1c, 0x3e, 0x2a, 0x49, 0x3b, 0x6d, 0x4e,
    /*  \     ]     ^     _     `     a     b     c     d     e     */
        0x00, 0x78, 0x00, 0x08, 0x02, 0x77, 0x1f, 0x4e, 0x3d, 0x4f,
    /*  f     g     h     i     j     k     l     m     n     o     */
        0x47, 0x5e, 0x37, 0x06, 0x38, 0x57, 0x0e, 0x76, 0x15, 0x1d,
    /*  p     q     r     s     t     u     v     w     x     y     */
        0x67, 0x73, 0x05, 0x5b, 0x0f, 0x1c, 0x3e, 0x2a, 0x49, 0x3b,
    /*  z     {     |     }     ~     */
        0x6d, 0x4e, 0x06, 0x78, 0x00
};

#define VAL_CLEAR_BCD       0x0f
#define VAL_CLEAR_NORMAL    0x00

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

/// @brief Contains converted data, 0bit is A
uint8_t max7219_disp_buff[ALL_DIGITS];

uint8_t max7219_pos_conv(uint8_t pos);
uint8_t max7219_convert_to_abc(uint8_t data);

static inline uint16_t shuffle(uint16_t val)
{
    return (val >> 8) | (val << 8);
}

static esp_err_t send(max7219_t *dev, uint8_t chip, uint16_t value)
{
    uint16_t buf[MAX7219_MAX_CASCADE_SIZE] = { 0 };
    if (chip == ALL_CHIPS)
    {
        for (uint8_t i = 0; i < dev->cascade_size; i++)
            buf[i] = shuffle(value);
    }
    else buf[chip] = shuffle(value);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = dev->cascade_size * 16;
    t.tx_buffer = buf;
    return spi_device_transmit(dev->spi_dev, &t);
}

inline static uint8_t get_char(max7219_t *dev, char c)
{
    if (dev->bcd)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        switch (c)
        {
            case '-':
                return 0x0a;
            case 'E':
            case 'e':
                return 0x0b;
            case 'H':
            case 'h':
                return 0x0c;
            case 'L':
            case 'l':
                return 0x0d;
            case 'P':
            case 'p':
                return 0x0e;
        }
        return VAL_CLEAR_BCD;
    }
    if (c < 0x20)
        return 0;

    return font_7seg[(c - 0x20) & 0x7f];
}

///////////////////////////////////////////////////////////////////////////////

esp_err_t max7219_init_desc(max7219_t *dev, spi_host_device_t host, uint32_t clock_speed_hz, gpio_num_t cs_pin)
{
    CHECK_ARG(dev);

    memset(&dev->spi_cfg, 0, sizeof(dev->spi_cfg));
    dev->spi_cfg.spics_io_num = cs_pin;
    dev->spi_cfg.clock_speed_hz = clock_speed_hz;
    dev->spi_cfg.mode = 0;
    dev->spi_cfg.queue_size = 1;
    dev->spi_cfg.flags = SPI_DEVICE_NO_DUMMY;

    return spi_bus_add_device(host, &dev->spi_cfg, &dev->spi_dev);
}

esp_err_t max7219_free_desc(max7219_t *dev)
{
    CHECK_ARG(dev);

    return spi_bus_remove_device(dev->spi_dev);
}

esp_err_t max7219_init(max7219_t *dev)
{
    CHECK_ARG(dev);
    if (!dev->cascade_size || dev->cascade_size > MAX7219_MAX_CASCADE_SIZE)
    {
        ESP_LOGE(TAG, "Invalid cascade size %d", dev->cascade_size);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t max_digits = dev->cascade_size * ALL_DIGITS;
    if (dev->digits > max_digits)
    {
        ESP_LOGE(TAG, "Invalid digits count %d, max %d", dev->digits, max_digits);
        return ESP_ERR_INVALID_ARG;
    }
    if (!dev->digits)
        dev->digits = max_digits;

    // Shutdown all chips
    CHECK(max7219_set_shutdown_mode(dev, true));
    // Disable test
    CHECK(send(dev, ALL_CHIPS, REG_DISPLAY_TEST));
    // Set max scan limit
    CHECK(send(dev, ALL_CHIPS, REG_SCAN_LIMIT | (ALL_DIGITS - 1)));
    // Set normal decode mode & clear display
    CHECK(max7219_set_decode_mode(dev, false));
    // Set minimal brightness
    CHECK(max7219_set_brightness(dev, 0));
    // Wake up
    CHECK(max7219_set_shutdown_mode(dev, false));

    return ESP_OK;
}

esp_err_t max7219_set_decode_mode(max7219_t *dev, bool bcd)
{
    CHECK_ARG(dev);

    dev->bcd = bcd;
    CHECK(send(dev, ALL_CHIPS, REG_DECODE_MODE | (bcd ? 0xff : 0)));
    CHECK(max7219_clear(dev));

    return ESP_OK;
}

esp_err_t max7219_set_brightness(max7219_t *dev, uint8_t value)
{
    CHECK_ARG(dev);
    CHECK_ARG(value <= MAX7219_MAX_BRIGHTNESS);

    CHECK(send(dev, ALL_CHIPS, REG_INTENSITY | value));

    return ESP_OK;
}

esp_err_t max7219_set_shutdown_mode(max7219_t *dev, bool shutdown)
{
    CHECK_ARG(dev);
    CHECK(send(dev, ALL_CHIPS, REG_SHUTDOWN | !shutdown));

    return ESP_OK;
}


/// @brief Draw 1 digit
/// @param dev 
/// @param digit - position
/// @param val - data to display, 0bit is A
/// @return 
esp_err_t max7219_set_digit(max7219_t *dev, uint8_t digit, uint8_t val)
{
    CHECK_ARG(dev);
    if (digit >= dev->digits)
    {
        ESP_LOGE(TAG, "Invalid digit: %d", digit);
        return ESP_ERR_INVALID_ARG;
    }

    if (dev->mirrored)
        digit = dev->digits - digit - 1;

    uint8_t c = digit / ALL_DIGITS;
    uint8_t d = digit % ALL_DIGITS;

    //ESP_LOGV(TAG, "Chip %d, digit %d val 0x%02x", c, d, val);

    CHECK(send(dev, c, (REG_DIGIT_0 + ((uint16_t)d << 8)) | val));

    return ESP_OK;
}

esp_err_t max7219_clear(max7219_t *dev)
{
    CHECK_ARG(dev);

    uint8_t val = dev->bcd ? VAL_CLEAR_BCD : VAL_CLEAR_NORMAL;
    for (uint8_t i = 0; i < ALL_DIGITS; i++)
        CHECK(send(dev, ALL_CHIPS, (REG_DIGIT_0 + ((uint16_t)i << 8)) | val));

    return ESP_OK;
}

esp_err_t max7219_send_int_buffer(max7219_t *dev)
{
    CHECK_ARG(dev);

    uint8_t tmp_buf[8];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    //max7219_disp_buff[0] = max7219_convert_to_abc(0x6d);
    //max7219_disp_buff[1] = max7219_convert_to_abc(0x79);
    //max7219_disp_buff[2] = 1;
    //max7219_disp_buff[3] = 2;

    for (uint8_t seg = 0; seg < 8; seg++) 
    {
        for (uint8_t pos = 0; pos < 4; pos++)//display number
        {
            if (max7219_disp_buff[pos] & (1 << seg))
                tmp_buf[seg] |= max7219_pos_conv(pos);
        }
    }

    for (uint8_t pos = 0; pos < 8; pos++)
    {
        CHECK(send(dev, ALL_CHIPS, (REG_DIGIT_0 + ((uint16_t)pos << 8)) | tmp_buf[pos]));
        //CHECK(send(dev, ALL_CHIPS, (REG_DIGIT_0 + ((uint16_t)pos << 8)) | 64));
    }    

    return ESP_OK;
}


uint8_t max7219_convert_to_abc(uint8_t data)
{
    uint8_t res = 0;

    if (data & 128)
        res |= (1 << 7);
    if (data & 64)
        res |= (1 << 0);
    if (data & 32)
        res |= (1 << 1);
    if (data & 16)
        res |= (1 << 2);
    if (data & 8)
        res |= (1 << 3);
    if (data & 4)
        res |= (1 << 4);
    if (data & 2)
        res |= (1 << 5);
    if (data & 1)
        res |= (1 << 6);

    return res;
}


uint8_t max7219_pos_conv(uint8_t pos)
{   
    switch (pos)
    {
    case 0:
        return 64;//a
    case 1:
        return 32;//b
    case 2:
        return 16;//c
    case 3:
        return 8;//d
    
    default:
        break;
    }
    return 0;//not implemented
}

esp_err_t max7219_draw_text_7seg(max7219_t *dev, uint8_t pos, const char *s)
{
    CHECK_ARG(dev && s);
    while (*s && (pos < dev->digits))
    {
        uint8_t c = get_char(dev, *s);
        if (*(s + 1) == '.')
        {
            c |= 0x80;
            s++;
        }
        if (pos < ALL_DIGITS)
            max7219_disp_buff[pos] = max7219_convert_to_abc(c);
        //CHECK(max7219_set_digit(dev, pos, c));
        pos++;
        s++;
    }
    max7219_send_int_buffer(dev);

    return ESP_OK;
}

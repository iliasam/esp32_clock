#include "clock_config.h"
#include "oled_display.h"
#include <string.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include <rom/ets_sys.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_TRY_CNT      5
#define BLACK 0
#define WHITE 1

#define SSD1306_CMD_DISPLAY_OFF									0xAE
#define SSD1306_CMD_DISPLAY_ON									0xAF
#define SSD1306_PARAM_ADDR_MODE_HORIZ_ADDR_MODE	0x00
#define SSD1306_CMD_SET_COM_HW_CONFIG						0xDA
#define SSD1306_CMD_SET_PRECHARGE_PERIOD				0xD9
#define SSD1306_CMD_SET_DIV_RATIO_OSC_CLOCK			0xD5
#define SSD1306_CMD_SET_DISPLAY_LINE_OFFSET			0xD3
#define SSD1306_CMD_SHOW_MEMORY_CONTENT					0xA4
#define SSD1306_PARAM_MAX_RATIO									0x3F
#define SSD1306_CMD_SET_MULTIPLEX_RATIO					0xA8
#define SSD1306_CMD_SET_NON_INVERSE_COLOR				0xA6
#define SSD1306_CMD_SET_CONTRAST								0x81
#define SSD1306_CMD_SET_COLUMN_ADDR							0x21
#define SSD1306_PARAM_START_COLUMN_ADDR_MIN			0x00
#define SSD1306_PARAM_START_COLUMN_ADDR_MAX			0x7F
#define SSD1306_CMD_SET_PAGE_ADDRESS_GDDRAM_MIN	0xB0
#define SSD1306_CMD_SET_BACKWARD_SCAN_DIRECTION	0xC8
#define SSD1306_CMD_SET_LOW_ADDRESS_COLUMN_MIN	0x00
#define SSD1306_CMD_SET_HIGH_ADDRESS_COLUMN_MIN	0x10
#define SSD1306_CMD_REMAP_COLUMN_0_SEG_ZERO			0xA0
#define SSD1306_CMD_SET_START_LINE_MIN					0x40
#define SSD1306_PARAM_END_COLUMN_ADDR_MAX				0x7F
#define SSD1306_CMD_SET_ADDRESSING_MODE					0x20

#define SSD1306_PAGE_COUNT						8U


static const char *TAG_G_DISP = "G_DISP";


bool oled_init_ok = false;
spi_device_interface_config_t oled_spi_cfg;
spi_device_handle_t oled_spi_dev;

esp_err_t oled_init_desc(spi_host_device_t host, uint32_t clock_speed_hz, gpio_num_t cs_pin);
esp_err_t oled_send_byte(uint8_t value);
esp_err_t oled_send_bytes(uint8_t *data, uint16_t bytes_count);
void oled_send_command(uint8_t d);
void oled_send_command_bytes(uint8_t *data, uint16_t bytes_count);
void oled_send_data(uint8_t d);
void oled_configure(void);

///////////////////////////////////////////////////////////////////////////////

/// @brief Init display HW
/// @param  
void oled_init(void)
{
  gpio_reset_pin(G_DISP_PIN_NUM_RES);//jtag pin
  gpio_set_direction(G_DISP_PIN_NUM_RES, GPIO_MODE_OUTPUT);
  gpio_set_level(G_DISP_PIN_NUM_RES, 1);
  ets_delay_us(1000*50);
  gpio_set_level(G_DISP_PIN_NUM_RES, 0);//reset
  ets_delay_us(1000*100);
  gpio_set_level(G_DISP_PIN_NUM_RES, 1);
  ets_delay_us(1000*100);

  ESP_LOGE(TAG_G_DISP, "Try to init");

  spi_bus_config_t tmp_vfd_cfg = 
  {
    .mosi_io_num = G_DISP_PIN_NUM_MOSI,
    .miso_io_num = -1,
    .sclk_io_num = G_DISP_PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 0,
    .flags = 0
  };
  ESP_ERROR_CHECK(spi_bus_initialize(G_DISP_SPI_HOST_NAME, &tmp_vfd_cfg, SPI_DMA_CH_AUTO));

  ESP_LOGE(TAG_G_DISP, "Try to init 2");
  uint8_t counter = 0;
  while ((oled_init_desc(G_DISP_SPI_HOST_NAME, 1e6, G_DISP_PIN_NUM_CS) != ESP_OK) && (counter < MAX_TRY_CNT)) 
  {
    ESP_LOGE(TAG_G_DISP, "Cannot do oled_init_desc()");
    vTaskDelay(pdMS_TO_TICKS(1000));
    counter++;
  }
  if (counter == MAX_TRY_CNT)
  {
    ESP_LOGE(TAG_G_DISP, "init failed!");
    return;
  }

  gpio_set_direction(G_DISP_PIN_NUM_CD, GPIO_MODE_OUTPUT);
  ESP_LOGE(TAG_G_DISP, "SPI init done!");
  oled_configure();
  ESP_LOGE(TAG_G_DISP, "init done!");
  oled_init_ok = true;
}

esp_err_t oled_init_desc(spi_host_device_t host, uint32_t clock_speed_hz, gpio_num_t cs_pin)
{
  memset(&oled_spi_cfg, 0, sizeof(oled_spi_cfg));
  oled_spi_cfg.spics_io_num = cs_pin;
  oled_spi_cfg.clock_speed_hz = clock_speed_hz;
  oled_spi_cfg.mode = 3; //CPOL = 1, CPHA = 1
  oled_spi_cfg.queue_size = 1;
  oled_spi_cfg.flags = SPI_DEVICE_NO_DUMMY;
  return spi_bus_add_device(host, &oled_spi_cfg, &oled_spi_dev);
}

/// @brief Send init commands to the VFD
/// @param  
void oled_configure(void)
{
  uint8_t tmp_buf[4];

  ets_delay_us(1000*300);
  oled_send_command(SSD1306_CMD_DISPLAY_OFF);

	tmp_buf[0] = SSD1306_CMD_SET_ADDRESSING_MODE;
	tmp_buf[1] = SSD1306_PARAM_ADDR_MODE_HORIZ_ADDR_MODE;
	oled_send_command_bytes(tmp_buf, 2);

	tmp_buf[0] = SSD1306_CMD_SET_COLUMN_ADDR;
	tmp_buf[1] = SSD1306_PARAM_START_COLUMN_ADDR_MIN;
	tmp_buf[2] = SSD1306_PARAM_END_COLUMN_ADDR_MAX;	
	oled_send_command_bytes(tmp_buf, 3);

	oled_send_command(SSD1306_CMD_SET_PAGE_ADDRESS_GDDRAM_MIN);
	oled_send_command(SSD1306_CMD_SET_BACKWARD_SCAN_DIRECTION);
	oled_send_command(SSD1306_CMD_SET_LOW_ADDRESS_COLUMN_MIN);
	oled_send_command(SSD1306_CMD_SET_HIGH_ADDRESS_COLUMN_MIN);
	oled_send_command(SSD1306_CMD_SET_START_LINE_MIN);

	tmp_buf[0] = SSD1306_CMD_SET_CONTRAST;
	tmp_buf[1] = 0xFF;
	oled_send_command_bytes(tmp_buf, 2);

	oled_send_command(SSD1306_CMD_REMAP_COLUMN_0_SEG_ZERO);

	oled_send_command(SSD1306_CMD_SET_NON_INVERSE_COLOR);

	tmp_buf[0] = SSD1306_CMD_SET_MULTIPLEX_RATIO;
	tmp_buf[1] = SSD1306_PARAM_MAX_RATIO;
	oled_send_command_bytes(tmp_buf, 2);

	oled_send_command(SSD1306_CMD_SHOW_MEMORY_CONTENT);

	tmp_buf[0] = SSD1306_CMD_SET_DISPLAY_LINE_OFFSET;
	tmp_buf[1] = 0U;
	oled_send_command_bytes(tmp_buf, 2);

	tmp_buf[0] = SSD1306_CMD_SET_DIV_RATIO_OSC_CLOCK;
	tmp_buf[1] = 0xF0;
	oled_send_command_bytes(tmp_buf, 2);

	tmp_buf[0] = SSD1306_CMD_SET_PRECHARGE_PERIOD;
	tmp_buf[1] = 0x22;
	tmp_buf[2] = 0x00;
	tmp_buf[3] = 0x04;
	oled_send_command_bytes(tmp_buf, 4);

	tmp_buf[0] = SSD1306_CMD_SET_COM_HW_CONFIG;
	tmp_buf[1] = 0x12;
	oled_send_command_bytes(tmp_buf, 2);

	tmp_buf[0] = 0xDB;
	tmp_buf[1] = 0x20;
	oled_send_command_bytes(tmp_buf, 2);

	//Enable DC-DC
	tmp_buf[0] = 0x8D;
	tmp_buf[1] = 0x14;
	oled_send_command_bytes(tmp_buf, 2);

	oled_send_command(SSD1306_CMD_DISPLAY_ON);
  ets_delay_us(1000*100);
}

esp_err_t oled_send_byte(uint8_t value)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &value;
    return spi_device_polling_transmit(oled_spi_dev, &t);
}

esp_err_t oled_send_bytes(uint8_t *data, uint16_t bytes_count)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * bytes_count;
    t.tx_buffer = data;
    return spi_device_polling_transmit(oled_spi_dev, &t);
}

void oled_send_command(uint8_t d)
{
  gpio_set_level(G_DISP_PIN_NUM_CD, 0);
  oled_send_byte(d);
  ets_delay_us(1);
}

void oled_send_command_bytes(uint8_t *data, uint16_t bytes_count)
{
  gpio_set_level(G_DISP_PIN_NUM_CD, 0);
  oled_send_bytes(data, bytes_count);
  ets_delay_us(1);
}

void oled_send_data(uint8_t d)
{
  gpio_set_level(G_DISP_PIN_NUM_CD, 1);
  oled_send_byte(d);
  ets_delay_us(1);
}

void oled_full_clear(void)
{
  if (oled_init_ok == false)
    return;

  //TODO
}

void oled_send_full_framebuffer(uint8_t *data)
{
  if (oled_init_ok == false)
    return;

 	for (uint8_t locCNT = 0U; locCNT < SSD1306_PAGE_COUNT; locCNT++)
  {
		//select page
    uint8_t tmp_byte = SSD1306_CMD_SET_PAGE_ADDRESS_GDDRAM_MIN + locCNT;
		oled_send_command(tmp_byte);
		//set pos at the start of the page
		oled_send_command(SSD1306_CMD_SET_LOW_ADDRESS_COLUMN_MIN);
		oled_send_command(SSD1306_CMD_SET_HIGH_ADDRESS_COLUMN_MIN);
		
    gpio_set_level(G_DISP_PIN_NUM_CD, 1);//data
		oled_send_bytes(&data[G_DISP_WIDTH * locCNT], G_DISP_WIDTH);
	}
}

/// @brief Set brightness
/// @param code 0-255
void oled_set_brightness(uint8_t code)
{
  uint8_t tmp_buf[2];
	tmp_buf[0] = SSD1306_CMD_SET_CONTRAST;
	tmp_buf[1] = code;
	oled_send_command_bytes(tmp_buf, 2);
}
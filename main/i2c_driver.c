#include "i2c_driver.h"
#include "clock_config.h"
#include "freertos/FreeRTOS.h"

#define WRITE_BIT                          I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                           I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                            0x0              /*!< I2C ack value */
#define NACK_VAL                           0x1              /*!< I2C nack value */

void i2c_driver_init(i2c_port_t i2c_num)
{
    int i2c_master_port = (int)i2c_num;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;

    conf.scl_io_num = I2C_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;
    conf.clk_flags = 0,
    i2c_param_config(i2c_master_port, &conf);

    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

esp_err_t i2c_driver_read_slave_reg(
    i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_rd, size_t size)
{
    if (size == 0) 
        return ESP_OK;

    esp_err_t res = i2c_master_write_to_device(i2c_num, i2c_addr, &i2c_reg, 1, 200 / portTICK_PERIOD_MS);
    if (res != ESP_OK)
        return res;

    res = i2c_master_read_from_device(i2c_num, i2c_addr, data_rd, size, 200 / portTICK_PERIOD_MS);
    if (res != ESP_OK)
        return res;

    return res;
}

esp_err_t i2c_master_write_slave_reg(
    i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    // first, send device address (indicating write) & register to be written
    i2c_master_write_byte(cmd, ( i2c_addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, i2c_reg, ACK_CHECK_EN);

    // write the data
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 300 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


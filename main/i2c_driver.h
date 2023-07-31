#ifndef _I2C_DRIVER_H
#define _I2C_DRIVER_H


#include "driver/i2c.h"

void i2c_driver_init(i2c_port_t i2c_num);

esp_err_t i2c_driver_read_slave_reg(
    i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_rd, size_t size);
esp_err_t i2c_master_write_slave_reg(
    i2c_port_t i2c_num, uint8_t i2c_addr, uint8_t i2c_reg, uint8_t* data_wr, size_t size);

#endif
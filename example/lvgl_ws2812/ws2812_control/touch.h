#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <Wire.h>

int read_touch(int *x, int *y);
int i2c_read(uint16_t addr, uint8_t reg_addr, uint8_t *reg_data, uint32_t length);
int i2c_write(uint8_t addr, uint8_t reg_addr, const uint8_t *reg_data, uint32_t length);


#endif

#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>

#define LCD_I2C_ADDRESS  0x27U

#define LCD_I2C_SDA_PIN       18U
#define LCD_I2C_SCL_PIN       19U

void lcd1602_init(void);
void lcd1602_clear(void);
void lcd1602_set_cursor(uint8_t line, uint8_t position);
void lcd1602_write_char(char c);
void lcd1602_write_string(const char *str);

#endif

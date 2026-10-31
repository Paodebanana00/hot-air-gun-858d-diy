#include "lcd1602.h"

#include "ch.h"
#include "hal.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * STUBS BARE-METAL PARA CALAR O GCC 14 (FORÇA BRUTA)
 * ============================================================ */
#include <sys/stat.h>
#include <sys/types.h>

#define FORCE_THUMB __attribute__((target("thumb"), used))

FORCE_THUMB caddr_t _sbrk(int incr) { (void)incr; return (caddr_t)-1; }
FORCE_THUMB int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
FORCE_THUMB int _close(int file) { (void)file; return -1; }
FORCE_THUMB int _fstat(int file, struct stat *st) { (void)file; (void)st; return 0; }
FORCE_THUMB int _isatty(int file) { (void)file; return 1; }
FORCE_THUMB int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
FORCE_THUMB int _read(int file, char *ptr, int len) { (void)file; (void)ptr; return 0; }
FORCE_THUMB void _exit(int status) { (void)status; while(1); }
FORCE_THUMB int _kill(int id, int sig) { (void)id; (void)sig; return -1; }
FORCE_THUMB int _getpid(void) { return 1; }
/* ============================================================
 * CONFIGURACAO
 * ============================================================ */


#define LCD_I2C_PORT          IOPORT1

#define LCD_I2C_DRIVER        I2CD1
#define LCD_I2C_SPEED         100000U

#define LCD_BACKLIGHT         0x08U
#define LCD_ENABLE_BIT        0x04U

#define LCD_CHARACTER         0x01U
#define LCD_COMMAND           0x00U

#define LCD_DELAY_US          600U

/* Comandos HD44780 */

#define LCD_CLEARDISPLAY      0x01U
#define LCD_ENTRYMODESET      0x04U
#define LCD_DISPLAYCONTROL    0x08U
#define LCD_FUNCTIONSET       0x20U

#define LCD_ENTRYLEFT         0x02U
#define LCD_DISPLAYON         0x04U
#define LCD_2LINE             0x08U

/* ============================================================
 * I2C
 * ============================================================ */

static const I2CConfig lcd_i2c_config = {
    .baudrate = LCD_I2C_SPEED
};

static bool lcd_initialized = false;

static bool lcd_i2c_write(uint8_t value)
{
    msg_t status;
    uint8_t data = value;

    i2cAcquireBus(&LCD_I2C_DRIVER);

    status = i2cMasterTransmitTimeout(
        &LCD_I2C_DRIVER,
        LCD_I2C_ADDRESS,
        &data,
        1U,
        NULL,
        0U,
        TIME_MS2I(20)
    );

    i2cReleaseBus(&LCD_I2C_DRIVER);

    if (status != MSG_OK) {
        lcd_initialized = false;
        return false;
    }

    return true;
}

/* ============================================================
 * ENABLE
 * ============================================================ */

static bool lcd_toggle_enable(uint8_t value)
{
    chThdSleepMicroseconds(LCD_DELAY_US);

    if (!lcd_i2c_write(value | LCD_ENABLE_BIT))
        return false;

    chThdSleepMicroseconds(LCD_DELAY_US);

    if (!lcd_i2c_write(value & (uint8_t)~LCD_ENABLE_BIT))
        return false;

    chThdSleepMicroseconds(LCD_DELAY_US);

    return true;
}

/* ============================================================
 * ENVIO DE BYTE
 * ============================================================ */

static bool lcd_send_byte(uint8_t value, uint8_t mode)
{
    uint8_t high;
    uint8_t low;

    high = mode | (value & 0xF0U) | LCD_BACKLIGHT;

    low = mode | ((value << 4) & 0xF0U) | LCD_BACKLIGHT;

    if (!lcd_i2c_write(high))
        return false;

    if (!lcd_toggle_enable(high))
        return false;

    if (!lcd_i2c_write(low))
        return false;

    if (!lcd_toggle_enable(low))
        return false;

    return true;
}

/* ============================================================
 * COMANDOS
 * ============================================================ */

void lcd1602_clear(void)
{
    if (!lcd_send_byte(LCD_CLEARDISPLAY, LCD_COMMAND))
        return;

    chThdSleepMilliseconds(2);
}

void lcd1602_set_cursor(uint8_t line, uint8_t position)
{
    uint8_t address;

    if (line > 1U)
        line = 1U;

    if (position > 15U)
        position = 15U;

    address = (line == 0U)
        ? (uint8_t)(0x80U + position)
        : (uint8_t)(0xC0U + position);

    lcd_send_byte(address, LCD_COMMAND);
}

void lcd1602_write_char(char c)
{
    lcd_send_byte((uint8_t)c, LCD_CHARACTER);
}

void lcd1602_write_string(const char *str)
{
    if (str == NULL)
        return;

    while (*str != '\0') {
        if (!lcd_send_byte((uint8_t)*str++, LCD_CHARACTER))
            return;
    }
}

/* ============================================================
 * INICIALIZACAO
 * ============================================================ */

void lcd1602_init(void)
{
    lcd_initialized = false;

    /* GP10 = SDA / GP11 = SCL */

    palSetPadMode(LCD_I2C_PORT, LCD_I2C_SDA_PIN, PAL_MODE_ALTERNATE_I2C);
    palSetPadMode(LCD_I2C_PORT, LCD_I2C_SCL_PIN, PAL_MODE_ALTERNATE_I2C);

    /* Inicializa I2C1 */

    i2cStart(&LCD_I2C_DRIVER, &lcd_i2c_config);

    chThdSleepMilliseconds(50);

    /* Inicializacao HD44780 em 4 bits */

    if (!lcd_i2c_write(0x30U | LCD_BACKLIGHT))
        return;

    chThdSleepMilliseconds(5);

    if (!lcd_i2c_write(0x30U | LCD_ENABLE_BIT | LCD_BACKLIGHT))
        return;

    if (!lcd_i2c_write(0x30U | LCD_BACKLIGHT))
        return;

    chThdSleepMilliseconds(5);

    if (!lcd_i2c_write(0x30U | LCD_ENABLE_BIT | LCD_BACKLIGHT))
        return;

    if (!lcd_i2c_write(0x30U | LCD_BACKLIGHT))
        return;

    chThdSleepMicroseconds(150);

    if (!lcd_i2c_write(0x20U | LCD_ENABLE_BIT | LCD_BACKLIGHT))
        return;

    if (!lcd_i2c_write(0x20U | LCD_BACKLIGHT))
        return;

    /* Configuracao normal */

    if (!lcd_send_byte(
        LCD_FUNCTIONSET | LCD_2LINE,
        LCD_COMMAND))
        return;

    if (!lcd_send_byte(
        LCD_DISPLAYCONTROL | LCD_DISPLAYON,
        LCD_COMMAND))
        return;

    if (!lcd_send_byte(
        LCD_ENTRYMODESET | LCD_ENTRYLEFT,
        LCD_COMMAND))
        return;

    if (!lcd_send_byte(
        LCD_CLEARDISPLAY,
        LCD_COMMAND))
        return;

    chThdSleepMilliseconds(2);

    lcd_initialized = true;
}

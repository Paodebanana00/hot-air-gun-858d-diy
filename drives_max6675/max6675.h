#ifndef MAX6675_H
#define MAX6675_H

#include "ch.h"
#include "hal.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX6675_OK             0U
#define MAX6675_OPEN_CIRCUIT   1U
#define MAX6675_NO_READING     2U

/* Raspberry Pi Pico / RP2040 */
#define MAX6675_CS_LINE        PAL_LINE(0U, 13U)
#define MAX6675_SCK_LINE       PAL_LINE(0U, 14U)
#define MAX6675_MISO_LINE      PAL_LINE(0U, 12U)

typedef struct {
    SPIDriver *spi;
    ioline_t cs_line;
    float offset;
    float temperature;
    uint8_t status;
    uint16_t raw;
} max6675_t;

static inline void max6675_init(max6675_t *sensor,
                                SPIDriver *spi,
                                ioline_t cs_line)
{
    if (sensor == NULL || spi == NULL) {
        return;
    }

    sensor->spi = spi;
    sensor->cs_line = cs_line;
    sensor->offset = 0.0f;
    sensor->temperature = 0.0f;
    sensor->status = MAX6675_NO_READING;
    sensor->raw = 0U;

    /* GP14 = SPI1 SCK; GP12 = SPI1 RX/MISO. */
    palSetLineMode(MAX6675_SCK_LINE, PAL_MODE_ALTERNATE(1U));
    palSetLineMode(MAX6675_MISO_LINE, PAL_MODE_ALTERNATE(1U));

    /* CS fica sob controle manual. */
    palSetLineMode(cs_line, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLine(cs_line);
}

static inline void max6675_set_offset(max6675_t *sensor,
                                      float offset)
{
    if (sensor != NULL) {
        sensor->offset = offset;
    }
}

static inline uint8_t max6675_read(max6675_t *sensor)
{
    uint8_t tx[2] = {0x00U, 0x00U};
    uint8_t rx[2] = {0x00U, 0x00U};
    uint16_t raw;

    if (sensor == NULL || sensor->spi == NULL) {
        return MAX6675_NO_READING;
    }

    sensor->status = MAX6675_NO_READING;

    /* O MAX6675 disponibiliza uma palavra de 16 bits. */
    palClearLine(sensor->cs_line);
    chThdSleepMicroseconds(2U);

    spiExchange(sensor->spi, 2U, tx, rx);

    palSetLine(sensor->cs_line);

    raw = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];
    sensor->raw = raw;

    /* D2 = 1: termopar aberto/desconectado. */
    if ((raw & 0x0004U) != 0U) {
        sensor->status = MAX6675_OPEN_CIRCUIT;
        return sensor->status;
    }

    sensor->temperature =
        (float)((raw >> 3U) & 0x0FFFU) * 0.25f;

    sensor->status = MAX6675_OK;
    return sensor->status;
}

static inline float max6675_get_celsius(const max6675_t *sensor)
{
    if (sensor == NULL) {
        return 0.0f;
    }

    return sensor->temperature + sensor->offset;
}

#endif /* MAX6675_H */

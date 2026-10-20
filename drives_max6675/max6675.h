#ifndef MAX6675_CHIBIOS_H
#define MAX6675_CHIBIOS_H

#include "ch.h"
#include "hal.h"
#include <stdint.h>

#define MAX6675_OK             0U
#define MAX6675_OPEN_CIRCUIT   4U
#define MAX6675_NO_READING     128U

typedef struct {
    SPIDriver *spi;
    ioline_t cs_line;
    float offset;
    float temperature;
    uint8_t status;
} max6675_t;

/*
 * O SPI precisa estar previamente configurado para:
 * - 8 bits por frame
 * - CPOL = 0
 * - CPHA = 0 (SPI Mode 0)
 * - MSB primeiro
 * - Clock conservador, por exemplo 500 kHz
 *
 * A compatibilidade exata de SPIConfig depende da versão
 * do ChibiOS e do port de hardware utilizado.
 */
static inline void max6675_init(max6675_t *sensor,
                                SPIDriver *spi,
                                ioline_t cs)
{
    sensor->spi = spi;
    sensor->cs_line = cs;
    sensor->offset = 0.0f;
    sensor->temperature = 0.0f;
    sensor->status = MAX6675_NO_READING;

    palSetLineMode(cs, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLine(cs);
}

static inline void max6675_set_offset(max6675_t *sensor,
                                      float offset)
{
    sensor->offset = offset;
}

static inline uint8_t max6675_read(max6675_t *sensor)
{
    uint8_t tx[2] = {0x00, 0x00};
    uint8_t rx[2] = {0x00, 0x00};
    uint16_t raw;

    /*
     * O MAX6675 não recebe comandos:
     * os bytes transmitidos apenas geram os clocks.
     */
    spiAcquireBus(sensor->spi);

    palClearLine(sensor->cs_line);
    spiSelect(sensor->spi);

    spiExchange(sensor->spi, 2, tx, rx);

    spiUnselect(sensor->spi);
    palSetLine(sensor->cs_line);

    spiReleaseBus(sensor->spi);

    raw = ((uint16_t)rx[0] << 8) | rx[1];

    /* D2 = 1: termopar aberto/desconectado. */
    if ((raw & 0x0004U) != 0U) {
        sensor->status = MAX6675_OPEN_CIRCUIT;
        return sensor->status;
    }

    /* D14:D3 contêm a temperatura em incrementos de 0,25 °C. */
    sensor->temperature =
        (float)((raw >> 3) & 0x0FFFU) * 0.25f;

    sensor->status = MAX6675_OK;
    return sensor->status;
}

static inline float max6675_get_celsius(max6675_t *sensor)
{
    return sensor->temperature + sensor->offset;
}

#endif
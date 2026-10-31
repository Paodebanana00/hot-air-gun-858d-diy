
#include "ch.h"
#include "hal.h"
#include "thermocouple.h"
#include "max6675.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Raspberry Pi Pico / RP2040:
 * SPI1: GP14 = SCK, GP12 = RX/MISO.
 * GP13 = CS manual.
 *
 * SSPCR0:
 * DSS = 7  -> 8 bits
 * SPO = 0  -> clock idle low
 * SPH = 0  -> amostragem na primeira borda
 * SCR = 15 -> divisor serial
 *
 * SSPCPSR = 2.
 * Com clock SPI de 125 MHz, SCK fica aproximadamente
 * em 3,9 MHz.
 */
static const SPIConfig max6675_spi_config = {
    .SSPCR0 = (15U << 8U) | 7U,
    .SSPCPSR = 2U
};

/* Intervalo entre leituras do termopar. */
#define THERMOCOUPLE_PERIOD_MS 250U

static max6675_t temp_sensor;
static THD_WORKING_AREA(waThermocoupleThread, 512);

/* Dados compartilhados com os outros threads. */
static volatile float latest_temperature = 0.0f;
static volatile bool temperature_valid = false;
static volatile uint16_t latest_raw = 0U;
static volatile uint8_t latest_status = MAX6675_NO_READING;

/* Controle das amostras */
static volatile uint32_t latest_sequence = 0U;
static volatile systime_t latest_timestamp = 0U;

static THD_FUNCTION(ThermocoupleThread, arg) {
    (void)arg;

    chRegSetThreadName("MAX6675_SPI1");

    while (true) {
        uint8_t status;
        uint16_t raw;
        float temperature;

        status = max6675_read(&temp_sensor);
        raw = temp_sensor.raw;
        temperature = max6675_get_celsius(&temp_sensor);

        /*
         * Publica diretamente a leitura do MAX6675.
         * Nenhum filtro digital aplicado.
         */
        const systime_t timestamp = chVTGetSystemTimeX();

        /*
         * Mantém a região crítica curta:
         * apenas publicação dos dados compartilhados.
         */

        chSysLock();

        latest_raw = raw;
        latest_status = status;

        if (status == MAX6675_OK) {
            latest_temperature = temperature;
            temperature_valid = true;

            latest_timestamp = timestamp;
            latest_sequence++;
        } else {
            temperature_valid = false;
        }

        chSysUnlock();

        chThdSleepMilliseconds(THERMOCOUPLE_PERIOD_MS);
    }
}

void thermocouple_start(void) {
    /* Inicializa o periférico SPI1 do RP2040. */
    spiStart(&SPID1, &max6675_spi_config);

    max6675_init(&temp_sensor, &SPID1, MAX6675_CS_LINE);
    max6675_set_offset(&temp_sensor, 0.0f);

    chThdCreateStatic(
        waThermocoupleThread,
        sizeof(waThermocoupleThread),
        NORMALPRIO,
        ThermocoupleThread,
        NULL
    );
}

bool thermocouple_get_temperature(float *temperature) {
    bool valid;

    if (temperature == NULL) {
        return false;
    }

    chSysLock();

    valid = temperature_valid;

    if (valid) {
        *temperature = latest_temperature;
    }

    chSysUnlock();

    return valid;
}

uint16_t thermocouple_get_raw(void) {
    uint16_t raw;

    chSysLock();
    raw = latest_raw;
    chSysUnlock();

    return raw;
}

uint8_t thermocouple_get_status(void) {
    uint8_t status;

    chSysLock();
    status = latest_status;
    chSysUnlock();

    return status;
}

/*
 * Obtém uma amostra completa do termopar.
 *
 * Retorna:
 * - Temperatura em graus Celsius.
 * - Contador de amostras válidas.
 * - Timestamp da aquisição.
 *
 * Os três valores são obtidos na mesma região crítica.
 */
bool thermocouple_get_sample(
    float *temperature,
    uint32_t *sequence,
    systime_t *timestamp
) {
    bool valid;

    if (temperature == NULL ||
        sequence == NULL ||
        timestamp == NULL) {
        return false;
    }

    chSysLock();

    valid = temperature_valid;

    if (valid) {
        *temperature = latest_temperature;
        *sequence = latest_sequence;
        *timestamp = latest_timestamp;
    }

    chSysUnlock();

    return valid;
}

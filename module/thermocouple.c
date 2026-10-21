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

/*
 * Filtro EMA:
 * 0.15 = suavização moderada.
 * Valores menores suavizam mais, mas respondem mais lentamente.
 */
#define EMA_ALPHA 0.15f

static max6675_t temp_sensor;
static THD_WORKING_AREA(waThermocoupleThread, 512);

/* Dados compartilhados com os outros threads. */
static volatile float latest_temperature = 0.0f;
static volatile bool temperature_valid = false;
static volatile uint16_t latest_raw = 0U;
static volatile uint8_t latest_status = MAX6675_NO_READING;

static THD_FUNCTION(ThermocoupleThread, arg) {
    (void)arg;

    float filtered_temp = 0.0f;
    bool first_reading = true;

    chRegSetThreadName("MAX6675_SPI1");

    while (true) {
        uint8_t status;
        uint16_t raw;
        float temperature;
        float next_filtered_temp = 0.0f;

        status = max6675_read(&temp_sensor);
        raw = temp_sensor.raw;
        temperature = max6675_get_celsius(&temp_sensor);

        if (status == MAX6675_OK) {
            if (first_reading) {
                /*
                 * Inicializa o filtro com a primeira leitura válida,
                 * evitando começar artificialmente em 0 °C.
                 */
                next_filtered_temp = temperature;
                first_reading = false;
            } else {
                next_filtered_temp =
                    (EMA_ALPHA * temperature) +
                    ((1.0f - EMA_ALPHA) * filtered_temp);
            }

            filtered_temp = next_filtered_temp;
        } else {
            /*
             * Uma leitura inválida invalida a temperatura publicada.
             * A próxima leitura válida reinicializará o filtro.
             */
            first_reading = true;
        }

        /*
         * Mantém a região crítica curta: apenas publicação dos dados
         * compartilhados, sem cálculos de ponto flutuante.
         */
        chSysLock();

        latest_raw = raw;
        latest_status = status;

        if (status == MAX6675_OK) {
            latest_temperature = next_filtered_temp;
            temperature_valid = true;
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
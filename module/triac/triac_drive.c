
#include "ch.h"
#include "hal.h"
#include "module/triac/triac_drive.h"

#include <stdbool.h>
#include <stdint.h>

#define PIN_TRIAC               PAL_LINE(IOPORT1, 0U)

#define POTENCIA_MIN            0
#define POTENCIA_MAX            100


#define TRIAC_SLOT_MS           10U

/*
 * Circuito PNP:
 * GPIO HIGH -> transistor desligado.
 * GPIO LOW  -> transistor ligado.
 */

static volatile int potencia_solicitada = 0;

static THD_WORKING_AREA(waTriacDriverThread, 256);

static inline void triac_acionar(void)
{
    palWriteLine(PIN_TRIAC, PAL_LOW);
}

static inline void triac_desacionar(void)
{
    palWriteLine(PIN_TRIAC, PAL_HIGH);
}

static int ler_potencia_solicitada(void)
{
    int potencia;

    chSysLock();
    potencia = potencia_solicitada;
    chSysUnlock();

    if (potencia < POTENCIA_MIN) {
        potencia = POTENCIA_MIN;
    } else if (potencia > POTENCIA_MAX) {
        potencia = POTENCIA_MAX;
    }

    return potencia;
}

static THD_FUNCTION(TriacDriverThread, arg)
{
    (void)arg;

    chRegSetThreadName("driver_moc3041");

    triac_desacionar();

    /*
     * Acumulador de densidade de pulsos.
     */
    uint32_t acumulador = 0U;

    while (true) {
        const int potencia = ler_potencia_solicitada();

        if (potencia == 0) {
            acumulador = 0U;
            triac_desacionar();
        } else if (potencia >= POTENCIA_MAX) {
            acumulador = 0U;
            triac_acionar();
        } else {
            acumulador += (uint32_t)potencia;

            if (acumulador >= 100U) {
                acumulador -= 100U;
                triac_acionar();
            } else {
                triac_desacionar();
            }
        }

        chThdSleepMilliseconds(TRIAC_SLOT_MS);
    }
}

void triac_driver_init(void)
{
    palSetLineMode(PIN_TRIAC, PAL_MODE_OUTPUT_PUSHPULL);

    triac_desacionar();

    chSysLock();
    potencia_solicitada = 0;
    chSysUnlock();

    chThdCreateStatic(
        waTriacDriverThread,
        sizeof(waTriacDriverThread),
                      NORMALPRIO,
                      TriacDriverThread,
                      NULL
    );
}

void triac_set_power(int power)
{
    if (power < POTENCIA_MIN) {
        power = POTENCIA_MIN;
    } else if (power > POTENCIA_MAX) {
        power = POTENCIA_MAX;
    }

    chSysLock();
    potencia_solicitada = power;
    chSysUnlock();
}

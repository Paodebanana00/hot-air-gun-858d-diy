#include "ch.h"
#include "hal.h"
#include "module/triac/triac_drive.h"

#include <stdbool.h>
#include <stdint.h>

#define PIN_TRIAC       PAL_LINE(IOPORT1, 0U)
#define JANELA_TOTAL_MS 1000U

#define POTENCIA_MIN    0
#define POTENCIA_MAX    100

/*
 * Circuito com PNP:
 *
 * GPIO HIGH -> transistor desligado -> LED do MOC3041 apagado
 * GPIO LOW  -> transistor ligado    -> LED do MOC3041 aceso
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

static THD_FUNCTION(TriacDriverThread, arg)
{
    (void)arg;

    chRegSetThreadName("driver_moc3041");

    /* Estado inicial seguro: LED do MOC3041 apagado. */
    triac_desacionar();

    while (true) {
        int potencia;
        systime_t inicio_janela;
        uint32_t tempo_ligado;

        inicio_janela = chVTGetSystemTimeX();

        chSysLock();
        potencia = potencia_solicitada;
        chSysUnlock();

        if (potencia < POTENCIA_MIN) {
            potencia = POTENCIA_MIN;
        } else if (potencia > POTENCIA_MAX) {
            potencia = POTENCIA_MAX;
        }

        if (potencia == 0) {
            /* 0%: saída inativa durante toda a janela. */
            triac_desacionar();
        } else if (potencia == 100) {
            /* 100%: saída ativa durante toda a janela. */
            triac_acionar();
        } else {
            tempo_ligado = (uint32_t)potencia * 10U;

            /* Parte ativa: GPIO LOW. */
            triac_acionar();
            chThdSleepMilliseconds(tempo_ligado);

            /* Parte inativa: GPIO HIGH. */
            triac_desacionar();
        }

        /* Mantém a janela total próxima de 1000 ms. */
        chThdSleepUntil(
            inicio_janela + TIME_MS2I(JANELA_TOTAL_MS)
        );
    }
}

void triac_driver_init(void)
{
    palSetLineMode(PIN_TRIAC, PAL_MODE_OUTPUT_PUSHPULL);

    /* Inicializa desligado antes de iniciar a thread. */
    triac_desacionar();

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

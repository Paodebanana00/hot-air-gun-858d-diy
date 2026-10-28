
#include "ch.h"
#include "hal.h"
#include "module/triac/triac_drive.h"

#include <stdbool.h>
#include <stdint.h>

#define PIN_TRIAC               PAL_LINE(IOPORT1, 0U)

#define POTENCIA_MIN            0
#define POTENCIA_MAX            100

/*
 * Intervalo entre decisoes de acionamento.
 *
 * O MOC3041 e um optotriac com deteccao de cruzamento
 * por zero. Nao usamos controle de angulo de fase.
 */
#define TRIAC_SLOT_MS           10U

/*
 * Circuito PNP:
 *
 * GPIO HIGH -> transistor desligado -> LED do MOC apagado
 * GPIO LOW  -> transistor ligado    -> LED do MOC aceso
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

    /*
     * Inicializacao segura: nao solicitar disparo.
     */
    triac_desacionar();

    /*
     * Acumulador de densidade de pulsos.
     *
     * A cada slot:
     *   acumulador += potencia
     *
     * Ao atingir 100, ativa um slot e subtrai 100.
     *
     * Isso distribui os slots ativos ao longo do tempo,
     * em vez de concentrar toda a potencia no inicio
     * de uma janela longa.
     */
    uint32_t acumulador = 0U;

    systime_t proximo_slot = chVTGetSystemTimeX();

    while (true) {
        const int potencia = ler_potencia_solicitada();

        if (potencia == 0) {
            /*
             * Desligamento imediato do LED do optotriac.
             * O triac de potencia ja disparado pode continuar
             * conduzindo ate a proxima passagem por zero.
             */
            acumulador = 0U;
            triac_desacionar();
        } else if (potencia >= 100) {
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

        proximo_slot += TIME_MS2I(TRIAC_SLOT_MS);
        chThdSleepUntil(proximo_slot);
    }
}

void triac_driver_init(void)
{
    /*
     * Configura e desliga a saida antes de criar a thread.
     */
    palSetLineMode(PIN_TRIAC, PAL_MODE_OUTPUT_PUSHPULL);
    triac_desacionar();

    potencia_solicitada = 0;

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

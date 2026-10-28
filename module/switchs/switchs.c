#include "switchs.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Os botoes/reed switch usam INPUT_PULLUP:
 * PAL_HIGH = contato aberto; PAL_LOW = contato fechado para GND.
 *
 * O Core 0 chama processar_botoes() periodicamente (no codigo atual,
 * aproximadamente a cada 50 ms). Tres amostras iguais filtram oscilacoes
 * curtas sem bloquear o loop principal.
 */
#define DEBOUNCE_AMOSTRAS 3U

volatile bool estado_sistema_1 = false;
volatile bool estado_sistema_2 = false;

static uint8_t estado_estavel_btn1 = PAL_HIGH;
static uint8_t estado_estavel_btn2 = PAL_HIGH;
static uint8_t candidato_btn1 = PAL_HIGH;
static uint8_t candidato_btn2 = PAL_HIGH;
static uint8_t contador_btn1 = 0U;
static uint8_t contador_btn2 = 0U;

static uint8_t debounce_atualizar(uint8_t leitura,
                                  uint8_t *candidato,
                                  uint8_t *contador,
                                  uint8_t estado_estavel)
{
    if (leitura != *candidato) {
        *candidato = leitura;
        *contador = 1U;
    } else if (*contador < DEBOUNCE_AMOSTRAS) {
        (*contador)++;
    }

    if (*contador >= DEBOUNCE_AMOSTRAS) {
        estado_estavel = *candidato;
    }

    return estado_estavel;
}

void botoes_init(void)
{
    palSetPadMode(PORTA_RP2040, GP_GND_TRUE, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(PORTA_RP2040, GP_SOL_TRUE, PAL_MODE_INPUT_PULLUP);

    estado_estavel_btn1 = palReadPad(PORTA_RP2040, GP_GND_TRUE);
    estado_estavel_btn2 = palReadPad(PORTA_RP2040, GP_SOL_TRUE);

    candidato_btn1 = estado_estavel_btn1;
    candidato_btn2 = estado_estavel_btn2;
    contador_btn1 = DEBOUNCE_AMOSTRAS;
    contador_btn2 = DEBOUNCE_AMOSTRAS;

    estado_sistema_1 = (estado_estavel_btn1 == PAL_LOW);
    /* true = reed switch fechado: soprador detectado na base. */
    estado_sistema_2 = (estado_estavel_btn2 == PAL_LOW);
}

void processar_botoes(void)
{
    const uint8_t leitura_btn1 = palReadPad(PORTA_RP2040, GP_GND_TRUE);
    const uint8_t leitura_btn2 = palReadPad(PORTA_RP2040, GP_SOL_TRUE);

    estado_estavel_btn1 = debounce_atualizar(leitura_btn1,
                                             &candidato_btn1,
                                             &contador_btn1,
                                             estado_estavel_btn1);
    estado_estavel_btn2 = debounce_atualizar(leitura_btn2,
                                             &candidato_btn2,
                                             &contador_btn2,
                                             estado_estavel_btn2);

    estado_sistema_1 = (estado_estavel_btn1 == PAL_LOW);
    /* true = na base; false = fora da base. */
    estado_sistema_2 = (estado_estavel_btn2 == PAL_LOW);
}

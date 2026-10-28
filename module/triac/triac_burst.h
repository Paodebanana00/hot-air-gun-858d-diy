
#ifndef TRIAC_BURST_H
#define TRIAC_BURST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t acumulador;
    volatile uint32_t potencia_solicitada;
} triac_burst_t;

/* Inicializa com a resistência desligada. */
void triac_burst_init(triac_burst_t *ctrl);

/* Define potência de 0 a 1000 (0% a 100%). */
void triac_burst_set_power(
    triac_burst_t *ctrl,
    uint32_t potencia
);

/*
 * Chamar uma vez por semiciclo, sincronizado
 * por um detector externo de cruzamento por zero.
 *
 * Retorna true se o semiciclo deve ser energizado.
 */
bool triac_burst_next_half_cycle(triac_burst_t *ctrl);

#endif

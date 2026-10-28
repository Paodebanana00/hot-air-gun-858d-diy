#ifndef STATION_CONTROL_H
#define STATION_CONTROL_H

#include "module/engine/engine_shared.h"

typedef enum {
    ESTADO_DESLIGADO = 0,
    ESTADO_LIGADO_NA_BASE,
    ESTADO_EM_OPERACAO,
    ESTADO_RESFRIAMENTO_SEGURO
} station_state_t;

/* Processamento executado pelo Core 1 */
void station_control_process(
    const core0_state_t *hardware,
    core1_commands_t *commands
);

#endif

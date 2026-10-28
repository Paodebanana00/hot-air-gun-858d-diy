
#include "triac_burst.h"

#define TRIAC_BURST_SCALE 1000U

void triac_burst_init(triac_burst_t *ctrl)
{
    if (ctrl == 0) return;

    ctrl->acumulador = 0;
    ctrl->potencia_solicitada = 0;
}

void triac_burst_set_power(
    triac_burst_t *ctrl,
    uint32_t potencia)
{
    if (ctrl == 0) return;

    if (potencia > TRIAC_BURST_SCALE)
        potencia = TRIAC_BURST_SCALE;

    ctrl->potencia_solicitada = potencia;
}

bool triac_burst_next_half_cycle(triac_burst_t *ctrl)
{
    if (ctrl == 0) return false;

    uint32_t potencia = ctrl->potencia_solicitada;

    if (potencia == 0) {
        ctrl->acumulador = 0;
        return false;
    }

    if (potencia >= TRIAC_BURST_SCALE) {
        ctrl->acumulador = 0;
        return true;
    }

    ctrl->acumulador += potencia;

    if (ctrl->acumulador >= TRIAC_BURST_SCALE) {
        ctrl->acumulador -= TRIAC_BURST_SCALE;
        return true;
    }

    return false;
}

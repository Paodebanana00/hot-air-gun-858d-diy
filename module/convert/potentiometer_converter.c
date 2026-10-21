
#include "module/potentiometers.h"
#include "chprintf.h"
#include <stdbool.h>

static void checar_inicializacao(void) {
    static bool inicializado = false;
    if (!inicializado) {
        potentiometers_start(); 
        inicializado = true;
    }
}


int poten(void) {
    checar_inicializacao();

    uint16_t pot27_local = 0U;
    uint16_t pot28_local = 0U;


    if (!potentiometers_read(&pot27_local, &pot28_local)) {
        return -1; 
    }


    uint16_t temperature_setpoint = 80U + ((uint32_t)pot28_local * 420U / 4095U);
    return (int)temperature_setpoint;
}


int raw_fan(void) {
    checar_inicializacao();

    uint16_t pot27_local = 0U;
    uint16_t pot28_local = 0U;


    if (!potentiometers_read(&pot27_local, &pot28_local)) {
        return 4095; 
    }


    return (int)pot27_local;
}
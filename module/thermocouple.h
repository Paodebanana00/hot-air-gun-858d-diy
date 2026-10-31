#ifndef THERMOCOUPLE_H
#define THERMOCOUPLE_H
#include "ch.h"
#include <stdbool.h>
#include <stdint.h>

void thermocouple_start(void);
bool thermocouple_get_temperature(float *temperature);

uint16_t thermocouple_get_raw(void);
uint8_t thermocouple_get_status(void);

bool thermocouple_get_sample(
    float *temperature,
    uint32_t *sequence,
    systime_t *timestamp
);

#endif
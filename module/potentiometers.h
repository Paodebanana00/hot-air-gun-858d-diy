#ifndef POTENTIOMETERS_H
#define POTENTIOMETERS_H

#include <stdbool.h>
#include <stdint.h>

void potentiometers_start(void);

bool potentiometers_read(uint16_t *gp27, uint16_t *gp28);

#endif
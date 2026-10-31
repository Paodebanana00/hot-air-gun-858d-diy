#ifndef DISPLAY_H
#define DISPLAY_H

#include "module/engine/engine_shared.h"

void display_init(void);

void display_update(const core0_state_t *state);

void display_clear(void);

#endif
    #ifndef THERMOCOUPLE_H
#define THERMOCOUPLE_H

#include <stdbool.h>

void thermocouple_start(void);
bool thermocouple_get_temperature(float *temperature);

#endif
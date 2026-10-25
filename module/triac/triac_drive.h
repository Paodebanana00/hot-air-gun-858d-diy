#ifndef TRIAC_DRIVER_H
#define TRIAC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

void triac_driver_init(void);


void triac_set_power(int power);

#ifdef __cplusplus
}
#endif

#endif /* TRIAC_DRIVER_H */

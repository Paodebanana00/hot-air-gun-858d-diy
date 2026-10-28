
#ifndef PID_H
#define PID_H

#include <stdbool.h>

typedef struct {
    float kp;
    float ki;
    float kd;

    float limite_min;
    float limite_max;
    float setpoint;

    float i_term;
    float temperatura_anterior;
    float derivada_filtrada;

    bool medicao_inicializada;
} pid_controller_t;

void pid_init(
    pid_controller_t *pid,
    float kp,
    float ki,
    float kd,
    float minimo,
    float maximo
);

void pid_set_target(pid_controller_t *pid, float target);
void pid_reset(pid_controller_t *pid);

int pid_compute(
    pid_controller_t *pid,
    float temp_atual,
    float dt,
    float saida_maxima
);

#endif

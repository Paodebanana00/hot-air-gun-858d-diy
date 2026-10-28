
#include "pid.h"

#include <stddef.h>
#include <stdbool.h>

static float limitar(float valor, float minimo, float maximo)
{
    if (valor < minimo) {
        return minimo;
    }

    if (valor > maximo) {
        return maximo;
    }

    return valor;
}

void pid_init(pid_controller_t *pid,
              float kp,
              float ki,
              float kd,
              float min,
              float max)
{
    if (pid == NULL || min >= max) {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->limite_min = min;
    pid->limite_max = max;
    pid->setpoint = 0.0f;
    pid->erro_anterior = 0.0f;
    pid->i_term = 0.0f;
}

void pid_set_target(pid_controller_t *pid, float target)
{
    if (pid == NULL) {
        return;
    }

    pid->setpoint = limitar(
        target,
        pid->limite_min,
        pid->limite_max
    );
}

int pid_compute(pid_controller_t *pid, float temp_atual)
{
    if (pid == NULL) {
        return 0;
    }

    /* Erro positivo significa que ainda falta aquecer. */
    float erro = pid->setpoint - temp_atual;

    float p_term = pid->kp * erro;
    float d_term = pid->kd * (erro - pid->erro_anterior);

    float saida_provisoria = p_term + pid->i_term + d_term;

    /* Anti-windup: integra apenas quando não piora a saturação. */
    bool saturado_alto = saida_provisoria >= pid->limite_max;
    bool saturado_baixo = saida_provisoria <= pid->limite_min;

    if ((!saturado_alto || erro < 0.0f) &&
        (!saturado_baixo || erro > 0.0f)) {
        pid->i_term += pid->ki * erro;
    }

    /* Limita a integral para manter a saída dentro da faixa. */
    pid->i_term = limitar(
        pid->i_term,
        pid->limite_min - p_term - d_term,
        pid->limite_max - p_term - d_term
    );

    float saida = p_term + pid->i_term + d_term;

    pid->erro_anterior = erro;

    saida = limitar(
        saida,
        pid->limite_min,
        pid->limite_max
    );

    /* Arredonda e garante uma potência de 0 a 100%. */
    return (int)(saida + 0.5f);
}

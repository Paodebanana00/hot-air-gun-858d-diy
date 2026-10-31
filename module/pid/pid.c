
#include <stddef.h>
#include <math.h>

#include "pid.h"

/*
 * PID para estação de ar quente 858D.
 *
 * Entrada:
 *   Temperatura em graus Celsius.
 *
 * Saída:
 *   Potência solicitada entre 0 e 100%.
 *
 * O acionamento físico continua sendo responsabilidade
 * exclusiva do driver do TRIAC.
 */

/* Filtro da derivada: constante de tempo em segundos. */
#define PID_DERIVATIVE_FILTER_TAU    0.8f

/*
 * Anti-windup por back-calculation.
 *
 * Constante de rastreamento:
 * Kaw = 1 / Tt
 */
#define PID_TRACKING_TIME            2.0f

/* Limite para eliminar comandos residuais. */
#define PID_MIN_OUTPUT_PERCENT       0.5f

#define PID_OUTPUT_MIN               0.0f
#define PID_OUTPUT_MAX               100.0f

#define PID_MIN_DT                   0.001f
#define PID_MAX_DT                   2.0f


static float limitar(
    float valor,
    float minimo,
    float maximo
)
{
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;

    return valor;
}


/* =====================================================
 * INICIALIZAÇÃO
 * ===================================================== */

void pid_init(
    pid_controller_t *pid,
    float kp,
    float ki,
    float kd,
    float minimo,
    float maximo
)
{
    if (pid == NULL) return;

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;

    pid->limite_min = minimo;
    pid->limite_max = maximo;

    pid->setpoint = minimo;

    pid_reset(pid);
}


/* =====================================================
 * RESET
 * ===================================================== */

void pid_reset(pid_controller_t *pid)
{
    if (pid == NULL) return;

    pid->i_term = 0.0f;

    pid->temperatura_anterior = 0.0f;

    pid->derivada_filtrada = 0.0f;

    pid->medicao_inicializada = false;
}


/* =====================================================
 * SETPOINT
 * ===================================================== */

void pid_set_target(
    pid_controller_t *pid,
    float target
)
{
    if (pid == NULL || !isfinite(target)) return;

    pid->setpoint = limitar(
        target,
        pid->limite_min,
        pid->limite_max
    );
}


/* =====================================================
 * CONTROLADOR PID
 * ===================================================== */

int pid_compute(
    pid_controller_t *pid,
    float temp_atual,
    float dt,
    float saida_maxima
)
{
    if (pid == NULL) return 0;

    /*
     * Falha de entrada:
     * nunca produzir potência com dados inválidos.
     */
    if (!isfinite(temp_atual) ||
        !isfinite(dt) ||
        !isfinite(saida_maxima) ||
        !isfinite(pid->setpoint)) {

        return 0;
    }

    if (dt < PID_MIN_DT || dt > PID_MAX_DT) {
        return 0;
    }

    if (saida_maxima <= 0.0f) {
        return 0;
    }

    const float limite_superior = limitar(
        saida_maxima,
        PID_OUTPUT_MIN,
        limitar(
            pid->limite_max,
            PID_OUTPUT_MIN,
            PID_OUTPUT_MAX
        )
    );

    if (limite_superior <= 0.0f) {
        return 0;
    }

    /*
     * 1. ERRO
     */

    const float erro =
        pid->setpoint - temp_atual;


    /*
     * 2. DERIVADA SOBRE A MEDIÇÃO
     *
     * Evita derivative kick ao alterar o setpoint.
     */

    float velocidade = 0.0f;

    if (pid->medicao_inicializada) {

        velocidade =
            (temp_atual - pid->temperatura_anterior) / dt;

        const float alpha =
            dt / (PID_DERIVATIVE_FILTER_TAU + dt);

        pid->derivada_filtrada +=
            alpha * (
                velocidade - pid->derivada_filtrada
            );

    } else {

        pid->derivada_filtrada = 0.0f;

        pid->medicao_inicializada = true;
    }

    pid->temperatura_anterior = temp_atual;


    /*
     * 3. TERMOS PROPORCIONAL E DERIVATIVO
     */

    const float p_term =
        pid->kp * erro;

    const float d_term =
        -pid->kd * pid->derivada_filtrada;


    /*
     * 4. PID SEM SATURAÇÃO
     */

    float saida_bruta =
        p_term +
        pid->i_term +
        d_term;


    /*
     * 5. SATURAÇÃO DA SAÍDA
     */

    float saida_limitada = limitar(
        saida_bruta,
        PID_OUTPUT_MIN,
        limite_superior
    );


    /*
     * 6. ANTI-WINDUP POR BACK-CALCULATION
     *
     * A integral recebe:
     *
     * a) Correção proporcional ao erro.
     *
     * b) Correção da diferença entre saída real
     *    e saída solicitada antes da saturação.
     *
     * Diferentemente da implementação anterior,
     * a integral também diminui quando a temperatura
     * ultrapassa o setpoint.
     */

    const float kaw =
        1.0f / PID_TRACKING_TIME;

    const float integracao =
        pid->ki * erro;

    const float correcao_windup =
        kaw * (saida_limitada - saida_bruta);

    pid->i_term += (
        integracao + correcao_windup
    ) * dt;


    /*
     * 7. LIMITAÇÃO DA INTEGRAL
     *
     * Como temos somente aquecimento,
     * a integral não precisa ser negativa.
     */

    pid->i_term = limitar(
        pid->i_term,
        0.0f,
        limite_superior
    );


    /*
     * 8. RECALCULAR A SAÍDA
     */

    saida_bruta =
        p_term +
        pid->i_term +
        d_term;

    float saida = limitar(
        saida_bruta,
        PID_OUTPUT_MIN,
        limite_superior
    );


    /*
     * 9. ELIMINAR SAÍDAS RESIDUAIS
     */

    if (saida < PID_MIN_OUTPUT_PERCENT) {
        saida = 0.0f;
    }


    /*
     * 10. SAÍDA FINAL
     */

    return (int)(saida + 0.5f);
}

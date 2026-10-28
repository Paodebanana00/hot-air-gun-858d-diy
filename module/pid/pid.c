
#include <stddef.h>
#include "pid.h"

/*
 * Controle gradual para estação de ar quente 858D.
 *
 * A saída é uma solicitação de potência de 0 a 100%.
 * A modulação burst continua sendo responsabilidade
 * exclusiva do triac_drive.c.
 */

/* Filtragem da derivada da temperatura. */
#define PID_DERIVATIVE_FILTER_TAU  0.8f

/* Começa a reduzir a potência 50 °C antes do alvo. */
#define PID_TAPER_RANGE_C          60.0f

/* Frenagem baseada na velocidade de subida da temperatura. */
#define PID_BRAKE_START_CPS        0.4f
#define PID_BRAKE_FULL_CPS         2.5f
#define PID_BRAKE_STRENGTH        0.90f

/* Elimina saídas residuais muito pequenas. */
#define PID_MIN_OUTPUT_PERCENT     0.5f

static float limitar(float valor, float minimo, float maximo)
{
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

void pid_init(
    pid_controller_t *pid,
    float kp,
    float ki,
    float kd,
    float minimo,
    float maximo)
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

void pid_reset(pid_controller_t *pid)
{
    if (pid == NULL) return;

    pid->i_term = 0.0f;
    pid->temperatura_anterior = 0.0f;
    pid->derivada_filtrada = 0.0f;
    pid->medicao_inicializada = false;
}

void pid_set_target(pid_controller_t *pid, float target)
{
    if (pid == NULL) return;

    pid->setpoint = limitar(
        target,
        pid->limite_min,
        pid->limite_max
    );
}

int pid_compute(
    pid_controller_t *pid,
    float temp_atual,
    float dt,
    float saida_maxima)
{
    if (pid == NULL || dt <= 0.0f || saida_maxima <= 0.0f) {
        return 0;
    }

    const float erro = pid->setpoint - temp_atual;

    /*
     * 1. Derivada sobre a medição.
     *
     * Temperatura subindo rapidamente = maior frenagem.
     */
    float velocidade = 0.0f;

    if (pid->medicao_inicializada) {
        velocidade =
            (temp_atual - pid->temperatura_anterior) / dt;

        const float alpha =
            dt / (PID_DERIVATIVE_FILTER_TAU + dt);

        pid->derivada_filtrada +=
            alpha * (velocidade - pid->derivada_filtrada);
    } else {
        pid->derivada_filtrada = 0.0f;
        pid->medicao_inicializada = true;
    }

    pid->temperatura_anterior = temp_atual;

    /*
     * 2. Limite de potência conforme a distância do alvo.
     *
     * 50 °C abaixo: 100% do limite disponível.
     * 25 °C abaixo:  50%.
     * 10 °C abaixo:  20%.
     *  5 °C abaixo:  10%.
     *
     * No alvo ou acima dele: aquecimento desligado.
     */
    float distancia = limitar(
        erro / PID_TAPER_RANGE_C,
        0.0f,
        1.0f
    );

    float limite_potencia = saida_maxima * distancia;

    /*
     * 3. Frenagem antecipada.
     *
     * A frenagem reduz o teto de potência quando a temperatura
     * continua subindo rapidamente.
     */
    float velocidade_positiva =
        pid->derivada_filtrada > 0.0f
            ? pid->derivada_filtrada
            : 0.0f;

    float frenagem = limitar(
        (velocidade_positiva - PID_BRAKE_START_CPS) /
        (PID_BRAKE_FULL_CPS - PID_BRAKE_START_CPS),
        0.0f,
        1.0f
    );

    float fator_frenagem =
        1.0f - PID_BRAKE_STRENGTH * frenagem;

    limite_potencia *= fator_frenagem;

    /*
     * 4. PID: proporcional + integral + derivativo.
     *
     * A derivada atua sobre a temperatura medida, não sobre
     * o setpoint, evitando um salto derivativo ao mudar o alvo.
     */
    const float p_term = pid->kp * erro;
    const float d_term =
        -pid->kd * pid->derivada_filtrada;

    const float saida_sem_integral = p_term + d_term;

    /*
     * 5. Integral com anti-windup condicional.
     *
     * Só integra enquanto estamos abaixo do alvo e quando
     * a correção integral ajuda a sair da saturação ou
     * quando a saída ainda está dentro dos limites.
     *
     * Ao atingir ou ultrapassar o alvo, congela a integral.
     * Não a zera a cada cruzamento, evitando perder toda
     * a correção acumulada durante a aproximação.
     */
    if (erro > 0.0f) {
        const float i_incremento = pid->ki * erro * dt;

        const bool dentro_dos_limites =
            saida_sem_integral >= 0.0f &&
            saida_sem_integral <= limite_potencia;

        const bool ajuda_a_subir =
            saida_sem_integral < limite_potencia &&
            i_incremento > 0.0f;

        const bool ajuda_a_descer =
            saida_sem_integral > 0.0f &&
            i_incremento < 0.0f;

        if (dentro_dos_limites ||
            ajuda_a_subir ||
            ajuda_a_descer) {
            pid->i_term += i_incremento;
        }
    }

    pid->i_term = limitar(
        pid->i_term,
        0.0f,
        saida_maxima
    );

    /*
     * 6. Saída limitada pelo PID e pelo teto de potência.
     */
    float saida =
        p_term + pid->i_term + d_term;

    saida = limitar(
        saida,
        0.0f,
        limite_potencia
    );

    /*
     * 7. Remove comandos residuais muito pequenos.
     * A saída é inteira, pois o driver recebe potência de 0 a 100.
     */
    if (saida < PID_MIN_OUTPUT_PERCENT) {
        saida = 0.0f;
    }

    return (int)(saida + 0.5f);
}

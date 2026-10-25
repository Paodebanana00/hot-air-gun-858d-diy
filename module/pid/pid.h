#ifndef PID_H
#define PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Estrutura que armazena os coeficientes e o estado interno do PID.
 */
typedef struct {
    float kp;               // Ganho Proporcional
    float ki;               // Ganho Integral
    float kd;               // Ganho Derivativo
    float setpoint;         // Temperatura alvo desejada
    float erro_anterior;    // Histórico do erro para o termo derivativo
    float i_term;           // Acumulador do termo integral
    float limite_max;       // Limite máximo de saída (ex: 100)
    float limite_min;       // Limite mínimo de saída (ex: 0)
} pid_controller_t;

/**
 * @brief Inicializa a estrutura do PID com os ganhos e limites configurados.
 */
void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float min, float max);

/**
 * @brief Altera a temperatura alvo (Setpoint).
 */
void pid_set_target(pid_controller_t *pid, float target);

/**
 * @brief Executa o cálculo matemático do PID.
 * @param pid Ponteiro para a estrutura de controle.
 * @param temp_atual A leitura atualizada do seu termopar.
 * @return int Retorna o valor bruto corrigido pronto para injetar no triac_set_power (0 a 100).
 */
int pid_compute(pid_controller_t *pid, float temp_atual);

#ifdef __cplusplus
}
#endif

#endif /* PID_H */

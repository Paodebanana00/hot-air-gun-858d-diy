#include "pid.h"

void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float min, float max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->limite_min = min;
    pid->limite_max = max;
    pid->setpoint = 0.0f;
    pid->erro_anterior = 0.0f;
    pid->i_term = 0.0f;
}

void pid_set_target(pid_controller_t *pid, float target) {
    pid->setpoint = target;
}

int pid_compute(pid_controller_t *pid, float temp_atual) {
    // 1. Cálculo do Erro atual
    float erro = pid->setpoint - temp_atual;

    // 2. Termo Proporcional
    float p_term = pid->kp * erro;

    // 3. Termo Integral com Filtro Anti-Windup Condicional
    // Só acumula o erro se a saída anterior não estiver saturada nos limites extremos
    float termo_provisorio = p_term + pid->i_term + (pid->kd * (erro - pid->erro_anterior));
    if (termo_provisorio > pid->limite_min && termo_provisorio < pid->limite_max) {
        pid->i_term += pid->ki * erro;
    }

    // 4. Termo Derivativo
    float d_term = pid->kd * (erro - pid->erro_anterior);
    pid->erro_anterior = erro; // Atualiza o histórico

    // 5. Soma total das forças
    float saida_bruta = p_term + pid->i_term + d_term;

    // 6. Limitador de Segurança (Clamping rígido)
    if (saida_bruta > pid->limite_max) saida_bruta = pid->limite_max;
    else if (saida_bruta < pid->limite_min) saida_bruta = pid->limite_min;

    // Retorna convertido para inteiro (Casamento perfeito de 0 a 100% com o seu Triac)
    return (int)saida_bruta;
}

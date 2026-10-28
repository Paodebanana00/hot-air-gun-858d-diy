
#include "station_control.h"
#include "module/pid/pid.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define FAN_PWM_OFF          0U
#define FAN_PWM_MAX          4095U
#define TEMP_RESFRIADO_CENTI 6000
#define SETPOINT_MIN_C       0
#define SETPOINT_MAX_C       100

static station_state_t estado_atual = ESTADO_DESLIGADO;
static pid_controller_t termostato;
static bool pid_inicializado = false;
static bool desligamento_solicitado = false;
static bool parada_remota_em_andamento = false;
static bool desligamento_remoto_concluido = false;

extern volatile bool remote_control_active;
extern volatile int32_t remote_setpoint;
extern volatile uint32_t remote_fan_speed;
extern volatile bool remote_stop_requested;

static int32_t last_physical_setpoint = 0;
static uint32_t last_physical_fan = 0;
static bool historico_inicializado = false;

static int32_t limitar_setpoint(int32_t valor)
{
    if (valor < SETPOINT_MIN_C) {
        return SETPOINT_MIN_C;
    }

    if (valor > SETPOINT_MAX_C) {
        return SETPOINT_MAX_C;
    }

    return valor;
}

void station_control_process(const core0_state_t *hardware,
                             core1_commands_t *commands)
{
    if (hardware == NULL || commands == NULL) {
        return;
    }

    if (!pid_inicializado) {
        pid_init(&termostato, 2.5f, 0.08f, 0.4f, 0.0f, 100.0f);
        pid_inicializado = true;
    }

    if (!historico_inicializado) {
        last_physical_setpoint = hardware->temperature_setpoint;
        last_physical_fan = hardware->fan_pot_raw;
        historico_inicializado = true;
    }

    // 1. Recebe a solicitação de parada remota.
    if (remote_stop_requested) {
        remote_stop_requested = false;
        remote_control_active = false;
        remote_setpoint = 0;
        remote_fan_speed = FAN_PWM_MAX;

        parada_remota_em_andamento = true;
        desligamento_remoto_concluido = false;
        desligamento_solicitado = true;
        estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
    }

    // 2. Parada remota: TRIAC desligado e ventilação determinada
    // exclusivamente pela temperatura.
    if (parada_remota_em_andamento) {
        commands->triac_power_command = 0U;

        if (!hardware->temperature_valid ||
            hardware->hardware_faults != 0U) {
            commands->fan_pwm_command = FAN_PWM_MAX;
            commands->shutdown_requested = false;
            return;
        }

        if (hardware->temperature_centi > TEMP_RESFRIADO_CENTI) {
            commands->fan_pwm_command = FAN_PWM_MAX;
            commands->shutdown_requested = false;
            return;
        }

        // Temperatura <= 60 °C: desliga completamente.
        commands->fan_pwm_command = FAN_PWM_OFF;
        commands->shutdown_requested = true;

        parada_remota_em_andamento = false;
        desligamento_remoto_concluido = true;
        desligamento_solicitado = false;
        estado_atual = ESTADO_DESLIGADO;
        return;
    }

    // 3. Depois da parada remota, não religa sozinho.
    if (desligamento_remoto_concluido) {
        commands->triac_power_command = 0U;
        commands->fan_pwm_command = FAN_PWM_OFF;
        commands->shutdown_requested = true;

        if (hardware->button_state_1 == 0U) {
            desligamento_remoto_concluido = false;
        }

        return;
    }

    // 4. Detecta alterações nos controles físicos.
    if (remote_control_active) {
        int32_t diff_setpoint =
            hardware->temperature_setpoint - last_physical_setpoint;

        int32_t diff_fan =
            (int32_t)hardware->fan_pot_raw -
            (int32_t)last_physical_fan;

        if (abs(diff_setpoint) > 3 || abs(diff_fan) > 100) {
            remote_control_active = false;
        }
    }

    last_physical_setpoint = hardware->temperature_setpoint;
    last_physical_fan = hardware->fan_pot_raw;

    int32_t setpoint_final = remote_control_active
        ? limitar_setpoint(remote_setpoint)
        : limitar_setpoint(hardware->temperature_setpoint);

    uint32_t fan_final = remote_control_active
        ? remote_fan_speed
        : hardware->fan_pot_raw;

    if (fan_final > FAN_PWM_MAX) {
        fan_final = FAN_PWM_MAX;
    }

    float t_atual = (float)hardware->temperature_centi / 100.0f;
    pid_set_target(&termostato, (float)setpoint_final);

    // 5. Máquina de estados.
    switch (estado_atual) {
        case ESTADO_DESLIGADO:
            commands->triac_power_command = 0U;
            commands->fan_pwm_command = FAN_PWM_OFF;
            commands->shutdown_requested = true;
            desligamento_solicitado = false;

            if (hardware->button_state_1 == 1U) {
                estado_atual = ESTADO_LIGADO_NA_BASE;
            }
            break;

        case ESTADO_LIGADO_NA_BASE:
            commands->triac_power_command = 0U;
            commands->fan_pwm_command = FAN_PWM_OFF;
            commands->shutdown_requested = false;

            if (hardware->button_state_2 == 0U) {
                estado_atual = ESTADO_EM_OPERACAO;
            }

            if (hardware->button_state_1 == 0U) {
                desligamento_solicitado = true;
                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
            }
            break;

        case ESTADO_EM_OPERACAO:
            commands->shutdown_requested = false;
            commands->fan_pwm_command = fan_final;

            // Nunca aquece com sensor inválido ou falha de hardware.
            if (!hardware->temperature_valid ||
                hardware->hardware_faults != 0U) {
                commands->triac_power_command = 0U;
            } else {
                float output = pid_compute(&termostato, t_atual);

                if (output < 0.0f) {
                    output = 0.0f;
                }

                if (output > 100.0f) {
                    output = 100.0f;
                }

                commands->triac_power_command = (uint32_t)output;
            }

            if (hardware->button_state_2 == 1U) {
                desligamento_solicitado = false;
                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
            }

            if (hardware->button_state_1 == 0U) {
                desligamento_solicitado = true;
                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
            }
            break;

        case ESTADO_RESFRIAMENTO_SEGURO:
            commands->triac_power_command = 0U;
            commands->shutdown_requested = false;

            if (!hardware->temperature_valid ||
                hardware->hardware_faults != 0U) {
                commands->fan_pwm_command = FAN_PWM_MAX;
                break;
            }

            if (hardware->temperature_centi > TEMP_RESFRIADO_CENTI) {
                commands->fan_pwm_command = FAN_PWM_MAX;
                break;
            }

            // Chegou a 60 °C ou menos: ventoinha OFF.
            commands->fan_pwm_command = FAN_PWM_OFF;

            if (desligamento_solicitado ||
                hardware->button_state_1 == 0U) {
                estado_atual = ESTADO_DESLIGADO;
                commands->shutdown_requested = true;
            } else {
                estado_atual = ESTADO_LIGADO_NA_BASE;
                commands->shutdown_requested = false;
            }
            break;

        default:
            estado_atual = ESTADO_DESLIGADO;
            commands->triac_power_command = 0U;
            commands->fan_pwm_command = FAN_PWM_OFF;
            commands->shutdown_requested = true;
            break;
    }

    // 6. Proteção global: falha de sensor ou hardware.
    // Esta proteção prevalece sobre os comandos normais.
    if (!hardware->temperature_valid ||
        hardware->hardware_faults != 0U) {
        commands->triac_power_command = 0U;
        commands->fan_pwm_command = FAN_PWM_MAX;
        commands->shutdown_requested = false;
    }
}

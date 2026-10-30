
#include "station_control.h"
#include "module/pid/pid.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* =========================================================
 * CONFIGURACAO
 * ========================================================= */

#define FAN_PWM_OFF                     0U
#define FAN_PWM_MAX                     4095U
#define FAN_PWM_MIN_AQUECIMENTO         1200U

/* Temperaturas configuraveis em graus Celsius */
#define TEMP_RESFRIADO_C                60
#define TEMP_SETPOINT_MIN_C              0
#define TEMP_SETPOINT_MAX_C            450

/* Conversao para centesimos de grau Celsius */
#define TEMP_RESFRIADO_CENTI \
    (TEMP_RESFRIADO_C * 100)

#define SETPOINT_MIN_C                  TEMP_SETPOINT_MIN_C
#define SETPOINT_MAX_C                  TEMP_SETPOINT_MAX_C

/* Parametros do PID */
#define PID_KP                          2.5f
#define PID_KI                          0.08f
#define PID_KD                          0.4f
#define PID_INTERVAL_SECONDS            0.1f

/* =========================================================
 * ESTADO INTERNO
 * ========================================================= */

static station_state_t estado_atual = ESTADO_DESLIGADO;

static pid_controller_t termostato;
static bool pid_inicializado = false;

static bool desligamento_solicitado = false;
static bool parada_remota_em_andamento = false;
static bool desligamento_remoto_concluido = false;

/* Controle remoto */
extern volatile bool remote_control_active;
extern volatile int32_t remote_setpoint;
extern volatile uint32_t remote_fan_speed;
extern volatile bool remote_stop_requested;

/* =========================================================
 * FUNCOES AUXILIARES
 * ========================================================= */

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

static uint32_t limitar_fan(uint32_t valor)
{
    return (valor > FAN_PWM_MAX) ? FAN_PWM_MAX : valor;
}

static uint32_t limitar_fan_operacao(uint32_t valor)
{
    valor = limitar_fan(valor);

    if (valor < FAN_PWM_MIN_AQUECIMENTO) {
        valor = FAN_PWM_MIN_AQUECIMENTO;
    }

    return valor;
}

static bool sensor_seguro(const core0_state_t *hardware)
{
    return hardware->temperature_valid &&
           hardware->hardware_faults == 0U;
}

static bool temperatura_resfriada(const core0_state_t *hardware)
{
    return sensor_seguro(hardware) &&
           hardware->temperature_centi <= TEMP_RESFRIADO_CENTI;
}

/*
 * Inicializa o PID na primeira chamada.
 * Nas chamadas seguintes, reinicia o estado interno.
 */
static void zerar_pid(void)
{
    if (!pid_inicializado) {
        pid_init(
            &termostato,
            PID_KP,
            PID_KI,
            PID_KD,
            0.0f,
            (float)SETPOINT_MAX_C
        );

        pid_inicializado = true;
    } else {
        pid_reset(&termostato);
    }
}

static void definir_saidas_seguras(core1_commands_t *commands)
{
    commands->triac_power_command = 0U;
    commands->fan_pwm_command = FAN_PWM_MAX;
    commands->shutdown_requested = false;
}

/* =========================================================
 * CONTROLE PRINCIPAL
 * ========================================================= */

void station_control_process(
    const core0_state_t *hardware,
    core1_commands_t *commands
)
{
    if (hardware == NULL || commands == NULL) {
        return;
    }

    if (!pid_inicializado) {
        zerar_pid();
    }

    /* Saidas iniciais */
    commands->triac_power_command = 0U;
    commands->fan_pwm_command = FAN_PWM_OFF;
    commands->shutdown_requested = false;

    const bool interruptor_ligado =
        (hardware->button_state_1 != 0U);

    const bool soprador_na_base =
        (hardware->button_state_2 != 0U);

    const bool sensor_ok =
        sensor_seguro(hardware);

    const bool frio_o_suficiente =
        temperatura_resfriada(hardware);

    /* =====================================================
     * 1. PARADA REMOTA
     * ===================================================== */

    if (remote_stop_requested) {
        remote_stop_requested = false;
        remote_control_active = false;
        remote_setpoint = 0;
        remote_fan_speed = FAN_PWM_MAX;

        parada_remota_em_andamento = true;
        desligamento_remoto_concluido = false;
        desligamento_solicitado = true;

        estado_atual = ESTADO_RESFRIAMENTO_SEGURO;

        zerar_pid();
    }

    if (parada_remota_em_andamento) {
        commands->triac_power_command = 0U;
        commands->fan_pwm_command = FAN_PWM_MAX;
        commands->shutdown_requested = false;

        /*
         * A parada remota so termina quando a temperatura
         * estiver segura e o sensor for confiavel.
         */
        if (!frio_o_suficiente) {
            return;
        }

        commands->fan_pwm_command = FAN_PWM_OFF;
        commands->shutdown_requested = true;

        parada_remota_em_andamento = false;
        desligamento_remoto_concluido = true;
        desligamento_solicitado = false;

        estado_atual = ESTADO_DESLIGADO;

        zerar_pid();

        return;
    }

    if (desligamento_remoto_concluido) {
        commands->triac_power_command = 0U;

        commands->fan_pwm_command = frio_o_suficiente
            ? FAN_PWM_OFF
            : FAN_PWM_MAX;

        commands->shutdown_requested = true;


        if (!interruptor_ligado) {
            desligamento_remoto_concluido = false;
        }

        return;
    }

    /* =====================================================
     * 2. FALHA DE SENSOR OU HARDWARE
     * ===================================================== */

    if (!sensor_ok) {
        definir_saidas_seguras(commands);
        zerar_pid();
        return;
    }

    /* =====================================================
     * 3. SELECAO DOS COMANDOS LOCAL OU REMOTO
     * ===================================================== */

    const int32_t setpoint_final = remote_control_active
        ? limitar_setpoint(remote_setpoint)
        : limitar_setpoint(hardware->temperature_setpoint);

    const uint32_t fan_final = remote_control_active
        ? limitar_fan(remote_fan_speed)
        : limitar_fan(hardware->fan_pot_raw);

    const float temperatura =
        (float)hardware->temperature_centi / 100.0f;

    pid_set_target(&termostato, (float)setpoint_final);

    /* =====================================================
     * 4. TRANSICOES DA MAQUINA DE ESTADOS
     * ===================================================== */

    if (!interruptor_ligado) {

        desligamento_solicitado = true;

        if (frio_o_suficiente) {
            estado_atual = ESTADO_DESLIGADO;
        } else {
            estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
        }

    } else {

        desligamento_solicitado = false;

        switch (estado_atual) {
            case ESTADO_DESLIGADO:

            case ESTADO_RESFRIAMENTO_SEGURO:

                if (soprador_na_base) {
                    estado_atual = ESTADO_LIGADO_NA_BASE;
                } else {
                    estado_atual = ESTADO_EM_OPERACAO;
                    zerar_pid();
                }

                break;

            case ESTADO_LIGADO_NA_BASE:

                if (!soprador_na_base) {
                    estado_atual = ESTADO_EM_OPERACAO;
                    zerar_pid();
                }

                break;

            case ESTADO_EM_OPERACAO:

                if (soprador_na_base) {
                    estado_atual = ESTADO_LIGADO_NA_BASE;
                    zerar_pid();
                }

                break;

            default:

                /*
                 * Estado inesperado: nao liberar aquecimento
                 * sem reinicializar o controlador.
                 */
                estado_atual = ESTADO_EM_OPERACAO;
                zerar_pid();

                break;
        }
    }

    /* =====================================================
     * 5. SAIDAS POR ESTADO
     * ===================================================== */

    switch (estado_atual) {
        case ESTADO_DESLIGADO:

            commands->triac_power_command = 0U;

            commands->fan_pwm_command = frio_o_suficiente
                ? FAN_PWM_OFF
                : FAN_PWM_MAX;

            commands->shutdown_requested = frio_o_suficiente;

            break;

        case ESTADO_LIGADO_NA_BASE:

            commands->triac_power_command = 0U;
            commands->shutdown_requested = false;

            commands->fan_pwm_command = frio_o_suficiente
                ? FAN_PWM_OFF
                : FAN_PWM_MAX;

            break;

        case ESTADO_EM_OPERACAO: {
            commands->shutdown_requested = false;

            commands->fan_pwm_command =
                limitar_fan_operacao(fan_final);

            /*
             * Controle PID da resistencia.
             * Entrada: temperatura em graus Celsius.
             * Saida: potencia de aquecimento de 0 a 100%.
             */
            int output = pid_compute(
                &termostato,
                temperatura,
                PID_INTERVAL_SECONDS,
                100.0f
            );

            if (output < 0) {
                output = 0;
            } else if (output > 100) {
                output = 100;
            }

            commands->triac_power_command = (uint32_t)output;

            break;
        }

        case ESTADO_RESFRIAMENTO_SEGURO:

            /*
             * Este estado so permanece ativo enquanto
             * o interruptor estiver desligado.
             */
            commands->triac_power_command = 0U;
            commands->fan_pwm_command = FAN_PWM_MAX;
            commands->shutdown_requested = false;

            break;

        default:

            commands->triac_power_command = 0U;
            commands->fan_pwm_command = FAN_PWM_MAX;
            commands->shutdown_requested = false;

            break;
    }

    /* =====================================================
     * 6. PROTECAO FINAL
     * ===================================================== */

    if (!sensor_seguro(hardware)) {
        definir_saidas_seguras(commands);
        zerar_pid();
        return;
    }

    /*
     * O resfriamento forcado so e aplicado quando
     * o interruptor estiver desligado.
     *
     * Com o interruptor ligado, a resistencia fica
     * sob controle do PID, independentemente de estar
     * acima ou abaixo da temperatura de resfriamento.
     */
    if (!interruptor_ligado) {
        commands->triac_power_command = 0U;

        if (!frio_o_suficiente) {
            commands->fan_pwm_command = FAN_PWM_MAX;
            commands->shutdown_requested = false;
        } else {
            commands->fan_pwm_command = FAN_PWM_OFF;
            commands->shutdown_requested = true;
        }
    }
}

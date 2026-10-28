#include "station_control.h"
#include "module/pid/pid.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* =========================================================
 * CONFIGURACAO
 * ========================================================= */

#define FAN_PWM_OFF                 0U
#define FAN_PWM_MAX                 4095U
#define FAN_PWM_MIN_AQUECIMENTO     1200U
#define TEMP_RESFRIADO_CENTI        6000

#define SETPOINT_MIN_C              0
#define SETPOINT_MAX_C              100

#define PID_KP                      2.5f
#define PID_KI                      0.08f
#define PID_KD                      0.4f

/*
 * Convencao dos sinais recebidos do Core 0:
 *
 * button_state_1:
 *   1 = permissao/interruptor ligado
 *   0 = desligado
 *
 * button_state_2:
 *   1 = soprador detectado na base
 *   0 = soprador fora da base
 */

/* =========================================================
 * ESTADO INTERNO
 * ========================================================= */

static station_state_t estado_atual = ESTADO_DESLIGADO;

static pid_controller_t termostato;
static bool pid_inicializado = false;

static bool desligamento_solicitado = false;
static bool parada_remota_em_andamento = false;
static bool desligamento_remoto_concluido = false;

/* Controle remoto: definidos no modulo de controle remoto. */
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

static void zerar_pid(void)
{
    pid_init(
        &termostato,
        PID_KP,
        PID_KI,
        PID_KD,
        0.0f,
        100.0f
    );

    pid_inicializado = true;
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

    /*
     * Estado seguro por padrao:
     * aquecimento desligado.
     */
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

    /* -----------------------------------------------------
     * 1. PARADA REMOTA
     *
     * Tem prioridade sobre todos os estados.
     * ----------------------------------------------------- */

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
         * Sem leitura confiavel, nunca presumir
         * que o soprador ja esfriou.
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

        /*
         * Exige que o interruptor seja desligado
         * antes de rearmar a estacao.
         */
        if (!interruptor_ligado) {
            desligamento_remoto_concluido = false;
        }

        return;
    }

    /* -----------------------------------------------------
     * 2. FALHA DE SENSOR/HARDWARE
     *
     * A resistencia desliga e o ventilador fica no maximo.
     * ----------------------------------------------------- */

    if (!sensor_ok) {
        definir_saidas_seguras(commands);
        zerar_pid();
        return;
    }

    /* -----------------------------------------------------
     * 3. SELECAO DOS COMANDOS LOCAL/REMOTO
     * ----------------------------------------------------- */

    const int32_t setpoint_final = remote_control_active
        ? limitar_setpoint(remote_setpoint)
        : limitar_setpoint(hardware->temperature_setpoint);

    const uint32_t fan_final = remote_control_active
        ? limitar_fan(remote_fan_speed)
        : limitar_fan(hardware->fan_pot_raw);

    const float temperatura =
        (float)hardware->temperature_centi / 100.0f;

    pid_set_target(&termostato, (float)setpoint_final);

    /* -----------------------------------------------------
     * 4. TRANSICOES DA MAQUINA DE ESTADOS
     *
     * Reed switch e uma deteccao de nivel:
     *
     * Na base (1): nao aquecer.
     * Fora da base (0): pode entrar em operacao.
     * ----------------------------------------------------- */

    if (!interruptor_ligado) {
        /*
         * Desligar a estacao nao pode interromper
         * o resfriamento de um soprador quente.
         */
        desligamento_solicitado = true;

        if (frio_o_suficiente) {
            estado_atual = ESTADO_DESLIGADO;
        } else {
            estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
        }
    } else {
        switch (estado_atual) {
            case ESTADO_DESLIGADO:

                desligamento_solicitado = false;

                if (!frio_o_suficiente) {
                    estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
                } else if (soprador_na_base) {
                    estado_atual = ESTADO_LIGADO_NA_BASE;
                } else {
                    estado_atual = ESTADO_EM_OPERACAO;
                    zerar_pid();
                }

                break;

            case ESTADO_LIGADO_NA_BASE:

                if (!frio_o_suficiente) {
                    estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
                } else if (!soprador_na_base) {
                    estado_atual = ESTADO_EM_OPERACAO;
                    zerar_pid();
                }

                break;

            case ESTADO_EM_OPERACAO:

                if (soprador_na_base) {
                    /*
                     * Base detectada:
                     * desliga o aquecimento imediatamente
                     * e inicia o resfriamento seguro.
                     */
                    estado_atual = ESTADO_RESFRIAMENTO_SEGURO;

                    desligamento_solicitado = false;

                    zerar_pid();
                }

                break;

            case ESTADO_RESFRIAMENTO_SEGURO:

                /*
                 * Mantem a fan no maximo ate confirmar
                 * temperatura <= 60 graus Celsius.
                 */
                if (frio_o_suficiente) {
                    if (desligamento_solicitado ||
                        !interruptor_ligado) {

                        estado_atual = ESTADO_DESLIGADO;
                        desligamento_solicitado = false;

                    } else if (soprador_na_base) {

                        estado_atual = ESTADO_LIGADO_NA_BASE;

                    } else {

                        estado_atual = ESTADO_EM_OPERACAO;
                        zerar_pid();
                    }
                }

                break;

            default:

                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
                desligamento_solicitado = true;

                zerar_pid();

                break;
        }
    }

    /* -----------------------------------------------------
     * 5. SAIDAS POR ESTADO
     * ----------------------------------------------------- */

    switch (estado_atual) {
        case ESTADO_DESLIGADO:

            commands->triac_power_command = 0U;

            /*
             * Frio: fan desligada.
             * Quente: fan no maximo para resfriar.
             */
            commands->fan_pwm_command = frio_o_suficiente
                ? FAN_PWM_OFF
                : FAN_PWM_MAX;

            commands->shutdown_requested = frio_o_suficiente;

            break;

        case ESTADO_LIGADO_NA_BASE:

            commands->triac_power_command = 0U;
            commands->shutdown_requested = false;

            /*
             * CORRECAO PRINCIPAL:
             *
             * Soprador na base e temperatura <= 60 C:
             * desliga a fan.
             *
             * Temperatura > 60 C:
             * mantem a fan no maximo.
             *
             * O potenciometro nao deve manter a fan ligada
             * quando o soprador esta frio e na base.
             */

            if (frio_o_suficiente) {
                commands->fan_pwm_command = FAN_PWM_OFF;
            } else {
                commands->fan_pwm_command = FAN_PWM_MAX;
            }

            break;

        case ESTADO_EM_OPERACAO: {
            commands->shutdown_requested = false;

            /*
             * Fora da base:
             * segue o potenciometro, respeitando
             * o limite minimo configurado.
             */
            commands->fan_pwm_command =
                limitar_fan_operacao(fan_final);

            int output = pid_compute(
                &termostato,
                temperatura
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
             * Durante o resfriamento:
             * resistencia desligada e fan no maximo.
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

    /* -----------------------------------------------------
     * 6. PROTECAO FINAL INDEPENDENTE DO ESTADO
     * ----------------------------------------------------- */

    if (!sensor_seguro(hardware)) {
        definir_saidas_seguras(commands);
        zerar_pid();
        return;
    }

    /*
     * Se estiver acima de 60 C e nao estiver aquecendo,
     * nunca desligar a fan.
     */
    if (hardware->temperature_centi > TEMP_RESFRIADO_CENTI &&
        estado_atual != ESTADO_EM_OPERACAO) {

        commands->triac_power_command = 0U;
        commands->fan_pwm_command = FAN_PWM_MAX;
        commands->shutdown_requested = false;
    }
}
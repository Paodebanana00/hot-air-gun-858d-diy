#include "station_control.h"
#include "module/pid/pid.h"
#include <stdlib.h> // Necessário para a função abs()

static station_state_t estado_atual = ESTADO_DESLIGADO;
static pid_controller_t termostato;
static bool pid_inicializado = false;

// Declaração das variáveis globais atualizadas pela Thread USB CDC
extern volatile bool remote_control_active;
extern volatile int32_t remote_setpoint;
extern volatile uint32_t remote_fan_speed;

// Histórico para detectar se o usuário girou os botões físicos na bancada
static int32_t last_physical_setpoint = 0;
static uint32_t last_physical_fan = 0;
static bool historico_inicializado = false;

void station_control_process(const core0_state_t *hardware, core1_commands_t *commands) {
    // Inicializa o PID uma única vez
    if (!pid_inicializado) {
        pid_init(&termostato, 2.5f, 0.08f, 0.4f, 0.0f, 100.0f);
        pid_inicializado = true;
    }

    // Inicializa o histórico físico na primeira execução para evitar saltos falsos
    if (!historico_inicializado) {
        last_physical_setpoint = hardware->temperature_setpoint;
        last_physical_fan = hardware->fan_pot_raw;
        historico_inicializado = true;
    }

    // 1. FILTRO DE PRIORIDADE: Verifica se houve ação manual nos potenciômetros da bancada
    if (remote_control_active) {
        int32_t diff_setpoint = hardware->temperature_setpoint - last_physical_setpoint;
        int32_t diff_fan = (int32_t)hardware->fan_pot_raw - (int32_t)last_physical_fan;

        // Se o operador mover os botões além da margem de ruído, cancela o controle remoto
        if (abs(diff_setpoint) > 3 || abs(diff_fan) > 100) {
            remote_control_active = false;
        }
    }

    // Atualiza o histórico para a próxima leitura de 20ms
    last_physical_setpoint = hardware->temperature_setpoint;
    last_physical_fan = hardware->fan_pot_raw;

    // 2. DEFINE OS ALVOS ATUAIS BASEADO EM QUEM TEM A PRIORIDADE (Remoto ou Físico)
    int32_t setpoint_final = remote_control_active ? remote_setpoint : hardware->temperature_setpoint;
    uint32_t fan_final = remote_control_active ? remote_fan_speed : hardware->fan_pot_raw;

    // Configura o alvo no controlador PID baseado na decisão de prioridade
    float t_desejada = (float)setpoint_final;
    float t_atual = (float)hardware->temperature_centi / 100.0f;
    pid_set_target(&termostato, t_desejada);

    // MÁQUINA DE ESTADOS DO PROJETO
    switch (estado_atual) {
        
        case ESTADO_DESLIGADO:
            commands->triac_power_command = 0;
            commands->fan_pwm_command = 0;
            commands->shutdown_requested = true;

            // Botão 1 vira chave -> vai de 0 para 1: Liga a estação
            if (hardware->button_state_1 == 1U) {
                estado_atual = ESTADO_LIGADO_NA_BASE;
            }
            break;

        case ESTADO_LIGADO_NA_BASE:
            commands->triac_power_command = 0;
            commands->fan_pwm_command = 1000; // Fluxo bem baixinho só por segurança
            commands->shutdown_requested = false;

            // Botão 2 sai da base -> vai de 1 para 0: Começa a esquentar
            if (hardware->button_state_2 == 0U) {
                estado_atual = ESTADO_EM_OPERACAO;
            }
            // Se o usuário desligar a chave principal
            if (hardware->button_state_1 == 0U) {
                estado_atual = ESTADO_DESLIGADO;
            }
            break;

        case ESTADO_EM_OPERACAO:
            commands->shutdown_requested = false;
            
            // Aplica a velocidade decidida (seja remota ou do potenciômetro)
            uint32_t cooler_pwm = fan_final;
            
            // PROTEÇÃO: Força um limite mínimo para o cooler não parar enquanto esquenta
            if (cooler_pwm < 1200U) {
                cooler_pwm = 1200U; 
            }
            commands->fan_pwm_command = cooler_pwm;

            // Calcula o PID dinâmico do TRIAC para buscar o setpoint_final
            commands->triac_power_command = (uint32_t)pid_compute(&termostato, t_atual);

            // Botão 2 volta pra base -> vai para 1: Aciona resfriamento imediato
            if (hardware->button_state_2 == 1U) {
                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
            }
            // Se desligar a chave geral no meio do uso, vai para o resfriamento seguro!
            if (hardware->button_state_1 == 0U) {
                estado_atual = ESTADO_RESFRIAMENTO_SEGURO;
            }
            break;

        case ESTADO_RESFRIAMENTO_SEGURO:
            commands->triac_power_command = 0; // CALOR CORTADO TOTALMENTE
            commands->fan_pwm_command = 4095;  // VENTO NO MÁXIMO PARA NÃO DERRETER
            commands->shutdown_requested = false;

            // PROTEÇÃO: Só sai do resfriamento quando baixar de 60.00°C
            if (hardware->temperature_centi < 6000) {
                // Decide se desliga tudo ou volta pro standby da base baseado na chave principal
                if (hardware->button_state_1 == 1U) {
                    estado_atual = ESTADO_LIGADO_NA_BASE;
                } else {
                    estado_atual = ESTADO_DESLIGADO;
                }
            }
            break;
    }

    // PROTEÇÃO 1 (Global): Falha de leitura desliga o aquecimento imediatamente acima de qualquer modo
    if (!hardware->temperature_valid || hardware->hardware_faults != 0U) {
        commands->triac_power_command = 0;
        commands->fan_pwm_command = 4095; // Sopra tudo no máximo para proteger o bocal
    }
}

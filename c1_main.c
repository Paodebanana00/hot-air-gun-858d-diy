#include "ch.h"
#include "hal.h"

#include "module/engine/engine_shared.h"
#include "module/engine/station_control.h" // Inclui o novo controlador separado

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * THREAD 1: ENGINE (PROCESSA A MÁQUINA DE ESTADOS E PID)
 * ============================================================ */

static THD_WORKING_AREA(waEngineThread, 512);

static THD_FUNCTION(EngineThread, arg) {
    (void)arg;

    chRegSetThreadName("engine_logic");

    core0_state_t hardware = {0};
    core1_commands_t commands = {0};

    while (true) {
        /* Recebe o estado físico publicado pelo Core 0 (Sensores, Potenciômetros e Botões). */
        core0_read_state(&hardware);

        /* 
         * Executa a lógica modular da estação (PID, Estados e Proteções).
         * Altera as variáveis commands.triac_power_command e commands.fan_pwm_command internamente.
         */
        station_control_process(&hardware, &commands);

        /* Atualiza os dados comuns de controle */
        commands.target_temperature_centi = hardware.temperature_setpoint * 100;
        commands.sequence++;

        /* Publica os comandos calculados para o Core 0 aplicar no hardware físico. */
        core1_publish_commands(&commands);

        chThdSleepMilliseconds(20);
    }
}


/* ============================================================
 * THREAD 2: DIAGNÓSTICO
 * ============================================================ */

static THD_WORKING_AREA(waDiagnosticThread, 512);

static THD_FUNCTION(DiagnosticThread, arg) {
    (void)arg;

    chRegSetThreadName("core1_diagnostic");

    core0_state_t hardware = {0};
    core1_diagnostic_t diagnostic = {0};

    while (true) {
        /* Lê o estado físico publicado pelo Core 0. */
        core0_read_state(&hardware);

        /*
         * Copia o snapshot recebido para a estrutura de diagnóstico.
         * Não imprime pela USB neste núcleo.
         */
        diagnostic.sequence = hardware.sequence;
        diagnostic.temperature_centi = hardware.temperature_centi;
        diagnostic.temperature_valid = hardware.temperature_valid;
        diagnostic.thermocouple_raw = hardware.thermocouple_raw;
        diagnostic.thermocouple_status = hardware.thermocouple_status;
        diagnostic.fan_pot_raw = hardware.fan_pot_raw;
        diagnostic.temperature_setpoint = hardware.temperature_setpoint;
        
        // Mantém atualizados os estados de entrada que você lê no terminal
        diagnostic.button_state_1 = hardware.button_state_1;
        diagnostic.button_state_2 = hardware.button_state_2;
        diagnostic.hardware_faults = hardware.hardware_faults;

        /* Publica os dados que o Core 0 deverá capturar e imprimir via JSON pela USB. */
        core1_publish_diagnostic(&diagnostic);

        chThdSleepMilliseconds(500);
    }
}


/* ============================================================
 * ENTRADA DO CORE 1
 * ============================================================ */

void c1_main(void) {
    chSysWaitSystemState(ch_sys_running);

    chInstanceObjectInit(&ch1, &ch_core1_cfg);

    chSysUnlock();

    /* Cria a tarefa lógica principal com maior prioridade de agendamento */
    chThdCreateStatic(
        waEngineThread,
        sizeof(waEngineThread),
        NORMALPRIO + 1,
        EngineThread,
        NULL
    );

    /* Cria a tarefa de diagnóstico e telemetria */
    chThdCreateStatic(
        waDiagnosticThread,
        sizeof(waDiagnosticThread),
        NORMALPRIO,
        DiagnosticThread,
        NULL
    );

    while (true) {
        chThdSleepMilliseconds(1000);
    }
}

#include "ch.h"
#include "hal.h"

#include "module/engine/engine_shared.h"
#include "module/engine/station_control.h"

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * THREAD 1: CONTROLE DA MÁQUINA DE ESTADOS
 * ============================================================ */

static THD_WORKING_AREA(waEngineThread, 768);

static THD_FUNCTION(EngineThread, arg)
{
    (void)arg;

    chRegSetThreadName("engine_logic");

    core0_state_t hardware = {0};
    core1_commands_t commands = {0};

    while (true) {
        /*
         * Só processa a máquina de estados se conseguiu
         * obter o snapshot publicado pelo Core 0.
         */
        if (core0_read_state(&hardware)) {

            /*
             * Toda a lógica de estados, PID e proteção
             * continua centralizada no controlador.
             */
            station_control_process(&hardware, &commands);

            commands.target_temperature_centi =
                hardware.temperature_setpoint * 100;

            commands.sequence++;

            /*
             * Publica os comandos para aplicação no Core 0.
             */
            core1_publish_commands(&commands);
        }

        chThdSleepMilliseconds(10);
    }
}

/* ============================================================
 * THREAD 2: DIAGNÓSTICO
 *
 * Esta thread nunca escreve diretamente na USB.
 * ============================================================ */

static THD_WORKING_AREA(waDiagnosticThread, 512);

static THD_FUNCTION(DiagnosticThread, arg)
{
    (void)arg;

    chRegSetThreadName("core1_diagnostic");

    core0_state_t hardware = {0};
    core1_diagnostic_t diagnostic = {0};

    while (true) {
        if (core0_read_state(&hardware)) {
            diagnostic.sequence = hardware.sequence;
            diagnostic.temperature_centi =
                hardware.temperature_centi;
            diagnostic.temperature_valid =
                hardware.temperature_valid;
            diagnostic.thermocouple_raw =
                hardware.thermocouple_raw;
            diagnostic.thermocouple_status =
                hardware.thermocouple_status;
            diagnostic.fan_pot_raw =
                hardware.fan_pot_raw;
            diagnostic.temperature_setpoint =
                hardware.temperature_setpoint;

            diagnostic.button_state_1 =
                hardware.button_state_1;
            diagnostic.button_state_2 =
                hardware.button_state_2;
            diagnostic.hardware_faults =
                hardware.hardware_faults;

            core1_publish_diagnostic(&diagnostic);
        }

        chThdSleepMilliseconds(200);
    }
}

/* ============================================================
 * ENTRADA DO CORE 1
 * ============================================================ */

void c1_main(void)
{
    /*
     * Mantém a sequência de inicialização multicore
     * utilizada pelo projeto original.
     *
     * A validade de chInstanceObjectInit() e chSysUnlock()
     * precisa ser confirmada para a versão/configuração
     * multicore específica do ChibiOS utilizada.
     */
    chSysWaitSystemState(ch_sys_running);

    chInstanceObjectInit(&ch1, &ch_core1_cfg);

    chSysUnlock();

    chThdCreateStatic(
        waEngineThread,
        sizeof(waEngineThread),
        NORMALPRIO + 1,
        EngineThread,
        NULL
    );

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

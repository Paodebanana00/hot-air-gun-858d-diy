
#include "ch.h"
#include "hal.h"

#include "module/engine/engine_shared.h"
#include "module/engine/station_control.h"

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * CONFIGURACAO
 * ============================================================ */

#define ENGINE_CONTROL_INTERVAL_MS  10U
#define DIAGNOSTIC_INTERVAL_MS      200U

/* ============================================================
 * THREAD 1: CONTROLE DA MAQUINA DE ESTADOS
 * ============================================================ */

static THD_WORKING_AREA(waEngineThread, 768);

static THD_FUNCTION(EngineThread, arg)
{
    (void)arg;

    chRegSetThreadName("engine_logic");

    core0_state_t hardware = {0};
    core1_commands_t commands = {0};

    while (true) {
        if (core0_read_state(&hardware)) {
            /*
             * Calcula as saidas de controle.
             * station_control_process deve chamar pid_compute
             * somente uma vez por ciclo de controle.
             */
            station_control_process(&hardware, &commands);

            /*
             * Publica o setpoint em centesimos de grau.
             * Verifique se este campo e usado por outros modulos.
             */
            commands.target_temperature_centi =
            hardware.temperature_setpoint * 100;

            commands.sequence++;

            /*
             * Publica os comandos para o Core 0.
             */
            core1_publish_commands(&commands);
        }

        chThdSleepMilliseconds(ENGINE_CONTROL_INTERVAL_MS);
    }
}

/* ============================================================
 * THREAD 2: DIAGNOSTICO
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

        chThdSleepMilliseconds(DIAGNOSTIC_INTERVAL_MS);
    }
}

/* ============================================================
 * ENTRADA DO CORE 1
 * ============================================================ */

void c1_main(void)
{
    /*
     * Inicializacao multicore mantida conforme
     * a estrutura original do projeto.
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

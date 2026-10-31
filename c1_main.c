#include "ch.h"
#include "hal.h"

#include "module/engine/engine_shared.h"
#include "module/engine/station_control.h"
#include "module/display/display.h"

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * CONFIGURACAO
 * ============================================================ */

#define ENGINE_CONTROL_INTERVAL_MS  10U
#define DIAGNOSTIC_INTERVAL_MS      200U
#define DISPLAY_UPDATE_INTERVAL_MS  200U

/* ============================================================
 * THREAD 1: CONTROLE
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

            station_control_process(&hardware, &commands);

            commands.target_temperature_centi =
                hardware.temperature_setpoint * 100;

            commands.sequence++;

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
            diagnostic.temperature_centi = hardware.temperature_centi;
            diagnostic.temperature_valid = hardware.temperature_valid;
            diagnostic.thermocouple_raw = hardware.thermocouple_raw;
            diagnostic.thermocouple_status = hardware.thermocouple_status;

            diagnostic.fan_pot_raw = hardware.fan_pot_raw;
            diagnostic.temperature_setpoint = hardware.temperature_setpoint;

            diagnostic.button_state_1 = hardware.button_state_1;
            diagnostic.button_state_2 = hardware.button_state_2;

            diagnostic.hardware_faults = hardware.hardware_faults;

            core1_publish_diagnostic(&diagnostic);
        }

        chThdSleepMilliseconds(DIAGNOSTIC_INTERVAL_MS);
    }
}

/* ============================================================
 * THREAD 3: DISPLAY LCD
 * ============================================================ */

static THD_WORKING_AREA(waDisplayThread, 768);

static THD_FUNCTION(DisplayThread, arg)
{
    (void)arg;

    chRegSetThreadName("lcd_display");

    core0_state_t hardware = {0};

    display_init();

    while (true) {

        if (core0_read_state(&hardware)) {
            display_update(&hardware);
        }

        chThdSleepMilliseconds(DISPLAY_UPDATE_INTERVAL_MS);
    }
}

/* ============================================================
 * ENTRADA DO CORE 1
 * ============================================================ */

void c1_main(void)
{
    chSysWaitSystemState(ch_sys_running);

    chInstanceObjectInit(&ch1, &ch_core1_cfg);

    chSysUnlock();

    /* Controle da estação */

    chThdCreateStatic(
        waEngineThread,
        sizeof(waEngineThread),
        NORMALPRIO + 1,
        EngineThread,
        NULL
    );

    /* Diagnóstico */

    chThdCreateStatic(
        waDiagnosticThread,
        sizeof(waDiagnosticThread),
        NORMALPRIO,
        DiagnosticThread,
        NULL
    );

    /* Interface LCD */

    chThdCreateStatic(
        waDisplayThread,
        sizeof(waDisplayThread),
        NORMALPRIO - 1,
        DisplayThread,
        NULL
    );

    while (true) {
        chThdSleepMilliseconds(1000);
    }
}
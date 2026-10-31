#ifndef ENGINE_SHARED_H
#define ENGINE_SHARED_H
#include "ch.h"
#include <stdbool.h>
#include <stdint.h>

/* Estado físico publicado pelo Core 0. */
typedef struct {
    uint32_t sequence;

    int32_t temperature_centi;
    bool temperature_valid;
    uint32_t thermocouple_raw;
    uint32_t thermocouple_status;

    /* Identificacao da amostra do MAX6675. */
    uint32_t thermocouple_sequence;
    systime_t thermocouple_timestamp;

    uint32_t fan_pot_raw;
    int32_t temperature_setpoint;

    uint32_t button_state_1;
    uint32_t button_state_2;

    uint32_t fan_pwm_applied;
    uint32_t triac_power_applied;
    uint32_t hardware_faults;
} core0_state_t;


/* Comandos calculados pelo Core 1. */
typedef struct {
    uint32_t sequence;

    uint32_t fan_pwm_command;
    uint32_t triac_power_command;
    int32_t target_temperature_centi;

    bool shutdown_requested;
} core1_commands_t;


/* Snapshot de diagnóstico publicado pelo Core 1. */
typedef struct {
    uint32_t sequence;

    int32_t temperature_centi;
    uint32_t temperature_valid;
    uint32_t thermocouple_raw;
    uint32_t thermocouple_status;

    uint32_t fan_pot_raw;
    int32_t temperature_setpoint;

    uint32_t button_state_1;
    uint32_t button_state_2;

    uint32_t hardware_faults;
} core1_diagnostic_t;


/* Inicialização do módulo compartilhado. */
void engine_shared_init(void);

/* Core 0 -> Core 1 */
void core0_publish_state(const core0_state_t *state);
bool core0_read_state(core0_state_t *state);

/* Core 1 -> Core 0 */
void core1_publish_commands(const core1_commands_t *commands);
bool core1_read_commands(core1_commands_t *commands);

/* Diagnóstico: Core 1 -> Core 0 */
void core1_publish_diagnostic(
    const core1_diagnostic_t *diagnostic);

bool core0_read_diagnostic(
    core1_diagnostic_t *diagnostic);

#endif
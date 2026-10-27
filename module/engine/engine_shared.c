#include "module/engine/engine_shared.h"

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Comunicação compartilhada entre Core 0 e Core 1.
 *
 * Cada tabela tem um único escritor:
 *   core0_state:       Core 0 escreve; Core 1 lê.
 *   core1_commands:    Core 1 escreve; Core 0 lê.
 *   core1_diagnostic:  Core 1 escreve; Core 0 lê.
 *
 * O spinlock de hardware impede que os dois núcleos
 * acessem simultaneamente as estruturas durante a cópia.
 *
 * IMPORTANTE:
 * Este exemplo pressupõe que o port do ChibiOS disponibiliza
 * os spinlocks SIO do RP2040 por meio de hardware.
 * Confirme a API de spinlock do seu port antes de usar.
 */

static core0_state_t shared_state;
static core1_commands_t shared_commands;
static core1_diagnostic_t shared_diagnostic;

static bool shared_initialized;

/*
 * Inicializa as tabelas.
 *
 * Deve ser chamada uma única vez pelo Core 0, antes de
 * publicar dados ou iniciar as threads que usam as tabelas.
 */
void engine_shared_init(void)
{
    memset(&shared_state, 0, sizeof(shared_state));
    memset(&shared_commands, 0, sizeof(shared_commands));
    memset(&shared_diagnostic, 0, sizeof(shared_diagnostic));

    /* Estado inicial seguro: aquecimento desligado. */
    shared_commands.triac_power_command = 0U;
    shared_commands.fan_pwm_command = 4095U;
    shared_commands.shutdown_requested = true;

    shared_initialized = true;
}

/*
 * Core 0 -> Core 1
 * Publica as leituras dos periféricos.
 */
void core0_publish_state(const core0_state_t *state)
{
    if ((state == NULL) || !shared_initialized) {
        return;
    }

    /*
     * TODO: proteger esta cópia com o spinlock de hardware
     * configurado para a comunicação multicore do RP2040.
     */
    shared_state = *state;
}

/*
 * Core 1 lê o último estado publicado pelo Core 0.
 */
bool core0_read_state(core0_state_t *state)
{
    if ((state == NULL) || !shared_initialized) {
        return false;
    }

    /*
     * TODO: usar o mesmo spinlock de hardware da publicação.
     */
    *state = shared_state;

    return true;
}

/*
 * Core 1 -> Core 0
 * Publica os comandos calculados pela engine.
 */
void core1_publish_commands(const core1_commands_t *commands)
{
    if ((commands == NULL) || !shared_initialized) {
        return;
    }

    /*
     * TODO: proteger esta cópia com o spinlock de hardware
     * configurado para a comunicação multicore do RP2040.
     */
    shared_commands = *commands;
}

/*
 * Core 0 lê os comandos publicados pelo Core 1.
 */
bool core1_read_commands(core1_commands_t *commands)
{
    if ((commands == NULL) || !shared_initialized) {
        return false;
    }

    /*
     * TODO: usar o mesmo spinlock de hardware da publicação.
     */
    *commands = shared_commands;

    return true;
}

/*
 * Core 1 -> Core 0
 * Publica um snapshot para diagnóstico e telemetria.
 */
void core1_publish_diagnostic(
    const core1_diagnostic_t *diagnostic)
{
    if ((diagnostic == NULL) || !shared_initialized) {
        return;
    }

    /*
     * TODO: proteger esta cópia com o spinlock de hardware
     * configurado para a comunicação multicore do RP2040.
     */
    shared_diagnostic = *diagnostic;
}

/*
 * Core 0 lê o diagnóstico publicado pelo Core 1.
 */
bool core0_read_diagnostic(
    core1_diagnostic_t *diagnostic)
{
    if ((diagnostic == NULL) || !shared_initialized) {
        return false;
    }

    /*
     * TODO: usar o mesmo spinlock de hardware da publicação.
     */
    *diagnostic = shared_diagnostic;

    return true;
}

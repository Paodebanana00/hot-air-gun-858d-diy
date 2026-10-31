
#include "module/engine/engine_shared.h"

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * RP2040 SIO hardware spinlock.
 */
#define ENGINE_SHARED_SPINLOCK  30U
#define SIO_SPINLOCK_BASE       0xD0000100UL

#define ENGINE_SHARED_LOCK_REG \
    (*(volatile uint32_t *)(SIO_SPINLOCK_BASE + \
                            (4UL * ENGINE_SHARED_SPINLOCK)))

#if defined(__DMB)
#define ENGINE_MEMORY_BARRIER() __DMB()
#elif defined(__ARM_ARCH)
#define ENGINE_MEMORY_BARRIER() __asm volatile ("dmb sy" ::: "memory")
#else
#error "Defina uma barreira de memoria compativel com o microcontrolador."
#endif

static core0_state_t shared_state;
static core1_commands_t shared_commands;
static core1_diagnostic_t shared_diagnostic;

static volatile bool shared_initialized;
static bool state_published;
static bool commands_published;
static bool diagnostic_published;

static void shared_lock(void)
{
    while (ENGINE_SHARED_LOCK_REG == 0U) {
        /* Aguarda o spinlock de hardware. */
    }

    ENGINE_MEMORY_BARRIER();
}

static void shared_unlock(void)
{
    ENGINE_MEMORY_BARRIER();
    ENGINE_SHARED_LOCK_REG = 1U;
}

void engine_shared_init(void)
{
    shared_lock();
    memset(&shared_state, 0, sizeof(shared_state));
    memset(&shared_commands, 0, sizeof(shared_commands));
    memset(&shared_diagnostic, 0, sizeof(shared_diagnostic));

    shared_state.sequence = 0U;

    shared_commands.sequence = 0U;
    shared_commands.triac_power_command = 0U;
    shared_commands.fan_pwm_command = 4095U;
    shared_commands.shutdown_requested = true;

    state_published = false;
    commands_published = false;
    diagnostic_published = false;

    ENGINE_MEMORY_BARRIER();
    shared_initialized = true;

    shared_unlock();
}

void core0_publish_state(const core0_state_t *state)
{
    if (state == NULL || !shared_initialized) {
        return;
    }

    shared_lock();

    shared_state = *state;
    state_published = true;

    shared_unlock();
}

bool core0_read_state(core0_state_t *state)
{
    if (state == NULL || !shared_initialized) {
        return false;
    }

    shared_lock();

    bool valid = state_published;

    if (valid) {
        *state = shared_state;
    }

    shared_unlock();

    return valid;
}

void core1_publish_commands(const core1_commands_t *commands)
{
    if (commands == NULL || !shared_initialized) {
        return;
    }

    shared_lock();

    shared_commands = *commands;
    commands_published = true;

    shared_unlock();
}

bool core1_read_commands(core1_commands_t *commands)
{
    if (commands == NULL || !shared_initialized) {
        return false;
    }

    shared_lock();

    bool valid = commands_published;

    if (valid) {
        *commands = shared_commands;
    }

    shared_unlock();

    return valid;
}

void core1_publish_diagnostic(
    const core1_diagnostic_t *diagnostic)
{
    if (diagnostic == NULL || !shared_initialized) {
        return;
    }

    shared_lock();

    shared_diagnostic = *diagnostic;
    diagnostic_published = true;

    shared_unlock();
}

bool core0_read_diagnostic(
    core1_diagnostic_t *diagnostic)
{
    if (diagnostic == NULL || !shared_initialized) {
        return false;
    }

    shared_lock();

    bool valid = diagnostic_published;

    if (valid) {
        *diagnostic = shared_diagnostic;
    }

    shared_unlock();

    return valid;
}
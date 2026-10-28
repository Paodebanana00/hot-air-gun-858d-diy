#include "ch.h"
#include "hal.h"
#include "module/pwm/pwm_fan.h"



/* ================================================================
 * CHIBIOS HAL CONFIGURATION STRUCTURE
 * ================================================================ */
static const PWMConfig output_pwm_config = {
    .frequency = OUTPUT_PWM_CLOCK,
    .period    = OUTPUT_PWM_PERIOD,
    .callback  = NULL,
    .channels  = {
        [0] = { // Only Channel 0 (Hardware Channel A -> Pin 16) is declared and active
            .mode     = PWM_OUTPUT_ACTIVE_HIGH,
            .callback = NULL
        }
    }
};

/* ================================================================
 * DRIVER FUNCTIONS
 * ================================================================ */

/**
 * @brief Initializes the PWM peripheral driver specifically for Pin 16.
 */
void output_pwm_init(void) {
    // 1. Starts the ChibiOS PWM driver with the stable 100MHz clock and 4095 period
    pwmStart(OUTPUT_PWM_DRIVER, &output_pwm_config);

    // 2. Configures only physical pin 16 to alternate PWM mode
    palSetPadMode(IOPORT1, TARGET_PIN_16, OUTPUT_PWM_PAL_MODE);

    // 3. Forces the channel to start completely turned off
    pwmDisableChannel(OUTPUT_PWM_DRIVER, OUTPUT_PWM_CHANNEL);
}

/**
 * @brief Sets the duty cycle output value linearly on Pin 16.
 * @param duty Raw 12-bit target value coming directly from the potentiometer ADC.
 */
void output_pwm_set_duty(uint16_t duty)
{
    if (duty > OUTPUT_PWM_PERIOD) {
        duty = OUTPUT_PWM_PERIOD;
    }

    pwmEnableChannel(
        OUTPUT_PWM_DRIVER,
        OUTPUT_PWM_CHANNEL,
        (pwmcnt_t)duty
    );
}
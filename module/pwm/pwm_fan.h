#pragma once
#include "ch.h"
#include "hal.h"

/* ================================================================
 * HARDWARE CONFIGURATION
 * ================================================================ */

#define OUTPUT_PWM_DRIVER         (&PWMD0)
#define OUTPUT_PWM_CHANNEL        0   // Channel 0 maps to Hardware Channel A (Pin 16)
#define TARGET_PIN_16             16  // Physical target pin fixed to 16

/* 
 * Stable 12-bit resolution configuration for the RP2040 (F_SYS = 125 MHz)
 * Automatic divider = 1.25 -> Timer Clock = 100 MHz
 * Period = 4095U -> Output frequency stable at ~24.4 kHz (Inaudible and linear)
 */
#define OUTPUT_PWM_CLOCK          32760000
#define OUTPUT_PWM_PERIOD         4095U     

#if defined(PAL_RP_PAD_DRIVE12)
# define OUTPUT_PWM_PAL_MODE      (PAL_MODE_ALTERNATE_PWM | PAL_RP_PAD_DRIVE12 | PAL_RP_GPIO_OE)
#else
# define OUTPUT_PWM_PAL_MODE      PAL_MODE_ALTERNATE_PWM
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the PWM peripheral driver on the designated pin.
 * @note  Call this once during system startup after halInit() and chSysInit().
 */
void output_pwm_init(void);

/**
 * @brief Sets the duty cycle output value.
 * @param duty Raw target value constrained by the driver's max period (0 to 4095).
 */
void output_pwm_set_duty(uint16_t duty);

#ifdef __cplusplus
}
#endif

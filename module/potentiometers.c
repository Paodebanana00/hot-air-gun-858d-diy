
#include "ch.h"
#include "hal.h"
#include "potentiometers.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Raspberry Pi Pico / RP2040:
 * GP27 = ADC1
 * GP28 = ADC2
 *
 * O driver ADC do ChibiOS-Contrib usa channel_mask
 * para selecionar os canais e faz a leitura por interrupção.
 */

#define POTENTIOMETERS_PERIOD_MS 20U

static const ADCConfig adc_config = {
    .div_int  = 479U,
    .div_frac = 0U,
    .shift    = false
};

static const ADCConversionGroup adc_group = {
    .circular     = false,
    .num_channels = 2U,
    .end_cb       = NULL,
    .error_cb     = NULL,
    .channel_mask = (1U << 1U) | (1U << 2U)
};

static THD_WORKING_AREA(waPotentiometersThread, 512);

static volatile uint16_t latest_gp27 = 0U;
static volatile uint16_t latest_gp28 = 0U;
static volatile bool readings_valid = false;

static THD_FUNCTION(PotentiometersThread, arg) {
    (void)arg;

    adcsample_t samples[2];

    chRegSetThreadName("Potentiometers");

    while (true) {
        msg_t result;

        result = adcConvert(&ADCD1, &adc_group, samples, 1U);

        chSysLock();

        if (result == MSG_OK) {
            latest_gp27 = samples[0];
            latest_gp28 = samples[1];
            readings_valid = true;
        } else {
            readings_valid = false;
        }

        chSysUnlock();

        chThdSleepMilliseconds(POTENTIOMETERS_PERIOD_MS);
    }
}

void potentiometers_start(void) {
    /* GP27 = ADC1; GP28 = ADC2. */
    palSetLineMode(PAL_LINE(0U, 27U), PAL_MODE_INPUT_ANALOG);
    palSetLineMode(PAL_LINE(0U, 28U), PAL_MODE_INPUT_ANALOG);

    adcStart(&ADCD1, &adc_config);

    chThdCreateStatic(
        waPotentiometersThread,
        sizeof(waPotentiometersThread),
        NORMALPRIO,
        PotentiometersThread,
        NULL
    );
}

bool potentiometers_read(uint16_t *gp27, uint16_t *gp28) {
    bool valid;

    if ((gp27 == NULL) || (gp28 == NULL)) {
        return false;
    }

    chSysLock();

    valid = readings_valid;

    if (valid) {
        *gp27 = latest_gp27;
        *gp28 = latest_gp28;
    }

    chSysUnlock();

    return valid;
}

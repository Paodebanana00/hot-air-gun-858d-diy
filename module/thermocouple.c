#include "ch.h"
#include "hal.h"
#include "max6675.h"
#include "thermocouple.h"

static max6675_t temp_sensor;

static THD_WORKING_AREA(waThermocoupleThread, 256);

static volatile float latest_temperature;
static volatile bool temperature_valid;

static THD_FUNCTION(ThermocoupleThread, arg)
{
    (void)arg;
    chRegSetThreadName("MAX6675_Reader");

    max6675_init(&temp_sensor, &SPID1, PAL_LINE(0U, 14U));
    max6675_set_offset(&temp_sensor, -3.0f);

    while (true) {
        if (max6675_read(&temp_sensor) == MAX6675_OK) {
            latest_temperature =
                max6675_get_celsius(&temp_sensor);
            temperature_valid = true;
        } else {
            temperature_valid = false;
        }

        chThdSleepMilliseconds(250);
    }
}

void thermocouple_start(void)
{
    chThdCreateStatic(
        waThermocoupleThread,
        sizeof(waThermocoupleThread),
        NORMALPRIO,
        ThermocoupleThread,
        NULL
    );
}

bool thermocouple_get_temperature(float *temperature)
{
    if (temperature == NULL || !temperature_valid) {
        return false;
    }

    *temperature = latest_temperature;
    return true;
}
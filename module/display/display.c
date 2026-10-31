
#include "display.h"
#include "lcd1602.h"

#include <stdio.h>
#include <stdint.h>

void display_init(void)
{
    lcd1602_init();

    lcd1602_set_cursor(0, 0);
    lcd1602_write_string("HOT AIR STATION");

    lcd1602_set_cursor(1, 0);
    lcd1602_write_string("INITIALIZING...");
}

void display_clear(void)
{
    lcd1602_clear();
}

void display_update(const core0_state_t *state)
{
    if (state == NULL)
        return;

    char line1[17];
    char line2[17];

    if (!state->temperature_valid) {

        snprintf(line1, sizeof(line1), "SENSOR ERROR");
        snprintf(line2, sizeof(line2), "HEATER: OFF");

    } else if (state->hardware_faults != 0U) {

        snprintf(line1, sizeof(line1), "HARDWARE FAULT");
        snprintf(line2, sizeof(line2), "CHECK STATION");

    } else {

        int32_t temp = state->temperature_centi;

        uint32_t fan =
            (state->fan_pwm_applied * 100U) / 4095U;

        snprintf(
            line1,
            sizeof(line1),
            "TEMP:%3ld.%1ldC",
            (long)(temp / 100),
            (long)((temp % 100) / 10)
        );

        snprintf(
            line2,
            sizeof(line2),
            "SET:%3ld FAN:%2lu%%",
            (long)state->temperature_setpoint,
            (unsigned long)fan
        );
    }

    lcd1602_set_cursor(0, 0);
    lcd1602_write_string(line1);

    lcd1602_set_cursor(1, 0);
    lcd1602_write_string(line2);
}

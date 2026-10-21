#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "usbcfg.h"
#include "module/thermocouple.h"

int main(void)
{
    halInit();
    chSysInit();

    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000);

    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    usbStart(serusbcfg.usbp, &usbcfg);
    chThdSleepMilliseconds(100);

    usbConnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(500);

    thermocouple_start();

    while (true) {
        float temperature = 0.0f;
        bool valid = thermocouple_get_temperature(&temperature);

        if ((SDU1.config != NULL) &&
            (serusbcfg.usbp->state == USB_ACTIVE)) {

            uint16_t raw = thermocouple_get_raw();
            uint8_t status = thermocouple_get_status();

            chprintf((BaseSequentialStream *)&SDU1,
                     "RAW=0x%04X STATUS=%u ",
                     (unsigned int)raw,
                     (unsigned int)status);

            if (valid) {
                int32_t centi = (int32_t)(temperature * 100.0f);
                int32_t whole = centi / 100;
                int32_t fraction = centi % 100;

                if (fraction < 0) {
                    fraction = -fraction;
                }

                chprintf((BaseSequentialStream *)&SDU1,
                         "TEMP=%d.%02d C\r\n",
                         (int)whole,
                         (int)fraction);
            } else {
                chprintf((BaseSequentialStream *)&SDU1,
                         "TEMP=INVALID\r\n");
            }
        }

        chThdSleepMilliseconds(500);
    }
}

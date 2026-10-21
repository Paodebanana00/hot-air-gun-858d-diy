#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "module/convert/potentiometer_converter.h"
#include "module/usb_cdc.h"
#include "module/thermocouple.h"
#include "usbcfg.h"
#include <stdbool.h>
#include <stdint.h>

int main(void) { halInit(); chSysInit();


uint16_t pot27 = 0U;
uint16_t pot28 = 0U;

usb_cdc_start();
thermocouple_start();



while (true) {
    float temperature = 0.0f;
    bool valid = thermocouple_get_temperature(&temperature);

    if ((SDU1.config != NULL) &&
        (serusbcfg.usbp->state == USB_ACTIVE)) {

        uint16_t raw = thermocouple_get_raw();
        uint8_t status = thermocouple_get_status();

        /* Imprime os dados do termopar. */
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
        int meu_setpoint = poten();

        // Agora você pode usar ou imprimir o valor retornado
        chprintf((BaseSequentialStream *)&SDU1, "Setpoint de Temperatura: %d C\r\n", meu_setpoint);

        int fan_setpoint = raw_fan();

        // Agora você pode usar ou imprimir o valor retornado
        chprintf((BaseSequentialStream *)&SDU1, "Setpoint de velocidade: %d\r\n", fan_setpoint);
        }
    chThdSleepMilliseconds(500);


}

}
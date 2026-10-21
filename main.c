#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "module/usb_cdc.h"
#include "module/thermocouple.h"
#include "module/potentiometers.h"
#include "usbcfg.h"

#include <stdbool.h>
#include <stdint.h>

int main(void) { halInit(); chSysInit();


uint16_t pot27 = 0U;
uint16_t pot28 = 0U;

usb_cdc_start();
thermocouple_start();

/* Inicializa o ADC dos potenciômetros. */
potentiometers_start();

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

        /* Lê e imprime os dois potenciômetros. */
        if (potentiometers_read(&pot27, &pot28)) {
            chprintf((BaseSequentialStream *)&SDU1,
                     "GP27: %u | GP28: %u\r\n",
                     (unsigned int)pot27,
                     (unsigned int)pot28);
        } else {
            chprintf((BaseSequentialStream *)&SDU1,
                     "Erro na leitura ADC\r\n");
        }

        chprintf((BaseSequentialStream *)&SDU1, "\r\n");
    }

    chThdSleepMilliseconds(500);
}
}
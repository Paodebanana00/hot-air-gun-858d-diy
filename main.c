#include "ch.h"
#include "hal.h"
#include "module/thermocouple.h"
#include "usbcfg.h"
#include "chprintf.h"

int main(void)
{
    halInit();
    chSysInit();

    /* Inicialização USB CDC */
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1500);

    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);

    /* Inicialização do termopar */
    thermocouple_start();

    while (true) {

        /* Aguarda a configuração USB pelo computador */
        if (SDU1.config != NULL) {
            float temperature;

            if (thermocouple_get_temperature(&temperature)) {
                chprintf((BaseSequentialStream *)&SDU1,
                         "Temperatura: %.2f C\r\n",
                         (double)temperature);
            } else {
                chprintf((BaseSequentialStream *)&SDU1,
                         "Erro na leitura do termopar!\r\n");
            }
        }

        chThdSleepMilliseconds(500);
    }
}

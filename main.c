#include <stdbool.h>
#include <stddef.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "usbcfg.h"
#include "module/thermocouple.h"

int main(void)
{
    /* 1. Inicializa o hardware básico e o Kernel do ChibiOS */
    halInit();
    chSysInit();

    /* 2. PADRÃO QMK/CHIBIOS: Força o USB a ficar totalmente desconectado eletricamente.
       Isso limpa qualquer lixo ou erro -110 que tenha ficado preso no Linux */
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000); // 1 segundo inteiro de silêncio absoluto no cabo USB

    /* 3. Inicializa as estruturas internas do driver em memória RAM */
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    /* 4. Liga a máquina de estados lógica do USB */
    usbStart(serusbcfg.usbp, &usbcfg);
    chThdSleepMilliseconds(100); // Curto respiro para estabilizar o clock de 48MHz

    /* 5. AGORA SIM: Conecta o Pull-Up físico na linha D+. 
       Para o Linux, é exatamente o equivalente a você plugar o cabo físico nesse instante */
    usbConnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(500); // Tempo para o Linux enumerar o dispositivo em paz

    /* 6. Inicializa o termopar (Sua estação de ar quente começa a trabalhar) */
    thermocouple_start();

    while (true) {
        float temperature;
        bool read_success = thermocouple_get_temperature(&temperature);

        // Transmissão segura e não-blocante
        if ((SDU1.config != NULL) && (serusbcfg.usbp->state == USB_ACTIVE)) {
            if (read_success) {
                chprintf((BaseSequentialStream *)&SDU1, "%.2f\r\n", (double)temperature);
            } else {
                chprintf((BaseSequentialStream *)&SDU1, "0.00\r\n");
            }
        }

        chThdSleepMilliseconds(500);
    }
}

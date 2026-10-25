#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "module/convert/potentiometer_converter.h"
#include "module/usb_cdc.h"
#include "module/thermocouple.h"
#include "usbcfg.h"
#include <stdbool.h>
#include <stdint.h>
#include "module/pwm/pwm_fan.h"

int main(void) { 
    halInit(); 
    chSysInit();

    /* Inicialização dos módulos de hardware */
    usb_cdc_start();
    usb_cdc_handler_init();
    thermocouple_start();
    output_pwm_init();

    while (true) {
        float temperature = 0.0f;
        bool valid = thermocouple_get_temperature(&temperature);

        // --- 1. CONTROLE DO HARDWARE (Roda independente da USB) ---
        // raw_fan() retorna de 0 a 4095, perfeito para o período do seu PWM
        int fan_setpoint = raw_fan(); 
        output_pwm_set_duty((uint16_t)fan_setpoint);

        // Se você ainda usar o poten() para ler uma temperatura alvo, ele fica aqui:
        int meu_setpoint = poten(); 

        // --- 2. TELEMETRIA VIA USB (Só roda se o terminal serial estiver aberto) ---
        if ((SDU1.config != NULL) && (serusbcfg.usbp->state == USB_ACTIVE)) {

            uint16_t raw = thermocouple_get_raw();
            uint8_t status = thermocouple_get_status();

            /* Imprime os dados brutos do termopar */
            chprintf((BaseSequentialStream *)&SDU1,
                     "RAW=0x%04X STATUS=%u ",
                     (unsigned int)raw,
                     (unsigned int)status);

            /* Trata e imprime a temperatura se for válida */
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
                chprintf((BaseSequentialStream *)&SDU1, "TEMP=INVALID\r\n");
            }
            
            /* Imprime as informações do Potenciômetro e PWM */
            chprintf((BaseSequentialStream *)&SDU1, "Setpoint de Temperatura: %d C\r\n", meu_setpoint);
            chprintf((BaseSequentialStream *)&SDU1, "PWM do Cooler (0-4095): %d\r\n", fan_setpoint);
        }

        /* Delay de controle do loop (500ms) */
        chThdSleepMilliseconds(500);
    }
}

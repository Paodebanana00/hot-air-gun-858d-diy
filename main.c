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
#include "module/triac/triac_drive.h" // Inclui o seu driver do TRIAC

// Definição direta do registrador SIO_CPUID do hardware da Raspberry Pi Pico
#define SIO_CPUID (*((volatile uint32_t *)(0xd0000000 + 0x000)))

int main(void) { 
    halInit(); 
    chSysInit();

    /* Inicialização dos módulos de hardware */
    usb_cdc_start();
    usb_cdc_handler_init();
    thermocouple_start();
    output_pwm_init();
    
    // 1. Inicializa o pino do MOC3041 e a thread cega de 1 segundo do TRIAC
    triac_driver_init();

    // 2. Passa LITERALMENTE os 40% de potência desejados para o teste e pronto!
    triac_set_power(40);

    while (true) {
        float temperature = 0.0f;
        bool valid = thermocouple_get_temperature(&temperature);

        // --- 1. CONTROLE DO HARDWARE (Roda independente da USB) ---
        int fan_setpoint = raw_fan(); 
        output_pwm_set_duty((uint16_t)fan_setpoint);

        int meu_setpoint = poten(); 

        // --- 2. TELEMETRIA VIA USB (Só roda se o terminal serial estiver aberto) ---
        if ((SDU1.config != NULL) && (serusbcfg.usbp->state == USB_ACTIVE)) {

            uint16_t raw = thermocouple_get_raw();
            uint8_t status = thermocouple_get_status();

            // Lendo direto do hardware da Pico: retorna 0 para o Core 0 e 1 para o Core 1
            int id_core_atual = (int)SIO_CPUID;

            /* Imprime as informações de diagnóstico e o núcleo ativo */
            chprintf((BaseSequentialStream *)&SDU1,
                     "[NUCLEO %d] RAW=0x%04X STATUS=%u ",
                     id_core_atual,
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
            
            /* Imprime as informações do Potenciômetro, PWM e TRIAC */
            chprintf((BaseSequentialStream *)&SDU1, "Setpoint de Temperatura: %d C\r\n", meu_setpoint);
            chprintf((BaseSequentialStream *)&SDU1, "PWM do Cooler (0-4095): %d\r\n", fan_setpoint);
            chprintf((BaseSequentialStream *)&SDU1, "Potencia Fixa Injetada no TRIAC: 40%%\r\n\r\n");
        }

        /* Delay de controle do loop (500ms) */
        chThdSleepMilliseconds(500);
    }
}

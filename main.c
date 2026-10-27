#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "module/convert/potentiometer_converter.h"
#include "module/usb_cdc.h"
#include "module/thermocouple.h"
#include "module/pwm/pwm_fan.h"
#include "module/triac/triac_drive.h"
#include "module/engine/engine_shared.h"
#include "module/switchs/switchs.h"
#include "module/engine/station_control.h"

#include "usbcfg.h"

#include <stdbool.h>
#include <stdint.h>

core0_state_t hw_state = {0};
core1_commands_t commands = {0};

int main(void)
{
    halInit();
    chSysInit();

    engine_shared_init();

    usb_cdc_start();
    usb_cdc_handler_init();

    thermocouple_start();
    output_pwm_init();
    triac_driver_init();

    /* Inicializa os botões no Core 0. */
    botoes_init();

    /*
     * Estado seguro inicial.
     * Não habilita o aquecimento até receber comandos válidos.
     */
    triac_set_power(0);
    output_pwm_set_duty(4095);

    while (true) {
        float temperature = 0.0f;

        bool valid =
            thermocouple_get_temperature(&temperature);

        /* 1. Atualiza os dados físicos. */
        hw_state.sequence++;

        hw_state.temperature_valid = valid;

        if (valid) {
            hw_state.temperature_centi =
                (int32_t)(temperature * 100.0f);
        }

        hw_state.thermocouple_raw =
            thermocouple_get_raw();

        hw_state.thermocouple_status =
            thermocouple_get_status();

        hw_state.fan_pot_raw =
            (uint32_t)raw_fan();

        hw_state.temperature_setpoint =
            poten();

        /* Atualiza os estados dos botões. */
        processar_botoes();

        hw_state.button_state_1 =
            estado_sistema_1 ? 1U : 0U;

        hw_state.button_state_2 =
            estado_sistema_2 ? 1U : 0U;

        /* 2. Publica os dados físicos para o Core 1. */
        core0_publish_state(&hw_state);

        /* 3. Recebe e aplica os comandos da engine. */
        if (core1_read_commands(&commands)) {
            uint32_t power = commands.triac_power_command;
            uint32_t fan_pwm = commands.fan_pwm_command;

            /*
             * Proteção básica:
             * falha na leitura da temperatura desliga o aquecimento.
             */
            if (!valid || hw_state.hardware_faults != 0U) {
                power = 0U;
            }

            if (commands.shutdown_requested) {
                power = 0U;
                fan_pwm = 4095U;
            }

            if (power > 100U) {
                power = 0U;
            }

            if (fan_pwm > 4095U) {
                fan_pwm = 4095U;
            }

            triac_set_power((uint8_t)power);
            output_pwm_set_duty((uint16_t)fan_pwm);

            hw_state.triac_power_applied = power;
            hw_state.fan_pwm_applied = fan_pwm;
        }

        /* 4. Imprime o diagnóstico recebido do Core 1 em formato JSON */
        if ((SDU1.config != NULL) &&
            (serusbcfg.usbp->state == USB_ACTIVE)) {

            core1_diagnostic_t d;

            if (core0_read_diagnostic(&d)) {
                // Deduz um ID de estado básico apenas pelos botões locais para não travar a compilação
                int estado_estimado = 0;
                if (hw_state.button_state_1 == 1U) {
                    estado_estimado = (hw_state.button_state_2 == 0U) ? 2 : 1; 
                }
                if (hw_state.fan_pwm_applied == 4095U && hw_state.triac_power_applied == 0U && hw_state.temperature_centi > 6000) {
                    estado_estimado = 3; // Resfriamento ativo
                }

                chprintf((BaseSequentialStream *)&SDU1,
                         "{"
                         "\"seq\":%u,"
                         "\"temp_ok\":%u,"
                         "\"temp\":%d.%02d,"
                         "\"raw_tc\":%u,"
                         "\"status_tc\":%u,"
                         "\"pot_fan\":%u,"
                         "\"setpoint\":%d,"
                         "\"btn1\":%u,"
                         "\"btn2\":%u,"
                         "\"faults\":%u,"
                         "\"triac_out\":%u,"
                         "\"fan_out\":%u,"
                         "\"state\":%d"
                         "}\r\n",
                         (unsigned int)d.sequence,
                         (unsigned int)d.temperature_valid,
                         (int)(d.temperature_centi / 100),
                         (int)(d.temperature_centi < 0 ? -(d.temperature_centi % 100) : d.temperature_centi % 100),
                         (unsigned int)d.thermocouple_raw,
                         (unsigned int)d.thermocouple_status,
                         (unsigned int)d.fan_pot_raw,
                         (int)d.temperature_setpoint,
                         (unsigned int)d.button_state_1,
                         (unsigned int)d.button_state_2,
                         (unsigned int)d.hardware_faults,
                         (unsigned int)hw_state.triac_power_applied,
                         (unsigned int)hw_state.fan_pwm_applied,
                         estado_estimado); // <-- Modificado para usar a estimativa local e compilar direto
            }
        }

        chThdSleepMilliseconds(50);
    }
}

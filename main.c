
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

/* Saídas aplicadas pelo Core 0.
 * A thread de telemetria apenas lê estes valores.
 */
static volatile uint32_t applied_triac_power = 0;
static volatile uint32_t applied_fan_pwm = 4095;

/* ============================================================
 * THREAD DE TELEMETRIA USB
 *
 * Nenhuma escrita USB ocorre no laço de controle.
 * Se a USB bloquear, somente esta thread será afetada.
 * ============================================================ */

static THD_WORKING_AREA(waTelemetryThread, 768);

static THD_FUNCTION(TelemetryThread, arg)
{
    (void)arg;

    chRegSetThreadName("usb_telemetry");

    while (true) {
        if (serusbcfg.usbp->state != USB_ACTIVE ||
            SDU1.config == NULL) {
            chThdSleepMilliseconds(200);
            continue;
        }

        core1_diagnostic_t d;

        if (core0_read_diagnostic(&d)) {
            uint32_t triac = applied_triac_power;
            uint32_t fan = applied_fan_pwm;

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
                     "\"fan_out\":%u"
                     "}\r\n",
                     (unsigned int)d.sequence,
                     (unsigned int)d.temperature_valid,
                     (int)(d.temperature_centi / 100),
                     (int)(d.temperature_centi < 0
                         ? -(d.temperature_centi % 100)
                         : d.temperature_centi % 100),
                     (unsigned int)d.thermocouple_raw,
                     (unsigned int)d.thermocouple_status,
                     (unsigned int)d.fan_pot_raw,
                     (int)d.temperature_setpoint,
                     (unsigned int)d.button_state_1,
                     (unsigned int)d.button_state_2,
                     (unsigned int)d.hardware_faults,
                     (unsigned int)triac,
                     (unsigned int)fan);
        }

        /* Limita a frequência da telemetria. */
        chThdSleepMilliseconds(200);
    }
}

/* ============================================================
 * MAIN DO CORE 0
 * ============================================================ */

int main(void)
{
    halInit();
    chSysInit();

    engine_shared_init();

    /* Inicializa USB e receptor de comandos. */
    usb_cdc_start();
    usb_cdc_handler_init();

    /* Inicializa sensores e saídas físicas. */
    thermocouple_start();
    output_pwm_init();
    triac_driver_init();

    botoes_init();

    /* Estado seguro inicial. */
    triac_set_power(0);
    output_pwm_set_duty(4095);

    applied_triac_power = 0;
    applied_fan_pwm = 4095;

    /* Telemetria isolada do controle físico. */
    chThdCreateStatic(
        waTelemetryThread,
        sizeof(waTelemetryThread),
        NORMALPRIO - 1,
        TelemetryThread,
        NULL
    );

    while (true) {
        float temperature = 0.0f;

        bool valid =
            thermocouple_get_temperature(&temperature);

        /* 1. Atualiza as entradas físicas. */
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

        hw_state.temperature_setpoint = poten();

        processar_botoes();

        hw_state.button_state_1 =
            estado_sistema_1 ? 1U : 0U;

        hw_state.button_state_2 =
            estado_sistema_2 ? 1U : 0U;

        /* 2. Publica o snapshot físico para o Core 1. */
        core0_publish_state(&hw_state);

        /*
         * 3. Obtém os comandos do controlador.
         *
         * Não modifica as saídas quando não há uma
         * nova publicação válida de comandos.
         */
        if (core1_read_commands(&commands)) {
            uint32_t power =
                commands.triac_power_command;

            uint32_t fan_pwm =
                commands.fan_pwm_command;

            /*
             * Falha de sensor ou de hardware:
             * desliga o aquecimento e solicita ventilação.
             */
            if (!valid || hw_state.hardware_faults != 0U) {
                power = 0U;
                fan_pwm = 4095U;
            }

            if (commands.shutdown_requested) {
                power = 0U;
            }

            /* Validação dos limites físicos. */
            if (power > 100U) {
                power = 0U;
            }

            if (fan_pwm > 4095U) {
                fan_pwm = 4095U;
            }

            /*
             * Aplica primeiro a política de segurança e
             * depois atualiza os valores de diagnóstico.
             */
            triac_set_power((uint8_t)power);
            output_pwm_set_duty((uint16_t)fan_pwm);

            applied_triac_power = power;
            applied_fan_pwm = fan_pwm;

            hw_state.triac_power_applied = power;
            hw_state.fan_pwm_applied = fan_pwm;
        }

        /*
         * 4. Nenhum chprintf, espera USB ou processamento
         * JSON é executado neste laço.
         */
        chThdSleepMilliseconds(50);
    }
}
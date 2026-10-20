
    #include "ch.h"
    #include "hal.h"
    #include "max6675.h"

    static max6675_t temp_sensor;

    static THD_WORKING_AREA(waThermocoupleThread, 256);

    static THD_FUNCTION(ThermocoupleThread, arg)
    {
        (void)arg;
        chRegSetThreadName("MAX6675_Reader");

        /*
        * Substitua SPID1 pelo driver SPI efetivamente
        * configurado no seu port ChibiOS.
        *
        * A inicialização SPIConfig e o mapeamento dos
        * pinos precisam estar configurados no projeto.
        */
        max6675_init(&temp_sensor, &SPID1, PAL_LINE(0U, 14U));
        max6675_set_offset(&temp_sensor, -3.0f);

        while (true) {
            if (max6675_read(&temp_sensor) == MAX6675_OK) {
                float temp_final =
                    max6675_get_celsius(&temp_sensor);

                /* Use temp_final no controle ou no display. */
                (void)temp_final;
            } else {
                /* Trate a desconexão do termopar. */
            }

            /* O MAX6675 precisa de tempo para uma nova conversão. */
            chThdSleepMilliseconds(250);
        }
    }

    int main(void)
    {
        halInit();
        chSysInit();

        /*
        * Inicie o driver SPI e configure os pinos
        * antes de criar a thread.
        */

        chThdCreateStatic(
            waThermocoupleThread,
            sizeof(waThermocoupleThread),
            NORMALPRIO,
            ThermocoupleThread,
            NULL
        );

        while (true) {
            chThdSleepMilliseconds(1000);
        }
    }

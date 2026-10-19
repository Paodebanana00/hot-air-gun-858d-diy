#include "ch.h"
#include "hal.h"

/*
 * Thread executada no núcleo 1.
 */
static THD_WORKING_AREA(waThreadCore1, 256);

static THD_FUNCTION(ThreadCore1, arg) {
  (void)arg;

  chRegSetThreadName("core1");

  while (true) {
    /*
     * O núcleo 1 permanece ativo.
     * Coloque aqui as tarefas que deseja executar nele.
     */
    chThdSleepMilliseconds(1000);
  }
}

/*
 * Ponto de entrada do núcleo 1.
 */
void c1_main(void) {

  /* Aguarda a inicialização do sistema no núcleo 0. */
  chSysWaitSystemState(ch_sys_running);

  /* Inicializa a instância do kernel neste núcleo. */
  chInstanceObjectInit(&ch1, &ch_core1_cfg);

  /* Libera o núcleo após a inicialização. */
  chSysUnlock();

  /* Cria a thread de trabalho do núcleo 1. */
  chThdCreateStatic(waThreadCore1,
                    sizeof(waThreadCore1),
                    NORMALPRIO,
                    ThreadCore1,
                    NULL);

  /* Mantém a thread principal do núcleo 1 ativa. */
  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
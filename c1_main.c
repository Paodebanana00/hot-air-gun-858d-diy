#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "usbcfg.h"
#include "module/switchs/switchs.h"

// Definição direta do registrador SIO_CPUID do hardware da Raspberry Pi Pico
#define SIO_CPUID (*((volatile uint32_t *)(0xd0000000 + 0x000)))

/* Thread executada no núcleo 1 para Diagnóstico e Relatório de Tela */
static THD_WORKING_AREA(waThreadCore1, 256);

static THD_FUNCTION(ThreadCore1, arg) {
  (void)arg;
  chRegSetThreadName("core1_diagnostic");

  while (true) {
    /* Se a USB estiver ativa, o Core 1 faz o print de tudo sem conflito */
    if ((SDU1.config != NULL) && (serusbcfg.usbp->state == USB_ACTIVE)) {
        int id_core_atual = (int)SIO_CPUID; 
        
        // 1. Print de Alerta de Hardware do Núcleo 1
        chprintf((BaseSequentialStream *)&SDU1, 
                 ">>> [ALERTA DE HARDWARE] Ola do Nucleo %d! Rodando em paralelo real.\r\n", 
                 id_core_atual);
                 
        // 2. JUNÇÃO PERFEITA: Print dos botões na mesma thread de tela
        chprintf((BaseSequentialStream *)&SDU1, 
                 "[NUCLEO 1] Botoes -> Estado 1: %d | Estado 2: %d\r\n\r\n", 
                 estado_sistema_1, 
                 estado_sistema_2);
    }

    // Delay de 500ms para você conseguir ler o terminal confortavelmente
    chThdSleepMilliseconds(500);
  }
}


/* Thread de leitura dos botões (Background) */
static THD_WORKING_AREA(waThreadBotoes, 128);

static THD_FUNCTION(ThreadBotoes, arg) {
    (void)arg;
    chRegSetThreadName("leitura_botoes");
    
    // Inicializa os pinos (Pull-up e Pull-down) assim que a Thread nasce
    botoes_init();

    while (true) {
        // Executa a leitura física e o debounce dos pinos no RP2040
        processar_botoes();
        
        // Dorme por 30ms (Tempo ideal de debounce para não perder cliques)
        // O chprintf pesado foi removido daqui para não travar a USB!
        chThdSleepMilliseconds(30); 
    }
}


/*
 * Ponto de entrada obrigatório do hardware do RP2040 para o núcleo 1.
 */
void c1_main(void) {

  /* Aguarda a inicialização do sistema no núcleo 0. */
  chSysWaitSystemState(ch_sys_running);

  /* Inicializa a instância do kernel neste núcleo. */
  chInstanceObjectInit(&ch1, &ch_core1_cfg);

  /* Libera o núcleo após a inicialização. */
  chSysUnlock();

  /* ----------------------------------------------------------------------- */
  /* START DAS THREADS NO NÚCLEO 1                                           */
  /* ----------------------------------------------------------------------- */

  /* 1. Cria a thread de diagnóstico e exibição de tela */
  chThdCreateStatic(waThreadCore1,
                    sizeof(waThreadCore1),
                    NORMALPRIO,
                    ThreadCore1,
                    NULL);

  /* 2. CORREÇÃO: Dá o start na Thread dos botões! Agora ela vai rodar */
  chThdCreateStatic(waThreadBotoes,
                    sizeof(waThreadBotoes),
                    NORMALPRIO + 1, // Prioridade levemente maior para ler os botões rápido
                    ThreadBotoes,
                    NULL);

  /* Mantém a thread principal do núcleo 1 activa. */
  while (true) {
    chThdSleepMilliseconds(1000);
  }
}

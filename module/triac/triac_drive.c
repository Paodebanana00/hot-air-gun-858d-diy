#include "ch.h"
#include "hal.h"
#include "module/triac/triac_drive.h"

#define PIN_TRIAC           PAL_LINE(IOPORT1, 0U) 
#define JANELA_TOTAL_MS     1000         // Janela macro de exatamente 1 segundo (1000ms)

static volatile int tempo_ligado_ms = 0; // Tempo em ms que o pino deve ficar em HIGH

/*
 * Thread de Tempo Real do Driver do TRIAC
 * Aplica a divisão direta da janela de 1 segundo
 */
static THD_WORKING_AREA(waTriacDriverThread, 256);
static THD_FUNCTION(TriacDriverThread, arg) {
  (void)arg;
  chRegSetThreadName("driver_moc3041_1s");

  while (true) {
    // Leitura segura do tempo ativo calculado para esta janela
    chSysLock();
    int t_on = tempo_ligado_ms;
    chSysUnlock();
    int t_off = JANELA_TOTAL_MS - t_on;

    // Caso especial: 0% de potência
    if (t_on <= 0) {
      palWriteLine(PIN_TRIAC, PAL_LOW);
      chThdSleepMilliseconds(JANELA_TOTAL_MS);
      continue;
    }

    // Caso especial: 100% de potência
    if (t_on >= JANELA_TOTAL_MS) {
      palWriteLine(PIN_TRIAC, PAL_HIGH);
      chThdSleepMilliseconds(JANELA_TOTAL_MS);
      continue;
    }

    // Parte ativa da janela (Liga e espera)
    palWriteLine(PIN_TRIAC, PAL_HIGH); // MOC3041 se vira para achar o ZC e conduzir
    chThdSleepMilliseconds(t_on);

    // Parte inativa da janela (Desliga e espera o resto do 1 segundo)
    palWriteLine(PIN_TRIAC, PAL_LOW);  // TRIAC cortará no próximo ZC
    chThdSleepMilliseconds(t_off);
  }
}

void triac_driver_init(void) {
  palSetLineMode(PIN_TRIAC, PAL_MODE_OUTPUT_PUSHPULL);
  palWriteLine(PIN_TRIAC, PAL_LOW);

  // Inicia a thread com prioridade comum, já que os tempos são na escala de milissegundos
  chThdCreateStatic(waTriacDriverThread, sizeof(waTriacDriverThread), 
                    NORMALPRIO, TriacDriverThread, NULL);
}

void triac_set_power(int power) {
  // Limita a entrada entre 0% e 100%
  if (power > 100) power = 100;
  if (power < 0) power = 0;

  // Regra de três direta: cada 1% equivale a 10ms dentro da janela de 1000ms
  chSysLock();
  tempo_ligado_ms = power * 10; 
  chSysUnlock();
}
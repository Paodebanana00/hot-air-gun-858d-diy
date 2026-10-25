#include "switchs.h"


// Variáveis globais em bool (true/false) para controle do seu sistema
volatile bool estado_sistema_1 = false;
volatile bool estado_sistema_2 = false;

// Histórico para o Debounce
static uint8_t ultimo_estado_btn1 = PAL_HIGH;
static uint8_t ultimo_estado_btn2 = PAL_LOW;

void botoes_init(void) {
    // Botão 1 precisa de Pull-up para ficar em 1 (HIGH) enquanto estiver solto do GND
    palSetPadMode(PORTA_RP2040, GP_GND_TRUE, PAL_MODE_INPUT_PULLUP);
    
    // Botão 2 usa Pull-down para manter a estabilidade em 0 (LOW) quando for solto do GND
    palSetPadMode(PORTA_RP2040, GP_SOL_TRUE, PAL_MODE_INPUT_PULLDOWN);
}

void processar_botoes(void) {

    uint8_t atual_btn1 = palReadPad(PORTA_RP2040, GP_GND_TRUE);
    uint8_t atual_btn2 = palReadPad(PORTA_RP2040, GP_SOL_TRUE);

    if (atual_btn1 == PAL_LOW) {
        estado_sistema_1 = true;
    } else {
        estado_sistema_1 = false;
    }

    if (atual_btn2 == PAL_HIGH) {
        estado_sistema_2 = true;
    } else {
        estado_sistema_2 = false;
    }
}

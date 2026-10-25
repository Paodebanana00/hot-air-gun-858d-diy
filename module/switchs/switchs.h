#ifndef BOTOES_H
#define BOTOES_H

#include "hal.h"
#include <stdbool.h>

/* ========================================================================== */
/* CONFIGURAÇÕES DE HARDWARE (RP2040)                                         */
/* ========================================================================== */
/* Mude os números abaixo para os pinos GP reais que você está usando */
#define PORTA_RP2040     IOPORT1  
#define GP_GND_TRUE      1   // Botão 1: Encostou no GND -> Ativa (Ex: GP2)
#define GP_SOL_TRUE      2   // Botão 2: Soltou do GND -> Ativa (Ex: GP3)

/* ========================================================================== */
/* VARIÁVEIS GLOBAIS DE ESTADO                                               */
/* ========================================================================== */
/* O termo 'extern' permite que você use essas variáveis em outros arquivos   */
/* do seu código (como no seu main.c ou no controle do PID).                 */
extern volatile bool estado_sistema_1;
extern volatile bool estado_sistema_2;

/* ========================================================================== */
/* FUNÇÕES PÚBLICAS                                                          */
/* ========================================================================== */

/**
 * @brief Inicializa os pinos dos botões com Pull-up/Pull-down no RP2040.
 */
void botoes_init(void);

/**
 * @brief Processa as regras de debounce e alternância (Toggle) dos botões.
 * @note Deve ser chamada periodicamente (ex: a cada 30ms).
 */
void processar_botoes(void);

#endif /* BOTOES_H */

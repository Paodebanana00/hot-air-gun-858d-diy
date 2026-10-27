#include "ch.h"
#include "hal.h"
#include "string.h"
#include "stdlib.h"
#include "usbcfg.h"
#include <stdint.h>
#include <stdbool.h>
#include "module/bootloader/bootloader.h"

// JSMN
#define JSMN_STATIC
#include "module/json/jsmn.h"

#define JSON_MAX_LEN  256
#define MAX_TOKENS    32

static char rx_buffer[JSON_MAX_LEN];
static uint16_t rx_index = 0;

// Variáveis compartilhadas de controle remoto
volatile bool remote_control_active = false;
volatile int32_t remote_setpoint = 0;
volatile uint32_t remote_fan_speed = 0;

typedef enum {
    CMD_DESCONHECIDO = 0,
    CMD_REBOOT,
    CMD_SETPOINT,
    CMD_FAN_SPEED,
    CMD_HEATER,
    CMD_STOP
} comando_t;


// =====================================================
// COMPARAÇÃO E LEITURA DO JSON
// =====================================================

static int json_equal(const char *json, const jsmntok_t *tok, const char *s)
{
    return tok->type == JSMN_STRING &&
           (int)strlen(s) == tok->end - tok->start &&
           strncmp(json + tok->start, s, tok->end - tok->start) == 0
           ? 0 : -1;
}

static jsmntok_t *json_get_field(const char *json, jsmntok_t *tokens, int count, const char *field)
{
    for (int i = 1; i < count - 1; i++) {
        if (json_equal(json, &tokens[i], field) == 0) {
            return &tokens[i + 1];
        }
    }
    return NULL;
}

static bool json_get_int(const char *json, const jsmntok_t *tok, int *result)
{
    if (tok == NULL || tok->type != JSMN_PRIMITIVE) {
        return false;
    }

    int len = tok->end - tok->start;
    char buf[16];

    if (len <= 0 || len >= (int)sizeof(buf)) {
        return false;
    }

    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';

    char *endptr;
    long value = strtol(buf, &endptr, 10);

    if (*endptr != '\0' || value < -2147483647L - 1 || value > 2147483647L) {
        return false;
    }

    *result = (int)value;
    return true;
}

static comando_t identificar_comando(const char *json, jsmntok_t *tok)
{
    if (json_equal(json, tok, "reboot") == 0)    return CMD_REBOOT;
    if (json_equal(json, tok, "setpoint") == 0)  return CMD_SETPOINT;
    if (json_equal(json, tok, "fan_speed") == 0) return CMD_FAN_SPEED;
    if (json_equal(json, tok, "heater") == 0)    return CMD_HEATER;
    if (json_equal(json, tok, "stop") == 0)      return CMD_STOP;

    return CMD_DESCONHECIDO;
}

// =====================================================
// FUNÇÕES DE CADA COMANDO
// =====================================================

static void cmd_reboot(void)
{
    bootloader_jump();
}

static void cmd_setpoint(int valor)
{
    remote_setpoint = (int32_t)valor;
    remote_control_active = true; // Interface assume o controle
}

static void cmd_fan_speed(int valor)
{
    remote_fan_speed = (uint32_t)valor;
    remote_control_active = true; // Interface assume o controle
}

static void cmd_heater(int valor)
{
    // Pode ser usado para ligar/desligar remotamente mudando o setpoint
    if (valor == 0) {
        remote_setpoint = 0;
    }
}

static void cmd_stop(void)
{
    remote_setpoint = 0;
    remote_fan_speed = 0;
    remote_control_active = true;
}

static void executar_comando(comando_t cmd, int valor, bool tem_valor)
{
    switch (cmd) {
        case CMD_REBOOT:    cmd_reboot(); break;
        case CMD_SETPOINT:  if (tem_valor) cmd_setpoint(valor); break;
        case CMD_FAN_SPEED: if (tem_valor) cmd_fan_speed(valor); break;
        case CMD_HEATER:    if (tem_valor) cmd_heater(valor); break;
        case CMD_STOP:      cmd_stop(); break;
        default: break;
    }
}

static void processar_pacote_json(const char *json_str)
{
    jsmn_parser parser;
    jsmntok_t tokens[MAX_TOKENS];

    jsmn_init(&parser);

    int r = jsmn_parse(&parser, json_str, strlen(json_str), tokens, MAX_TOKENS);
    if (r < 1 || tokens[0].type != JSMN_OBJECT) return;

    jsmntok_t *cmd_tok = json_get_field(json_str, tokens, r, "comando");
    if (cmd_tok == NULL || cmd_tok->type != JSMN_STRING) return;

    comando_t cmd = identificar_comando(json_str, cmd_tok);
    int valor = 0;
    bool tem_valor = false;

    if (cmd == CMD_SETPOINT || cmd == CMD_FAN_SPEED || cmd == CMD_HEATER) {
        jsmntok_t *val_tok = json_get_field(json_str, tokens, r, "valor");
        if (!json_get_int(json_str, val_tok, &valor)) return;
        tem_valor = true;
    }

    executar_comando(cmd, valor, tem_valor);
}

// =====================================================
// THREAD USB CDC
// =====================================================
static THD_WORKING_AREA(waUsbCdcThread, 512);
static THD_FUNCTION(UsbCdcThread, arg)
{
    (void)arg;
    chRegSetThreadName("usb_cdc_listener");

    while (true) {
        if (serusbcfg.usbp->state == USB_ACTIVE) {
            msg_t b = chnGetTimeout(&SDU1, TIME_IMMEDIATE);

            while (b >= MSG_OK) {
                char c = (char)b;

                if (c == '\n' || c == '\r') {
                    if (rx_index > 0) {
                        rx_buffer[rx_index] = '\0';
                        processar_pacote_json(rx_buffer);
                        rx_index = 0;
                    }
                }
                else if (rx_index < JSON_MAX_LEN - 1) {
                    rx_buffer[rx_index++] = c;
                }
                else {
                    rx_index = 0;
                }
                b = chnGetTimeout(&SDU1, TIME_IMMEDIATE);
            }
            chThdSleepMilliseconds(2);
        }
        else {
            chThdSleepMilliseconds(500);
            rx_index = 0;
        }
    }
}

void usb_cdc_handler_init(void)
{
    chThdCreateStatic(waUsbCdcThread, sizeof(waUsbCdcThread), NORMALPRIO + 1, UsbCdcThread, NULL);
}

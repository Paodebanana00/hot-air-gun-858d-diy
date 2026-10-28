#include "ch.h"
#include "hal.h"
#include "string.h"
#include "stdlib.h"
#include "usbcfg.h"
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "module/bootloader/bootloader.h"

// JSMN
#define JSMN_STATIC
#include "module/json/jsmn.h"

#define JSON_MAX_LEN  256
#define MAX_TOKENS    32
#define SETPOINT_MIN  0
#define SETPOINT_MAX  100
#define FAN_SPEED_MIN 0
#define FAN_SPEED_MAX 4095

static char rx_buffer[JSON_MAX_LEN];
static uint16_t rx_index = 0;
static bool rx_discarding = false;

// Variáveis compartilhadas de controle remoto
volatile bool remote_control_active = false;
volatile int32_t remote_setpoint = 0;
volatile uint32_t remote_fan_speed = 0;
volatile bool remote_stop_requested = false;

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
    if (json == NULL || tok == NULL || s == NULL) {
        return -1;
    }

    return tok->type == JSMN_STRING &&
           (int)strlen(s) == tok->end - tok->start &&
           strncmp(json + tok->start, s, tok->end - tok->start) == 0
           ? 0 : -1;
}

static jsmntok_t *json_get_field(
    const char *json,
    jsmntok_t *tokens,
    int count,
    const char *field)
{
    if (json == NULL || tokens == NULL || field == NULL ||
        count < 1 || tokens[0].type != JSMN_OBJECT) {
        return NULL;
    }

    int i = 1;

    while (i < count && tokens[i].start < tokens[0].end) {
        jsmntok_t *key = &tokens[i];
        int key_end = key->end;

        if (key->start < tokens[0].start || key_end > tokens[0].end) {
            return NULL;
        }

        if (key->type != JSMN_STRING || i + 1 >= count) {
            return NULL;
        }

        jsmntok_t *value = &tokens[i + 1];

        if (json_equal(json, key, field) == 0) {
            return value;
        }

        int next = i + 2;

        while (next < count && tokens[next].start < value->end) {
            next++;
        }

        i = next;
    }

    return NULL;
}

static bool json_get_int(
    const char *json,
    const jsmntok_t *tok,
    int *result)
{
    if (json == NULL || tok == NULL || result == NULL ||
        tok->type != JSMN_PRIMITIVE) {
        return false;
    }

    int len = tok->end - tok->start;
    char buf[16];

    if (len <= 0 || len >= (int)sizeof(buf)) {
        return false;
    }

    memcpy(buf, json + tok->start, (size_t)len);
    buf[len] = '\0';

    char *endptr;
    long value = strtol(buf, &endptr, 10);

    if (endptr == buf || *endptr != '\0' ||
        value < INT_MIN || value > INT_MAX) {
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
    if (valor < SETPOINT_MIN || valor > SETPOINT_MAX) {
        return;
    }

    remote_setpoint = (int32_t)valor;
    remote_control_active = true;
}

static void cmd_fan_speed(int valor)
{
    if (valor < FAN_SPEED_MIN || valor > FAN_SPEED_MAX) {
        return;
    }

    remote_fan_speed = (uint32_t)valor;
    remote_control_active = true;
}

static void cmd_heater(int valor)
{
    if (valor != 0 && valor != 1) {
        return;
    }

    if (valor == 0) {
        remote_setpoint = 0;
        remote_control_active = true;
    }
}

static void cmd_stop(void)
{
    remote_setpoint = 0;
    remote_fan_speed = 4095;
    remote_control_active = false;
    remote_stop_requested = true;
}

static void executar_comando(comando_t cmd, int valor, bool tem_valor)
{
    switch (cmd) {
        case CMD_REBOOT:
            cmd_reboot();
            break;

        case CMD_SETPOINT:
            if (tem_valor) {
                cmd_setpoint(valor);
            }
            break;

        case CMD_FAN_SPEED:
            if (tem_valor) {
                cmd_fan_speed(valor);
            }
            break;

        case CMD_HEATER:
            if (tem_valor) {
                cmd_heater(valor);
            }
            break;

        case CMD_STOP:
            cmd_stop();
            break;

        default:
            break;
    }
}

static void processar_pacote_json(const char *json_str)
{
    if (json_str == NULL || json_str[0] == '\0') {
        return;
    }

    jsmn_parser parser;
    jsmntok_t tokens[MAX_TOKENS];

    jsmn_init(&parser);

    int r = jsmn_parse(
        &parser,
        json_str,
        strlen(json_str),
        tokens,
        MAX_TOKENS
    );

    if (r < 1 || tokens[0].type != JSMN_OBJECT) {
        return;
    }

    jsmntok_t *cmd_tok =
        json_get_field(json_str, tokens, r, "comando");

    if (cmd_tok == NULL || cmd_tok->type != JSMN_STRING) {
        return;
    }

    comando_t cmd = identificar_comando(json_str, cmd_tok);

    int valor = 0;
    bool tem_valor = false;

    if (cmd == CMD_SETPOINT ||
        cmd == CMD_FAN_SPEED ||
        cmd == CMD_HEATER) {

        jsmntok_t *val_tok =
            json_get_field(json_str, tokens, r, "valor");

        if (!json_get_int(json_str, val_tok, &valor)) {
            return;
        }

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
                    if (!rx_discarding && rx_index > 0) {
                        rx_buffer[rx_index] = '\0';
                        processar_pacote_json(rx_buffer);
                    }

                    rx_index = 0;
                    rx_discarding = false;
                }
                else if (!rx_discarding) {
                    if (rx_index < JSON_MAX_LEN - 1) {
                        rx_buffer[rx_index++] = c;
                    } else {
                        rx_index = 0;
                        rx_discarding = true;
                    }
                }

                b = chnGetTimeout(&SDU1, TIME_IMMEDIATE);
            }

            chThdSleepMilliseconds(2);
        }
        else {
            chThdSleepMilliseconds(500);
            rx_index = 0;
            rx_discarding = false;
        }
    }
}

void usb_cdc_handler_init(void)
{
    chThdCreateStatic(
        waUsbCdcThread,
        sizeof(waUsbCdcThread),
        NORMALPRIO + 1,
        UsbCdcThread,
        NULL
    );
}
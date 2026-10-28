
#include "ch.h"
#include "hal.h"
#include "string.h"
#include "stdlib.h"
#include "usbcfg.h"

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "module/bootloader/bootloader.h"

#define JSMN_STATIC
#include "module/json/jsmn.h"

/* =====================================================
 * CONFIGURAÇÃO
 * ===================================================== */

#define JSON_MAX_LEN       256U
#define MAX_TOKENS         32
#define SETPOINT_MIN       0
#define SETPOINT_MAX       100
#define FAN_SPEED_MIN      0
#define FAN_SPEED_MAX      4095

#define USB_RX_TIMEOUT_MS  10U
#define USB_BATCH_MAX      32U

/* =====================================================
 * BUFFER DE RECEPÇÃO
 *
 * Utilizado exclusivamente pela thread USB.
 * ===================================================== */

static char rx_buffer[JSON_MAX_LEN];
static size_t rx_index = 0U;
static bool rx_discarding = false;

/* =====================================================
 * VARIÁVEIS DE CONTROLE REMOTO
 *
 * Mantidos os nomes e tipos da API existente.
 * ===================================================== */

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

/* =====================================================
 * UTILITÁRIOS JSON
 * ===================================================== */

static int json_equal(
    const char *json,
    const jsmntok_t *tok,
    const char *s)
{
    if (json == NULL || tok == NULL || s == NULL) {
        return -1;
    }

    if (tok->type != JSMN_STRING ||
        tok->start < 0 ||
        tok->end < tok->start) {
        return -1;
    }

    size_t token_len =
        (size_t)(tok->end - tok->start);

    size_t string_len = strlen(s);

    if (token_len != string_len) {
        return -1;
    }

    return memcmp(
        json + tok->start,
        s,
        token_len
    ) == 0 ? 0 : -1;
}

/*
 * Procura uma chave no objeto JSON raiz.
 *
 * Avança sobre todos os tokens descendentes de cada
 * valor para não confundir campos de objetos aninhados.
 */
static jsmntok_t *json_get_field(
    const char *json,
    jsmntok_t *tokens,
    int count,
    const char *field)
{
    if (json == NULL ||
        tokens == NULL ||
        field == NULL ||
        count < 1 ||
        tokens[0].type != JSMN_OBJECT) {
        return NULL;
    }

    int i = 1;

    while (i < count &&
           tokens[i].start < tokens[0].end) {

        jsmntok_t *key = &tokens[i];

        if (key->type != JSMN_STRING ||
            i + 1 >= count) {
            return NULL;
        }

        jsmntok_t *value = &tokens[i + 1];

        if (json_equal(json, key, field) == 0) {
            return value;
        }

        /*
         * Ignora o valor inteiro, inclusive os seus
         * descendentes se for objeto ou array.
         */
        int next = i + 2;

        while (next < count &&
               tokens[next].start < value->end) {
            next++;
        }

        i = next;
    }

    return NULL;
}

/*
 * Lê exclusivamente números inteiros decimais.
 *
 * Exemplos aceitos: 0, 1, 100, 4095, -1.
 * Exemplos rejeitados: 1.5, true, null, "".
 */
static bool json_get_int(
    const char *json,
    const jsmntok_t *tok,
    int *result)
{
    if (json == NULL ||
        tok == NULL ||
        result == NULL ||
        tok->type != JSMN_PRIMITIVE ||
        tok->start < 0 ||
        tok->end <= tok->start) {
        return false;
    }

    int len = tok->end - tok->start;

    char buf[16];

    if (len <= 0 ||
        len >= (int)sizeof(buf)) {
        return false;
    }

    memcpy(
        buf,
        json + tok->start,
        (size_t)len
    );

    buf[len] = '\0';

    char *endptr = NULL;
    long value = strtol(buf, &endptr, 10);

    if (endptr == buf ||
        *endptr != '\0' ||
        value < INT_MIN ||
        value > INT_MAX) {
        return false;
    }

    *result = (int)value;

    return true;
}

static comando_t identificar_comando(
    const char *json,
    jsmntok_t *tok)
{
    if (json_equal(json, tok, "reboot") == 0) {
        return CMD_REBOOT;
    }

    if (json_equal(json, tok, "setpoint") == 0) {
        return CMD_SETPOINT;
    }

    if (json_equal(json, tok, "fan_speed") == 0) {
        return CMD_FAN_SPEED;
    }

    if (json_equal(json, tok, "heater") == 0) {
        return CMD_HEATER;
    }

    if (json_equal(json, tok, "stop") == 0) {
        return CMD_STOP;
    }

    return CMD_DESCONHECIDO;
}

/* =====================================================
 * ATUALIZAÇÃO DAS VARIÁVEIS REMOTAS
 * ===================================================== */

/*
 * IMPORTANTE:
 * chSysLock protege contra outras threads que respeitem
 * o mesmo mecanismo de sincronização no mesmo núcleo.
 *
 * Não constitui, por si só, um mutex entre dois núcleos.
 * Se o Core 1 ler diretamente estas variáveis, será
 * necessário sincronizar a transferência entre os cores.
 */

static void cmd_setpoint(int valor)
{
    if (valor < SETPOINT_MIN ||
        valor > SETPOINT_MAX) {
        return;
    }

    chSysLock();

    remote_setpoint = (int32_t)valor;
    remote_control_active = true;

    chSysUnlock();
}

static void cmd_fan_speed(int valor)
{
    if (valor < FAN_SPEED_MIN ||
        valor > FAN_SPEED_MAX) {
        return;
    }

    chSysLock();

    remote_fan_speed = (uint32_t)valor;
    remote_control_active = true;

    chSysUnlock();
}

static void cmd_heater(int valor)
{
    if (valor != 0 && valor != 1) {
        return;
    }

    chSysLock();

    if (valor == 0) {
        remote_setpoint = 0;
        remote_control_active = true;
    }

    /*
     * heater=1 é aceito, mas não altera o setpoint.
     * Isso preserva a semântica do código original.
     */

    chSysUnlock();
}

static void cmd_stop(void)
{
    chSysLock();

    remote_setpoint = 0;
    remote_fan_speed = FAN_SPEED_MAX;
    remote_control_active = false;
    remote_stop_requested = true;

    chSysUnlock();
}

static void cmd_reboot(void)
{
    bootloader_jump();

    /*
     * Caso o bootloader retorne, não continua a executar
     * o processamento como se o reboot tivesse ocorrido.
     */
    chSysHalt("bootloader_jump returned");
}

static void executar_comando(
    comando_t cmd,
    int valor,
    bool tem_valor)
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

        case CMD_DESCONHECIDO:
        default:
            break;
    }
}

/* =====================================================
 * PROCESSAMENTO DO PACOTE JSON
 * ===================================================== */

static void processar_pacote_json(
    const char *json_str)
{
    if (json_str == NULL ||
        json_str[0] == '\0') {
        return;
    }

    size_t len = strlen(json_str);

    if (len == 0U ||
        len >= JSON_MAX_LEN) {
        return;
    }

    jsmn_parser parser;
    jsmntok_t tokens[MAX_TOKENS];

    jsmn_init(&parser);

    int r = jsmn_parse(
        &parser,
        json_str,
        len,
        tokens,
        MAX_TOKENS
    );

    /*
     * JSON malformado, vazio ou com tokens insuficientes:
     * não executa nenhum comando.
     */
    if (r < 1 ||
        tokens[0].type != JSMN_OBJECT) {
        return;
    }

    jsmntok_t *cmd_tok = json_get_field(
        json_str,
        tokens,
        r,
        "comando"
    );

    if (cmd_tok == NULL ||
        cmd_tok->type != JSMN_STRING) {
        return;
    }

    comando_t cmd =
        identificar_comando(json_str, cmd_tok);

    if (cmd == CMD_DESCONHECIDO) {
        return;
    }

    int valor = 0;
    bool tem_valor = false;

    if (cmd == CMD_SETPOINT ||
        cmd == CMD_FAN_SPEED ||
        cmd == CMD_HEATER) {

        jsmntok_t *val_tok = json_get_field(
            json_str,
            tokens,
            r,
            "valor"
        );

        if (!json_get_int(
                json_str,
                val_tok,
                &valor)) {
            return;
        }

        tem_valor = true;
    }

    executar_comando(cmd, valor, tem_valor);
}

/* =====================================================
 * RECEPÇÃO USB CDC
 * ===================================================== */

static void resetar_receptor(void)
{
    rx_index = 0U;
    rx_discarding = false;
    rx_buffer[0] = '\0';
}

static void processar_byte(char c)
{
    if (c == '\n' || c == '\r') {

        /*
         * CR e LF são terminadores independentes.
         * O segundo terminador de CRLF simplesmente
         * encontra um buffer vazio.
         */
        if (!rx_discarding && rx_index > 0U) {
            rx_buffer[rx_index] = '\0';

            processar_pacote_json(rx_buffer);
        }

        resetar_receptor();

        return;
    }

    if (rx_discarding) {
        /*
         * Descarta o restante do pacote até o próximo
         * terminador. Não tenta executar JSON truncado.
         */
        return;
    }

    if (rx_index < JSON_MAX_LEN - 1U) {
        rx_buffer[rx_index++] = c;
        return;
    }

    /*
     * Pacote excedeu o tamanho permitido:
     * descarta até o próximo CR/LF.
     */
    resetar_receptor();
    rx_discarding = true;
}

static THD_WORKING_AREA(waUsbCdcThread, 768);

static THD_FUNCTION(UsbCdcThread, arg)
{
    (void)arg;

    chRegSetThreadName("usb_cdc_listener");

    while (true) {

        if (serusbcfg.usbp == NULL ||
            serusbcfg.usbp->state != USB_ACTIVE ||
            SDU1.config == NULL) {

            resetar_receptor();

            chThdSleepMilliseconds(100);

            continue;
        }

        /*
         * Aguarda o primeiro byte por um tempo limitado.
         * A thread principal e as outras threads continuam
         * independentes deste timeout.
         */
        msg_t b = chnGetTimeout(
            &SDU1,
            TIME_MS2I(USB_RX_TIMEOUT_MS)
        );

        if (b < MSG_OK) {
            continue;
        }

        unsigned int lote = 0U;

        do {
            processar_byte((char)(uint8_t)b);

            lote++;

            /*
             * Cede a CPU em rajadas grandes para evitar
             * monopolizar o agendador.
             */
            if (lote >= USB_BATCH_MAX) {
                lote = 0U;
                chThdYield();
            }

            b = chnGetTimeout(
                &SDU1,
                TIME_IMMEDIATE
            );

        } while (b >= MSG_OK);
    }
}

/* =====================================================
 * INICIALIZAÇÃO PÚBLICA
 * ===================================================== */

void usb_cdc_handler_init(void)
{
    chThdCreateStatic(
        waUsbCdcThread,
        sizeof(waUsbCdcThread),
        NORMALPRIO,
        UsbCdcThread,
        NULL
    );
}

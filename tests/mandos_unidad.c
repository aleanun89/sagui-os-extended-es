// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos_unidad.c - pruebas en el PC del analizador HID y de las cuentas del puerto de
 * juegos (D307). Lo compila tests/h307_mandos.py con kernel/lib/hid_informe.c y
 * kernel/lib/portajogos_conv.c (-DSAGUI_HOST).
 *
 * Descriptores (volcados públicos de los dispositivos, tal como los da GET_DESCRIPTOR 0x22):
 *   - Logitech Dual Action (046d:c216), que es también el F310 en modo DirectInput (interruptor
 *     en «D»): 4 ejes de 8 bits, un hat de 4 bits con estado nulo, 12 botones y 16 bits del
 *     fabricante; informe de 8 bytes sin identificador;
 *   - joystick USB genérico «DragonRise» (0079:0006): 5 ejes de 8 bits con X repetido cuatro
 *     veces, un hat y 12 botones;
 *   - un mando con identificador de informe, ejes de 16 bits con signo y botones por rango
 *     (escrito según el ejemplo de la especificación HID 1.11, apéndice E);
 *   - un ratón (no es un mando) y descriptores rotos (no deben colgar ni salirse). */
#include <stdio.h>
#include <string.h>
#include "hid_informe.h"
#include "portajogos_conv.h"

static int fallos;
#define VER(c, ...) do { if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const uint8_t dual_action[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x04, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x35, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
    0x75, 0x04, 0x95, 0x01, 0x25, 0x07, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65,
    0x00, 0x75, 0x01, 0x95, 0x0C, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0C, 0x81,
    0x02, 0x06, 0x00, 0xFF, 0x75, 0x01, 0x95, 0x10, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01, 0x81, 0x02,
    0xC0, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xFF, 0x00, 0x26, 0xFF, 0x00, 0x09, 0x02, 0x91,
    0x02, 0xC0, 0xC0,
};

static const uint8_t dragonrise[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x05, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x35, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x30, 0x09, 0x30, 0x09, 0x30, 0x09, 0x31,
    0x81, 0x02, 0x75, 0x04, 0x95, 0x01, 0x25, 0x07, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81,
    0x42, 0x65, 0x00, 0x75, 0x01, 0x95, 0x0C, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29,
    0x0C, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x75, 0x01, 0x95, 0x08, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01,
    0x81, 0x02, 0xC0, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xFF, 0x00, 0x26, 0xFF, 0x00, 0x09,
    0x02, 0x91, 0x02, 0xC0, 0xC0,
};

/* Game Pad con Report ID 3: X/Y de 16 bits con signo, Rx/Ry, slider, 2 hats de 4 direcciones
 * (rango 0-3) y 16 botones; Push/Pop por medio para comprobar la pila. */
static const uint8_t com_id[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0x85, 0x03,                         /* Report ID 3 */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02,
    0x05, 0x01, 0xA4,                   /* Push */
    0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x04,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34, 0x81, 0x02,
    0xB4,                               /* Pop: vuelve a 1 bit, 0..1, 16 de cuenta */
    0x15, 0x00, 0x25, 0x03, 0x75, 0x04, 0x95, 0x02, 0x09, 0x39, 0x09, 0x39, 0x81, 0x42,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01, 0x09, 0x36, 0x81, 0x02,
    0xC0,
    /* otra aplicación con otro id (teclas multimedia): no es del mando */
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x04, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x09, 0xE9, 0x81, 0x02, 0xC0,
};

static const uint8_t rato[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0xC0, 0xC0,
};

static void dual_action_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    int n = hid_analisar(dual_action, sizeof dual_action, &d);
    VER(n > 0, "Dual Action: el descriptor se analiza (%d)", n);
    VER(hid_mando_extrair(&d, &m), "Dual Action: es un mando");
    VER(m.aplicacao == HID_USO_JOYSTICK, "Dual Action: aplicación joystick");
    VER(m.n_eixos == 4 && m.n_hats == 1 && m.n_botoes == 12, "Dual Action: 4 ejes, 1 hat, 12 botones (%d %d %d)",
        m.n_eixos, m.n_hats, m.n_botoes);
    VER(!m.usa_ids, "Dual Action: sin identificador de informe");
    VER(m.eixo[0].uso == HID_USO_X && m.eixo[3].uso == HID_USO_RZ && m.eixo[3].bit == 24, "Dual Action: orden y posición de los ejes");
    VER(m.hat[0].bit == 32 && m.hat[0].tam == 4 && m.hat[0].lmax == 7, "Dual Action: hat en el bit 32");
    VER(m.botao_bit[0] == 36 && m.botao_bit[11] == 47, "Dual Action: botones del bit 36 al 47");
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    /* stick izquierdo a la izquierda y abajo, derecho centrado; hat suelto (8); botones 2 y 10 */
    const uint8_t r1[8] = { 0x00, 0xFF, 0x80, 0x80, 0x08 | (1 << 5), 0x20, 0x00, 0x00 };
    VER(hid_mando_decodificar(&m, r1, 8, &e), "Dual Action: informe aceptado");
    VER(e.eixo[0] == -32768 && e.eixo[1] == 32767, "Dual Action: X=-32768 Y=32767 (%d %d)", e.eixo[0], e.eixo[1]);
    VER(e.eixo[2] >= 0 && e.eixo[2] < 200, "Dual Action: Z cerca del centro (%d)", e.eixo[2]);
    VER(e.hat[0] == HAT_CENTRO, "Dual Action: hat suelto con el estado nulo");
    VER(e.botoes == ((1u << 1) | (1u << 9)), "Dual Action: botones 2 y 10 (%08x)", e.botoes);
    const uint8_t r2[8] = { 0x80, 0x80, 0x80, 0x80, 0x03 | (0xF << 4), 0xFF, 0xFF, 0xFF };
    hid_mando_decodificar(&m, r2, 8, &e);
    VER(e.hat[0] == (HAT_BAIXO | HAT_DIREITA), "Dual Action: hat 3 = abajo a la derecha (%d)", e.hat[0]);
    VER(e.botoes == 0xFFF, "Dual Action: los 12 botones, sin los bits del fabricante (%08x)", e.botoes);
}

/* Joystick de 3 ejes de 8 bits (X, Y y el acelerador como Z) y 11 botones con 5 bits de
 * relleno, informe de 5 bytes: la forma del Logitech Attack 3 (046d:c214) según los volcados
 * públicos (la D250, 8.5.4.10r2). En reposo, X e Y tienen que salir cerca de 0. */
static const uint8_t joy3[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02,
    0x75, 0x08, 0x95, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x35, 0x00, 0x46, 0xFF, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x81, 0x02,
    0x95, 0x01, 0x09, 0x32, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x0B, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0B, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0xC0, 0xC0,
};

/* Mando genérico con el máximo lógico 255 en UN byte (0x25 0xFF, que con signo sería -1) y,
 * después de un eje con signo (-127..127), un X/Y que da el máximo ANTES que el mínimo: el
 * signo del máximo no puede decidirse con el mínimo del campo anterior. */
static const uint8_t max_1byte[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x09, 0x35, 0x81, 0x02,   /* Rz con signo */
    0x25, 0xFF, 0x15, 0x00, 0x09, 0x30, 0x09, 0x31, 0x95, 0x02, 0x81, 0x02,   /* X, Y 0..255 */
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x81, 0x02,
    0xC0,
};

/* Joystick de 10 bits (0..1023 en dos bytes, 0x26 0xFF 0x03), como el Extreme 3D Pro: X e Y
 * de 10 bits seguidos, un hat de 4 bits, 8 botones y el giro de 8 bits. */
static const uint8_t diez_bits[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02,
    0x75, 0x0A, 0x95, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x09, 0x30, 0x09, 0x31, 0x81, 0x02,
    0x75, 0x04, 0x95, 0x01, 0x25, 0x07, 0x09, 0x39, 0x81, 0x42,
    0x75, 0x08, 0x95, 0x01, 0x26, 0xFF, 0x00, 0x09, 0x35, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x08, 0x25, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x81, 0x02,
    0xC0, 0xC0,
};

static void joy3_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(joy3, sizeof joy3, &d) > 0 && hid_mando_extrair(&d, &m), "joystick 3 ejes: es un mando");
    VER(m.n_eixos == 3 && m.n_botoes == 11 && m.n_hats == 0, "joystick 3 ejes: 3 ejes, 11 botones (%d %d)", m.n_eixos, m.n_botoes);
    VER(m.eixo[0].lmin == 0 && m.eixo[0].lmax == 255 && !m.eixo[0].sinal && m.eixo[2].bit == 16,
        "joystick 3 ejes: 0..255 sin signo, Z en el bit 16");
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    const uint8_t reposo[5] = { 0x80, 0x7F, 0x00, 0x00, 0x00 };
    VER(hid_mando_decodificar(&m, reposo, 5, &e), "joystick 3 ejes: informe aceptado");
    VER(e.eixo[0] > -300 && e.eixo[0] < 300 && e.eixo[1] > -300 && e.eixo[1] < 300,
        "joystick 3 ejes: en reposo X e Y cerca de 0 (%d %d)", e.eixo[0], e.eixo[1]);
    VER(e.eixo[2] == -32768, "joystick 3 ejes: acelerador a 0 = -32768 (%d)", e.eixo[2]);
    const uint8_t esquina[5] = { 0xFF, 0x00, 0xFF, 0x01, 0x04 };
    hid_mando_decodificar(&m, esquina, 5, &e);
    VER(e.eixo[0] == 32767 && e.eixo[1] == -32768 && e.eixo[2] == 32767, "joystick 3 ejes: esquina (%d %d %d)",
        e.eixo[0], e.eixo[1], e.eixo[2]);
    VER(e.botoes == ((1u << 0) | (1u << 10)), "joystick 3 ejes: botones 1 y 11 (%08x)", e.botoes);
}

static void max_1byte_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(max_1byte, sizeof max_1byte, &d) > 0 && hid_mando_extrair(&d, &m), "máximo de un byte: es un mando");
    VER(m.n_eixos == 3, "máximo de un byte: 3 ejes (%d)", m.n_eixos);
    VER(m.eixo[0].uso == HID_USO_X && m.eixo[0].lmin == 0 && m.eixo[0].lmax == 255 && !m.eixo[0].sinal,
        "máximo de un byte: X 0..255 aunque el máximo venga antes que el mínimo (%d..%d)", m.eixo[0].lmin, m.eixo[0].lmax);
    VER(m.eixo[2].uso == HID_USO_RZ && m.eixo[2].lmin == -127 && m.eixo[2].lmax == 127 && m.eixo[2].sinal,
        "máximo de un byte: Rz -127..127 con signo");
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    const uint8_t r[4] = { 0x00, 0x80, 0x80, 0x00 };   /* Rz 0 (con signo: 0), X 128, Y 128 */
    hid_mando_decodificar(&m, r, 4, &e);
    VER(e.eixo[0] > -300 && e.eixo[0] < 300 && e.eixo[1] > -300 && e.eixo[1] < 300 && e.eixo[2] > -300 && e.eixo[2] < 300,
        "máximo de un byte: en reposo todo cerca de 0 (%d %d %d)", e.eixo[0], e.eixo[1], e.eixo[2]);
}

static void diez_bits_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(diez_bits, sizeof diez_bits, &d) > 0 && hid_mando_extrair(&d, &m), "10 bits: es un mando");
    VER(m.n_eixos == 3 && m.n_hats == 1 && m.n_botoes == 8, "10 bits: 3 ejes, 1 hat, 8 botones");
    VER(m.eixo[0].tam == 10 && m.eixo[0].lmax == 1023 && m.eixo[1].bit == 10, "10 bits: X 0..1023, Y en el bit 10");
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    /* X = 512, Y = 511, hat 8 (suelto), giro 128, botón 3 */
    uint32_t x = 512, y = 511;
    uint8_t r[5];
    r[0] = (uint8_t)x; r[1] = (uint8_t)((x >> 8) | (y << 2)); r[2] = (uint8_t)((y >> 6) | (8u << 4));
    r[3] = 0x80; r[4] = 0x04;
    hid_mando_decodificar(&m, r, 5, &e);
    VER(e.eixo[0] > -100 && e.eixo[0] < 100 && e.eixo[1] > -100 && e.eixo[1] < 100,
        "10 bits: en el centro X e Y cerca de 0 (%d %d)", e.eixo[0], e.eixo[1]);
    VER(e.hat[0] == HAT_CENTRO && e.botoes == 4, "10 bits: hat suelto y botón 3");
}

static void dragonrise_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(dragonrise, sizeof dragonrise, &d) > 0, "DragonRise: se analiza");
    VER(hid_mando_extrair(&d, &m), "DragonRise: es un mando");
    VER(m.n_eixos == 5 && m.n_hats == 1 && m.n_botoes == 12, "DragonRise: 5 ejes (X repetido), 1 hat, 12 botones (%d %d %d)",
        m.n_eixos, m.n_hats, m.n_botoes);
    VER(m.eixo[4].uso == HID_USO_Y && m.eixo[4].bit == 32, "DragonRise: Y el último, en el byte 4");
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    const uint8_t r[8] = { 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x06, 0x01, 0x00 };
    hid_mando_decodificar(&m, r, 8, &e);
    VER(e.eixo[4] == -32768 && e.eixo[3] == -32768, "DragonRise: Y arriba (%d)", e.eixo[4]);
    VER(e.hat[0] == HAT_ESQUERDA, "DragonRise: hat 6 = izquierda");
    VER(e.botoes == (1u << 4), "DragonRise: botón 5 (%08x)", e.botoes);
}

static void com_id_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(com_id, sizeof com_id, &d) > 0, "con id: se analiza");
    VER(hid_mando_extrair(&d, &m), "con id: es un mando");
    VER(m.aplicacao == HID_USO_GAMEPAD && m.usa_ids, "con id: Game Pad con identificador");
    VER(m.n_eixos == 5 && m.n_hats == 2 && m.n_botoes == 16, "con id: 5 ejes, 2 hats, 16 botones (%d %d %d)",
        m.n_eixos, m.n_hats, m.n_botoes);
    VER(m.eixo[0].sinal && m.eixo[0].tam == 16 && m.eixo[0].bit == 16, "con id: X de 16 bits con signo en el bit 16");
    VER(m.hat[0].tam == 4 && m.hat[0].lmax == 3, "con id: el Pop devuelve el tamaño anterior y el rango del hat");
    VER(m.eixo[4].uso == HID_USO_SLIDER && m.eixo[4].bit == 88, "con id: el deslizador detrás de los hats (bit %d)", m.eixo[4].bit);
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    uint8_t r[13] = { 3, 0x01, 0x80, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x21, 0xFF };
    VER(hid_mando_decodificar(&m, r, 13, &e), "con id: el informe 3 se acepta");
    VER(e.botoes == 0x8001, "con id: botones 1 y 16 (%08x)", e.botoes);
    VER(e.eixo[0] == 32767 && e.eixo[1] == -32768, "con id: X y Y en los extremos (%d %d)", e.eixo[0], e.eixo[1]);
    VER(e.eixo[2] == -1 || e.eixo[2] == 0, "con id: Rx centrado (%d)", e.eixo[2]);
    VER(e.hat[0] == HAT_DIREITA && e.hat[1] == HAT_BAIXO, "con id: hats derecha y abajo (%d %d)", e.hat[0], e.hat[1]);
    VER(e.eixo[4] == 32767, "con id: deslizador al máximo");
    uint8_t outro[2] = { 4, 0x01 };
    VER(!hid_mando_decodificar(&m, outro, 2, &e), "con id: el informe 4 (multimedia) no es del mando");
    VER(e.botoes == 0x8001, "con id: el informe ajeno no cambia nada");
}

static void rato_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    VER(hid_analisar(rato, sizeof rato, &d) == 5, "ratón: 3 botones y 2 ejes de entrada (%d)", d.n);
    VER(!hid_mando_extrair(&d, &m), "ratón: no es un mando");
}

static void rotos_prueba(void)
{
    static hid_desc_t d;
    hid_mando_t m;
    /* Cortes en todos los puntos: nunca se sale del buffer, nunca da un mando a medias. */
    for (unsigned k = 0; k < sizeof dual_action; k++) {
        int r = hid_analisar(dual_action, (int)k, &d);
        if (r >= 0) hid_mando_extrair(&d, &m);
    }
    const uint8_t mal1[] = { 0xC0 };                          /* fin de colección sin abrir */
    VER(hid_analisar(mal1, 1, &d) < 0, "End Collection sin Collection");
    const uint8_t mal2[] = { 0x05, 0x01, 0x09, 0x04, 0xA1, 0x01 };   /* colección sin cerrar */
    VER(hid_analisar(mal2, sizeof mal2, &d) < 0, "Collection sin cerrar");
    const uint8_t mal3[] = { 0xB4 };                          /* Pop sin Push */
    VER(hid_analisar(mal3, 1, &d) < 0, "Pop sin Push");
    const uint8_t mal4[] = { 0x85, 0x00 };                    /* Report ID 0 */
    VER(hid_analisar(mal4, 2, &d) < 0, "Report ID 0 no vale");
    const uint8_t mal5[] = { 0x75, 0x40, 0x95, 0x01, 0x81, 0x02 };   /* campo de 64 bits */
    VER(hid_analisar(mal5, sizeof mal5, &d) < 0, "campo de más de 32 bits");
    /* 200 campos: no se pasa de HID_CAMPOS_MAX */
    uint8_t muchos[400];
    int k = 0;
    muchos[k++] = 0x05; muchos[k++] = 0x01; muchos[k++] = 0x09; muchos[k++] = 0x04; muchos[k++] = 0xA1; muchos[k++] = 0x01;
    muchos[k++] = 0x75; muchos[k++] = 0x01; muchos[k++] = 0x95; muchos[k++] = 200; muchos[k++] = 0x25; muchos[k++] = 0x01;
    muchos[k++] = 0x05; muchos[k++] = 0x09; muchos[k++] = 0x19; muchos[k++] = 0x01; muchos[k++] = 0x29; muchos[k++] = 200;
    muchos[k++] = 0x81; muchos[k++] = 0x02; muchos[k++] = 0xC0;
    VER(hid_analisar(muchos, k, &d) == HID_CAMPOS_MAX, "200 botones: se queda en HID_CAMPOS_MAX");
    VER(hid_mando_extrair(&d, &m) && m.n_botoes == 32, "200 botones: el mando tiene 32 (%d)", m.n_botoes);
}

static void xbox_prueba(void)
{
    hid_estado_t e;
    memset(&e, 0, sizeof e);
    /* A + Start + cruceta arriba-izquierda; LT a fondo; LX a la derecha; LY arriba; RY -32768 */
    uint8_t r[20] = { 0x00, 0x14, 0x01 | 0x04 | 0x10, 0x10, 0xFF, 0x00, 0xFF, 0x7F, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x80 };
    VER(xbox360_decodificar(r, 20, &e), "Xbox 360: informe de entrada");
    VER(e.hat[0] == (HAT_CIMA | HAT_ESQUERDA), "Xbox 360: cruceta arriba-izquierda (%d)", e.hat[0]);
    VER(e.botoes == ((1u << 0) | (1u << 7)), "Xbox 360: A y Start (%08x)", e.botoes);
    VER(e.eixo[0] == 32767 && e.eixo[1] == -32767, "Xbox 360: LX derecha, LY arriba = negativo (%d %d)", e.eixo[0], e.eixo[1]);
    VER(e.eixo[2] == 32767 && e.eixo[5] == -32768, "Xbox 360: LT a fondo, RT suelto (%d %d)", e.eixo[2], e.eixo[5]);
    VER(e.eixo[4] == 32767, "Xbox 360: RY -32768 invertido sin desbordar (%d)", e.eixo[4]);
    uint8_t luces[3] = { 0x01, 0x03, 0x02 };
    VER(!xbox360_decodificar(luces, 3, &e), "Xbox 360: el mensaje de las luces no es un informe");
}

static void puerto_prueba(void)
{
    /* t = 24,2 us + 0,011 us/ohmio: 0 ohmios -> 24,2 us; 100 kohmios -> 1124,2 us */
    VER(pj_tempo_esperado(0) == 242, "puerto: 0 ohmios = 24,2 us");
    VER(pj_tempo_esperado(100000) == 11242, "puerto: 100 kohmios = 1124,2 us (%u)", pj_tempo_esperado(100000));
    /* TSC: 1124,2 us a 1 GHz son 1124200 ciclos; a 75 MHz, 84315 */
    VER(pj_decimas_us(1124200, 1000000) == 11242, "puerto: ciclos a 1 GHz");
    VER(pj_decimas_us(84315, 75000) == 11242, "puerto: ciclos a 75 MHz (%u)", pj_decimas_us(84315, 75000));
    VER(pj_decimas_us(3000000000u, 3000000) == 10000000, "puerto: sin desbordar a 3 GHz (%u)", pj_decimas_us(3000000000u, 3000000));
    VER(pj_decimas_us(100, 0) == PJ_SEM_EIXO, "puerto: sin TSC medido no hay tiempo");
    pj_cal_t c;
    uint32_t centro = pj_tempo_esperado(50000);
    pj_cal_inicial(centro, &c);
    VER(pj_cal_valida(&c), "puerto: calibración inicial válida");
    VER(pj_normalizar(centro, &c) == 0, "puerto: el centro es 0");
    VER(pj_normalizar(c.min, &c) == -32767 && pj_normalizar(c.max, &c) == 32767, "puerto: extremos");
    int16_t medio = pj_normalizar((centro + c.max) / 2, &c);
    VER(medio > 16000 && medio < 16800, "puerto: a medio camino ~16383 (%d)", medio);
    pj_cal_ampliar(pj_tempo_esperado(100000) + 200, &c);
    VER(c.max == pj_tempo_esperado(100000) + 200, "puerto: la calibración automática amplía el máximo");
    pj_cal_ampliar(PJ_SEM_EIXO, &c);
    VER(c.max != PJ_SEM_EIXO, "puerto: un eje ausente no estropea la calibración");
    pj_cal_t mala = { 100, 100, 200 };
    VER(!pj_cal_valida(&mala) && pj_normalizar(150, &mala) == 0, "puerto: calibración inválida = 0");
    VER(pj_normalizar(PJ_SEM_EIXO, &c) == 0, "puerto: eje ausente = 0");
}

int main(void)
{
    dual_action_prueba();
    dragonrise_prueba();
    joy3_prueba();
    max_1byte_prueba();
    diez_bits_prueba();
    com_id_prueba();
    rato_prueba();
    rotos_prueba();
    xbox_prueba();
    puerto_prueba();
    if (fallos) { printf("%d fallos\n", fallos); return 1; }
    printf("todo bien\n");
    return 0;
}

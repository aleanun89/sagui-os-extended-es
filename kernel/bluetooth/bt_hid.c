// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_hid.c - HIDP (D279): el perfil HID de Bluetooth sobre dos canales L2CAP, control (PSM 0x11) e
 * interrupción (PSM 0x13). Solo especificación pública (HID Profile 1.1 y HID Specification 1.11,
 * apéndice B: informes del protocolo boot). Los informes salen hacia los mismos receptores que usa
 * el HID USB (kernel/usb/hid.c) a través de bt_plat_t. */
#include "bt.h"

#define HIDP_HANDSHAKE  0x0
#define HIDP_CONTROL    0x1
#define HIDP_SET_PROTO  0x7
#define HIDP_DATA       0xA

int bt_hid_protocolo_boot(int canal_ctl)
{
    uint8_t m = (HIDP_SET_PROTO << 4) | 0;   /* 0 = boot */
    return bt_l2cap_enviar(canal_ctl, &m, 1);
}

int bt_hid_leds(int canal_int, uint8_t leds)
{
    uint8_t m[2] = { (HIDP_DATA << 4) | 2, leds };   /* DATA, informe de salida del teclado boot */
    return bt_l2cap_enviar(canal_int, m, 2);
}

int bt_hid_desconectar_virtual(int canal_ctl)
{
    uint8_t m = (HIDP_CONTROL << 4) | 5;   /* VIRTUAL_CABLE_UNPLUG */
    return bt_l2cap_enviar(canal_ctl, &m, 1);
}

void bt_hid_dados_ctl(int canal, const uint8_t *d, int n)
{
    if (n < 1) return;
    uint8_t t = d[0] >> 4, p = d[0] & 15;
    if (t == HIDP_HANDSHAKE) {
        if (p) bt_log("[BT] el dispositivo HID rechazo una orden (handshake %u)", p);
    } else if (t == HIDP_CONTROL && p == 5) {
        bt_g_hid_desenchufado(canal);
    }
}

void bt_hid_dados_int(int canal, const uint8_t *d, int n, uint8_t tipo, bool boot)
{
    (void)canal;
    const bt_plat_t *pl = bt_plat();
    if (n < 2 || (d[0] >> 4) != HIDP_DATA || (d[0] & 15) != 1) return;   /* solo informes de entrada */
    d++;
    n--;
    if (tipo == BT_T_TECLADO || (tipo != BT_T_RATO && n >= 8)) {
        if (n == 9 && d[0] == 1) { d++; n--; }   /* con identificador de informe */
        if (n >= 8 && pl->teclado) pl->teclado(d, 8);
    } else if (pl->raton && n >= 3) {
        pl->raton(d, n, boot);
    }
}

// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_l2cap.c - L2CAP básico (D279): señalización por el canal 1, canales dinámicos en modo básico,
 * reensamblado de las tramas ACL fragmentadas. Solo especificación pública (Core, volumen 3 parte A).
 * No hay retransmisión ni control de flujo (modo básico): lo que usan SDP y HID. */
#include "bt.h"

#define CID_SINAL      0x0001
#define CID_PRIMERO    0x0040
#define MTU_LOCAL      672
#define L2_MAX         (BT_SDP_MAX + 64)
#define TIEMPO_PETICION 10000
#define TIEMPO_CIERRE   2000

#define L2_CMD_RECHAZO 1
#define L2_CONEX_PET   2
#define L2_CONEX_RESP  3
#define L2_CONF_PET    4
#define L2_CONF_RESP   5
#define L2_DESC_PET    6
#define L2_DESC_RESP   7
#define L2_ECO_PET     8
#define L2_ECO_RESP    9
#define L2_INFO_PET    10
#define L2_INFO_RESP   11

static bt_canal_t canal[BT_MAX_CANAIS];
static uint8_t proximo_id;

static struct {
    bool     activo;
    uint16_t manejador, total, tengo;
    uint8_t  d[L2_MAX];
} reens[BT_MAX_LIGACOES + 2];

void bt_l2cap_reiniciar(void)
{
    memset(canal, 0, sizeof canal);
    memset(reens, 0, sizeof reens);
    proximo_id = 0;
}

const bt_canal_t *bt_l2cap_canal(int i) { return i >= 0 && i < BT_MAX_CANAIS && canal[i].usado ? &canal[i] : NULL; }

static uint8_t nuevo_id(void)
{
    if (++proximo_id == 0) proximo_id = 1;
    return proximo_id;
}

static int enviar_l2(uint16_t manejador, uint16_t cid, const uint8_t *d, int n)
{
    uint8_t p[BT_ACL_FILA_TAM];
    if (n + 4 > BT_ACL_FILA_TAM) return -1;
    bt_poe16(p, (uint16_t)n);
    bt_poe16(p + 2, cid);
    memcpy(p + 4, d, (size_t)n);
    return bt_hci_acl_enviar(manejador, p, n + 4);
}

/* Una orden de señalización con su cuerpo. */
static int enviar_sinal(uint16_t manejador, uint8_t code, uint8_t id, const uint8_t *cuerpo, int n)
{
    uint8_t p[BT_ACL_FILA_TAM - 4];
    if (n + 4 > (int)sizeof p) return -1;
    p[0] = code;
    p[1] = id;
    bt_poe16(p + 2, (uint16_t)n);
    if (n) memcpy(p + 4, cuerpo, (size_t)n);
    return enviar_l2(manejador, CID_SINAL, p, n + 4);
}

static int por_scid(uint16_t scid)
{
    for (int i = 0; i < BT_MAX_CANAIS; i++)
        if (canal[i].usado && canal[i].scid == scid) return i;
    return -1;
}

static int nuevo_canal(uint16_t manejador, uint16_t psm)
{
    for (int i = 0; i < BT_MAX_CANAIS; i++) {
        if (canal[i].usado) continue;
        memset(&canal[i], 0, sizeof canal[i]);
        canal[i].usado = true;
        canal[i].manejador = manejador;
        canal[i].psm = psm;
        canal[i].scid = (uint16_t)(CID_PRIMERO + i);
        canal[i].mtu_remoto = 48;   /* el mínimo que impone la especificación */
        canal[i].desde = bt_agora();
        return i;
    }
    return -1;
}

static void liberar(int i, int motivo)
{
    bt_canal_t c = canal[i];
    memset(&canal[i], 0, sizeof canal[i]);
    bt_g_canal_fechado(i, c.manejador, c.psm, motivo);
}

static void mandar_config(int i)
{
    uint8_t b[8];
    bt_poe16(b, canal[i].dcid);
    bt_poe16(b + 2, 0);       /* sin continuación */
    b[4] = 1;                 /* opción MTU */
    b[5] = 2;
    bt_poe16(b + 6, MTU_LOCAL);
    canal[i].id = nuevo_id();
    enviar_sinal(canal[i].manejador, L2_CONF_PET, canal[i].id, b, 8);
}

static void quiza_abierto(int i)
{
    if (canal[i].estado == L2_CONFIGURANDO && canal[i].cfg_local && canal[i].cfg_remoto) {
        canal[i].estado = L2_ABERTO;
        bt_g_canal_aberto(i);
    }
}

int bt_l2cap_conectar(uint16_t manejador, uint16_t psm)
{
    int i = nuevo_canal(manejador, psm);
    if (i < 0) return -1;
    uint8_t b[4];
    bt_poe16(b, psm);
    bt_poe16(b + 2, canal[i].scid);
    canal[i].iniciado_aqui = true;
    canal[i].estado = L2_ESPERA_CONEX;
    canal[i].id = nuevo_id();
    if (enviar_sinal(manejador, L2_CONEX_PET, canal[i].id, b, 4) < 0) {
        memset(&canal[i], 0, sizeof canal[i]);
        return -1;
    }
    return i;
}

void bt_l2cap_cerrar(int i)
{
    if (i < 0 || i >= BT_MAX_CANAIS || !canal[i].usado) return;
    if (canal[i].estado == L2_ABERTO || canal[i].estado == L2_CONFIGURANDO) {
        uint8_t b[4];
        bt_poe16(b, canal[i].dcid);
        bt_poe16(b + 2, canal[i].scid);
        canal[i].estado = L2_CERRANDO;
        canal[i].desde = bt_agora();
        canal[i].id = nuevo_id();
        enviar_sinal(canal[i].manejador, L2_DESC_PET, canal[i].id, b, 4);
    } else if (canal[i].estado != L2_CERRANDO) {
        liberar(i, 0);
    }
}

int bt_l2cap_enviar(int i, const uint8_t *d, int n)
{
    if (i < 0 || i >= BT_MAX_CANAIS || !canal[i].usado || canal[i].estado != L2_ABERTO) return -1;
    if (n > canal[i].mtu_remoto) return -1;
    return enviar_l2(canal[i].manejador, canal[i].dcid, d, n);
}

/* ------------------------------------------------------------------ señalización */

static void rechazar(uint16_t m, uint8_t id, uint16_t razon)
{
    uint8_t b[2];
    bt_poe16(b, razon);
    enviar_sinal(m, L2_CMD_RECHAZO, id, b, 2);
}

static void sinal_cmd(uint16_t m, uint8_t code, uint8_t id, const uint8_t *d, int n)
{
    switch (code) {
    case L2_CONEX_PET: {
        if (n < 4) { rechazar(m, id, 0); return; }
        uint16_t psm = bt_le16(d), rcid = bt_le16(d + 2);
        uint8_t r[8];
        bt_poe16(r + 2, rcid);
        bt_poe16(r + 6, 0);
        if (!bt_g_aceitar_psm(m, psm)) {
            bt_poe16(r, 0);
            bt_poe16(r + 4, 2);   /* PSM no admitido */
            enviar_sinal(m, L2_CONEX_RESP, id, r, 8);
            return;
        }
        int i = nuevo_canal(m, psm);
        if (i < 0) {
            bt_poe16(r, 0);
            bt_poe16(r + 4, 4);   /* sin recursos */
            enviar_sinal(m, L2_CONEX_RESP, id, r, 8);
            return;
        }
        canal[i].dcid = rcid;
        canal[i].estado = L2_CONFIGURANDO;
        bt_poe16(r, canal[i].scid);
        bt_poe16(r + 4, 0);
        enviar_sinal(m, L2_CONEX_RESP, id, r, 8);
        mandar_config(i);
        break;
    }
    case L2_CONEX_RESP: {
        if (n < 8) return;
        uint16_t dcid = bt_le16(d), scid = bt_le16(d + 2), res = bt_le16(d + 4);
        int i = por_scid(scid);
        if (i < 0 || canal[i].estado != L2_ESPERA_CONEX || canal[i].manejador != m) return;
        if (res == 1) { canal[i].desde = bt_agora(); return; }   /* pendiente: seguimos esperando */
        if (res != 0) { liberar(i, res); return; }
        canal[i].dcid = dcid;
        canal[i].estado = L2_CONFIGURANDO;
        canal[i].desde = bt_agora();
        mandar_config(i);
        break;
    }
    case L2_CONF_PET: {
        if (n < 4) { rechazar(m, id, 0); return; }
        int i = por_scid(bt_le16(d));
        if (i < 0 || canal[i].manejador != m) { rechazar(m, id, 2); return; }
        uint16_t result = 0;
        uint8_t rechazadas[16];
        int nr = 0;
        for (int k = 4; k + 2 <= n;) {
            uint8_t t = d[k], l = d[k + 1];
            if (k + 2 + l > n) break;
            if ((t & 0x7F) == 1 && l == 2) canal[i].mtu_remoto = bt_le16(d + k + 2);
            else if ((t & 0x7F) == 4 && l >= 1 && d[k + 2] != 0) result = 2;   /* solo el modo básico */
            else if ((t & 0x7F) > 4 || (t & 0x7F) == 0) {
                if (!(t & 0x80) && nr + 2 <= (int)sizeof rechazadas) { rechazadas[nr++] = t; rechazadas[nr++] = l; result = 3; }
            }
            k += 2 + l;
        }
        uint8_t r[6 + 16];
        bt_poe16(r, canal[i].dcid);
        bt_poe16(r + 2, 0);
        bt_poe16(r + 4, result);
        int rn = 6;
        if (result == 3) { memcpy(r + 6, rechazadas, (size_t)nr); rn += nr; }
        enviar_sinal(m, L2_CONF_RESP, id, r, rn);
        if (!result) { canal[i].cfg_remoto = true; quiza_abierto(i); }
        else liberar(i, result);
        break;
    }
    case L2_CONF_RESP: {
        if (n < 6) return;
        int i = por_scid(bt_le16(d));
        if (i < 0 || canal[i].manejador != m) return;
        if (bt_le16(d + 4) == 0) { canal[i].cfg_local = true; quiza_abierto(i); }
        else {   /* parámetros que no aceptan: este nivel de pila no negocia más */
            bt_l2cap_cerrar(i);
        }
        break;
    }
    case L2_DESC_PET: {
        if (n < 4) return;
        int i = por_scid(bt_le16(d));   /* el DCID de la petición es nuestro canal */
        uint8_t r[4];
        memcpy(r, d, 4);
        if (i < 0 || canal[i].manejador != m || canal[i].dcid != bt_le16(d + 2)) { rechazar(m, id, 2); return; }
        enviar_sinal(m, L2_DESC_RESP, id, r, 4);
        liberar(i, 0);
        break;
    }
    case L2_DESC_RESP: {
        if (n < 4) return;
        int i = por_scid(bt_le16(d + 2));
        if (i >= 0 && canal[i].estado == L2_CERRANDO && canal[i].manejador == m) liberar(i, 0);
        break;
    }
    case L2_ECO_PET: enviar_sinal(m, L2_ECO_RESP, id, d, n > 40 ? 40 : n); break;
    case L2_INFO_PET: {
        if (n < 2) return;
        uint8_t r[4];
        memcpy(r, d, 2);
        bt_poe16(r + 2, 1);   /* no admitido */
        enviar_sinal(m, L2_INFO_RESP, id, r, 4);
        break;
    }
    case L2_CMD_RECHAZO:
        for (int i = 0; i < BT_MAX_CANAIS; i++)
            if (canal[i].usado && canal[i].manejador == m && canal[i].id == id && canal[i].estado != L2_ABERTO) {
                liberar(i, 0x100);
                break;
            }
        break;
    default:
        rechazar(m, id, 0);
        break;
    }
}

static void trama(uint16_t m, const uint8_t *d, int n)
{
    if (n < 4) return;
    uint16_t len = bt_le16(d), cid = bt_le16(d + 2);
    if (len + 4 != n) return;
    d += 4;
    if (cid == CID_SINAL) {
        for (int k = 0; k + 4 <= len;) {
            uint16_t l = bt_le16(d + k + 2);
            if (k + 4 + l > len) break;
            sinal_cmd(m, d[k], d[k + 1], d + k + 4, l);
            k += 4 + l;
        }
        return;
    }
    int i = por_scid(cid);
    if (i >= 0 && canal[i].manejador == m && canal[i].estado == L2_ABERTO) bt_g_canal_dados(i, d, len);
}

void bt_l2cap_acl(uint16_t m, uint8_t pb, const uint8_t *d, int n)
{
    int k = -1, libre = -1;
    for (int i = 0; i < (int)(sizeof reens / sizeof reens[0]); i++) {
        if (reens[i].activo && reens[i].manejador == m) k = i;
        else if (!reens[i].activo && libre < 0) libre = i;
    }
    if (pb == 2 || pb == 0) {   /* inicio (0 solo lo manda un controlador raro: se trata igual) */
        if (n < 4) return;
        uint16_t len = bt_le16(d);
        if (len + 4 > L2_MAX) { if (k >= 0) reens[k].activo = false; return; }
        if (n == len + 4) { if (k >= 0) reens[k].activo = false; trama(m, d, n); return; }
        if (n > len + 4) return;
        if (k < 0) k = libre;
        if (k < 0) return;
        reens[k].activo = true;
        reens[k].manejador = m;
        reens[k].total = (uint16_t)(len + 4);
        memcpy(reens[k].d, d, (size_t)n);
        reens[k].tengo = (uint16_t)n;
    } else if (pb == 1) {
        if (k < 0) return;   /* continuación sin inicio */
        if (reens[k].tengo + n > reens[k].total) { reens[k].activo = false; return; }
        memcpy(reens[k].d + reens[k].tengo, d, (size_t)n);
        reens[k].tengo = (uint16_t)(reens[k].tengo + n);
        if (reens[k].tengo == reens[k].total) {
            reens[k].activo = false;
            trama(m, reens[k].d, reens[k].total);
        }
    }
}

void bt_l2cap_ligacao_caiu(uint16_t m)
{
    for (int i = 0; i < (int)(sizeof reens / sizeof reens[0]); i++)
        if (reens[i].activo && reens[i].manejador == m) reens[i].activo = false;
    for (int i = 0; i < BT_MAX_CANAIS; i++)
        if (canal[i].usado && canal[i].manejador == m) liberar(i, -2);
}

void bt_l2cap_tic(void)
{
    uint32_t ahora = bt_agora();
    for (int i = 0; i < BT_MAX_CANAIS; i++) {
        if (!canal[i].usado) continue;
        uint32_t lim = canal[i].estado == L2_CERRANDO ? TIEMPO_CIERRE : TIEMPO_PETICION;
        if (canal[i].estado == L2_ABERTO || (ahora - canal[i].desde) < lim) continue;
        if (canal[i].estado == L2_CERRANDO) liberar(i, 0);
        else liberar(i, -1);
    }
}

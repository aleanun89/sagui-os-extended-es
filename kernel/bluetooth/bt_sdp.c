// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_sdp.c - cliente SDP (D279): busca el servicio HID de un dispositivo con una sola petición
 * ServiceSearchAttributeRequest y lee de la respuesta los PSM de los canales, la subclase, si es
 * dispositivo de arranque y el descriptor de informe. Solo especificación pública (Core, vol. 3
 * parte B, y HID Profile 1.1, sección 5.3). */
#include "bt.h"

#define SDP_ERRO        0x01
#define SDP_BUSCA_ATRIB 0x06
#define SDP_RESP_ATRIB  0x07

#define UUID_L2CAP 0x0100
#define UUID_HIDP  0x0011
#define UUID_HID   0x1124

#define DE_NIL  0
#define DE_UINT 1
#define DE_INT  2
#define DE_UUID 3
#define DE_TXT  4
#define DE_BOOL 5
#define DE_SEQ  6
#define DE_ALT  7
#define DE_URL  8

typedef struct { int tipo; const uint8_t *v; int n; uint32_t num; int total; } de_t;

static uint8_t acc[BT_SDP_MAX];
static int acc_n;
static uint8_t cont[17];       /* estado de continuación: longitud y bytes */
static uint16_t tid;
static bool en_curso;

void bt_sdp_reiniciar(void) { acc_n = 0; cont[0] = 0; en_curso = false; }

/* Lee un elemento de datos. total = bytes que ocupa (cabecera incluida); <0 si está mal. */
static int de_ler(const uint8_t *p, int n, de_t *e)
{
    if (n < 1) return -1;
    int tipo = p[0] >> 3, sz = p[0] & 7, hdr = 1, len;
    switch (sz) {
    case 0: len = tipo == DE_NIL ? 0 : 1; break;
    case 1: len = 2; break;
    case 2: len = 4; break;
    case 3: len = 8; break;
    case 4: len = 16; break;
    case 5: if (n < 2) return -1; len = p[1]; hdr = 2; break;
    case 6: if (n < 3) return -1; len = bt_be16(p + 1); hdr = 3; break;
    default: if (n < 5) return -1; len = (int)((uint32_t)p[1] << 24 | p[2] << 16 | p[3] << 8 | p[4]); hdr = 5; break;
    }
    if (len < 0 || hdr + len > n) return -1;
    e->tipo = tipo;
    e->v = p + hdr;
    e->n = len;
    e->total = hdr + len;
    e->num = 0;
    for (int i = 0; i < len && i < 4; i++) e->num = e->num << 8 | e->v[i];
    if (len > 4 && (tipo == DE_UINT || tipo == DE_INT || tipo == DE_UUID)) e->num = 0;   /* no cabe en 32 bits */
    return e->total;
}

/* El siguiente hijo de una secuencia; *pos avanza. */
static bool hijo(const de_t *pai, int *pos, de_t *f)
{
    if (*pos >= pai->n) return false;
    int r = de_ler(pai->v + *pos, pai->n - *pos, f);
    if (r < 0) return false;
    *pos += r;
    return true;
}

/* ProtocolDescriptorList: [[L2CAP, psm], [HIDP]] -> psm (0 si no hay). */
static uint16_t psm_de_lista(const de_t *lista)
{
    de_t d;
    int pos = 0;
    while (hijo(lista, &pos, &d)) {
        if (d.tipo != DE_SEQ) continue;
        de_t a, b;
        int q = 0;
        if (hijo(&d, &q, &a) && a.tipo == DE_UUID && a.num == UUID_L2CAP && hijo(&d, &q, &b) && b.tipo == DE_UINT)
            return (uint16_t)b.num;
    }
    return 0;
}

static void atributo(uint16_t id, const de_t *v, bt_sdp_hid_t *r)
{
    de_t h;
    int pos = 0;
    switch (id) {
    case 0x0004:
        if (v->tipo == DE_SEQ) { uint16_t p = psm_de_lista(v); if (p) r->psm_ctl = p; }
        break;
    case 0x000D:   /* lista de listas: la del canal de interrupción */
        if (v->tipo == DE_SEQ)
            while (hijo(v, &pos, &h))
                if (h.tipo == DE_SEQ) { uint16_t p = psm_de_lista(&h); if (p) { r->psm_int = p; break; } }
        break;
    case 0x0100:
        if (v->tipo == DE_TXT) {
            int k = v->n < BT_NOME_MAX - 1 ? v->n : BT_NOME_MAX - 1;
            memcpy(r->nome, v->v, (size_t)k);
            r->nome[k] = 0;
        }
        break;
    case 0x0202: if (v->tipo == DE_UINT) r->subclasse = (uint8_t)v->num; break;
    case 0x020E: if (v->tipo == DE_BOOL) r->boot = v->num != 0; break;
    case 0x0206:   /* HIDDescriptorList: [[tipo 0x22, descriptor]] */
        if (v->tipo != DE_SEQ) break;
        while (hijo(v, &pos, &h)) {
            de_t t, d;
            int q = 0;
            if (h.tipo != DE_SEQ || !hijo(&h, &q, &t) || t.tipo != DE_UINT || t.num != 0x22 || !hijo(&h, &q, &d) || d.tipo != DE_TXT) continue;
            if (d.n <= BT_DESC_MAX) { memcpy(r->desc, d.v, (size_t)d.n); r->desc_tam = (uint16_t)d.n; }
            break;
        }
        break;
    }
}

/* Analiza los datos acumulados (la lista de listas de atributos). */
int bt_sdp_analisar(const uint8_t *dados, int n, bt_sdp_hid_t *r)
{
    memset(r, 0, sizeof *r);
    de_t top;
    if (de_ler(dados, n, &top) < 0 || top.tipo != DE_SEQ) return -1;
    de_t rec;
    int pos = 0;
    bool algo = false;
    while (hijo(&top, &pos, &rec)) {
        if (rec.tipo != DE_SEQ) continue;
        int q = 0;
        de_t id, val;
        while (hijo(&rec, &q, &id) && hijo(&rec, &q, &val)) {
            if (id.tipo != DE_UINT || id.n != 2) continue;
            atributo((uint16_t)id.num, &val, r);
            algo = true;
        }
        if (algo) break;   /* solo el primer servicio HID */
    }
    if (!algo) return -1;
    r->ok = true;
    if (!r->psm_ctl) r->psm_ctl = PSM_HID_CTL;
    if (!r->psm_int) r->psm_int = PSM_HID_INT;
    return 0;
}

/* ------------------------------------------------------------------ petición */

static void mandar(int canal_)
{
    uint8_t p[64];
    int k = 0;
    static const uint8_t cuerpo[] = {
        0x35, 0x03, 0x19, 0x11, 0x24,              /* patrón: el servicio HID (0x1124) */
        0x02, 0x80,                                /* bytes de atributos que acepta: 640 */
        0x35, 0x12, 0x09, 0x00, 0x04, 0x09, 0x00, 0x0D, 0x09, 0x01, 0x00,
        0x09, 0x02, 0x02, 0x09, 0x02, 0x06, 0x09, 0x02, 0x0E,   /* atributos */
    };
    int pl = (int)sizeof cuerpo + 1 + cont[0];
    p[k++] = SDP_BUSCA_ATRIB;
    tid++;
    bt_poe_be16(p + k, tid); k += 2;
    bt_poe_be16(p + k, (uint16_t)pl); k += 2;
    memcpy(p + k, cuerpo, sizeof cuerpo); k += (int)sizeof cuerpo;
    p[k++] = cont[0];
    memcpy(p + k, cont + 1, cont[0]); k += cont[0];
    bt_l2cap_enviar(canal_, p, k);
}

void bt_sdp_pedir(int canal_)
{
    acc_n = 0;
    cont[0] = 0;
    en_curso = true;
    mandar(canal_);
}

int bt_sdp_resposta(int canal_, const uint8_t *d, int n, bt_sdp_hid_t *r)
{
    if (!en_curso || n < 5) return -1;
    if (d[0] == SDP_ERRO) { en_curso = false; return -2; }
    if (d[0] != SDP_RESP_ATRIB || bt_be16(d + 1) != tid) return -1;
    int pl = bt_be16(d + 3);
    if (5 + pl > n || pl < 3) return -1;
    int bytes = bt_be16(d + 5);
    if (7 + bytes + 1 > 5 + pl) return -1;
    if (acc_n + bytes > BT_SDP_MAX) { en_curso = false; return -3; }   /* demasiado grande */
    memcpy(acc + acc_n, d + 7, (size_t)bytes);
    acc_n += bytes;
    int cl = d[7 + bytes];
    if (cl > 16 || 7 + bytes + 1 + cl > 5 + pl) return -1;
    if (cl) {
        cont[0] = (uint8_t)cl;
        memcpy(cont + 1, d + 8 + bytes, (size_t)cl);
        mandar(canal_);
        return 0;
    }
    en_curso = false;
    return bt_sdp_analisar(acc, acc_n, r) < 0 ? -4 : 1;
}

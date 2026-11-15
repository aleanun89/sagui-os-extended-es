// SPDX-License-Identifier: GPL-2.0-or-later
/* uvc_nucleo.c - la parte pura del driver UVC 1.1 (D277). Ver uvc_nucleo.h. */
#include "uvc_nucleo.h"
#ifdef UVC_ANFITRION
#include <stddef.h>
#include <string.h>
#else
#include "texto.h"
#endif

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void esc16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void esc32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

/* ------------------------------------------------------------ descriptores */

int uvc_analizar(const uint8_t *b, int n, uvc_info_t *o)
{
    memset(o, 0, sizeof *o);
    int cls = -1, sub = -1, alt = 0, ifn = 0;
    bool tiene_vc = false, tiene_vs = false;
    uvc_formato_t *fmt = NULL;                 /* formato en curso (NULL = no se usa) */
    for (int i = 0; i + 1 < n && b[i] >= 2 && i + b[i] <= n; i += b[i]) {
        int l = b[i], t = b[i + 1];
        if (t == 4 && l >= 9) {                 /* interfaz */
            ifn = b[i + 2]; alt = b[i + 3]; cls = b[i + 5]; sub = b[i + 6];
            if (cls == 14 && sub == 1 && !tiene_vc) { o->iface_vc = (uint8_t)ifn; tiene_vc = true; }
            if (cls == 14 && sub == 2 && !tiene_vs) { o->iface_vs = (uint8_t)ifn; tiene_vs = true; }
            if (!(cls == 14 && sub == 2 && ifn == o->iface_vs && alt == 0)) fmt = NULL;
        } else if (t == 0x24 && cls == 14 && sub == 1 && ifn == o->iface_vc) {
            if (b[i + 2] == 1 && l >= 12) o->bcd = le16(b + i + 3);                       /* VC_HEADER */
            else if (b[i + 2] == 5 && l >= 8 && !o->pu_id) {                             /* PROCESSING_UNIT */
                o->pu_id = b[i + 3];
                int cs = b[i + 7];
                o->pu_controles = (uint16_t)((cs >= 1 && l >= 9 ? b[i + 8] : 0) | (cs >= 2 && l >= 10 ? b[i + 9] << 8 : 0));
            }
        } else if (t == 0x24 && cls == 14 && sub == 2 && tiene_vs && ifn == o->iface_vs && alt == 0) {
            int st = b[i + 2];
            if ((st == 4 && l >= 27) || (st == 6 && l >= 11)) {                          /* FORMAT_UNCOMPRESSED / MJPEG */
                fmt = NULL;
                bool yuy2 = st == 4 && b[i + 5] == 'Y' && b[i + 6] == 'U' && b[i + 7] == 'Y' && b[i + 8] == '2';
                if ((st == 6 || yuy2) && o->nf < UVC_MAX_FORMATOS) {
                    fmt = &o->f[o->nf++];
                    fmt->idx = b[i + 3];
                    fmt->tipo = st == 6 ? UVC_FMT_MJPEG : UVC_FMT_YUY2;
                    fmt->bits = st == 4 ? b[i + 21] : 0;
                }
            } else if ((st == 5 || st == 7) && l >= 26 && fmt && fmt->nq < UVC_MAX_QUADROS) {   /* FRAME_UNCOMPRESSED / MJPEG */
                uvc_quadro_t *q = &fmt->q[fmt->nq];
                memset(q, 0, sizeof *q);
                q->idx = b[i + 3];
                q->larg = le16(b + i + 5);
                q->alt = le16(b + i + 7);
                q->tam_max = le32(b + i + 17);
                q->intervalo_def = le32(b + i + 21);
                int tipo_int = b[i + 25];
                for (int k = 0; k < tipo_int && k < UVC_MAX_INTERVALOS && i + 26 + 4 * k + 4 <= i + l; k++)
                    q->ints[q->n_int++] = le32(b + i + 26 + 4 * k);
                if (q->larg && q->alt) fmt->nq++;
            }
        } else if (t == 5 && l >= 7 && cls == 14 && sub == 2 && tiene_vs && ifn == o->iface_vs && alt > 0 && o->nalt < UVC_MAX_ALT) {
            uint8_t dir = b[i + 2];
            if ((dir & 0x80) && (b[i + 3] & 3) == 1) {                                   /* isócrono de entrada */
                uint16_t w = le16(b + i + 4);
                uvc_alt_t *a = &o->alt[o->nalt++];
                a->alt = (uint8_t)alt;
                a->ep = dir & 0x0F;
                a->mps = w & 0x7FFu;
                a->intervalo = b[i + 6];
                a->mult = (uint8_t)(((w >> 11) & 3) + 1);
                if (a->mult > 3) a->mult = 3;
                if (!a->mps) o->nalt--;
            }
        }
    }
    int utiles = 0;
    for (int k = 0; k < o->nf; k++) if (o->f[k].nq) utiles++;
    if (!tiene_vs || !o->nalt || !utiles) return 0;
    /* Los formatos sin ningún cuadro se descartan (se compactan). */
    int d = 0;
    for (int k = 0; k < o->nf; k++) if (o->f[k].nq) { if (d != k) o->f[d] = o->f[k]; d++; }
    o->nf = (uint8_t)d;
    return d;
}

/* ------------------------------------------------------------ PROBE / COMMIT */

int uvc_ctl_serializar(const uvc_ctl_t *c, uint8_t *o, int tam)
{
    if (tam != UVC_CTL_TAM_10 && tam != UVC_CTL_TAM_11) return -1;
    memset(o, 0, (size_t)tam);
    esc16(o, c->hint);
    o[2] = c->formato;
    o[3] = c->quadro;
    esc32(o + 4, c->intervalo);
    esc16(o + 8, c->kf); esc16(o + 10, c->pf); esc16(o + 12, c->calidad); esc16(o + 14, c->ventana); esc16(o + 16, c->retardo);
    esc32(o + 18, c->tam_quadro);
    esc32(o + 22, c->tam_payload);
    if (tam == UVC_CTL_TAM_11) {
        esc32(o + 26, c->reloj);
        o[30] = c->framing; o[31] = c->version_pref; o[32] = c->version_min; o[33] = c->version_max;
    }
    return tam;
}

bool uvc_ctl_leer(const uint8_t *b, int n, uvc_ctl_t *c)
{
    memset(c, 0, sizeof *c);
    if (n < UVC_CTL_TAM_10) return false;
    c->hint = le16(b);
    c->formato = b[2];
    c->quadro = b[3];
    c->intervalo = le32(b + 4);
    c->kf = le16(b + 8); c->pf = le16(b + 10); c->calidad = le16(b + 12); c->ventana = le16(b + 14); c->retardo = le16(b + 16);
    c->tam_quadro = le32(b + 18);
    c->tam_payload = le32(b + 22);
    if (n >= UVC_CTL_TAM_11) {
        c->reloj = le32(b + 26);
        c->framing = b[30]; c->version_pref = b[31]; c->version_min = b[32]; c->version_max = b[33];
    }
    return true;
}

int uvc_negociar(uvc_pedido_f pedido, void *ctx, uint8_t iface_vs, int tam_ctl,
                 uint8_t formato, uint8_t quadro, uint32_t intervalo, uvc_ctl_t *res)
{
    uint8_t buf[UVC_CTL_TAM_11];
    uvc_ctl_t c;
    memset(&c, 0, sizeof c);
    c.hint = 1;                                   /* manda dwFrameInterval */
    c.formato = formato;
    c.quadro = quadro;
    c.intervalo = intervalo;
    if (uvc_ctl_serializar(&c, buf, tam_ctl) < 0) return -1;
    if (pedido(ctx, 0x21, 0x01, 0x0100, iface_vs, buf, (uint16_t)tam_ctl) < 0) return -2;       /* SET_CUR(PROBE) */
    memset(buf, 0, sizeof buf);
    int n = pedido(ctx, 0xA1, 0x81, 0x0100, iface_vs, buf, (uint16_t)tam_ctl);                  /* GET_CUR(PROBE) */
    if (n < UVC_CTL_TAM_10 || !uvc_ctl_leer(buf, n, &c)) return -3;
    if (!c.tam_payload || c.formato != formato || c.quadro != quadro) return -4;                /* la cámara no aceptó */
    if (uvc_ctl_serializar(&c, buf, n >= UVC_CTL_TAM_11 && tam_ctl == UVC_CTL_TAM_11 ? UVC_CTL_TAM_11 : UVC_CTL_TAM_10) < 0) return -1;
    int tam_c = n >= UVC_CTL_TAM_11 && tam_ctl == UVC_CTL_TAM_11 ? UVC_CTL_TAM_11 : UVC_CTL_TAM_10;
    if (pedido(ctx, 0x21, 0x01, 0x0200, iface_vs, buf, (uint16_t)tam_c) < 0) return -5;         /* SET_CUR(COMMIT) */
    if (res) *res = c;
    return 0;
}

const uvc_alt_t *uvc_elegir_alt(const uvc_info_t *i, uint32_t tam_payload)
{
    const uvc_alt_t *mejor = NULL, *mayor = NULL;
    for (int k = 0; k < i->nalt; k++) {
        const uvc_alt_t *a = &i->alt[k];
        uint32_t cap = (uint32_t)a->mps * a->mult;
        if (!mayor || cap > (uint32_t)mayor->mps * mayor->mult) mayor = a;
        if (cap >= tam_payload && (!mejor || cap < (uint32_t)mejor->mps * mejor->mult)) mejor = a;
    }
    return mejor ? mejor : mayor;
}

bool uvc_buscar(const uvc_info_t *i, uint8_t tipo, uint16_t larg, uint16_t alt, const uvc_formato_t **f, const uvc_quadro_t **q)
{
    for (int k = 0; k < i->nf; k++) {
        if (i->f[k].tipo != tipo) continue;
        const uvc_quadro_t *mejor = NULL;
        uint32_t dm = 0xFFFFFFFFu;
        for (int j = 0; j < i->f[k].nq; j++) {
            const uvc_quadro_t *c = &i->f[k].q[j];
            uint32_t dw = c->larg > larg ? c->larg - larg : larg - c->larg, dh = c->alt > alt ? c->alt - alt : alt - c->alt;
            if (dw + dh < dm) { dm = dw + dh; mejor = c; }
        }
        if (mejor) { *f = &i->f[k]; *q = mejor; return true; }
    }
    return false;
}

/* ------------------------------------------------------------ montaje de cuadros */

void uvc_montador_iniciar(uvc_montador_t *m, uint8_t *b0, uint8_t *b1, uint32_t cap, uint8_t tipo, uint16_t larg, uint16_t alt)
{
    memset(m, 0, sizeof *m);
    m->buf[0] = b0;
    m->buf[1] = b1;
    m->cap = cap;
    m->tipo = tipo;
    m->larg = larg;
    m->alt = alt;
    m->fid = -1;
}

bool uvc_cuadro_valido(uint8_t tipo, uint16_t larg, uint16_t alt, const uint8_t *d, uint32_t n)
{
    if (tipo == UVC_FMT_YUY2) return n >= (uint32_t)larg * alt * 2u;
    if (tipo == UVC_FMT_MJPEG) {
        if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
        uint32_t ini = n > 16 ? n - 16 : 2;                 /* algunas cámaras rellenan con ceros tras el EOI */
        for (uint32_t k = ini; k + 1 < n; k++) if (d[k] == 0xFF && d[k + 1] == 0xD9) return true;
        return false;
    }
    return false;
}

static void cerrar_cuadro(uvc_montador_t *m)
{
    if (m->len == 0) { m->malo = false; return; }
    if (!m->malo && uvc_cuadro_valido(m->tipo, m->larg, m->alt, m->buf[0], m->len)) {
        uint8_t *t = m->buf[0];
        m->buf[0] = m->buf[1];
        m->buf[1] = t;
        m->ult_len = m->tipo == UVC_FMT_YUY2 ? (uint32_t)m->larg * m->alt * 2u : m->len;
        m->seq++;
    } else
        m->descartados++;
    m->len = 0;
    m->malo = false;
}

void uvc_montador_paquete(uvc_montador_t *m, const uint8_t *p, uint32_t n)
{
    m->paquetes++;
    if (n == 0) return;                                      /* paquete vacío: la cámara no tiene nada que mandar */
    /* una cabecera imposible solo estropea el cuadro si ya había uno en curso (si no, lo que se perdió
     * es el principio del siguiente, y la validación del cuadro lo descarta de todos modos) */
    if (n < 2 || p[0] < 2 || p[0] > n) { m->errores++; if (m->len) m->malo = true; return; }
    uint32_t hl = p[0];
    uint8_t info = p[1];
    int fid = info & 1;
    if (info & 0x40) { m->errores++; m->malo = true; }       /* bit ERR de la cabecera */
    if (m->fid >= 0 && fid != m->fid) cerrar_cuadro(m);      /* cambió FID sin EOF: terminó el cuadro anterior */
    m->fid = fid;
    uint32_t d = n - hl;
    if (d) {
        if (m->len + d > m->cap) m->malo = true;             /* más de lo que cabe: el cuadro se descarta */
        else { memcpy(m->buf[0] + m->len, p + hl, d); m->len += d; }
    }
    if (info & 0x02) { cerrar_cuadro(m); m->fid = -1; }      /* EOF */
}

const uint8_t *uvc_montador_ultimo(const uvc_montador_t *m) { return m->buf[1]; }

/* ------------------------------------------------------------ fuente de prueba */

void uvc_gerar_padrao(uint8_t *o, uint16_t larg, uint16_t alt, uint32_t seq)
{
    static const uint8_t barras[8][3] = {
        { 235, 128, 128 }, { 210, 16, 146 }, { 170, 166, 16 }, { 145, 54, 34 },
        { 106, 202, 222 }, { 81, 90, 240 }, { 41, 240, 110 }, { 16, 128, 128 } };
    uint32_t lado = (larg < alt ? larg : alt) / 6, franja = alt / 8;
    uint32_t bx = larg > lado ? (seq * 8u) % (larg - lado) : 0, by = alt / 3;
    for (uint32_t y = 0; y < alt; y++) {
        for (uint32_t x = 0; x < larg; x += 2) {
            uint8_t Y0, Y1, U, V;
            if (y >= (uint32_t)alt - franja) {                 /* contador de 16 bits en bloques */
                uint32_t bit = x * 16u / larg;
                bool uno = (seq >> (15 - bit)) & 1u;
                Y0 = Y1 = uno ? 235 : 16; U = V = 128;
            } else {
                const uint8_t *c = barras[x * 8u / larg];
                Y0 = Y1 = c[0]; U = c[1]; V = c[2];
                if (y >= by && y < by + lado) {                /* el cuadro blanco que se mueve */
                    if (x >= bx && x < bx + lado) { Y0 = 235; U = V = 128; }
                    if (x + 1 >= bx && x + 1 < bx + lado) { Y1 = 235; U = V = 128; }
                }
            }
            uint8_t *p = o + ((size_t)y * larg + x) * 2;
            p[0] = Y0; p[1] = U; p[2] = Y1; p[3] = V;
        }
    }
}

uint32_t uvc_gerar_paquete(const uint8_t *quadro, uint32_t tam, uint32_t *pos, uint8_t fid, uint32_t mps, uint8_t *saida)
{
    if (*pos >= tam || mps <= 12) return 0;
    uint32_t d = tam - *pos, cab = 12;
    if (d > mps - cab) d = mps - cab;
    memset(saida, 0, cab);
    saida[0] = (uint8_t)cab;
    saida[1] = (uint8_t)(0x80 | 0x08 | 0x04 | (fid & 1) | (*pos + d >= tam ? 0x02 : 0));   /* EOH, SCR, PTS, FID, EOF */
    memcpy(saida + cab, quadro + *pos, d);
    *pos += d;
    return cab + d;
}

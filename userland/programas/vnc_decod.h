// SPDX-License-Identifier: GPL-2.0-or-later
/* vnc_decod.h - la parte del cliente VNC (mejora 097 fase 2, D254) que no toca el
 * sistema: formato de píxel del servidor y los decodificadores de Raw, CopyRect,
 * Hextile, ZRLE y Tight (con zlib en sus cuatro flujos, filtros copy, palette y
 * gradient, y JPEG por una función que pone quien llama), más la pseudocodificación
 * Cursor y la traducción de teclas de SaguiOS a keysyms de X11. Se prueba en el
 * anfitrión (tests/vnc_cliente_unidad.c) y la usa userland/programas/vnc.c.
 * Código propio, a partir de la RFC 6143 y la especificación pública de Tight.
 *
 * Convención de los decodificadores de rectángulo: reciben los bytes que hay
 * (n) y devuelven los consumidos (>= 0), o < 0 si el servidor manda algo
 * inválido. Si faltan datos no consumen un rectángulo a medias: Raw y Hextile
 * avanzan por filas y baldosas (lo hecho se recuerda en el estado) y ZRLE y
 * Tight, que traen su longitud, esperan a tenerlo todo (devuelven 0 y no han
 * tocado nada). *hecho dice si el rectángulo está completo. */
#ifndef VNC_DECOD_H
#define VNC_DECOD_H
#include "sagui.h"
#include "compressao.h"
#include "vnc_codec.h"

#define VNC_ENC_RAW       0
#define VNC_ENC_COPYRECT  1
#define VNC_ENC_HEXTILE   5
#define VNC_ENC_TIGHT     7
#define VNC_ENC_ZRLE      16
#define VNC_ENC_CURSOR    (-239)
#define VNC_ENC_TAMANO    (-223)   /* DesktopSize */
#define VNC_ENC_ULTIMO    (-224)   /* LastRect */

#define VNC_ERR_FORMATO   (-1)
#define VNC_ERR_FUERA     (-2)   /* el rectángulo se sale de la pantalla */
#define VNC_ERR_ZLIB      (-3)
#define VNC_ERR_JPEG      (-4)
#define VNC_ERR_MEMORIA   (-5)
#define VNC_ERR_ENC       (-6)   /* codificación desconocida o no pedida */

/* Descodifica un JPEG a w x h píxeles 0x00RRGGBB (px, ya reservado); false = no se pudo. */
typedef bool (*vnc_jpeg_t)(const uint8_t *datos, int n, int w, int h, uint32_t *px);

typedef struct {
    vnc_pf_t  pf;
    uint8_t   inv[3][256];   /* valor del canal en el formato del servidor -> 0..255 */
    bool      tpix3;         /* Tight: TPIXEL de 3 bytes R, G, B */
    uint32_t *fb;            /* la pantalla remota, 0x00RRGGBB */
    int       fw, fh;
    vnc_jpeg_t jpeg;
    zinflar_t zrle;          /* ZRLE: un flujo */
    zinflar_t tight[4];      /* Tight: cuatro */
    uint8_t  *trab;          /* salida de la descompresión */
    long      trab_cap;
    /* estado del rectángulo en curso (Raw y Hextile) */
    long      prog;
    uint32_t  hbg, hfg;
    /* qué codificaciones se han descodificado alguna vez (bits VNC_VISTO_*), para el registro y las pruebas */
    uint32_t  visto;
} vnc_dec_t;

#define VNC_VISTO_RAW 0
#define VNC_VISTO_COPYRECT 1
#define VNC_VISTO_HEXTILE 2
#define VNC_VISTO_ZRLE 3
#define VNC_VISTO_TIGHT_COPY 4
#define VNC_VISTO_TIGHT_PALETA 5
#define VNC_VISTO_TIGHT_GRADIENTE 6
#define VNC_VISTO_TIGHT_RELLENO 7
#define VNC_VISTO_TIGHT_JPEG 8
#define VNC_VISTO_CURSOR 9

/* ------------------------------------------------------------ formato y pantalla */

/* Fija el formato de píxel de las respuestas (el que se mandó con SetPixelFormat). false = no admisible. */
static inline bool vnc_dec_formato(vnc_dec_t *d, const vnc_pf_t *pf)
{
    d->pf = *pf;
    if (!vnc_pf_preparar(&d->pf)) return false;
    for (int c = 0; c < 3; c++) {
        if (d->pf.max[c] > 255) return false;
        for (int i = 0; i < 256; i++)
            d->inv[c][i] = (uint8_t)(i <= d->pf.max[c] ? (i * 255 + d->pf.max[c] / 2) / d->pf.max[c] : 255);
    }
    d->tpix3 = d->pf.bpp == 32 && d->pf.profundidad == 24 && d->pf.max[0] == 255 && d->pf.max[1] == 255 && d->pf.max[2] == 255;
    return true;
}

static inline void vnc_dec_reiniciar_flujos(vnc_dec_t *d)
{
    zinflar_iniciar(&d->zrle);
    for (int i = 0; i < 4; i++) zinflar_iniciar(&d->tight[i]);
}

static inline void vnc_dec_liberar(vnc_dec_t *d)
{
    if (d->fb) libera(d->fb);
    if (d->trab) libera(d->trab);
    d->fb = NULL; d->trab = NULL; d->trab_cap = 0;
}

/* Pantalla nueva (negra). false = sin memoria. */
static inline bool vnc_dec_pantalla(vnc_dec_t *d, int w, int h)
{
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (long)w * h > 8L * 1024 * 1024) return false;
    uint32_t *n = aloca_zerado((size_t)w * h * 4);
    if (!n) return false;
    if (d->fb) libera(d->fb);
    d->fb = n; d->fw = w; d->fh = h;
    return true;
}

static inline bool vnc_dec_trabajo(vnc_dec_t *d, long n)
{
    if (n <= d->trab_cap) return true;
    uint8_t *t = aloca((size_t)n);
    if (!t) return false;
    if (d->trab) libera(d->trab);
    d->trab = t; d->trab_cap = n;
    return true;
}

/* ------------------------------------------------------------ píxeles */

static inline uint32_t vnc_dec_rgb(const vnc_dec_t *d, uint32_t v)
{
    const vnc_pf_t *pf = &d->pf;
    return (uint32_t)d->inv[0][(v >> pf->desp[0]) & pf->max[0]] << 16 | (uint32_t)d->inv[1][(v >> pf->desp[1]) & pf->max[1]] << 8
         | d->inv[2][(v >> pf->desp[2]) & pf->max[2]];
}

/* Un píxel entero (bpp / 8 bytes) en el formato del servidor. */
static inline uint32_t vnc_dec_pix(const vnc_dec_t *d, const uint8_t *p)
{
    int n = d->pf.bpp / 8;
    uint32_t v = 0;
    if (d->pf.big) for (int i = 0; i < n; i++) v = v << 8 | p[i];
    else for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return vnc_dec_rgb(d, v);
}

/* Un CPIXEL de ZRLE (3 bytes si caben, si no el píxel entero). */
static inline uint32_t vnc_dec_cpix(const vnc_dec_t *d, const uint8_t *p)
{
    if (d->pf.cp != 3) return vnc_dec_pix(d, p);
    uint32_t v = d->pf.big ? (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2] : (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
    if (d->pf.cp_alto) v <<= 8;
    return vnc_dec_rgb(d, v);
}

/* Un TPIXEL de Tight. */
static inline int vnc_dec_tpix_bytes(const vnc_dec_t *d) { return d->tpix3 ? 3 : d->pf.bpp / 8; }
static inline uint32_t vnc_dec_tpix(const vnc_dec_t *d, const uint8_t *p)
{
    if (d->tpix3) return (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
    return vnc_dec_pix(d, p);
}

static inline void vnc_dec_llenar(vnc_dec_t *d, int x, int y, int w, int h, uint32_t c)
{
    for (int j = 0; j < h; j++) {
        uint32_t *f = d->fb + (long)(y + j) * d->fw + x;
        for (int i = 0; i < w; i++) f[i] = c;
    }
}

static inline bool vnc_dec_dentro(const vnc_dec_t *d, int x, int y, int w, int h)
{
    return x >= 0 && y >= 0 && w >= 0 && h >= 0 && x + w <= d->fw && y + h <= d->fh;
}

/* ------------------------------------------------------------ Raw */

static inline int vnc_dec_raw(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    int pb = d->pf.bpp / 8;
    long total = (long)w * h;
    long k = n / pb;
    if (k > total - d->prog) k = total - d->prog;
    for (long i = 0; i < k; i++) {
        long q = d->prog + i;
        d->fb[(long)(y + q / w) * d->fw + x + q % w] = vnc_dec_pix(d, p + i * pb);
    }
    d->prog += k;
    *hecho = d->prog >= total;
    if (*hecho) d->visto |= 1u << VNC_VISTO_RAW;
    return (int)(k * pb);
}

/* ------------------------------------------------------------ CopyRect */

static inline int vnc_dec_copyrect(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    *hecho = false;
    if (n < 4) return 0;
    int sx = p[0] << 8 | p[1], sy = p[2] << 8 | p[3];
    if (!vnc_dec_dentro(d, sx, sy, w, h)) return VNC_ERR_FUERA;
    if (sy >= y) for (int j = 0; j < h; j++) memmove(d->fb + (long)(y + j) * d->fw + x, d->fb + (long)(sy + j) * d->fw + sx, (size_t)w * 4);
    else for (int j = h - 1; j >= 0; j--) memmove(d->fb + (long)(y + j) * d->fw + x, d->fb + (long)(sy + j) * d->fw + sx, (size_t)w * 4);
    *hecho = true;
    d->visto |= 1u << VNC_VISTO_COPYRECT;
    return 4;
}

/* ------------------------------------------------------------ Hextile */

static inline int vnc_dec_hextile(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    int pb = d->pf.bpp / 8, columnas = (w + 15) / 16, filas = (h + 15) / 16;
    long total = (long)columnas * filas;
    int usado = 0;
    while (d->prog < total) {
        int tx = (int)(d->prog % columnas) * 16, ty = (int)(d->prog / columnas) * 16;
        int tw = w - tx < 16 ? w - tx : 16, th = h - ty < 16 ? h - ty : 16;
        const uint8_t *t = p + usado;
        int disp = n - usado;
        if (disp < 1) break;
        int sub = t[0], o = 1;
        uint32_t bg = d->hbg, fg = d->hfg;
        if (sub & 1) {   /* Raw */
            if (disp < 1 + tw * th * pb) break;
            for (int j = 0; j < th; j++) for (int i = 0; i < tw; i++)
                d->fb[(long)(y + ty + j) * d->fw + x + tx + i] = vnc_dec_pix(d, t + o + (j * tw + i) * pb);
            usado += 1 + tw * th * pb;
            d->prog++;
            continue;
        }
        if (sub & 2) { if (disp < o + pb) break; bg = vnc_dec_pix(d, t + o); o += pb; }
        if (sub & 4) { if (disp < o + pb) break; fg = vnc_dec_pix(d, t + o); o += pb; }
        int ns = 0;
        if (sub & 8) { if (disp < o + 1) break; ns = t[o++]; }
        int coloreado = sub & 16;
        int por = coloreado ? pb + 2 : 2;
        if (disp < o + ns * por) break;
        vnc_dec_llenar(d, x + tx, y + ty, tw, th, bg);
        for (int s = 0; s < ns; s++) {
            uint32_t c = fg;
            if (coloreado) { c = vnc_dec_pix(d, t + o); o += pb; }
            int sx = t[o] >> 4, sy = t[o] & 15, sw = (t[o + 1] >> 4) + 1, sh = (t[o + 1] & 15) + 1;
            o += 2;
            if (sx + sw > tw || sy + sh > th) return VNC_ERR_FORMATO;
            vnc_dec_llenar(d, x + tx + sx, y + ty + sy, sw, sh, c);
        }
        d->hbg = bg; d->hfg = fg;
        usado += o;
        d->prog++;
    }
    *hecho = d->prog >= total;
    if (*hecho) d->visto |= 1u << VNC_VISTO_HEXTILE;
    return usado;
}

/* ------------------------------------------------------------ ZRLE */

/* Descodifica los datos ya descomprimidos de un rectángulo ZRLE. */
static inline int vnc_dec_zrle_datos(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n)
{
    int cp = d->pf.cp, o = 0;
    static uint32_t paleta[128];
    for (int ty = 0; ty < h; ty += 64) for (int tx = 0; tx < w; tx += 64) {
        int tw = w - tx < 64 ? w - tx : 64, th = h - ty < 64 ? h - ty : 64;
        if (o >= n) return VNC_ERR_FORMATO;
        int sub = p[o++], np = sub & 127;
        bool rle = sub & 128;
        uint32_t *dst0 = d->fb + (long)(y + ty) * d->fw + x + tx;
        if (!rle && np == 0) {   /* píxeles sueltos */
            if (n - o < tw * th * cp) return VNC_ERR_FORMATO;
            for (int j = 0; j < th; j++) for (int i = 0; i < tw; i++) { dst0[(long)j * d->fw + i] = vnc_dec_cpix(d, p + o); o += cp; }
            continue;
        }
        if (!rle && np == 1) {
            if (n - o < cp) return VNC_ERR_FORMATO;
            vnc_dec_llenar(d, x + tx, y + ty, tw, th, vnc_dec_cpix(d, p + o));
            o += cp;
            continue;
        }
        if (!rle && np >= 2 && np <= 16) {   /* paleta empaquetada */
            if (n - o < np * cp) return VNC_ERR_FORMATO;
            for (int k = 0; k < np; k++) { paleta[k] = vnc_dec_cpix(d, p + o); o += cp; }
            int bits = np <= 2 ? 1 : np <= 4 ? 2 : 4, fila = (tw * bits + 7) / 8;
            if (n - o < fila * th) return VNC_ERR_FORMATO;
            for (int j = 0; j < th; j++) for (int i = 0; i < tw; i++) {
                int bit = i * bits;
                int idx = (p[o + j * fila + bit / 8] >> (8 - bits - (bit & 7))) & ((1 << bits) - 1);
                if (idx >= np) return VNC_ERR_FORMATO;
                dst0[(long)j * d->fw + i] = paleta[idx];
            }
            o += fila * th;
            continue;
        }
        if (rle && np == 0) {   /* RLE llano */
            long pos = 0, tot = (long)tw * th;
            while (pos < tot) {
                if (n - o < cp + 1) return VNC_ERR_FORMATO;
                uint32_t c = vnc_dec_cpix(d, p + o);
                o += cp;
                long len = 1;
                for (;;) { if (o >= n) return VNC_ERR_FORMATO; int b = p[o++]; len += b; if (b != 255) break; }
                if (pos + len > tot) return VNC_ERR_FORMATO;
                for (long q = pos; q < pos + len; q++) dst0[(q / tw) * d->fw + q % tw] = c;
                pos += len;
            }
            continue;
        }
        if (rle && np >= 2) {   /* RLE con paleta */
            if (n - o < np * cp) return VNC_ERR_FORMATO;
            for (int k = 0; k < np; k++) { paleta[k] = vnc_dec_cpix(d, p + o); o += cp; }
            long pos = 0, tot = (long)tw * th;
            while (pos < tot) {
                if (o >= n) return VNC_ERR_FORMATO;
                int idx = p[o++];
                long len = 1;
                if (idx & 128) {
                    idx &= 127;
                    for (;;) { if (o >= n) return VNC_ERR_FORMATO; int b = p[o++]; len += b; if (b != 255) break; }
                }
                if (idx >= np || pos + len > tot) return VNC_ERR_FORMATO;
                for (long q = pos; q < pos + len; q++) dst0[(q / tw) * d->fw + q % tw] = paleta[idx];
                pos += len;
            }
            continue;
        }
        return VNC_ERR_FORMATO;   /* 17 a 127 sin RLE, o RLE con una sola entrada */
    }
    return o;
}

/* Cota del tamaño de lo descomprimido de un rectángulo ZRLE de w x h. */
static inline long vnc_zrle_maximo(const vnc_dec_t *d, int w, int h)
{
    long tiles = (long)((w + 63) / 64) * ((h + 63) / 64);
    return (long)w * h * (d->pf.cp + 1) + tiles * (1 + 128L * d->pf.cp) + 16;
}

static inline int vnc_dec_zrle(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    *hecho = false;
    if (n < 4) return 0;
    uint32_t len = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    if (len > 64u * 1024 * 1024) return VNC_ERR_FORMATO;
    if ((uint32_t)(n - 4) < len) return 0;
    long max = vnc_zrle_maximo(d, w, h);
    if (!vnc_dec_trabajo(d, max)) return VNC_ERR_MEMORIA;
    int r = zinflar(&d->zrle, p + 4, (int)len, d->trab, (int)max);
    if (r < 0) return VNC_ERR_ZLIB;
    r = vnc_dec_zrle_datos(d, x, y, w, h, d->trab, r);
    if (r < 0) return r;
    *hecho = true;
    d->visto |= 1u << VNC_VISTO_ZRLE;
    return 4 + (int)len;
}

/* ------------------------------------------------------------ Tight */

/* La longitud compacta de Tight: 1 a 3 bytes. Devuelve los bytes que ocupa, 0 si faltan, < 0 si es absurda. */
static inline int vnc_tight_longitud(const uint8_t *p, int n, int *len)
{
    if (n < 1) return 0;
    int v = p[0] & 127;
    if (!(p[0] & 128)) { *len = v; return 1; }
    if (n < 2) return 0;
    v |= (p[1] & 127) << 7;
    if (!(p[1] & 128)) { *len = v; return 2; }
    if (n < 3) return 0;
    v |= p[2] << 14;
    *len = v;
    return 3;
}

/* El filtro gradient: los datos son diferencias respecto a izquierda + arriba - arriba izquierda,
 * por canal. 'in' trae los valores de canal del servidor (3 por píxel); sale 0x00RRGGBB. */
static inline void vnc_tight_gradiente(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *in, uint8_t *fila_ant)
{
    const vnc_pf_t *pf = &d->pf;
    int bpp = pf->bpp / 8;
    uint8_t *ant = fila_ant, *act = fila_ant + w * 3;
    memset(ant, 0, (size_t)w * 3);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint32_t v[3];
            if (d->tpix3) { v[0] = in[(j * w + i) * 3]; v[1] = in[(j * w + i) * 3 + 1]; v[2] = in[(j * w + i) * 3 + 2]; }
            else {
                const uint8_t *q = in + (j * w + i) * bpp;
                uint32_t pv = 0;
                if (pf->big) for (int k = 0; k < bpp; k++) pv = pv << 8 | q[k];
                else for (int k = bpp - 1; k >= 0; k--) pv = pv << 8 | q[k];
                for (int c = 0; c < 3; c++) v[c] = (pv >> pf->desp[c]) & pf->max[c];
            }
            for (int c = 0; c < 3; c++) {
                int izq = i ? act[(i - 1) * 3 + c] : 0, arr = ant[i * 3 + c], ai = i ? ant[(i - 1) * 3 + c] : 0;
                int pred = izq + arr - ai;
                if (pred < 0) pred = 0;
                if (pred > pf->max[c]) pred = pf->max[c];
                act[i * 3 + c] = (uint8_t)((v[c] + pred) & pf->max[c]);
            }
            d->fb[(long)(y + j) * d->fw + x + i] = (uint32_t)d->inv[0][act[i * 3]] << 16 | (uint32_t)d->inv[1][act[i * 3 + 1]] << 8 | d->inv[2][act[i * 3 + 2]];
        }
        uint8_t *t = ant; ant = act; act = t;
    }
}

static inline int vnc_dec_tight(vnc_dec_t *d, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    *hecho = false;
    if (n < 1) return 0;
    int ctl = p[0], c = ctl >> 4, o = 1, tp = vnc_dec_tpix_bytes(d);
    if (c == 8) {   /* relleno */
        if (n < o + tp) return 0;
        for (int i = 0; i < 4; i++) if (ctl & (1 << i)) zinflar_iniciar(&d->tight[i]);
        vnc_dec_llenar(d, x, y, w, h, vnc_dec_tpix(d, p + o));
        *hecho = true;
        d->visto |= 1u << VNC_VISTO_TIGHT_RELLENO;
        return o + tp;
    }
    if (c == 9) {   /* JPEG */
        int len, k = vnc_tight_longitud(p + o, n - o, &len);
        if (k <= 0) return k;
        o += k;
        if (n < o + len) return 0;
        for (int i = 0; i < 4; i++) if (ctl & (1 << i)) zinflar_iniciar(&d->tight[i]);
        if (!d->jpeg) return VNC_ERR_ENC;
        uint32_t *px = aloca((size_t)w * h * 4);
        if (!px) return VNC_ERR_MEMORIA;
        if (!d->jpeg(p + o, len, w, h, px)) { libera(px); return VNC_ERR_JPEG; }
        for (int j = 0; j < h; j++) memcpy(d->fb + (long)(y + j) * d->fw + x, px + (long)j * w, (size_t)w * 4);
        libera(px);
        *hecho = true;
        d->visto |= 1u << VNC_VISTO_TIGHT_JPEG;
        return o + len;
    }
    if (c > 9) return VNC_ERR_ENC;   /* 10 = PNG, que no se pide; el resto no existe */
    int flujo = c & 3, filtro = 0;
    if (c & 4) { if (n < o + 1) return 0; filtro = p[o++]; }
    int ncol = 0;
    const uint8_t *pal = NULL;
    if (filtro == 1) {
        if (n < o + 1) return 0;
        ncol = p[o++] + 1;
        if (n < o + ncol * tp) return 0;
        pal = p + o;
        o += ncol * tp;
        if (ncol < 2) return VNC_ERR_FORMATO;
    } else if (filtro == 2) {
        if (d->pf.bpp == 8) return VNC_ERR_FORMATO;
    } else if (filtro != 0) return VNC_ERR_FORMATO;
    long esperado;
    if (filtro == 1) esperado = ncol == 2 ? (long)((w + 7) / 8) * h : (long)w * h;
    else esperado = (long)w * h * tp;
    const uint8_t *datos;
    int ndatos;
    int consumido;
    bool comprimido = esperado >= 12;
    if (!comprimido) {
        if (n < o + esperado) return 0;
        datos = p + o; ndatos = (int)esperado; consumido = o + (int)esperado;
    } else {
        int len, k = vnc_tight_longitud(p + o, n - o, &len);
        if (k <= 0) return k;
        o += k;
        if (n < o + len) return 0;
        datos = p + o; ndatos = len; consumido = o + len;
    }
    for (int i = 0; i < 4; i++) if (ctl & (1 << i)) zinflar_iniciar(&d->tight[i]);
    const uint8_t *crudo = datos;
    if (comprimido) {
        long extra = filtro == 2 ? (long)w * 6 : 0;   /* filas del gradiente */
        if (!vnc_dec_trabajo(d, esperado + extra)) return VNC_ERR_MEMORIA;
        int r = zinflar(&d->tight[flujo], datos, ndatos, d->trab, (int)esperado);
        if (r < 0) return VNC_ERR_ZLIB;
        if (r != esperado) return VNC_ERR_FORMATO;
        crudo = d->trab;
    }
    if (filtro == 0) {
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++)
            d->fb[(long)(y + j) * d->fw + x + i] = vnc_dec_tpix(d, crudo + ((long)j * w + i) * tp);
    } else if (filtro == 1) {
        static uint32_t colores[256];
        for (int k = 0; k < ncol; k++) colores[k] = vnc_dec_tpix(d, pal + k * tp);
        if (ncol == 2) {
            int fila = (w + 7) / 8;
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++)
                d->fb[(long)(y + j) * d->fw + x + i] = colores[(crudo[j * fila + i / 8] >> (7 - (i & 7))) & 1];
        } else {
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
                int idx = crudo[(long)j * w + i];
                if (idx >= ncol) return VNC_ERR_FORMATO;
                d->fb[(long)(y + j) * d->fw + x + i] = colores[idx];
            }
        }
    } else {
        /* el filtro gradient necesita dos filas de trabajo detrás de los datos */
        uint8_t *filas = d->trab + (comprimido ? esperado : 0);
        if (!comprimido) {
            if (!vnc_dec_trabajo(d, (long)w * 6)) return VNC_ERR_MEMORIA;
            filas = d->trab;
        }
        vnc_tight_gradiente(d, x, y, w, h, crudo, filas);
    }
    *hecho = true;
    d->visto |= 1u << (filtro == 0 ? VNC_VISTO_TIGHT_COPY : filtro == 1 ? VNC_VISTO_TIGHT_PALETA : VNC_VISTO_TIGHT_GRADIENTE);
    return consumido;
}

/* ------------------------------------------------------------ Cursor */

/* La forma del cursor (pseudocodificación -239): w x h píxeles del formato del servidor y luego una
 * máscara de bits por fila. Saca 0x00RRGGBB en px y 0/1 en mascara (w * h cada uno). Devuelve los
 * bytes consumidos, 0 si faltan, < 0 si hay un error. */
static inline int vnc_dec_cursor(const vnc_dec_t *d, int w, int h, const uint8_t *p, int n, uint32_t *px, uint8_t *mascara)
{
    int pb = d->pf.bpp / 8, fila = (w + 7) / 8;
    long total = (long)w * h * pb + (long)fila * h;
    if (n < total) return 0;
    for (long i = 0; i < (long)w * h; i++) px[i] = vnc_dec_pix(d, p + i * pb);
    const uint8_t *m = p + (long)w * h * pb;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) mascara[j * w + i] = (m[j * fila + i / 8] >> (7 - (i & 7))) & 1;
    return (int)total;
}

/* ------------------------------------------------------------ rectángulo */

/* Empieza un rectángulo nuevo. */
static inline void vnc_dec_empezar(vnc_dec_t *d) { d->prog = 0; d->hbg = d->hfg = 0; }

/* Descodifica (una parte de) un rectángulo con la codificación enc. */
static inline int vnc_dec_rect(vnc_dec_t *d, int enc, int x, int y, int w, int h, const uint8_t *p, int n, bool *hecho)
{
    *hecho = false;
    if (!vnc_dec_dentro(d, x, y, w, h)) return VNC_ERR_FUERA;
    if (w == 0 || h == 0) { *hecho = true; return 0; }
    switch (enc) {
    case VNC_ENC_RAW:      return vnc_dec_raw(d, x, y, w, h, p, n, hecho);
    case VNC_ENC_COPYRECT: return vnc_dec_copyrect(d, x, y, w, h, p, n, hecho);
    case VNC_ENC_HEXTILE:  return vnc_dec_hextile(d, x, y, w, h, p, n, hecho);
    case VNC_ENC_ZRLE:     return vnc_dec_zrle(d, x, y, w, h, p, n, hecho);
    case VNC_ENC_TIGHT:    return vnc_dec_tight(d, x, y, w, h, p, n, hecho);
    default:               return VNC_ERR_ENC;
    }
}

/* ------------------------------------------------------------ teclas */

/* Código de tecla de SaguiOS (Latin-1 o TECLA_*) -> keysym de X11; 0 = sin traducción.
 * Es la inversa de vnc_tecla (vnc_codec.h): Latin-1 y Unicode coinciden con el keysym. */
static inline uint32_t vnc_keysym(unsigned t)
{
    if (t >= 0x20 && t <= 0x7E) return t;
    if (t >= 0xA0 && t <= 0xFF) return t;
    switch (t) {
    case '\b': return 0xFF08;
    case '\t': return 0xFF09;
    case '\n': case '\r': return 0xFF0D;
    case TECLA_ESC: return 0xFF1B;
    case TECLA_HOME: return 0xFF50;
    case TECLA_ESQUERDA: return 0xFF51;
    case TECLA_CIMA: return 0xFF52;
    case TECLA_DIREITA: return 0xFF53;
    case TECLA_BAIXO: return 0xFF54;
    case TECLA_PGUP: return 0xFF55;
    case TECLA_PGDN: return 0xFF56;
    case TECLA_END: return 0xFF57;
    case TECLA_INSERT: return 0xFF63;
    case TECLA_DELETE: return 0xFFFF;
    case 0x7F: return 0xFFFF;
    case TECLA_SUPER: return 0xFFEB;
    default: break;
    }
    if (t >= 0x10A && t <= 0x115 && t != TECLA_F11) return 0xFFBE + (t - 0x10A);   /* F1 a F12 (F11 es del sistema) */
    return 0;
}

#endif

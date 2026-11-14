// SPDX-License-Identifier: GPL-2.0-or-later
/* vnc_codec.h - la parte del servidor VNC (mejora 097, D252) que no toca el
 * sistema: formato de píxel del cliente, las codificaciones Raw, Hextile y
 * ZRLE (RFC 6143, 7.7), la traducción de teclas X11 (keysyms) a las de
 * SaguiOS y la detección de cambios por bloques. Se prueba en el anfitrión
 * (tests/vnc_unidad.c) y la usa userland/programas/vncd.c. Código propio. */
#ifndef VNC_CODEC_H
#define VNC_CODEC_H
#include "sagui.h"
#include "compressao.h"

/* ------------------------------------------------------------ formato de píxel */

typedef struct {
    uint8_t  bpp, profundidad, big, color_verdadero;
    uint16_t max[3];     /* rojo, verde, azul */
    uint8_t  desp[3];
    uint32_t tabla[3][256];   /* canal de 8 bits -> su parte del valor del píxel */
    int      cp;              /* bytes de un CPIXEL (ZRLE): 3 o bpp/8 */
    bool     cp_alto;         /* CPIXEL de 3 bytes: los tres más significativos del valor */
} vnc_pf_t;

/* Formato propio de SaguiOS: 32 bpp, 0x00RRGGBB, little endian. */
static inline void vnc_pf_nativo(vnc_pf_t *pf)
{
    memset(pf, 0, sizeof *pf);
    pf->bpp = 32; pf->profundidad = 24; pf->big = 0; pf->color_verdadero = 1;
    pf->max[0] = pf->max[1] = pf->max[2] = 255;
    pf->desp[0] = 16; pf->desp[1] = 8; pf->desp[2] = 0;
}

/* Calcula las tablas y el CPIXEL; false si el formato no es admisible
 * (ni 8, 16 ni 32 bpp, o con paleta de colores). */
static inline bool vnc_pf_preparar(vnc_pf_t *pf)
{
    if (pf->bpp != 8 && pf->bpp != 16 && pf->bpp != 32) return false;
    if (!pf->color_verdadero) return false;
    uint32_t usados = 0;
    for (int c = 0; c < 3; c++) {
        if (pf->max[c] == 0 || pf->desp[c] > 31) return false;
        for (int i = 0; i < 256; i++) pf->tabla[c][i] = (((uint32_t)i * pf->max[c] + 127) / 255) << pf->desp[c];
        uint32_t m = (uint32_t)pf->max[c] << pf->desp[c];
        if (pf->bpp < 32 && (m >> pf->bpp)) return false;   /* los colores no caben en el píxel */
        usados |= m;
    }
    pf->cp = pf->bpp / 8;
    pf->cp_alto = false;
    if (pf->bpp == 32 && pf->profundidad <= 24) {
        if (!(usados & 0xFF000000u)) pf->cp = 3;
        else if (!(usados & 0x000000FFu)) { pf->cp = 3; pf->cp_alto = true; }
    }
    return true;
}

static inline uint32_t vnc_valor(const vnc_pf_t *pf, uint32_t rgb)
{
    return pf->tabla[0][(rgb >> 16) & 255] | pf->tabla[1][(rgb >> 8) & 255] | pf->tabla[2][rgb & 255];
}

static inline uint8_t *vnc_poner_pix(const vnc_pf_t *pf, uint8_t *o, uint32_t v)
{
    int n = pf->bpp / 8;
    if (pf->big) for (int i = 0; i < n; i++) *o++ = (uint8_t)(v >> (8 * (n - 1 - i)));
    else for (int i = 0; i < n; i++) *o++ = (uint8_t)(v >> (8 * i));
    return o;
}

static inline uint8_t *vnc_poner_cpix(const vnc_pf_t *pf, uint8_t *o, uint32_t v)
{
    if (pf->cp != 3) return vnc_poner_pix(pf, o, v);
    if (pf->cp_alto) v >>= 8;
    v &= 0xFFFFFF;
    if (pf->big) { *o++ = (uint8_t)(v >> 16); *o++ = (uint8_t)(v >> 8); *o++ = (uint8_t)v; }
    else { *o++ = (uint8_t)v; *o++ = (uint8_t)(v >> 8); *o++ = (uint8_t)(v >> 16); }
    return o;
}

/* Cota superior del tamaño codificado de un rectángulo de w x h píxeles, con cualquier codificación. */
static inline int vnc_limite(int w, int h) { return w * h * 4 + ((w + 15) / 16 + 1) * ((h + 15) / 16 + 1) * 8 + ((w + 63) / 64 + 1) * ((h + 63) / 64 + 1) * 8 + 64; }

/* Convierte un rectángulo 0x00RRGGBB (con paso en píxeles) a valores del formato del cliente. */
static inline void vnc_convertir(const vnc_pf_t *pf, const uint32_t *ent, int paso, int w, int h, uint32_t *sal)
{
    for (int y = 0; y < h; y++) {
        const uint32_t *f = ent + (long)y * paso;
        uint32_t *s = sal + (long)y * w;
        for (int x = 0; x < w; x++) s[x] = vnc_valor(pf, f[x]);
    }
}

/* ------------------------------------------------------------ Raw (0) */

static inline int vnc_raw(const vnc_pf_t *pf, const uint32_t *v, int w, int h, uint8_t *sal)
{
    uint8_t *o = sal;
    for (long i = 0; i < (long)w * h; i++) o = vnc_poner_pix(pf, o, v[i]);
    return (int)(o - sal);
}

/* ------------------------------------------------------------ Hextile (5) */

static inline int vnc_hextile(const vnc_pf_t *pf, const uint32_t *v, int w, int h, uint8_t *sal)
{
    uint8_t *o = sal;
    uint32_t ult_fondo = 0;
    bool fondo_valido = false;
    int pb = pf->bpp / 8;
    for (int ty = 0; ty < h; ty += 16) for (int tx = 0; tx < w; tx += 16) {
        int tw = w - tx < 16 ? w - tx : 16, th = h - ty < 16 ? h - ty : 16;
        const uint32_t *t = v + (long)ty * w + tx;
        uint32_t col[32]; int cnt[32], nc = 0;
        bool muchos = false;
        for (int y = 0; y < th && !muchos; y++) for (int x = 0; x < tw; x++) {
            uint32_t p = t[(long)y * w + x];
            int k = 0;
            while (k < nc && col[k] != p) k++;
            if (k == nc) { if (nc == 32) { muchos = true; break; } col[nc] = p; cnt[nc++] = 0; }
            cnt[k]++;
        }
        int raw_bytes = tw * th * pb;
        if (!muchos) {
            int mejor = 0;
            for (int k = 1; k < nc; k++) if (cnt[k] > cnt[mejor]) mejor = k;
            uint32_t fondo = col[mejor];
            uint8_t sub[256 * 6];   /* (color, xy, wh) por subrectángulo, con color de hasta 4 bytes */
            uint8_t *so = sub;
            int nsub = 0;
            uint8_t hecho[16][16];
            memset(hecho, 0, sizeof hecho);
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
                uint32_t p = t[(long)y * w + x];
                if (p == fondo || hecho[y][x]) continue;
                int rw = 1;
                while (x + rw < tw && t[(long)y * w + x + rw] == p && !hecho[y][x + rw]) rw++;
                int rh = 1;
                for (;;) {
                    if (y + rh >= th) break;
                    bool ok = true;
                    for (int k = 0; k < rw; k++) if (t[(long)(y + rh) * w + x + k] != p || hecho[y + rh][x + k]) { ok = false; break; }
                    if (!ok) break;
                    rh++;
                }
                for (int yy = 0; yy < rh; yy++) for (int xx = 0; xx < rw; xx++) hecho[y + yy][x + xx] = 1;
                if (nsub < 255) {
                    so = vnc_poner_pix(pf, so, p);
                    *so++ = (uint8_t)(x << 4 | y);
                    *so++ = (uint8_t)((rw - 1) << 4 | (rh - 1));
                }
                nsub++;
            }
            bool poner_fondo = !fondo_valido || fondo != ult_fondo;
            int bytes = 1 + (poner_fondo ? pb : 0) + (nsub ? 1 + (int)(so - sub) : 0);
            if (nsub <= 255 && bytes < 1 + raw_bytes) {
                uint8_t fl = (uint8_t)((poner_fondo ? 2 : 0) | (nsub ? 8 | 16 : 0));
                *o++ = fl;
                if (poner_fondo) o = vnc_poner_pix(pf, o, fondo);
                if (nsub) { *o++ = (uint8_t)nsub; memcpy(o, sub, (size_t)(so - sub)); o += so - sub; }
                ult_fondo = fondo; fondo_valido = true;
                continue;
            }
        }
        *o++ = 1;   /* Raw */
        for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) o = vnc_poner_pix(pf, o, t[(long)y * w + x]);
        fondo_valido = false;
    }
    return (int)(o - sal);
}

/* ------------------------------------------------------------ ZRLE (16) */

/* Una longitud de racha (>= 1): 255 repetidos y el resto, todo menos uno. */
static inline uint8_t *zrle_longitud(uint8_t *o, int len)
{
    len -= 1;
    while (len >= 255) { *o++ = 255; len -= 255; }
    *o++ = (uint8_t)len;
    return o;
}

static inline int zrle_bytes_racha(int len) { return 1 + (len - 1) / 255; }

/* Los datos de ZRLE de un rectángulo (sin comprimir ni longitud): baldosas de 64 x 64. */
static inline int vnc_zrle_datos(const vnc_pf_t *pf, const uint32_t *v, int w, int h, uint8_t *sal)
{
    uint8_t *o = sal;
    int cp = pf->cp;
    static uint32_t tile[64 * 64];
    for (int ty = 0; ty < h; ty += 64) for (int tx = 0; tx < w; tx += 64) {
        int tw = w - tx < 64 ? w - tx : 64, th = h - ty < 64 ? h - ty : 64, n = tw * th;
        for (int y = 0; y < th; y++) memcpy(tile + y * tw, v + (long)(ty + y) * w + tx, (size_t)tw * 4);
        /* paleta (hasta 16) y rachas */
        uint32_t pal[17]; int np = 0; bool pal_ok = true;
        long rle_llano = 0, rle_pal = 0;
        int nrachas = 0;
        for (int i = 0; i < n;) {
            int j = i + 1;
            while (j < n && tile[j] == tile[i]) j++;
            int len = j - i;
            rle_llano += cp + zrle_bytes_racha(len);
            rle_pal += len == 1 ? 1 : 1 + zrle_bytes_racha(len);
            nrachas++;
            if (pal_ok) {
                int k = 0;
                while (k < np && pal[k] != tile[i]) k++;
                if (k == np) { if (np == 16) pal_ok = false; else pal[np++] = tile[i]; }
            }
            i = j;
        }
        if (pal_ok && np == 1) {
            *o++ = 1;
            o = vnc_poner_cpix(pf, o, pal[0]);
            continue;
        }
        long b_raw = 1 + (long)n * cp, mejor = b_raw;
        int modo = 0;   /* 0 raw, 1 paleta empaquetada, 2 RLE llano, 3 RLE con paleta */
        int bits = np <= 2 ? 1 : np <= 4 ? 2 : 4;
        if (pal_ok) {
            long b_emp = 1 + (long)np * cp + (long)th * ((tw * bits + 7) / 8);
            long b_rp = 1 + (long)np * cp + rle_pal;
            if (b_emp < mejor) { mejor = b_emp; modo = 1; }
            if (b_rp < mejor) { mejor = b_rp; modo = 3; }
        }
        if (1 + rle_llano < mejor) { mejor = 1 + rle_llano; modo = 2; }
        if (modo == 0) {
            *o++ = 0;
            for (int i = 0; i < n; i++) o = vnc_poner_cpix(pf, o, tile[i]);
        } else if (modo == 1) {
            *o++ = (uint8_t)np;
            for (int k = 0; k < np; k++) o = vnc_poner_cpix(pf, o, pal[k]);
            for (int y = 0; y < th; y++) {
                uint32_t acum = 0; int nb = 0;
                for (int x = 0; x < tw; x++) {
                    int k = 0;
                    while (pal[k] != tile[y * tw + x]) k++;
                    acum = acum << bits | (uint32_t)k;
                    nb += bits;
                    if (nb == 8) { *o++ = (uint8_t)acum; acum = 0; nb = 0; }
                }
                if (nb) *o++ = (uint8_t)(acum << (8 - nb));
            }
        } else if (modo == 2) {
            *o++ = 128;
            for (int i = 0; i < n;) {
                int j = i + 1;
                while (j < n && tile[j] == tile[i]) j++;
                o = vnc_poner_cpix(pf, o, tile[i]);
                o = zrle_longitud(o, j - i);
                i = j;
            }
        } else {
            *o++ = (uint8_t)(128 + np);
            for (int k = 0; k < np; k++) o = vnc_poner_cpix(pf, o, pal[k]);
            for (int i = 0; i < n;) {
                int j = i + 1;
                while (j < n && tile[j] == tile[i]) j++;
                int k = 0;
                while (pal[k] != tile[i]) k++;
                if (j - i == 1) *o++ = (uint8_t)k;
                else { *o++ = (uint8_t)(k | 128); o = zrle_longitud(o, j - i); }
                i = j;
            }
        }
        (void)nrachas;
    }
    return (int)(o - sal);
}

/* ------------------------------------------------------------ teclas */

/* Keysym de X11 -> código de tecla de SaguiOS (Latin-1 o TECLA_*); 0 = ignorar.
 * Los modificadores (Shift, Ctrl, Alt) no se traducen: van en el campo de modificadores. */
static inline int vnc_tecla(uint32_t ks)
{
    if (ks >= 0x20 && ks <= 0x7E) return (int)ks;
    if (ks >= 0xA0 && ks <= 0xFF) return (int)ks;
    if (ks >= 0x01000000u && ks <= 0x010000FFu) return (int)(ks & 0xFF);   /* Unicode directo, solo lo que cabe en Latin-1 */
    if (ks >= 0xFFB0 && ks <= 0xFFB9) return (int)('0' + (ks - 0xFFB0));    /* teclado numérico */
    switch (ks) {
    case 0xFF08: return '\b';
    case 0xFF09: return '\t';
    case 0xFF0D: case 0xFF8D: return '\n';
    case 0xFF1B: return TECLA_ESC;
    case 0xFF50: return TECLA_HOME;
    case 0xFF51: return TECLA_ESQUERDA;
    case 0xFF52: return TECLA_CIMA;
    case 0xFF53: return TECLA_DIREITA;
    case 0xFF54: return TECLA_BAIXO;
    case 0xFF55: return TECLA_PGUP;
    case 0xFF56: return TECLA_PGDN;
    case 0xFF57: return TECLA_END;
    case 0xFF63: return TECLA_INSERT;
    case 0xFFFF: return TECLA_DELETE;
    case 0xFFAA: return '*'; case 0xFFAB: return '+'; case 0xFFAD: return '-';
    case 0xFFAE: return '.'; case 0xFFAF: return '/';
    case 0xFFEB: case 0xFFEC: return TECLA_SUPER;
    default: break;
    }
    if (ks >= 0xFFBE && ks <= 0xFFC9) return 0x10A + (int)(ks - 0xFFBE);   /* F1-F12 */
    return 0;
}

/* ¿Es un modificador? Devuelve el INY_MOD_* (0 si no lo es). */
static inline int vnc_modificador(uint32_t ks)
{
    if (ks == 0xFFE1 || ks == 0xFFE2) return INY_MOD_SHIFT;
    if (ks == 0xFFE3 || ks == 0xFFE4) return INY_MOD_CTRL;
    if (ks == 0xFFE9 || ks == 0xFFEA) return INY_MOD_ALT;   /* Alt (sin AltGr: el kernel no tiene teclas muertas) */
    return 0;
}

/* ------------------------------------------------------------ cambios por bloques */

#define VNC_BLOQUE 16

/* Suma de comprobación de un bloque de 16 x 16 (o menos en el borde) de una imagen 0x00RRGGBB. */
static inline uint32_t vnc_hash_bloque(const uint32_t *img, int ancho, int bx, int by, int alto)
{
    int x0 = bx * VNC_BLOQUE, y0 = by * VNC_BLOQUE;
    int bw = ancho - x0 < VNC_BLOQUE ? ancho - x0 : VNC_BLOQUE, bh = alto - y0 < VNC_BLOQUE ? alto - y0 : VNC_BLOQUE;
    uint32_t h = 2166136261u;
    for (int y = 0; y < bh; y++) {
        const uint32_t *f = img + (long)(y0 + y) * ancho + x0;
        for (int x = 0; x < bw; x++) h = (h ^ f[x]) * 16777619u;
        h ^= h >> 15;
    }
    return h;
}

typedef struct { int x, y, w, h; } vnc_rect_t;

/* Agrupa los bloques sucios (sucio[by * bw + bx] != 0) en rectángulos: rachas
 * horizontales fundidas con las filas siguientes que tengan la misma racha. Si
 * salen más de "max", devuelve uno solo con el contorno de todo. Los rectángulos
 * se recortan a la pantalla (ancho x alto). "usado" es un búfer de trabajo de bw * bh bytes. */
static inline int vnc_agrupar(const uint8_t *sucio, uint8_t *usado, int bw, int bh, int ancho, int alto, vnc_rect_t *r, int max)
{
    int n = 0;
    memset(usado, 0, (size_t)bw * bh);
    int x0 = bw, y0 = bh, x1 = -1, y1 = -1;
    bool exceso = false;
    for (int by = 0; by < bh; by++) for (int bx = 0; bx < bw;) {
        if (!sucio[by * bw + bx] || usado[by * bw + bx]) { bx++; continue; }
        int e = bx;
        while (e < bw && sucio[by * bw + e] && !usado[by * bw + e]) e++;
        int a = 1;
        for (;;) {
            if (by + a >= bh) break;
            bool igual = true;
            for (int k = bx; k < e; k++) if (!sucio[(by + a) * bw + k] || usado[(by + a) * bw + k]) { igual = false; break; }
            if (!igual) break;
            a++;
        }
        for (int yy = 0; yy < a; yy++) for (int k = bx; k < e; k++) usado[(by + yy) * bw + k] = 1;
        if (bx < x0) x0 = bx;
        if (by < y0) y0 = by;
        if (e > x1) x1 = e;
        if (by + a > y1) y1 = by + a;
        if (n < max) {
            r[n].x = bx * VNC_BLOQUE; r[n].y = by * VNC_BLOQUE;
            r[n].w = (e - bx) * VNC_BLOQUE; r[n].h = a * VNC_BLOQUE;
            if (r[n].x + r[n].w > ancho) r[n].w = ancho - r[n].x;
            if (r[n].y + r[n].h > alto) r[n].h = alto - r[n].y;
            n++;
        } else exceso = true;
        bx = e;
    }
    if (exceso) {
        r[0].x = x0 * VNC_BLOQUE; r[0].y = y0 * VNC_BLOQUE;
        r[0].w = x1 * VNC_BLOQUE - r[0].x; r[0].h = y1 * VNC_BLOQUE - r[0].y;
        if (r[0].x + r[0].w > ancho) r[0].w = ancho - r[0].x;
        if (r[0].y + r[0].h > alto) r[0].h = alto - r[0].y;
        return 1;
    }
    return n;
}

#endif

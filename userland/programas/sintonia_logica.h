// SPDX-License-Identifier: GPL-2.0-or-later
/* sintonia_logica.h - la lógica pura de Sintonía, el reproductor de música (D225).
 *
 * Todo lo que no toca la pantalla, el sonido ni la red vive aquí, en funciones `static` de un solo
 * archivo de cabecera, para poder compilarlo en el PC con -DSINTONIA_ANFITRION y probarlo sin QEMU
 * (tests/sintonia_unidad.c):
 *
 *   - FFT de 256 puntos en enteros (el userland no usa la FPU: -mgeneral-regs-only) y su paso a barras
 *     de nivel 0..100 para el espectro;
 *   - lectura de listas M3U (con #EXTINF) y PLS, y su escritura;
 *   - lectura de metadatos de un archivo a partir de sus primeros bytes: ID3v2 (2.2, 2.3 y 2.4), ID3v1,
 *     comentarios Vorbis de FLAC, cabecera WAV; duración de un MP3 por su cabecera Xing/Info o por el
 *     tamaño (tasa constante). Así la lista se rellena sin abrir el decodificador, que solo admite un
 *     archivo abierto a la vez;
 *   - el filtro de los metadatos ICY de la radio (StreamTitle) y el corte de un flujo MP3 en cuadros
 *     enteros, para guardarlo en trozos que el decodificador de archivos pueda abrir;
 *   - el tema (colores y disposición) y el estado guardado, en texto «clave=valor»;
 *   - el orden aleatorio y la búsqueda sin distinguir mayúsculas.
 *
 * Los textos salen en ISO-8859-1 (lo que espera el terminal y el resto del sistema). */
#ifndef SINTONIA_LOGICA_H
#define SINTONIA_LOGICA_H
#ifdef SINTONIA_ANFITRION
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#else
#include "sagui.h"
#endif

#define SL_USADA __attribute__((unused))

/* ------------------------------------------------------------ textos */

static SL_USADA unsigned sl_len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }

/* Copia como mucho max-1 bytes y termina en cero. */
static SL_USADA void sl_copiar(char *dst, unsigned max, const char *src)
{
    unsigned i = 0;
    if (!max) return;
    for (; src[i] && i + 1 < max; i++) dst[i] = src[i];
    dst[i] = 0;
}

static SL_USADA void sl_juntar(char *dst, unsigned max, const char *src)
{
    unsigned n = sl_len(dst);
    if (n + 1 < max) sl_copiar(dst + n, max - n, src);
}

static SL_USADA int sl_cmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static SL_USADA int sl_cmp_n(const char *a, const char *b, unsigned n)
{
    for (; n; n--, a++, b++) { if (*a != *b) return (unsigned char)*a - (unsigned char)*b; if (!*a) break; }
    return 0;
}

static SL_USADA int sl_min(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* Igualdad sin distinguir mayúsculas (solo ASCII) de los n primeros bytes de a con b (b completo). */
static SL_USADA bool sl_prefijo_ci(const char *a, const char *b)
{
    while (*b) { if (sl_min((unsigned char)*a) != sl_min((unsigned char)*b)) return false; a++; b++; }
    return true;
}

/* ¿aguja aparece en texto (sin distinguir mayúsculas)? Una aguja vacía siempre aparece. */
static SL_USADA bool sl_contiene(const char *texto, const char *aguja)
{
    if (!aguja[0]) return true;
    for (; *texto; texto++) if (sl_prefijo_ci(texto, aguja)) return true;
    return false;
}

static SL_USADA bool sl_es_url(const char *r) { return sl_prefijo_ci(r, "http://") || sl_prefijo_ci(r, "https://"); }

/* Entero con signo opcional; devuelve cuántos caracteres leyó (0 = no había número). */
static SL_USADA int sl_leer_int(const char *s, int *v)
{
    int i = 0, neg = 0, n = 0;
    if (s[0] == '-') { neg = 1; i++; }
    else if (s[0] == '+') i++;
    int ini = i;
    while (s[i] >= '0' && s[i] <= '9' && n < 100000000) { n = n * 10 + (s[i] - '0'); i++; }
    if (i == ini) return 0;
    *v = neg ? -n : n;
    return i;
}

/* Escribe n en decimal en dst; devuelve los caracteres escritos. */
static SL_USADA int sl_itoa(char *dst, unsigned max, int v)
{
    char t[12];
    int k = 0, neg = v < 0;
    unsigned u = neg ? (unsigned)(-(v + 1)) + 1u : (unsigned)v;
    do { t[k++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) t[k++] = '-';
    int o = 0;
    while (k && (unsigned)o + 1 < max) dst[o++] = t[--k];
    if (max) dst[o] = 0;
    return o;
}

/* "m:ss" o "h:mm:ss" (segundos < 0 = "--:--"). */
static SL_USADA void sl_tiempo(char *dst, unsigned max, int seg)
{
    if (seg < 0) { sl_copiar(dst, max, "--:--"); return; }
    char a[12];
    unsigned h = (unsigned)seg / 3600, m = (unsigned)seg / 60 % 60, s = (unsigned)seg % 60;
    dst[0] = 0;
    if (h) { sl_itoa(a, sizeof a, (int)h); sl_juntar(dst, max, a); sl_juntar(dst, max, ":"); if (m < 10) sl_juntar(dst, max, "0"); }
    sl_itoa(a, sizeof a, (int)m); sl_juntar(dst, max, a); sl_juntar(dst, max, ":");
    if (s < 10) sl_juntar(dst, max, "0");
    sl_itoa(a, sizeof a, (int)s); sl_juntar(dst, max, a);
}

/* ------------------------------------------------------------ FFT de 256 puntos en enteros */

#define SL_FFT_N 256
#define SL_BARRAS_MAX 32

/* Un cuarto de onda del seno en Q15: sin(2*pi*k/256) para k = 0..64. */
static const int16_t sl_seno_q[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793, 12539, 13279,
    14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731,
    24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571,
    30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767
};

/* sin(2*pi*i/256) en Q15, para cualquier i entero. */
static SL_USADA int sl_sen(int i)
{
    i &= 255;
    if (i <= 64) return sl_seno_q[i];
    if (i <= 128) return sl_seno_q[128 - i];
    if (i <= 192) return -sl_seno_q[i - 128];
    return -sl_seno_q[256 - i];
}
static SL_USADA int sl_cos(int i) { return sl_sen(i + 64); }

/* FFT en el sitio, con re[] cargado e im[] a cero. Cada etapa divide por 2: la salida es la FFT / 256, así
 * el módulo complejo nunca pasa de 32767 y los productos con el factor de giro (Q15) caben en 32 bits. */
static SL_USADA void sl_fft(int32_t *re, int32_t *im)
{
    for (int i = 1, j = 0; i < SL_FFT_N; i++) {
        int bit = SL_FFT_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            int32_t t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= SL_FFT_N; len <<= 1) {
        int mitad = len / 2, paso = SL_FFT_N / len;
        for (int i = 0; i < SL_FFT_N; i += len) {
            for (int k = 0; k < mitad; k++) {
                int wr = sl_cos(k * paso), wi = -sl_sen(k * paso);
                int a = i + k, b = a + mitad;
                int32_t tr = (re[b] * wr - im[b] * wi) >> 15;
                int32_t ti = (re[b] * wi + im[b] * wr) >> 15;
                re[b] = (re[a] - tr) >> 1; im[b] = (im[a] - ti) >> 1;
                re[a] = (re[a] + tr) >> 1; im[a] = (im[a] + ti) >> 1;
            }
        }
    }
}

/* log2 en octavos de octava: 8 * log2(v), con interpolación lineal dentro de la octava. v <= 0 -> 0. */
static SL_USADA int sl_log2x8(int32_t v)
{
    if (v <= 0) return 0;
    int e = 0;
    while ((v >> e) > 1) e++;
    int mant = e >= 3 ? (int)((v >> (e - 3)) & 7) : (int)((v << (3 - e)) & 7);
    return e * 8 + mant;
}

/* Del bloque de 256 muestras (mono, Q15 de 16 bits) saca nb barras de 0 a 100 (nb <= SL_BARRAS_MAX).
 * Ventana de Hann, módulo aproximado max + min/2 y reparto de los 127 compartimentos útiles en barras
 * cuyo ancho crece (aproximadamente logarítmico); cada barra toma el pico de los suyos. */
static SL_USADA void sl_espectro(const int16_t *mono, int nb, uint8_t *niveles)
{
    int32_t re[SL_FFT_N], im[SL_FFT_N];
    for (int n = 0; n < SL_FFT_N; n++) {
        int hann = (32767 - sl_cos(n)) / 2;   /* 0 .. 32767 */
        re[n] = (mono[n] * hann) >> 15;
        im[n] = 0;
    }
    sl_fft(re, im);
    int ini = 1;
    for (int i = 0; i < nb; i++) {
        int fin = 1 + (i + 1) * (i + 1) * (SL_FFT_N / 2 - 1) / (nb * nb);
        if (fin <= ini) fin = ini + 1;
        if (fin > SL_FFT_N / 2) fin = SL_FFT_N / 2;
        if (ini >= SL_FFT_N / 2) ini = SL_FFT_N / 2 - 1;
        int32_t pico = 0;
        for (int k = ini; k < fin; k++) {
            int32_t a = re[k] < 0 ? -re[k] : re[k], b = im[k] < 0 ? -im[k] : im[k];
            int32_t m = a > b ? a + b / 2 : b + a / 2;
            if (m > pico) pico = m;
        }
        /* una senoide a fondo de escala da ~8192 en su compartimento: 2^3 .. 2^13 -> 0 .. 100 */
        int nivel = (sl_log2x8(pico) - 24) * 100 / 80;
        niveles[i] = (uint8_t)(nivel < 0 ? 0 : nivel > 100 ? 100 : nivel);
        ini = fin;
    }
}

/* ------------------------------------------------------------ pistas y listas M3U / PLS */

#define SL_RUTA_MAX 200
typedef struct {
    char ruta[SL_RUTA_MAX];   /* archivo local o URL http(s):// de una emisora */
    char titulo[80];
    char artista[48];
    int  dur_s;               /* -1 = desconocida (o flujo sin fin) */
    uint8_t meta;             /* 1 = ya se leyeron las etiquetas del archivo */
} sl_pista_t;

static SL_USADA const char *sl_nombre_base(const char *r)
{
    const char *b = r;
    for (const char *p = r; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

/* El texto que se enseña en la lista: "artista - título", el título, o el nombre del archivo. */
static SL_USADA void sl_nombre_pista(const sl_pista_t *p, char *dst, unsigned max)
{
    if (p->titulo[0] && p->artista[0]) {
        sl_copiar(dst, max, p->artista);
        sl_juntar(dst, max, " - ");
        sl_juntar(dst, max, p->titulo);
    } else if (p->titulo[0]) sl_copiar(dst, max, p->titulo);
    else sl_copiar(dst, max, sl_nombre_base(p->ruta));
}

/* Extensión de lista: 1 = M3U/M3U8, 2 = PLS, 0 = no es una lista. */
static SL_USADA int sl_tipo_lista(const char *r)
{
    unsigned n = sl_len(r), p = n;
    while (p && r[p - 1] != '.' && r[p - 1] != '/') p--;
    if (!p || r[p - 1] != '.') return 0;
    const char *e = r + p;
    if (sl_prefijo_ci(e, "m3u") && (e[3] == 0 || (e[3] == '8' && e[4] == 0))) return 1;
    if (sl_prefijo_ci(e, "pls") && e[3] == 0) return 2;
    return 0;
}

/* Deja en dst la ruta de una entrada de lista: las relativas van a la carpeta base (con "/" al final),
 * las de Windows pasan sus "\" a "/". Las URL y las rutas absolutas se dejan como están. */
static SL_USADA void sl_resolver(char *dst, unsigned max, const char *base, const char *ruta)
{
    char t[SL_RUTA_MAX];
    if (!ruta[0]) { dst[0] = 0; return; }
    sl_copiar(t, sizeof t, ruta);
    if (!sl_es_url(t)) for (char *p = t; *p; p++) if (*p == '\\') *p = '/';
    if (sl_es_url(t) || t[0] == '/' || !base || !base[0]) { sl_copiar(dst, max, t); return; }
    const char *r = t;
    if (r[0] == '.' && r[1] == '/') r += 2;
    sl_copiar(dst, max, base);
    unsigned n = sl_len(dst);
    if (n && dst[n - 1] != '/') sl_juntar(dst, max, "/");
    sl_juntar(dst, max, r);
}

/* Siguiente línea de txt[*pos..n): la copia sin el fin de línea en lin (como mucho max-1 bytes). false al acabar. */
static SL_USADA bool sl_linea(const char *txt, unsigned n, unsigned *pos, char *lin, unsigned max)
{
    if (*pos >= n) return false;
    unsigned k = 0;
    while (*pos < n && txt[*pos] != '\n') {
        char c = txt[(*pos)++];
        if (c != '\r' && k + 1 < max) lin[k++] = c;
    }
    if (*pos < n) (*pos)++;
    while (k && (lin[k - 1] == ' ' || lin[k - 1] == '\t')) k--;
    lin[k] = 0;
    return true;
}

static SL_USADA void sl_pista_limpiar(sl_pista_t *p)
{
    memset(p, 0, sizeof *p);
    p->dur_s = -1;
}

/* Separa "Artista - Título" de un #EXTINF o de un Title= (si no hay " - ", todo va al título). */
static SL_USADA void sl_partir_titulo(sl_pista_t *p, const char *t)
{
    for (const char *s = t; *s; s++) {
        if (s[0] == ' ' && s[1] == '-' && s[2] == ' ' && s > t) {
            char a[sizeof p->artista];
            unsigned k = (unsigned)(s - t);
            if (k >= sizeof a) k = sizeof a - 1;
            memcpy(a, t, k);
            a[k] = 0;
            sl_copiar(p->artista, sizeof p->artista, a);
            sl_copiar(p->titulo, sizeof p->titulo, s + 3);
            return;
        }
    }
    sl_copiar(p->titulo, sizeof p->titulo, t);
}

/* Lee una lista M3U (clásica o extendida) o PLS, detectada por su contenido. Devuelve las pistas (hasta max). */
static SL_USADA int sl_lista_leer(const char *txt, unsigned n, const char *base, sl_pista_t *out, int max)
{
    unsigned pos = 0;
    char lin[360];
    if (n >= 3 && (uint8_t)txt[0] == 0xEF && (uint8_t)txt[1] == 0xBB && (uint8_t)txt[2] == 0xBF) pos = 3;   /* BOM de UTF-8 */
    unsigned p0 = pos;
    bool pls = false;
    while (sl_linea(txt, n, &pos, lin, sizeof lin)) {
        if (!lin[0]) continue;
        pls = sl_prefijo_ci(lin, "[playlist]");
        break;
    }
    pos = p0;
    int cuenta = 0;
    if (pls) {
        for (int i = 0; i < max; i++) sl_pista_limpiar(&out[i]);
        int mayor = 0;
        while (sl_linea(txt, n, &pos, lin, sizeof lin)) {
            int campo = 0;   /* 1 File, 2 Title, 3 Length */
            const char *r = lin;
            if (sl_prefijo_ci(r, "file")) { campo = 1; r += 4; }
            else if (sl_prefijo_ci(r, "title")) { campo = 2; r += 5; }
            else if (sl_prefijo_ci(r, "length")) { campo = 3; r += 6; }
            if (!campo) continue;
            int idx, k = sl_leer_int(r, &idx);
            if (!k || r[k] != '=' || idx < 1 || idx > max) continue;
            const char *v = r + k + 1;
            sl_pista_t *p = &out[idx - 1];
            if (campo == 1) { sl_resolver(p->ruta, sizeof p->ruta, base, v); if (idx > mayor) mayor = idx; }
            else if (campo == 2) sl_partir_titulo(p, v);
            else { int d; if (sl_leer_int(v, &d)) p->dur_s = d < 0 ? -1 : d; }
        }
        for (int i = 0; i < mayor; i++)
            if (out[i].ruta[0]) { if (cuenta != i) out[cuenta] = out[i]; cuenta++; }
        return cuenta;
    }
    int dur = -1;
    char tit[160];
    tit[0] = 0;
    while (sl_linea(txt, n, &pos, lin, sizeof lin) && cuenta < max) {
        if (!lin[0]) continue;
        if (lin[0] == '#') {
            if (sl_prefijo_ci(lin, "#EXTINF:")) {
                int d, k = sl_leer_int(lin + 8, &d);
                dur = k && d >= 0 ? d : -1;
                const char *c = lin + 8 + k;
                while (*c && *c != ',') c++;
                sl_copiar(tit, sizeof tit, *c == ',' ? c + 1 : "");
            }
            continue;
        }
        sl_pista_t *p = &out[cuenta++];
        sl_pista_limpiar(p);
        sl_resolver(p->ruta, sizeof p->ruta, base, lin);
        p->dur_s = dur;
        if (tit[0]) sl_partir_titulo(p, tit);
        dur = -1;
        tit[0] = 0;
    }
    return cuenta;
}

typedef int (*sl_emitir_t)(void *ctx, const char *s, unsigned n);

static SL_USADA int sl_emitir_texto(sl_emitir_t f, void *ctx, const char *s)
{
    return f(ctx, s, sl_len(s));
}

/* Escribe la lista por f: M3U extendido (pls = false) o PLS. Devuelve 0, o <0 si f falló. */
static SL_USADA int sl_lista_escribir(const sl_pista_t *pistas, int n, bool pls, sl_emitir_t f, void *ctx)
{
    char t[SL_RUTA_MAX + 160], num[12];
    int r = 0;
    r |= sl_emitir_texto(f, ctx, pls ? "[playlist]\n" : "#EXTM3U\n");
    for (int i = 0; i < n && r >= 0; i++) {
        const sl_pista_t *p = &pistas[i];
        char nom[160];
        sl_nombre_pista(p, nom, sizeof nom);
        if (pls) {
            sl_itoa(num, sizeof num, i + 1);
            sl_copiar(t, sizeof t, "File"); sl_juntar(t, sizeof t, num); sl_juntar(t, sizeof t, "="); sl_juntar(t, sizeof t, p->ruta); sl_juntar(t, sizeof t, "\n");
            r |= sl_emitir_texto(f, ctx, t);
            sl_copiar(t, sizeof t, "Title"); sl_juntar(t, sizeof t, num); sl_juntar(t, sizeof t, "="); sl_juntar(t, sizeof t, nom); sl_juntar(t, sizeof t, "\n");
            r |= sl_emitir_texto(f, ctx, t);
            sl_copiar(t, sizeof t, "Length"); sl_juntar(t, sizeof t, num); sl_juntar(t, sizeof t, "=");
            sl_itoa(num, sizeof num, p->dur_s); sl_juntar(t, sizeof t, num); sl_juntar(t, sizeof t, "\n");
            r |= sl_emitir_texto(f, ctx, t);
        } else {
            sl_copiar(t, sizeof t, "#EXTINF:");
            sl_itoa(num, sizeof num, p->dur_s); sl_juntar(t, sizeof t, num); sl_juntar(t, sizeof t, ",");
            sl_juntar(t, sizeof t, nom); sl_juntar(t, sizeof t, "\n");
            r |= sl_emitir_texto(f, ctx, t);
            sl_copiar(t, sizeof t, p->ruta); sl_juntar(t, sizeof t, "\n");
            r |= sl_emitir_texto(f, ctx, t);
        }
    }
    if (pls && r >= 0) {
        sl_copiar(t, sizeof t, "NumberOfEntries="); sl_itoa(num, sizeof num, n); sl_juntar(t, sizeof t, num);
        sl_juntar(t, sizeof t, "\nVersion=2\n");
        r |= sl_emitir_texto(f, ctx, t);
    }
    return r < 0 ? -1 : 0;
}

/* ------------------------------------------------------------ metadatos de archivos */

typedef struct {
    char titulo[80];
    char artista[48];
    char album[48];
    int  dur_s;      /* -1 = no se pudo saber */
    int  formato;    /* 0 desconocido, 1 MP3, 2 FLAC, 3 WAV, 4 MIDI */
} sl_meta_t;

static uint32_t sl_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t sl_le32(const uint8_t *p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static uint32_t sl_sync7(const uint8_t *p) { return (uint32_t)(p[0] & 127) << 21 | (uint32_t)(p[1] & 127) << 14 | (uint32_t)(p[2] & 127) << 7 | (p[3] & 127); }

/* UTF-8 -> Latin-1 (lo que no cabe, "?"); los bytes mal formados se copian tal cual. */
static SL_USADA void sl_utf8_latin1(char *dst, unsigned max, const uint8_t *s, unsigned n)
{
    unsigned o = 0;
    for (unsigned i = 0; i < n && s[i] && o + 1 < max;) {
        uint32_t c = s[i++];
        if (c >= 0xC0) {
            unsigned extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
            c &= c >= 0xF0 ? 7u : c >= 0xE0 ? 15u : 31u;
            while (extra-- && i < n && (s[i] & 0xC0) == 0x80) c = c << 6 | (s[i++] & 63u);
        }
        dst[o++] = c < 256 ? (char)c : '?';
    }
    while (o && (dst[o - 1] == ' ' || dst[o - 1] == '\r' || dst[o - 1] == '\n')) o--;
    dst[o] = 0;
}

/* El texto de un marco ID3v2: el primer byte es la codificación (0 Latin-1, 1 UTF-16 con BOM, 2 UTF-16 BE, 3 UTF-8). */
static SL_USADA void sl_texto_id3(char *dst, unsigned max, const uint8_t *d, unsigned n)
{
    dst[0] = 0;
    if (!n || !max) return;
    unsigned enc = d[0], o = 0;
    d++; n--;
    if (enc == 0) {
        for (unsigned i = 0; i < n && d[i] && o + 1 < max; i++) dst[o++] = (char)d[i];
    } else if (enc == 3) {
        sl_utf8_latin1(dst, max, d, n);
        return;
    } else {
        bool be = enc == 2;
        unsigned i = 0;
        if (enc == 1 && n >= 2) {
            if (d[0] == 0xFF && d[1] == 0xFE) { be = false; i = 2; }
            else if (d[0] == 0xFE && d[1] == 0xFF) { be = true; i = 2; }
        }
        for (; i + 1 < n && o + 1 < max; i += 2) {
            uint32_t u = be ? (uint32_t)d[i] << 8 | d[i + 1] : (uint32_t)d[i + 1] << 8 | d[i];
            if (!u) break;
            dst[o++] = u < 256 ? (char)u : '?';
        }
    }
    while (o && dst[o - 1] == ' ') o--;
    dst[o] = 0;
}

/* Bytes que ocupa la etiqueta ID3v2 al principio (con su cabecera), o 0 si no hay. */
static SL_USADA uint32_t sl_id3v2_tam(const uint8_t *b, unsigned n)
{
    if (n < 10 || b[0] != 'I' || b[1] != 'D' || b[2] != '3' || b[3] == 0xFF || b[3] < 2 || b[3] > 4) return 0;
    if ((b[6] | b[7] | b[8] | b[9]) & 0x80) return 0;
    return 10 + sl_sync7(b + 6) + ((b[3] == 4 && (b[5] & 0x10)) ? 10u : 0u);
}

/* Cabecera de un cuadro MP3 Layer III: devuelve su tamaño en bytes (0 = no es una cabecera válida). */
static SL_USADA unsigned sl_mp3_cuadro(const uint8_t *p, int *taxa, int *kbps, int *canales, int *muestras, int *version)
{
    static const uint16_t br1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
    static const uint8_t br2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
    static const uint16_t sr1[3] = { 44100, 48000, 32000 };
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return 0;
    int ver = (p[1] >> 3) & 3, capa = (p[1] >> 1) & 3;
    if (ver == 1 || capa != 1) return 0;   /* reservado, o no es Layer III */
    int bi = p[2] >> 4, si = (p[2] >> 2) & 3, pad = (p[2] >> 1) & 1;
    if (bi == 0 || bi == 15 || si == 3) return 0;
    bool v1 = ver == 3;
    int sr = v1 ? sr1[si] : (ver == 2 ? sr1[si] / 2 : sr1[si] / 4);
    int br = v1 ? br1[bi] : br2[bi];
    unsigned tam = (unsigned)((v1 ? 144 : 72) * br * 1000 / sr + pad);
    if (taxa) *taxa = sr;
    if (kbps) *kbps = br;
    if (canales) *canales = (p[3] >> 6) == 3 ? 1 : 2;
    if (muestras) *muestras = v1 ? 1152 : 576;
    if (version) *version = ver;
    return tam;
}

/* Duración en segundos de un MP3 a partir de su primer cuadro (b, n bytes: unos 200 bastan) y de los
 * bytes de audio que tiene el archivo: con cabecera Xing/Info cuenta los cuadros; si no, tasa constante.
 * -1 si b no empieza en un cuadro. */
static SL_USADA int sl_mp3_duracion(const uint8_t *b, unsigned n, uint32_t bytes_audio)
{
    int sr, kbps, can, spf, ver;
    if (n < 4 || !sl_mp3_cuadro(b, &sr, &kbps, &can, &spf, &ver)) return -1;
    unsigned si = ver == 3 ? (can == 1 ? 17u : 32u) : (can == 1 ? 9u : 17u);
    unsigned xo = 4 + si + ((b[1] & 1) ? 0u : 2u);   /* con CRC hay 2 bytes más tras la cabecera */
    if (xo + 12 <= n && (memcmp(b + xo, "Xing", 4) == 0 || memcmp(b + xo, "Info", 4) == 0) && (b[xo + 7] & 1)) {
        uint32_t cuadros = sl_be32(b + xo + 8);
        if (cuadros) return (int)(cuadros / (unsigned)sr * (unsigned)spf + (cuadros % (unsigned)sr) * (unsigned)spf / (unsigned)sr);
    }
    return (int)(bytes_audio / ((unsigned)kbps * 125u));
}

static void sl_meta_id3v2(sl_meta_t *m, const uint8_t *b, unsigned n, int *tlen_ms)
{
    uint32_t total = sl_id3v2_tam(b, n);
    if (!total) return;
    int ver = b[3];
    unsigned pos = 10, fin = total - (ver == 4 && (b[5] & 0x10) ? 10u : 0u);
    if (fin > n) fin = n;   /* solo lo que tenemos */
    if (b[5] & 0x40) {      /* cabecera extendida: se salta */
        if (pos + 4 > fin) return;
        pos += ver == 4 ? sl_sync7(b + pos) : sl_be32(b + pos) + 4;
    }
    unsigned cab = ver == 2 ? 6u : 10u;
    while (pos + cab <= fin) {
        const uint8_t *f = b + pos;
        if (!f[0]) break;   /* relleno */
        uint32_t tam = ver == 2 ? (uint32_t)f[3] << 16 | (uint32_t)f[4] << 8 | f[5] : ver == 4 ? sl_sync7(f + 4) : sl_be32(f + 4);
        if (pos + cab + tam > fin) break;
        const uint8_t *d = f + cab;
        char t[96];
        bool tit, art, alb, len;
        if (ver == 2) {
            tit = !memcmp(f, "TT2", 3); art = !memcmp(f, "TP1", 3); alb = !memcmp(f, "TAL", 3); len = !memcmp(f, "TLE", 3);
        } else {
            tit = !memcmp(f, "TIT2", 4); art = !memcmp(f, "TPE1", 4); alb = !memcmp(f, "TALB", 4); len = !memcmp(f, "TLEN", 4);
        }
        if (tit || art || alb || len) {
            sl_texto_id3(t, sizeof t, d, tam);
            if (tit) sl_copiar(m->titulo, sizeof m->titulo, t);
            else if (art) sl_copiar(m->artista, sizeof m->artista, t);
            else if (alb) sl_copiar(m->album, sizeof m->album, t);
            else { int v; if (sl_leer_int(t, &v) && v > 0) *tlen_ms = v; }
        }
        pos += cab + tam;
    }
}

static void sl_meta_id3v1(sl_meta_t *m, const uint8_t *c)
{
    if (c[0] != 'T' || c[1] != 'A' || c[2] != 'G') return;
    char t[32];
    for (int campo = 0; campo < 3; campo++) {
        const uint8_t *s = c + 3 + campo * 30;
        unsigned k = 0;
        while (k < 30 && s[k]) { t[k] = (char)s[k]; k++; }
        while (k && t[k - 1] == ' ') k--;
        t[k] = 0;
        if (!t[0]) continue;
        if (campo == 0 && !m->titulo[0]) sl_copiar(m->titulo, sizeof m->titulo, t);
        if (campo == 1 && !m->artista[0]) sl_copiar(m->artista, sizeof m->artista, t);
        if (campo == 2 && !m->album[0]) sl_copiar(m->album, sizeof m->album, t);
    }
}

/* Comentarios Vorbis (los de FLAC): vendor, cuenta y "CLAVE=valor" en UTF-8. */
static void sl_meta_vorbis(sl_meta_t *m, const uint8_t *d, unsigned n)
{
    if (n < 8) return;
    uint32_t v = sl_le32(d);
    if (v > n - 8) return;
    unsigned pos = 4 + v;
    uint32_t cuenta = sl_le32(d + pos);
    pos += 4;
    for (uint32_t i = 0; i < cuenta && pos + 4 <= n; i++) {
        uint32_t l = sl_le32(d + pos);
        pos += 4;
        if (l > n - pos) break;
        const uint8_t *c = d + pos;
        pos += l;
        unsigned k = 0;
        while (k < l && c[k] != '=') k++;
        if (k >= l) continue;
        char clave[12];
        if (k >= sizeof clave) continue;
        for (unsigned q = 0; q < k; q++) clave[q] = (char)sl_min(c[q]);
        clave[k] = 0;
        char *dst = 0;
        unsigned mx = 0;
        if (!sl_cmp(clave, "title")) { dst = m->titulo; mx = sizeof m->titulo; }
        else if (!sl_cmp(clave, "artist")) { dst = m->artista; mx = sizeof m->artista; }
        else if (!sl_cmp(clave, "album")) { dst = m->album; mx = sizeof m->album; }
        if (dst) sl_utf8_latin1(dst, mx, c + k + 1, l - k - 1);
    }
}

/* Rellena m con lo que se sepa de un archivo a partir de:
 *   cab (ncab bytes del principio; con 16 KB basta casi siempre),
 *   aud (bytes en el offset sl_id3v2_tam(cab): el primer cuadro MP3; puede ser NULL si cae dentro de cab),
 *   cola (los últimos 128 bytes, para el ID3v1; puede ser NULL),
 *   tam (tamaño del archivo).
 * Devuelve true si reconoció el formato. */
static SL_USADA bool sl_meta_leer(const uint8_t *cab, unsigned ncab, const uint8_t *aud, unsigned naud, const uint8_t *cola,
                                  uint32_t tam, sl_meta_t *m)
{
    memset(m, 0, sizeof *m);
    m->dur_s = -1;
    if (ncab < 12) return false;
    if (!memcmp(cab, "fLaC", 4)) {
        m->formato = 2;
        unsigned pos = 4;
        while (pos + 4 <= ncab) {
            unsigned tipo = cab[pos] & 0x7F, ultimo = cab[pos] >> 7;
            uint32_t l = (uint32_t)cab[pos + 1] << 16 | (uint32_t)cab[pos + 2] << 8 | cab[pos + 3];
            pos += 4;
            if (pos + l > ncab) break;
            if (tipo == 0 && l >= 18) {   /* STREAMINFO */
                const uint8_t *s = cab + pos;
                uint32_t taxa = (uint32_t)s[10] << 12 | (uint32_t)s[11] << 4 | (s[12] >> 4);
                uint32_t hi = s[13] & 15, lo = sl_be32(s + 14);   /* muestras totales: 36 bits */
                if (taxa && !hi) m->dur_s = (int)(lo / taxa);
                else if (taxa) m->dur_s = (int)((hi << 28) / taxa * 16u + lo / taxa);
            } else if (tipo == 4) sl_meta_vorbis(m, cab + pos, l);
            pos += l;
            if (ultimo) break;
        }
        return true;
    }
    if (!memcmp(cab, "RIFF", 4) && !memcmp(cab + 8, "WAVE", 4)) {
        m->formato = 3;
        unsigned pos = 12;
        uint32_t brate = 0;
        while (pos + 8 <= ncab) {
            uint32_t l = sl_le32(cab + pos + 4);
            if (!memcmp(cab + pos, "fmt ", 4) && pos + 8 + 12 <= ncab) brate = sl_le32(cab + pos + 16);
            else if (!memcmp(cab + pos, "data", 4)) {
                if (brate) m->dur_s = (int)(l / brate);
                break;
            }
            pos += 8 + l + (l & 1);
        }
        return true;
    }
    if (!memcmp(cab, "MThd", 4)) { m->formato = 4; return true; }
    uint32_t id3 = sl_id3v2_tam(cab, ncab);
    bool mp3 = id3 || (cab[0] == 0xFF && (cab[1] & 0xE0) == 0xE0);
    if (!mp3) return false;
    m->formato = 1;
    int tlen_ms = 0;
    if (id3) sl_meta_id3v2(m, cab, ncab, &tlen_ms);
    if (cola) sl_meta_id3v1(m, cola);
    uint32_t datos = tam > id3 ? tam - id3 : 0;
    if (cola && cola[0] == 'T' && cola[1] == 'A' && cola[2] == 'G' && datos >= 128) datos -= 128;
    if (!aud && id3 < ncab) { aud = cab + id3; naud = ncab - id3; }
    else if (!aud && !id3) { aud = cab; naud = ncab; }
    if (tlen_ms > 0) m->dur_s = (tlen_ms + 500) / 1000;
    else if (aud) m->dur_s = sl_mp3_duracion(aud, naud, datos);
    return true;
}

/* ------------------------------------------------------------ radio: ICY y trozos de MP3 */

/* Filtro de los metadatos ICY intercalados en el flujo: cada metaint bytes de audio viene un byte L y L*16
 * bytes de texto ("StreamTitle='Artista - Título';StreamUrl='...';"). */
typedef struct {
    unsigned metaint;      /* 0 = el servidor no manda metadatos */
    unsigned hasta_meta;   /* bytes de audio que faltan para el próximo bloque */
    unsigned falta_meta;   /* bytes del bloque de metadatos que faltan (0 = estamos en el audio) */
    bool     leyendo_long; /* el siguiente byte es la longitud */
    unsigned n;            /* bytes guardados en meta */
    char     meta[528];
    char     titulo[96];   /* último StreamTitle, en Latin-1 */
    bool     nuevo;        /* titulo cambió desde la última vez que se miró */
} sl_icy_t;

static SL_USADA void sl_icy_iniciar(sl_icy_t *s, unsigned metaint)
{
    memset(s, 0, sizeof *s);
    s->metaint = metaint;
    s->hasta_meta = metaint;
}

static void sl_icy_analizar(sl_icy_t *s)
{
    s->meta[s->n] = 0;
    const char *p = s->meta;
    for (; *p; p++) {
        if (!sl_cmp_n(p, "StreamTitle='", 13)) {
            const char *a = p + 13, *z = a;
            /* acaba en "';": el título puede llevar comillas simples dentro */
            while (*z && !(z[0] == '\'' && (z[1] == ';' || z[1] == 0))) z++;
            char t[sizeof s->titulo];
            unsigned k = (unsigned)(z - a);
            if (k >= sizeof t) k = sizeof t - 1;
            memcpy(t, a, k);
            t[k] = 0;
            char l1[sizeof s->titulo];
            sl_utf8_latin1(l1, sizeof l1, (const uint8_t *)t, k);
            if (sl_cmp(l1, s->titulo)) { sl_copiar(s->titulo, sizeof s->titulo, l1); s->nuevo = true; }
            return;
        }
    }
}

/* Copia a out (que debe caber n bytes) el audio de in[0..n) quitando los metadatos; devuelve los bytes de audio. */
static SL_USADA unsigned sl_icy_filtrar(sl_icy_t *s, const uint8_t *in, unsigned n, uint8_t *out)
{
    unsigned o = 0, i = 0;
    if (!s->metaint) { memcpy(out, in, n); return n; }
    while (i < n) {
        if (s->leyendo_long) {
            s->falta_meta = (unsigned)in[i++] * 16;
            s->leyendo_long = false;
            s->n = 0;
            if (!s->falta_meta) s->hasta_meta = s->metaint;
            continue;
        }
        if (s->falta_meta) {
            unsigned t = n - i < s->falta_meta ? n - i : s->falta_meta;
            for (unsigned k = 0; k < t; k++) if (s->n + 1 < sizeof s->meta) s->meta[s->n++] = (char)in[i + k];
            i += t;
            s->falta_meta -= t;
            if (!s->falta_meta) { sl_icy_analizar(s); s->hasta_meta = s->metaint; }
            continue;
        }
        unsigned t = n - i < s->hasta_meta ? n - i : s->hasta_meta;
        memcpy(out + o, in + i, t);
        o += t; i += t;
        s->hasta_meta -= t;
        if (!s->hasta_meta) s->leyendo_long = true;
    }
    return o;
}

/* Corta un trozo de flujo MP3 en cuadros enteros. b[0..n) son los bytes recibidos y alvo el tamaño que se
 * quiere como mínimo. Devuelve el final (exclusivo) del último cuadro completo una vez superado alvo, y en
 * *ini el principio del primer cuadro (los bytes anteriores son basura de antes de la sincronía); 0 = aún
 * no hay bastante. Un cuadro se da por bueno si el siguiente también tiene cabecera válida. */
static SL_USADA unsigned sl_mp3_cortar(const uint8_t *b, unsigned n, unsigned alvo, unsigned *ini)
{
    unsigned s = 0, len = 0;
    for (;; s++) {
        if (s + 4 > n) return 0;
        len = sl_mp3_cuadro(b + s, 0, 0, 0, 0, 0);
        if (!len) continue;
        if (s + len + 4 > n) return 0;   /* aún no se puede confirmar con el siguiente */
        if (sl_mp3_cuadro(b + s + len, 0, 0, 0, 0, 0)) break;
    }
    *ini = s;
    unsigned p = s;
    while (p + 4 <= n) {
        len = sl_mp3_cuadro(b + p, 0, 0, 0, 0, 0);
        if (!len) return p > s ? p : 0;   /* datos rotos: se corta aquí y el resto se resincroniza */
        if (p + len > n) return 0;
        p += len;
        if (p - s >= alvo) return p;
    }
    return 0;
}

/* ------------------------------------------------------------ tema y estado (texto "clave=valor") */

typedef struct {
    uint32_t fondo, panel, panel2, texto, texto_fraco, acento, barras, picos, osciloscopio, seleccion, lista_fondo;
    int  visualizador;   /* 0 barras, 1 osciloscopio, 2 las dos */
    int  botones_arriba; /* 1 = la fila de botones va encima de la barra de posición */
    int  lista;          /* 1 = lista visible al abrir */
    int  eq;             /* 1 = ecualizador visible al abrir */
} sl_tema_t;

/* El tema de fábrica: panel oscuro con acento verde. */
static SL_USADA void sl_tema_defecto(sl_tema_t *t)
{
    t->fondo = 0xDDE1E5; t->panel = 0x0E1A14; t->panel2 = 0x1B2B22; t->texto = 0x1E2328; t->texto_fraco = 0x5A646E;
    t->acento = 0x2FD070; t->barras = 0x39E07F; t->picos = 0xF2E85A; t->osciloscopio = 0x7DF0AE;
    t->seleccion = 0x2A6FD8; t->lista_fondo = 0xFFFFFF;
    t->visualizador = 0; t->botones_arriba = 0; t->lista = 1; t->eq = 0;
}

/* Parte "clave=valor" de una línea (sin espacios alrededor, claves en minúscula). Comentarios con '#' o ';'
 * al principio. true si la línea tiene una clave. */
static SL_USADA bool sl_clave_valor(const char *lin, char *clave, unsigned mc, char *valor, unsigned mv)
{
    while (*lin == ' ' || *lin == '\t') lin++;
    if (!*lin || *lin == '#' || *lin == ';') return false;
    unsigned k = 0;
    while (lin[k] && lin[k] != '=') k++;
    if (!lin[k]) return false;
    unsigned a = 0, z = k;
    while (z > 0 && (lin[z - 1] == ' ' || lin[z - 1] == '\t')) z--;
    if (z - a + 1 > mc) return false;
    for (unsigned i = a; i < z; i++) clave[i - a] = (char)sl_min((unsigned char)lin[i]);
    clave[z - a] = 0;
    const char *v = lin + k + 1;
    while (*v == ' ' || *v == '\t') v++;
    sl_copiar(valor, mv, v);
    unsigned l = sl_len(valor);
    while (l && (valor[l - 1] == ' ' || valor[l - 1] == '\t')) valor[--l] = 0;
    return true;
}

/* "#RRGGBB", "0xRRGGBB" o "RRGGBB". */
static SL_USADA bool sl_color(const char *s, uint32_t *c)
{
    if (s[0] == '#') s++;
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t v = 0;
    int n = 0;
    for (; *s; s++, n++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0 || n >= 6) return false;
        v = v << 4 | (uint32_t)d;
    }
    if (n != 6) return false;
    *c = v;
    return true;
}

/* Aplica una línea del archivo de tema. Devuelve true si la entendió. */
static SL_USADA bool sl_tema_linea(sl_tema_t *t, const char *lin)
{
    char k[24], v[48];
    if (!sl_clave_valor(lin, k, sizeof k, v, sizeof v)) return false;
    uint32_t c;
    struct { const char *n; uint32_t *p; } cols[] = {
        { "fondo", &t->fondo }, { "panel", &t->panel }, { "panel2", &t->panel2 }, { "texto", &t->texto },
        { "texto_fraco", &t->texto_fraco }, { "acento", &t->acento }, { "barras", &t->barras }, { "picos", &t->picos },
        { "osciloscopio", &t->osciloscopio }, { "seleccion", &t->seleccion }, { "lista_fondo", &t->lista_fondo },
    };
    for (unsigned i = 0; i < sizeof cols / sizeof cols[0]; i++)
        if (!sl_cmp(k, cols[i].n)) { if (!sl_color(v, &c)) return false; *cols[i].p = c; return true; }
    if (!sl_cmp(k, "visualizador")) {
        if (!sl_cmp(v, "barras")) t->visualizador = 0;
        else if (!sl_cmp(v, "osciloscopio")) t->visualizador = 1;
        else if (!sl_cmp(v, "ambos")) t->visualizador = 2;
        else return false;
        return true;
    }
    int b = !sl_cmp(v, "si") || !sl_cmp(v, "1") || !sl_cmp(v, "arriba"), n = !sl_cmp(v, "no") || !sl_cmp(v, "0") || !sl_cmp(v, "abajo");
    if (!b && !n) return false;
    if (!sl_cmp(k, "botones")) t->botones_arriba = b;
    else if (!sl_cmp(k, "lista")) t->lista = b;
    else if (!sl_cmp(k, "eq")) t->eq = b;
    else return false;
    return true;
}

/* Lee un tema entero (varias líneas); devuelve cuántas líneas entendió. */
static SL_USADA int sl_tema_leer(sl_tema_t *t, const char *txt, unsigned n)
{
    unsigned pos = 0;
    char lin[96];
    int ok = 0;
    while (sl_linea(txt, n, &pos, lin, sizeof lin)) ok += sl_tema_linea(t, lin);
    return ok;
}

/* ------------------------------------------------------------ orden aleatorio */

static SL_USADA uint32_t sl_xorshift(uint32_t *e)
{
    uint32_t x = *e ? *e : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *e = x;
}

/* Permutación aleatoria de 0..n-1 (Fisher-Yates). */
static SL_USADA void sl_orden_aleatorio(int *orden, int n, uint32_t *semilla)
{
    for (int i = 0; i < n; i++) orden[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(sl_xorshift(semilla) % (uint32_t)(i + 1));
        int t = orden[i]; orden[i] = orden[j]; orden[j] = t;
    }
}

#endif

// SPDX-License-Identifier: GPL-2.0-or-later
/* terminal_logica.h - la lógica pura del Terminal en ventana (D231).
 *
 * Lo que no toca la pantalla, el kernel ni los procesos vive aquí, en funciones `static` de un solo
 * archivo de cabecera, para compilarlo en el PC con -DTERMINAL_ANFITRION y probarlo sin QEMU
 * (tests/terminal_unidad.c):
 *
 *   - el analizador de secuencias de escape de la salida (ESC [ ... letra y ESC ] ... BEL);
 *   - los atributos de una celda (color de letra y de fondo ANSI, negrita, inverso) y SGR (ESC [ n m);
 *   - los temas de colores propios, la paleta ANSI de 16 colores y el archivo terminal.conf;
 *   - la búsqueda de texto sin distinguir mayúsculas, el reconocimiento de un enlace http(s) bajo
 *     el ratón y el título de una pestaña a partir del indicador (prompt) del shell.
 *
 * Los textos son ISO-8859-1, como en el resto del sistema. */
#ifndef TERMINAL_LOGICA_H
#define TERMINAL_LOGICA_H
#ifdef TERMINAL_ANFITRION
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define tl_formato snprintf
#else
#include "sagui.h"
#define tl_formato formatar_texto
#endif

#define TL_USADA __attribute__((unused))

/* ------------------------------------------------------------ textos */

static TL_USADA unsigned tl_len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }

static TL_USADA void tl_copiar(char *dst, unsigned max, const char *src)
{
    unsigned i = 0;
    if (!max) return;
    for (; src[i] && i + 1 < max; i++) dst[i] = src[i];
    dst[i] = 0;
}

static TL_USADA int tl_minuscula(int c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;   /* Latin-1 con acentos */
    return c;
}

static TL_USADA bool tl_comeca(const char *s, const char *p)
{
    while (*p) { if (*s++ != *p++) return false; }
    return true;
}

/* ------------------------------------------------------------ secuencias de escape */

#define TL_PAR 16
#define TL_ESC_NAO   0   /* un carácter normal: hay que imprimirlo */
#define TL_ESC_COMER 1   /* consumido por la secuencia */
#define TL_ESC_CSI   2   /* secuencia CSI completa: final, par[0..n-1], privado */
#define TL_ESC_OSC   3   /* secuencia OSC completa: el texto está en osc[] (D263: título de la ventana) */
#define TL_ESC_ESC   4   /* D320: ESC + (intermedio) + final completa: inter (0 si no hay) y final (ESC 7, ESC M, ESC ( 0...) */
#define TL_OSC_MAX   80

typedef struct {
    int  estado;          /* 0 normal, 1 tras ESC, 2 parámetros CSI, 3 OSC, 4 ESC dentro de OSC, 5 ESC + intermedio */
    int  par[TL_PAR];
    int  n;               /* parámetros recibidos (0 si no hubo ninguno) */
    bool tem;             /* el parámetro actual ya tiene algún dígito */
    bool privado;         /* ESC [ ? ... */
    char final;
    char marca;           /* D320: «<», «=», «>» o «?» tras «ESC [» (0 si no hay) */
    char inter;           /* D320: último byte intermedio (0x20-0x2F: «!», «$», espacio...) */
    char osc_tipo;        /* D320: «]» OSC (se entrega); P X ^ _ (DCS, SOS, PM, APC): se descartan */
    char osc[TL_OSC_MAX];  /* texto de la OSC en curso («0;título») */
    int  osc_n;
} tl_esc_t;

static TL_USADA int tl_esc_alimentar(tl_esc_t *e, unsigned char c)
{
    switch (e->estado) {
    case 0:
        if (c == 27) { e->estado = 1; return TL_ESC_COMER; }
        return TL_ESC_NAO;
    case 1:
        if (c == '[') {
            e->estado = 2; e->n = 0; e->tem = false; e->privado = false; e->final = 0; e->marca = 0; e->inter = 0;
            for (int i = 0; i < TL_PAR; i++) e->par[i] = 0;
        } else if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
            e->estado = 3; e->osc_n = 0; e->osc[0] = 0; e->osc_tipo = (char)c;
        } else if (c >= 0x20 && c <= 0x2F) { e->estado = 5; e->inter = (char)c; }
        else if (c == 27) e->estado = 1;
        else if (c >= 0x30 && c <= 0x7E) { e->estado = 0; e->inter = 0; e->final = (char)c; return TL_ESC_ESC; }
        else e->estado = 0;
        return TL_ESC_COMER;
    case 5:   /* ESC ( 0, ESC # 8...: un último byte */
        if (c == 27) { e->estado = 1; return TL_ESC_COMER; }
        e->estado = 0;
        if (c < 0x20) return TL_ESC_COMER;
        e->final = (char)c;
        return TL_ESC_ESC;
    case 2:
        if (c >= '0' && c <= '9') {
            if (e->n == 0) e->n = 1;
            if (e->n <= TL_PAR) {
                int *p = &e->par[e->n - 1];
                if (*p < 10000) *p = *p * 10 + (c - '0');
            }
            e->tem = true;
        } else if (c == ';' || c == ':') {   /* «38:5:n» se lee como «38;5;n» */
            if (e->n == 0) e->n = 1;
            if (e->n < TL_PAR) e->n++;
            e->tem = false;
        } else if (c >= '<' && c <= '?') { e->marca = (char)c; e->privado = c == '?'; }
        else if (c >= 0x20 && c <= 0x2F) e->inter = (char)c;
        else if (c >= 0x40 && c <= 0x7E) {
            e->estado = 0;
            e->final = (char)c;
            return TL_ESC_CSI;
        } else if (c == 27) e->estado = 1;   /* secuencia cortada por otra */
        else if (c == 0x18 || c == 0x1A) e->estado = 0;   /* CAN y SUB la cancelan */
        /* lo demás (otros controles): se ignora */
        return TL_ESC_COMER;
    case 3:   /* OSC: hasta BEL o ESC \ ; se guarda el texto (el título de la ventana, D263) */
        if (c == 7) { e->estado = 0; return e->osc_tipo == ']' ? TL_ESC_OSC : TL_ESC_COMER; }
        if (c == 27) e->estado = 4;
        else if (e->osc_n < TL_OSC_MAX - 1) { e->osc[e->osc_n++] = (char)c; e->osc[e->osc_n] = 0; }
        return TL_ESC_COMER;
    default:
        if (c == '\\') { e->estado = 0; return e->osc_tipo == ']' ? TL_ESC_OSC : TL_ESC_COMER; }
        e->estado = 3;
        return TL_ESC_COMER;
    }
}

/* Parámetro i de la secuencia, o def si falta o es 0 (como en las terminales). */
static TL_USADA int tl_par(const tl_esc_t *e, int i, int def)
{
    if (i >= e->n || i >= TL_PAR || e->par[i] == 0) return def;
    return e->par[i];
}

/* ------------------------------------------------------------ atributos de celda */

/* 16 bits: bits 0-4 color de letra (0 = el de la tema, 1..16 = ANSI 0..15), bits 5-9 el de fondo,
 * bit 10 negrita, bit 11 inverso, y (D320) bit 12 tenue, 13 subrayado, 14 oculto y 15 «dibujo de línea DEC»
 * (la letra guardada es la del juego ESC ( 0: «q» = línea horizontal...). Cero es el atributo por defecto:
 * la memoria a cero ya vale. */
#define TL_AT_NEGRITA (1u << 10)
#define TL_AT_INVERSO (1u << 11)
#define TL_AT_TENUE   (1u << 12)
#define TL_AT_SUBRAYA (1u << 13)
#define TL_AT_OCULTO  (1u << 14)
#define TL_AT_DEC     (1u << 15)
#define TL_AT_FONDO   (31u << 5)
#define TL_AT_FG(a)   ((a) & 31u)
#define TL_AT_BG(a)   (((a) >> 5) & 31u)

static TL_USADA uint16_t tl_sgr(uint16_t at, int p)
{
    if (p == 0) return (uint16_t)(at & TL_AT_DEC);   /* el «dibujo DEC» es del juego de caracteres, no del SGR */
    if (p == 1) return (uint16_t)(at | TL_AT_NEGRITA);
    if (p == 2) return (uint16_t)(at | TL_AT_TENUE);
    if (p == 22) return (uint16_t)(at & ~(TL_AT_NEGRITA | TL_AT_TENUE));
    if (p == 4) return (uint16_t)(at | TL_AT_SUBRAYA);
    if (p == 24) return (uint16_t)(at & ~TL_AT_SUBRAYA);
    if (p == 8) return (uint16_t)(at | TL_AT_OCULTO);
    if (p == 28) return (uint16_t)(at & ~TL_AT_OCULTO);
    if (p == 7) return (uint16_t)(at | TL_AT_INVERSO);
    if (p == 27) return (uint16_t)(at & ~TL_AT_INVERSO);
    if (p >= 30 && p <= 37) return (uint16_t)((at & ~31u) | (unsigned)(p - 30 + 1));
    if (p == 39) return (uint16_t)(at & ~31u);
    if (p >= 40 && p <= 47) return (uint16_t)((at & ~(31u << 5)) | (unsigned)(p - 40 + 1) << 5);
    if (p == 49) return (uint16_t)(at & ~(31u << 5));
    if (p >= 90 && p <= 97) return (uint16_t)((at & ~31u) | (unsigned)(p - 90 + 9));
    if (p >= 100 && p <= 107) return (uint16_t)((at & ~(31u << 5)) | (unsigned)(p - 100 + 9) << 5);
    return at;
}

/* D263: un color de la tabla de 256 (xterm) o RGB al más parecido de los 16 ANSI (índice 0..15). */
static TL_USADA int tl_rgb_a_16(int r, int g, int b)   /* componentes 0..5 */
{
    int m = r > g ? r : g;
    if (b > m) m = b;
    if (m == 0) return 0;
    if (m <= 1) return 8;
    int idx = (r * 2 >= m ? 1 : 0) | (g * 2 >= m ? 2 : 0) | (b * 2 >= m ? 4 : 0);
    if (idx == 7 && m <= 2) return 8;
    return idx + (m >= 4 ? 8 : 0);
}

static TL_USADA int tl_256_a_16(int n)
{
    if (n < 0) n = 0;
    if (n < 16) return n;
    if (n >= 232) { int v = n - 232; return v < 6 ? 0 : v < 12 ? 8 : v < 18 ? 7 : 15; }
    n -= 16;
    return tl_rgb_a_16(n / 36, (n / 6) % 6, n % 6);
}

/* Aplica ESC [ ... m entera. 38/48 con ;5;n (256 colores) y ;2;r;g;b (RGB) se llevan a la paleta de 16. */
static TL_USADA uint16_t tl_sgr_todo(uint16_t at, const tl_esc_t *e)
{
    if (e->n == 0) return (uint16_t)(at & TL_AT_DEC);
    for (int i = 0; i < e->n && i < TL_PAR; i++) {
        int p = e->par[i];
        if (p == 38 || p == 48) {
            int modo = i + 1 < e->n ? e->par[i + 1] : 0;
            int c = -1;
            if (modo == 5 && i + 2 < e->n) c = tl_256_a_16(e->par[i + 2]);
            else if (modo == 2 && i + 4 < e->n)
                c = tl_rgb_a_16((e->par[i + 2] * 5 + 127) / 255, (e->par[i + 3] * 5 + 127) / 255, (e->par[i + 4] * 5 + 127) / 255);
            if (c >= 0) at = p == 38 ? (uint16_t)((at & ~31u) | (unsigned)(c + 1)) : (uint16_t)((at & ~(31u << 5)) | (unsigned)(c + 1) << 5);
            i += modo == 5 ? 2 : modo == 2 ? 4 : 1;
            continue;
        }
        at = tl_sgr(at, p);
    }
    return at;
}

/* ------------------------------------------------------------ temas y configuración */

#define TL_TEMAS 6
#define TL_PERSONALIZADO TL_TEMAS   /* índice del «tema» con los colores propios del usuario */

typedef struct {
    const char *id, *nome;
    uint32_t fundo, letra, cursor, selecao;
    int      base;     /* 0 paleta ANSI para fondos oscuros, 1 para fondos claros */
    uint32_t tinte;    /* distinto de 0: una sola tinta (fósforo verde, ámbar) */
} tl_tema_t;

/* Temas propios de SaguiOS (ninguno copia una paleta ajena). */
static const TL_USADA tl_tema_t tl_temas[TL_TEMAS] = {
    { "oscuro",  "Oscuro",         0x12161E, 0xD8DDE2, 0xD8DDE2, 0x6D9BD8, 0, 0 },
    { "claro",   "Claro",          0xF8F8F4, 0x1E2328, 0x1E2328, 0xA9C8F0, 1, 0 },
    { "verde",   "Verde fósforo",  0x050D05, 0x3DFF6A, 0x3DFF6A, 0x1C7A35, 0, 0x3DFF6A },
    { "ambar",   "Ámbar",          0x120A00, 0xFFB000, 0xFFB000, 0x8A5A00, 0, 0xFFB000 },
    { "mar",     "Mar",            0x0F2A33, 0xCFE8E8, 0xF2C94C, 0x2F6F7F, 0, 0 },
    { "papel",   "Papel",          0xF1E8D0, 0x3B3228, 0x3B3228, 0xD6C28F, 1, 0 },
};

static const TL_USADA uint32_t tl_ansi_escuro[16] = {
    0x2B303B, 0xE06C6C, 0x7FC77F, 0xE5C07B, 0x6C9EE0, 0xC586C0, 0x56B6C2, 0xC8CCD4,
    0x5C6370, 0xFF8E8E, 0xA0E8A0, 0xF5D98B, 0x8DBBFF, 0xE0A8DC, 0x7ED8E4, 0xFFFFFF,
};
static const TL_USADA uint32_t tl_ansi_claro[16] = {
    0x1E2328, 0xB3261E, 0x1E7B34, 0x8A6500, 0x1F5FBF, 0x8E3FA0, 0x0C7A86, 0x6A737C,
    0x4A5560, 0xD9453B, 0x2E9E4A, 0xA67C00, 0x3C80E0, 0xA85BBA, 0x1E97A5, 0x3A424A,
};

#define TL_COLS_MIN 20
#define TL_COLS_MAX 400
#define TL_LINS_MIN 5
#define TL_LINS_MAX 150
#define TL_HIST_MAX 5000
#define TL_PX_MIN 10
#define TL_PX_MAX 48
#define TL_INTER_MAX 8
#define TL_INTER_PADRAO 2   /* D385: 2 px de aire entre filas (la letra interna es de 16: filas de 18) */

enum { TL_CUR_BLOCO, TL_CUR_SUBLINHADO, TL_CUR_BARRA, TL_CUR_QTD };
enum { TL_CAMP_NADA, TL_CAMP_VISUAL, TL_CAMP_SOM, TL_CAMP_QTD };

typedef struct {
    int      tema;              /* 0..TL_TEMAS-1, o TL_PERSONALIZADO */
    uint32_t fundo, letra, cursor_cor, selecao, ansi[16];
    char     fonte[96];         /* ruta de una fuente vectorial; vacía = la interna 8x16 */
    int      fpx;               /* tamaño de la fuente vectorial, en píxeles */
    int      cols, lins;        /* tamaño inicial */
    int      hist;              /* líneas de historial */
    int      cursor, parpadeo, campana;
    int      barra_siempre;     /* 1 = la barra de pestañas también con una sola */
    int      inter;             /* D385: interlineado, píxeles de más entre filas (0..TL_INTER_MAX) */
} tl_conf_t;

static TL_USADA unsigned tl_luma(uint32_t c)
{
    return (((c >> 16) & 255) * 30 + ((c >> 8) & 255) * 59 + (c & 255) * 11) / 100;
}

/* La paleta ANSI de un tema: las 16 de base, o su tinta modulada por la luminosidad. */
static TL_USADA void tl_paleta_do_tema(const tl_tema_t *t, uint32_t *ansi)
{
    const uint32_t *base = t->base ? tl_ansi_claro : tl_ansi_escuro;
    for (int i = 0; i < 16; i++) {
        if (!t->tinte) { ansi[i] = base[i]; continue; }
        unsigned f = 70 + tl_luma(base[i]) * 185 / 255;   /* 70..255 */
        unsigned r = ((t->tinte >> 16) & 255) * f / 255, g = ((t->tinte >> 8) & 255) * f / 255, b = (t->tinte & 255) * f / 255;
        ansi[i] = r << 16 | g << 8 | b;
    }
}

static TL_USADA void tl_conf_tema(tl_conf_t *c, int tema)
{
    if (tema < 0 || tema >= TL_TEMAS) return;
    const tl_tema_t *t = &tl_temas[tema];
    c->tema = tema;
    c->fundo = t->fundo; c->letra = t->letra; c->cursor_cor = t->cursor; c->selecao = t->selecao;
    tl_paleta_do_tema(t, c->ansi);
}

static TL_USADA void tl_conf_padrao(tl_conf_t *c)
{
    memset(c, 0, sizeof *c);
    tl_conf_tema(c, 0);
    c->fpx = 16; c->cols = 94; c->lins = 28; c->hist = 500;
    c->cursor = TL_CUR_BLOCO; c->parpadeo = 0; c->campana = TL_CAMP_VISUAL;
    c->inter = TL_INTER_PADRAO;
}

static const TL_USADA char *const tl_nomes_cursor[TL_CUR_QTD] = { "bloque", "subrayado", "barra" };
static const TL_USADA char *const tl_nomes_campana[TL_CAMP_QTD] = { "ninguna", "visual", "sonora" };

static TL_USADA int tl_hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "RRGGBB" o "#RRGGBB". */
static TL_USADA bool tl_cor_de_texto(const char *s, uint32_t *cor)
{
    if (*s == '#') s++;
    uint32_t v = 0;
    int n = 0;
    for (; tl_hex(*s) >= 0; s++, n++) v = v << 4 | (uint32_t)tl_hex(*s);
    while (*s == ' ' || *s == '\r') s++;
    if (n != 6 || (*s && *s != '\n')) return false;
    *cor = v;
    return true;
}

static TL_USADA int tl_num(const char *s, int min, int max, int def)
{
    if (*s < '0' || *s > '9') return def;
    int v = 0;
    while (*s >= '0' && *s <= '9') { if (v < 100000) v = v * 10 + (*s - '0'); s++; }
    return v < min ? min : v > max ? max : v;
}

static TL_USADA int tl_indice_nome(const char *s, const char *const *nomes, int n, int def)
{
    for (int i = 0; i < n; i++) {
        unsigned k = tl_len(nomes[i]);
        if (tl_comeca(s, nomes[i]) && (s[k] == 0 || s[k] == '\n' || s[k] == '\r' || s[k] == ' ')) return i;
    }
    return def;
}

/* Lee el contenido de terminal.conf (texto «clave=valor», '#' comenta). Lo desconocido se ignora. */
static TL_USADA void tl_conf_ler(tl_conf_t *c, const char *texto)
{
    /* primero el tema (las demás claves de color solo cuentan con tema=personalizado) */
    bool personalizado = false;
    for (const char *p = texto; *p;) {
        if (tl_comeca(p, "tema=")) {
            const char *v = p + 5;
            if (tl_comeca(v, "personalizado")) personalizado = true;
            for (int i = 0; i < TL_TEMAS; i++) {
                unsigned k = tl_len(tl_temas[i].id);
                if (tl_comeca(v, tl_temas[i].id) && (v[k] == 0 || v[k] == '\n' || v[k] == '\r')) tl_conf_tema(c, i);
            }
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    if (personalizado) c->tema = TL_PERSONALIZADO;
    for (const char *p = texto; *p;) {
        uint32_t cor;
        if (personalizado && tl_comeca(p, "fondo=") && tl_cor_de_texto(p + 6, &cor)) c->fundo = cor;
        else if (personalizado && tl_comeca(p, "letra=") && tl_cor_de_texto(p + 6, &cor)) c->letra = cor;
        else if (personalizado && tl_comeca(p, "cursor_color=") && tl_cor_de_texto(p + 13, &cor)) c->cursor_cor = cor;
        else if (personalizado && tl_comeca(p, "seleccion=") && tl_cor_de_texto(p + 10, &cor)) c->selecao = cor;
        else if (personalizado && tl_comeca(p, "ansi") && p[4] >= '0' && p[4] <= '9') {
            int i = p[4] - '0', k = 5;
            if (p[5] >= '0' && p[5] <= '9') { i = i * 10 + (p[5] - '0'); k = 6; }
            if (p[k] == '=' && i < 16 && tl_cor_de_texto(p + k + 1, &cor)) c->ansi[i] = cor;
        }
        else if (tl_comeca(p, "fuente=")) {
            unsigned n = 0;
            const char *v = p + 7;
            while (v[n] && v[n] != '\n' && v[n] != '\r') n++;
            if (n >= sizeof c->fonte) n = sizeof c->fonte - 1;
            memcpy(c->fonte, v, n);
            c->fonte[n] = 0;
        }
        else if (tl_comeca(p, "tamano=")) c->fpx = tl_num(p + 7, TL_PX_MIN, TL_PX_MAX, c->fpx);
        else if (tl_comeca(p, "columnas=")) c->cols = tl_num(p + 9, TL_COLS_MIN, TL_COLS_MAX, c->cols);
        else if (tl_comeca(p, "filas=")) c->lins = tl_num(p + 6, TL_LINS_MIN, TL_LINS_MAX, c->lins);
        else if (tl_comeca(p, "historial=")) c->hist = tl_num(p + 10, 0, TL_HIST_MAX, c->hist);
        else if (tl_comeca(p, "cursor=")) c->cursor = tl_indice_nome(p + 7, tl_nomes_cursor, TL_CUR_QTD, c->cursor);
        else if (tl_comeca(p, "parpadeo=")) c->parpadeo = tl_comeca(p + 9, "si") ? 1 : 0;
        else if (tl_comeca(p, "campana=")) c->campana = tl_indice_nome(p + 8, tl_nomes_campana, TL_CAMP_QTD, c->campana);
        else if (tl_comeca(p, "barra=")) c->barra_siempre = tl_comeca(p + 6, "siempre") ? 1 : 0;
        else if (tl_comeca(p, "interlineado=")) c->inter = tl_num(p + 13, 0, TL_INTER_MAX, c->inter);
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

/* Escribe la configuración como texto. Devuelve los bytes. */
static TL_USADA int tl_conf_escrever(const tl_conf_t *c, char *d, int max)
{
    int n = 0;
#define TL_ADD(...) do { if (n < max) { int k_ = tl_formato(d + n, (unsigned)(max - n), __VA_ARGS__); if (k_ > 0) n += k_; } } while (0)
    TL_ADD("# Preferencias del Terminal (se escribe desde Ctrl+, o Preferencias)\n");
    TL_ADD("tema=%s\n", c->tema == TL_PERSONALIZADO ? "personalizado" : tl_temas[c->tema].id);
    if (c->tema == TL_PERSONALIZADO) {
        /* D385: «x» minúscula: el formatar_texto de SaguiOS no tiene «X» (salía «%X» y se perdían los colores) */
        TL_ADD("fondo=%06x\nletra=%06x\ncursor_color=%06x\nseleccion=%06x\n",
               (unsigned)c->fundo, (unsigned)c->letra, (unsigned)c->cursor_cor, (unsigned)c->selecao);
        for (int i = 0; i < 16; i++) TL_ADD("ansi%d=%06x\n", i, (unsigned)c->ansi[i]);
    }
    TL_ADD("fuente=%s\ntamano=%d\ncolumnas=%d\nfilas=%d\nhistorial=%d\n", c->fonte, c->fpx, c->cols, c->lins, c->hist);
    TL_ADD("cursor=%s\nparpadeo=%s\ncampana=%s\nbarra=%s\n", tl_nomes_cursor[c->cursor], c->parpadeo ? "si" : "no",
           tl_nomes_campana[c->campana], c->barra_siempre ? "siempre" : "auto");
    TL_ADD("interlineado=%d\n", c->inter);
#undef TL_ADD
    if (n >= max) n = max - 1;
    return n;
}

/* Mezcla a con b: pct % de b. */
static TL_USADA uint32_t tl_misturar(uint32_t a, uint32_t b, int pct)
{
    unsigned r = (((a >> 16) & 255) * (unsigned)(100 - pct) + ((b >> 16) & 255) * (unsigned)pct) / 100;
    unsigned g = (((a >> 8) & 255) * (unsigned)(100 - pct) + ((b >> 8) & 255) * (unsigned)pct) / 100;
    unsigned bl = ((a & 255) * (unsigned)(100 - pct) + (b & 255) * (unsigned)pct) / 100;
    return r << 16 | g << 8 | bl;
}

/* Colores de una celda: (letra, fondo) a partir del atributo y la tema. fundo_def = 0xFFFFFFFF
 * en la salida significa «el fondo de la tema» (no hace falta pintarlo aparte). */
static TL_USADA void tl_cores_da_celula(const tl_conf_t *c, uint16_t at, uint32_t *fg, uint32_t *bg)
{
    unsigned f = TL_AT_FG(at), b = TL_AT_BG(at);
    uint32_t cf = c->letra, cb = c->fundo;
    if (f) { unsigned i = f - 1; if ((at & TL_AT_NEGRITA) && i < 8) i += 8; cf = c->ansi[i & 15]; }
    else if (at & TL_AT_NEGRITA) cf = tl_misturar(c->letra, 0xFFFFFF, 35);
    if (b) cb = c->ansi[(b - 1) & 15];
    if (at & TL_AT_INVERSO) { uint32_t t = cf; cf = cb; cb = t; }
    if (at & TL_AT_TENUE) cf = tl_misturar(cf, cb, 45);
    if (at & TL_AT_OCULTO) cf = cb;
    *fg = cf; *bg = cb;
}

/* ------------------------------------------------------------ búsqueda, enlaces, títulos */

/* Primera aparición de `aguja` (largo n) en s[desde..largo-1] sin distinguir mayúsculas; -1 si no. */
static TL_USADA int tl_achar(const char *s, int largo, const char *aguja, int n, int desde)
{
    if (n <= 0) return -1;
    for (int i = desde < 0 ? 0 : desde; i + n <= largo; i++) {
        int k = 0;
        while (k < n && tl_minuscula((unsigned char)s[i + k]) == tl_minuscula((unsigned char)aguja[k])) k++;
        if (k == n) return i;
    }
    return -1;
}

/* Última aparición cuyo inicio es < antes (para buscar hacia atrás); -1 si no. */
static TL_USADA int tl_achar_atras(const char *s, int largo, const char *aguja, int n, int antes)
{
    int ultima = -1, d = 0;
    for (;;) {
        int i = tl_achar(s, largo, aguja, n, d);
        if (i < 0 || i >= antes) break;
        ultima = i;
        d = i + 1;
    }
    return ultima;
}

static TL_USADA bool tl_char_url(unsigned char c)
{
    return c > ' ' && c < 0x7F && c != '<' && c != '>' && c != '"' && c != '\'' && c != '`' && c != '(' && c != ')';
}

/* Enlace http:// o https:// que pasa por la columna c de la línea (largo caracteres).
 * Copia la URL en out y devuelve su largo, o 0 si no hay. */
static TL_USADA int tl_url_en(const char *s, int largo, int c, char *out, int max)
{
    if (c < 0 || c >= largo || !tl_char_url((unsigned char)s[c])) return 0;
    int a = c, b = c;
    while (a > 0 && tl_char_url((unsigned char)s[a - 1])) a--;
    while (b + 1 < largo && tl_char_url((unsigned char)s[b + 1])) b++;
    while (b > a && (s[b] == '.' || s[b] == ',' || s[b] == ';' || s[b] == ':' || s[b] == '!' || s[b] == '?')) b--;
    int n = b - a + 1;
    bool http = n > 7 && tl_comeca(s + a, "http://"), https = n > 8 && tl_comeca(s + a, "https://");
    if (!http && !https) return 0;
    if (n >= max) return 0;
    memcpy(out, s + a, (size_t)n);
    out[n] = 0;
    return n;
}

/* Título de la pestaña a partir de una línea con el indicador del shell ("sagui:/home$ ls"): el último
 * componente de la carpeta ("home"; "/" para la raíz). Devuelve false si la línea no tiene ese aspecto. */
static TL_USADA bool tl_titulo_do_prompt(const char *s, int largo, char *out, int max)
{
    int d = 0;
    while (d < largo && d < 32 && s[d] != ':' && s[d] != ' ') d++;
    if (d >= largo || s[d] != ':') return false;
    int a = d + 1, b = a;
    while (b < largo && s[b] != '$' && s[b] != '#' && s[b] != ' ') b++;
    if (b >= largo || (s[b] != '$' && s[b] != '#') || b == a) return false;
    if (b + 1 < largo && s[b + 1] != ' ') return false;
    int k = b;
    while (k > a + 1 && s[k - 1] == '/') k--;   /* sin barra final */
    int ini = k;
    while (ini > a && s[ini - 1] != '/') ini--;
    int n = k - ini;
    if (n <= 0) { tl_copiar(out, (unsigned)max, "/"); return true; }
    if (n >= max) n = max - 1;
    memcpy(out, s + ini, (size_t)n);
    out[n] = 0;
    return true;
}

#endif

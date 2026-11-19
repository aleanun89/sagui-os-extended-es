// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu3d_diag.h - diagnóstico del 3D común a todos los drivers (D390; generaliza lo que la D386
 * hizo para la R5xx del Amilo).
 *
 * Tres piezas, todas lógica pura (sin registros ni memoria de vídeo): la capa
 * (kernel/gui/gpu3d.c) las usa con cualquier driver y se prueban en el PC
 * (tests/gpu3d_diag_unidad.c, dentro de h287 unidad):
 *   - sentinelas: antes de cada etapa de la autoprueba la CPU llena el color del
 *     destino con G3D_SENTINELA_COR y el Z con G3D_SENTINELA_Z; si después de la
 *     etapa siguen ahí, la GPU no escribió;
 *   - el análisis del destino (g3d_analisar): cuántos píxeles siguen con la
 *     sentinela, los colores más frecuentes, cuántos Z siguen intactos o están al
 *     máximo (el valor de «limpiar»);
 *   - los ajustes de registros de /sistema/conf/gpu3d-<driver>.conf (solo root):
 *     «inicio REG VALOR» (al estado fijo) o «dibujo REG VALOR» (con el estado de
 *     cada dibujo), en hexadecimal, solo dentro de los rangos que declara cada
 *     driver (nunca el MC, los relojes, la energía ni la pantalla). Las líneas con
 *     «#» o vacías se ignoran; una línea mala o fuera de los rangos, también (y se
 *     cuenta).
 * Las vistas (g3d_vista_t) son lo que el driver deja ver a la CPU del destino: el
 * color y el Z tal como están en la memoria, con su paso y su formato. */
#ifndef SAGUI_GPU3D_DIAG_H
#define SAGUI_GPU3D_DIAG_H
#include "tipos.h"

#define G3D_SENTINELA_COR 0x00A1B2C3u    /* lo que pone la CPU en el color antes de cada etapa */
#define G3D_SENTINELA_Z   0x5A5A5A5Au    /* y en el Z (con Z de 16 bits, 0x5A5A) */

/* ------------------------------------------------------------ vistas del destino */

typedef struct {
    volatile uint8_t *cpu;     /* NULL: la CPU no lo ve (p. ej. un Z en mosaico) */
    uint32_t passo;            /* bytes por fila */
    int      larg, alt;
    uint8_t  bpp;              /* 32 o 16 */
    uint8_t  zbits;            /* Z: 24 o 16 (0 en el color) */
    uint8_t  zdesloc;          /* Z: primer bit del Z en la palabra (R5xx: 8, el Z24 en los bits altos; Intel: 0) */
    bool     cache;            /* la CPU lo ve con caché: hay que vaciarla tras escribir y antes de leer */
} g3d_vista_t;

static inline uint32_t g3d_vista_ler(const g3d_vista_t *v, int x, int y)
{
    volatile uint8_t *p = v->cpu + (uint32_t)y * v->passo;
    return v->bpp == 16 ? ((volatile uint16_t *)p)[x] : ((volatile uint32_t *)p)[x];
}

static inline void g3d_vista_encher(const g3d_vista_t *v, uint32_t valor)
{
    for (int y = 0; y < v->alt; y++) {
        volatile uint8_t *p = v->cpu + (uint32_t)y * v->passo;
        for (int x = 0; x < v->larg; x++) {
            if (v->bpp == 16) ((volatile uint16_t *)p)[x] = (uint16_t)valor;
            else ((volatile uint32_t *)p)[x] = valor;
        }
    }
}

/* El valor crudo del Z con la sentinela intacta. */
static inline bool g3d_z_sentinela(const g3d_vista_t *z, uint32_t cru)
{
    return z->bpp == 16 ? cru == (G3D_SENTINELA_Z & 0xFFFFu) : cru == G3D_SENTINELA_Z;
}

/* La profundidad (zbits bits) de un valor crudo. */
static inline uint32_t g3d_z_valor(const g3d_vista_t *z, uint32_t cru)
{
    uint32_t m = z->zbits >= 32 ? 0xFFFFFFFFu : (1u << z->zbits) - 1u;
    return (cru >> z->zdesloc) & m;
}

static inline bool g3d_z_maximo(const g3d_vista_t *z, uint32_t cru)
{
    uint32_t m = z->zbits >= 32 ? 0xFFFFFFFFu : (1u << z->zbits) - 1u;
    return g3d_z_valor(z, cru) == m;
}

/* Para enseñarlo («gpu3d probar --ver»): 0xFFFF con la sentinela; si no, los 8 bits altos del Z. */
static inline uint16_t g3d_z_codigo(const g3d_vista_t *z, uint32_t cru)
{
    if (g3d_z_sentinela(z, cru)) return 0xFFFFu;
    return (uint16_t)(g3d_z_valor(z, cru) >> (z->zbits > 8 ? z->zbits - 8 : 0));
}

/* ------------------------------------------------------------ el análisis */

#define G3D_ANALISE_CORES 4
typedef struct {
    uint32_t total;                        /* píxeles mirados */
    uint32_t cor_intacta;                  /* con G3D_SENTINELA_COR (la GPU no escribió) */
    uint32_t cores[G3D_ANALISE_CORES];     /* los primeros colores distintos de la sentinela */
    uint32_t n_cores[G3D_ANALISE_CORES];
    uint32_t resto;                        /* píxeles de otros colores */
    bool     tem_z;                        /* la CPU ve el Z */
    uint32_t z_intacto, z_maximo;
} g3d_analise_t;

/* «cor»: 0x00RRGGBB (o ARGB: se mira solo RGB). «z» puede ser NULL o sin cpu. Mira larg x alt
 * de la vista del color (y lo mismo del Z, si es menor, solo lo que tiene). */
static inline void g3d_analisar(const g3d_vista_t *cor, const g3d_vista_t *z, g3d_analise_t *an)
{
    uint8_t *p = (uint8_t *)an;
    for (uint32_t i = 0; i < sizeof *an; i++) p[i] = 0;
    if (cor && cor->cpu)
        for (int y = 0; y < cor->alt; y++)
            for (int x = 0; x < cor->larg; x++) {
                uint32_t c = g3d_vista_ler(cor, x, y) & 0x00FFFFFFu;
                an->total++;
                if (c == G3D_SENTINELA_COR) { an->cor_intacta++; continue; }
                int k = 0;
                while (k < G3D_ANALISE_CORES && an->n_cores[k] && an->cores[k] != c) k++;
                if (k < G3D_ANALISE_CORES) { an->cores[k] = c; an->n_cores[k]++; }
                else an->resto++;
            }
    if (z && z->cpu) {
        an->tem_z = true;
        for (int y = 0; y < z->alt; y++)
            for (int x = 0; x < z->larg; x++) {
                uint32_t v = g3d_vista_ler(z, x, y);
                if (g3d_z_sentinela(z, v)) an->z_intacto++;
                else if (g3d_z_maximo(z, v)) an->z_maximo++;
            }
    }
}

/* ------------------------------------------------------------ los ajustes de registros */

typedef struct { uint32_t ini, fim; } g3d_faixa_t;   /* registros permitidos (los dos extremos incluidos) */

#define G3D_AJUSTES_MAX 64
#define G3D_AJ_INICIO   0
#define G3D_AJ_DIBUJO   1
typedef struct {
    uint32_t reg[G3D_AJUSTES_MAX];
    uint32_t val[G3D_AJUSTES_MAX];
    uint8_t  quando[G3D_AJUSTES_MAX];      /* G3D_AJ_* */
    int      n;
    int      malas;                        /* líneas ignoradas (incluidas las de fuera) */
    int      fora;                         /* de ellas, bien escritas pero fuera de los rangos del driver */
} g3d_ajustes_t;

static inline bool g3d_aj_hex(const char **pp, const char *fim, uint32_t *v)
{
    const char *p = *pp;
    while (p < fim && (*p == ' ' || *p == '\t')) p++;
    if (p + 1 < fim && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    uint32_t r = 0;
    int d = 0;
    for (; p < fim; p++, d++) {
        char c = *p;
        uint32_t x;
        if (c >= '0' && c <= '9') x = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') x = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') x = (uint32_t)(c - 'A' + 10);
        else break;
        if (d >= 8) return false;
        r = (r << 4) | x;
    }
    if (!d) return false;
    *pp = p;
    *v = r;
    return true;
}

static inline bool g3d_aj_comeca(const char *l, const char *fim, const char *pal)
{
    for (; *pal; pal++, l++)
        if (l >= fim || *l != *pal) return false;
    return true;
}

static inline bool g3d_faixa_ok(const g3d_faixa_t *fx, int nf, uint32_t reg)
{
    if (reg & 3) return false;
    for (int i = 0; i < nf; i++)
        if (reg >= fx[i].ini && reg <= fx[i].fim) return true;
    return false;
}

/* Lee el fichero (t, n bytes). Sin rangos (nf 0), ningún ajuste vale: el driver no los admite. */
static inline void g3d_ajustes_ler(const char *t, uint32_t n, const g3d_faixa_t *fx, int nf, g3d_ajustes_t *a)
{
    a->n = a->malas = a->fora = 0;
    const char *p = t, *fim = t + n;
    while (p < fim) {
        const char *l = p;
        while (p < fim && *p != '\n') p++;
        const char *lf = p;
        if (p < fim) p++;
        while (l < lf && (*l == ' ' || *l == '\t' || *l == '\r')) l++;
        while (lf > l && (lf[-1] == ' ' || lf[-1] == '\t' || lf[-1] == '\r')) lf--;
        if (l == lf || *l == '#') continue;
        int quando;
        if (g3d_aj_comeca(l, lf, "inicio ")) { quando = G3D_AJ_INICIO; l += 7; }
        else if (g3d_aj_comeca(l, lf, "dibujo ")) { quando = G3D_AJ_DIBUJO; l += 7; }
        else { a->malas++; continue; }
        uint32_t reg, val;
        if (!g3d_aj_hex(&l, lf, &reg) || !g3d_aj_hex(&l, lf, &val) || l != lf || a->n >= G3D_AJUSTES_MAX) { a->malas++; continue; }
        if (!g3d_faixa_ok(fx, nf, reg)) { a->malas++; a->fora++; continue; }
        a->reg[a->n] = reg; a->val[a->n] = val; a->quando[a->n] = (uint8_t)quando; a->n++;
    }
}

#endif

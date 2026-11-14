// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_fonte.c - las tipografías del motor de PDF (mejora 059, D138).
 *
 * Aquí se leen los programas de fuente que van incrustados dentro de un PDF
 * y se convierten en contornos que el rasterizador sabe dibujar:
 *
 *   TrueType (FontFile2)  programa sfnt: tablas 'head', 'maxp', 'hhea',
 *                         'hmtx', 'loca', 'glyf', 'cmap', 'name' y 'post'.
 *   CFF (FontFile3)       programa CFF: diccionarios INDEX/DICT y charstrings
 *                         de tipo 2.
 *   Type 1 (FontFile)     programa PostScript: el bloque 'eexec' va cifrado
 *                         (el cifrado propio de Type 1, r = 55665) y dentro
 *                         están los charstrings de tipo 1 (r = 4330).
 *
 * Las 14 fuentes básicas (Helvetica, Times, Courier y variantes) no van
 * incrustadas en el PDF: se dibujan con la fuente vectorial propia que está
 * al final de este fichero.
 *
 * Los contornos de TrueType son curvas cuadráticas y el rasterizador solo
 * conoce las cúbicas, así que aquí cada cuadrática se convierte en su
 * cúbica equivalente.
 *
 * Los identificadores van en portugués y los comentarios en español, como en
 * el resto del proyecto. */

#include "pdf_fonte.h"
#include "pdf_geom.h"

/* ------------------------------------------------------------- utilidades */

/* Lectores de enteros con el orden de bytes del fichero. */
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static int16_t sbe16(const uint8_t *p)
{
    return (int16_t)be16(p);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Un entero con signo de 32 bits (para el CFF y el Type 1): los dos lo
 * guardan en big-endian (el operando 29 del DICT, el 255 de los
 * charstrings). */
static int32_t sbe32(const uint8_t *p)
{
    return (int32_t)be32(p);
}

/* Compara dos cadenas (no hay libc). */
static int igual(const char *a, const char *b)
{
    if (!a || !b) return a == b ? 0 : 1;
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return 0;
    return 1;
}

/* Copia una cadena terminada en NUL en un buffer fijo. */
static void copiar_nome(char *destino, int tam, const char *origem)
{
    int i = 0;
    if (!destino || tam <= 0) return;
    destino[0] = '\0';
    if (!origem) return;
    while (origem[i] && i < tam - 1) {
        destino[i] = origem[i];
        i++;
    }
    destino[i] = '\0';
}

/* ------------------------------------------------------------- estructuras */

/* Un INDEX del CFF (lo usan el CFF y el Type 1). */
typedef struct {
    const uint8_t *p;       /* el comienzo del INDEX */
    int      n;             /* cuantos objetos hay */
    int      offsize;       /* bytes de cada desplazamiento */
    const uint8_t *base;    /* donde empiezan los datos */
} cff_idx_t;

#define FONE_NOME_MAX  64
#define FONE_MAX_COD   256

/* Una fuente cargada. La estructura es privada de este fichero. */
struct pdf_fonte {
    int       tipo;                  /* PDF_FONTE_* */
    uint8_t  *dados;                 /* copia del programa de fuente */
    int       tam;
    char      nome[FONE_NOME_MAX];
    int       ascendente, descendente;

    /* Las diferencias del PDF: codigo -> nombre de glifo. */
    char     *dif[FONE_MAX_COD];

    /* ---- fuentes básicas ---- */
    int       basica;               /* 0 = ninguna; si no, indice de la tabla */
    int       b_espessura;          /* grosor del trazo */
    double    b_inclina;            /* inclinación de las oblicuas */

    /* ---- TrueType ---- */
    int       tt_upem;              /* unidades por em */
    int       tt_glyfs;             /* numero de glifos */
    int       tt_hmetrics;          /* numero de entradas de hmtx con ancho */
    const uint8_t *tt_hmtx;
    const uint8_t *tt_loca;
    int       tt_loca_longo;        /* indexToLocFormat */
    const uint8_t *tt_glyf;
    int       tt_glyf_tam;
    const uint8_t *tt_cmap;         /* subtabla elegida del cmap */
    int       tt_cmap_fmt;
    int       tt_simbolo;
    char    **tt_post;              /* nombre de cada glifo (post 2.0) */
    int       tt_post_n;

    /* ---- CFF ---- */
    int       cff_tam;               /* tamaño del programa CFF */
    cff_idx_t cff_chars_idx;         /* los charstrings */
    cff_idx_t cff_subrs_idx;         /* los subrs locales */
    cff_idx_t cff_gsubrs_idx;        /* los subrs globales */
    int       cff_tem_subrs;          /* hay subrs locales */
    int16_t   cff_cod[FONE_MAX_COD]; /* codigo -> indice de glifo */
    double    cff_mat[6];           /* FontMatrix */
    double    cff_escala;            /* factor para pasar a 1/1000 de em */
    int       cff_larg_def;          /* defaultWidthX */
    int       cff_larg_nom;          /* nominalWidthX */
    char    **cff_charset;           /* nombre de cada glifo (charset) */
    int       cff_ncharset;
    int       cff_cid;               /* es una fuente CID */
    const uint8_t *cff_fdarray;       /* FDArray (fuente CID) */
    int       cff_nfd;
    const uint8_t *cff_fdselect;      /* FDSelect (fuente CID) */
    int       cff_fdselect_fmt;
    cff_idx_t cff_fd_subrs[32];      /* subrs locales de cada FD */
    int       cff_fd_tem_subrs[32];
    uint16_t *cff_cid_gid;           /* CID -> glifo (fuentes CID), o NULL */
    int       cff_cid_max;
    int       cff_nglifos;
    int       cff_fd_larg_def[32];
    int       cff_fd_larg_nom[32];

    /* ---- Type 1 ---- */
    char    **t1_nomes;              /* nombre de cada charstring */
    int       t1_n;
    uint8_t **t1_chars;              /* charstring de cada nombre */
    int      *t1_tam;
    const uint8_t **t1_subrs_dados;  /* los subrs locales */
    int      *t1_subrs_tam;
    double    t1_mat[6];             /* FontMatrix */
    double    t1_escala;
    int       t1_cod[FONE_MAX_COD];  /* codigo -> indice de charstring */
    int       t1_subrs;              /* cuantos subrs locales hay */
};

/* Las tablas de datos que se generan en otro fichero del módulo. */
static bool tt_carregar(pdf_fonte_t *f);
static bool cff_carregar(pdf_fonte_t *f);
static bool t1_carregar(pdf_fonte_t *f);
static bool tt_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g);
static bool cff_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g);
static bool t1_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g);
static bool basica_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g);
static int  fonte_largura(pdf_fonte_t *f, int codigo);
static int  tt_largura(pdf_fonte_t *f, int codigo);
static int  cff_largura(pdf_fonte_t *f, int codigo);
static int  t1_largura(pdf_fonte_t *f, int codigo);
static int  basica_largura(pdf_fonte_t *f, int codigo);


/* --------------------------------------------------------------- la API */

/* Carga una fuente incrustada. */
pdf_fonte_t *pdf_fonte_carregar(int tipo, const uint8_t *dados, int tam)
{
    pdf_fonte_t *f;
    bool ok = false;

    if (!dados || tam <= 0) return NULL;

    f = (pdf_fonte_t *)aloca_zerado(sizeof(pdf_fonte_t));
    if (!f) return NULL;

    f->tipo = tipo;
    f->tam = tam;
    f->dados = (uint8_t *)aloca((size_t)tam);
    if (!f->dados) {
        libera(f);
        return NULL;
    }
    memcpy(f->dados, dados, (size_t)tam);
    f->ascendente = 750;
    f->descendente = -250;

    switch (tipo) {
        case PDF_FONTE_TRUETYPE: ok = tt_carregar(f); break;
        case PDF_FONTE_CFF:      ok = cff_carregar(f); break;
        case PDF_FONTE_TYPE1:    ok = t1_carregar(f); break;
        default: break;
    }

    if (!ok) {
        pdf_fonte_liberar(f);
        return NULL;
    }
    return f;
}

/* Libera una fuente. */
void pdf_fonte_liberar(pdf_fonte_t *f)
{
    int i;

    if (!f) return;

    for (i = 0; i < FONE_MAX_COD; i++) {
        if (f->dif[i]) {
            libera(f->dif[i]);
            f->dif[i] = NULL;
        }
    }
    if (f->tt_post) {
        for (i = 0; i < f->tt_post_n; i++) {
            if (f->tt_post[i]) libera(f->tt_post[i]);
        }
        libera(f->tt_post);
    }
    if (f->cff_charset) {
        for (i = 0; i < f->cff_ncharset; i++) {
            if (f->cff_charset[i]) libera(f->cff_charset[i]);
        }
        libera(f->cff_charset);
    }
    if (f->t1_nomes) {
        for (i = 0; i < f->t1_n; i++) {
            if (f->t1_nomes[i]) libera(f->t1_nomes[i]);
        }
        libera(f->t1_nomes);
    }
    if (f->t1_chars) {
        for (i = 0; i < f->t1_n; i++) {
            if (f->t1_chars[i]) libera(f->t1_chars[i]);
        }
        libera(f->t1_chars);
    }
    if (f->t1_tam) libera(f->t1_tam);
    if (f->t1_subrs_dados) {
        for (i = 0; i < f->t1_subrs; i++)
            if (f->t1_subrs_dados[i]) libera((void *)f->t1_subrs_dados[i]);
        libera(f->t1_subrs_dados);
    }
    if (f->t1_subrs_tam) libera(f->t1_subrs_tam);

    if (f->dados) libera(f->dados);
    libera(f);
}

/* El nombre de la fuente. */
const char *pdf_fonte_nome(const pdf_fonte_t *f)
{
    if (!f) return "";
    return f->nome;
}

/* El glifo de un código de caracter. */
bool pdf_fonte_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *glifo)
{
    if (!f || !glifo) return false;

    memset(glifo, 0, sizeof(pdf_glifo_t));
    glifo->largura = 0;

    switch (f->tipo) {
        case PDF_FONTE_TRUETYPE: return tt_glifo(f, codigo, glifo);
        case PDF_FONTE_CFF:      return cff_glifo(f, codigo, glifo);
        case PDF_FONTE_TYPE1:    return t1_glifo(f, codigo, glifo);
        default: break;
    }
    if (f->basica) return basica_glifo(f, codigo, glifo);
    return false;
}

/* Libera el contorno de un glifo. */
void pdf_glifo_liberar(pdf_glifo_t *g)
{
    if (!g) return;
    pdf_caminho_liberar(&g->contorno);
    g->largura = 0;
}

/* El ancho de un código de caracter (en 1/1000 de em). */
int pdf_fonte_largura(pdf_fonte_t *f, int codigo)
{
    if (!f) return 0;
    return fonte_largura(f, codigo);
}

/* El ancho, segun el tipo de fuente. */
static int fonte_largura(pdf_fonte_t *f, int codigo)
{
    switch (f->tipo) {
        case PDF_FONTE_TRUETYPE: return tt_largura(f, codigo);
        case PDF_FONTE_CFF:      return cff_largura(f, codigo);
        case PDF_FONTE_TYPE1:    return t1_largura(f, codigo);
        default: break;
    }
    if (f->basica) return basica_largura(f, codigo);
    return 0;
}

/* Aplica la tabla /Differences del PDF. */
void pdf_fonte_differences(pdf_fonte_t *f, int codigo, const char *nome_glifo)
{
    int n;

    if (!f || codigo < 0 || codigo >= FONE_MAX_COD) return;

    if (f->dif[codigo]) {
        libera(f->dif[codigo]);
        f->dif[codigo] = NULL;
    }
    if (!nome_glifo || !nome_glifo[0]) return;

    n = 0;
    while (nome_glifo[n] && n < 63) n++;
    f->dif[codigo] = (char *)aloca((size_t)n + 1);
    if (!f->dif[codigo]) return;
    memcpy(f->dif[codigo], nome_glifo, (size_t)n);
    f->dif[codigo][n] = '\0';
}

/* El alto y el bajo de la fuente (en 1/1000 de em). */
int pdf_fonte_ascendente(pdf_fonte_t *f)
{
    if (!f) return 0;
    return f->ascendente;
}

int pdf_fonte_descendente(pdf_fonte_t *f)
{
    if (!f) return 0;
    return f->descendente;
}
/* =============================================================== TrueType ===
 *
 * El programa de fuente TrueType es un programa sfnt: una cabecera con el
 * numero de tablas y un directorio con la etiqueta, el desplazamiento y la
 * longitud de cada tabla. Las que hacen falta son:
 *
 *   head    unitsPerEm e indexToLocFormat
 *   maxp    numero de glifos
 *   hhea    alto, bajo y numero de entradas de 'hmtx' con ancho
 *   hmtx    anchos de avance
 *   loca    desplazamiento de cada glifo dentro de 'glyf'
 *   glyf    los contornos
 *   cmap    codigo de caracter -> indice de glifo
 *   name    el nombre de la fuente
 *   post    (opcional) el nombre de cada glifo
 */

/* Una matriz 2x3 propia, para no depender de las convenciones de otro
 * módulo: x' = a*x + c*y + e, y' = b*x + d*y + f. */
typedef struct {
    double a, b, c, d, e, f;
} mat2_t;

static void mat2_aplica(const mat2_t *m, double x, double y,
                        double *rx, double *ry)
{
    *rx = m->a * x + m->c * y + m->e;
    *ry = m->b * x + m->d * y + m->f;
}

/* El resultado es el padre compuesto con el hijo: primero el hijo, luego
 * el padre. */
static void mat2_comp(mat2_t *r, const mat2_t *pai, const mat2_t *filho)
{
    r->a = pai->a * filho->a + pai->c * filho->b;
    r->b = pai->b * filho->a + pai->d * filho->b;
    r->c = pai->a * filho->c + pai->c * filho->d;
    r->d = pai->b * filho->c + pai->d * filho->d;
    r->e = pai->a * filho->e + pai->c * filho->f + pai->e;
    r->f = pai->b * filho->e + pai->d * filho->f + pai->f;
}

/* Busca una tabla del programa sfnt. */
static bool tt_tabela(const pdf_fonte_t *f, const char *tag,
                      const uint8_t **dados, int *tam)
{
    const uint8_t *p = f->dados;
    uint32_t marca;
    int ntab, i;

    if (f->tam < 12) return false;
    marca = be32(p);          /* la marca del sfnt va en orden grande */
    if (marca != 0x00010000u && marca != 0x74727565u && marca != 0x4f54544fu)
        return false;

    ntab = be16(p + 4);          /* el numero de tablas va en orden grande */
    if (ntab < 0 || 12 + ntab * 16 > f->tam) return false;

    for (i = 0; i < ntab; i++) {
        const uint8_t *e = p + 12 + i * 16;
        uint32_t off, len;

        if (e[0] != (uint8_t)tag[0] || e[1] != (uint8_t)tag[1] ||
            e[2] != (uint8_t)tag[2] || e[3] != (uint8_t)tag[3])
            continue;
        off = be32(e + 8);      /* el directorio del sfnt va en orden grande */
        len = be32(e + 12);
        if (off > (uint32_t)f->tam) return false;
        if (off + len > (uint32_t)f->tam) len = (uint32_t)f->tam - off;
        *dados = p + off;
        *tam = (int)len;
        return true;
    }
    return false;
}

/* Un valor F2Dot14 de TrueType (un número de punto fijo con 14 decimales). */
static double tt_f2dot14(const uint8_t *p)
{
    return (double)sbe16(p) / 16384.0;
}

/* ---- la tabla cmap ---- */

/* Elige la subtabla del cmap: se prefiere la de Unicode (plataforma 3),
 * luego la de Macintosh y por último la simbólica. */
static bool tt_elegir_cmap(pdf_fonte_t *f)
{
    const uint8_t *cm, *melhor = NULL;
    int tam, nsub, i, melhor_plat = -1, melhor_enc = -1;

    if (!tt_tabela(f, "cmap", &cm, &tam)) return false;
    if (tam < 4) return false;

    nsub = be16(cm + 2);          /* el cmap va en orden grande */
    if (nsub < 0 || 4 + nsub * 8 > tam) return false;

    for (i = 0; i < nsub; i++) {
        const uint8_t *e = cm + 4 + i * 8;
        uint32_t off = be32(e + 4);      /* tambien en orden grande */
        int plat = be16(e), enc = be16(e + 2);
        int pontos;

        if (off + 4 > (uint32_t)tam) continue;
        pontos = 0;
        if (plat == 3 && (enc == 10 || enc == 1)) pontos = 3;
        else if (plat == 0) pontos = 2;
        else if (plat == 3 && enc == 0) pontos = 1;
        else if (plat == 1 && enc == 0) pontos = 0;

        if (pontos > melhor_plat ||
            (pontos == melhor_plat && enc > melhor_enc)) {
            melhor_plat = pontos;
            melhor_enc = enc;
            melhor = cm + off;
            if (pontos == 3 && enc == 10) break;
        }
    }
    if (!melhor) return false;

    f->tt_cmap = melhor;
    f->tt_cmap_fmt = be16(melhor);
    f->tt_simbolo = (melhor_plat == 1);
    return true;
}

/* Busca un código de caracter en la subtabla del cmap. */
static int tt_cmap_gid(const pdf_fonte_t *f, int codigo)
{
    const uint8_t *cm = f->tt_cmap;

    if (!cm || codigo < 0) return 0;

    switch (f->tt_cmap_fmt) {
        case 0: {
            /* Byte encoding: 256 bytes de glifos. */
            if (codigo > 255) return 0;
            if (cm + 6 + codigo >= f->dados + f->tam) return 0;
            return cm[6 + codigo];
        }
        case 4: {
            /* Segment mapping to delta values. */
            int segX2 = be16(cm + 6);
            int nseg = segX2 / 2;
            const uint8_t *fin = cm + 14;
            const uint8_t *inicio = fin + segX2 + 2;
            const uint8_t *delta = inicio + segX2;
            const uint8_t *ro = delta + segX2;
            int i;

            if (codigo > 0xffff) return 0;
            for (i = 0; i < nseg; i++) {
                int c = be16(fin + i * 2);
                if (c < codigo) continue;
                {
                    int s = be16(inicio + i * 2);
                    int d = sbe16(delta + i * 2);
                    int r = be16(ro + i * 2);
                    if (s > codigo) return 0;
                    if (r == 0) return (codigo + d) & 0xffff;
                    {
                        const uint8_t *p = ro + i * 2 + r + (codigo - s) * 2;
                        int g;
                        if (p + 1 >= f->dados + f->tam) return 0;
                        g = be16(p);
                        if (g == 0) return 0;
                        return (g + d) & 0xffff;
                    }
                }
            }
            return 0;
        }
        case 6: {
            /* Trimmed table mapping. */
            int primeiro = be16(cm + 6);
            int n = be16(cm + 8);
            int i = codigo - primeiro;
            if (i < 0 || i >= n) return 0;
            if (cm + 10 + i * 2 + 1 >= f->dados + f->tam) return 0;
            return be16(cm + 10 + i * 2);
        }
        case 12: {
            /* Segmented coverage. */
            uint32_t ngroup = be32(cm + 12);
            uint32_t i;
            if (cm + 16 > f->dados + f->tam) return 0;
            for (i = 0; i < ngroup; i++) {
                const uint8_t *g = cm + 16 + i * 12;
                uint32_t c0, c1, g0;
                if (g + 11 >= f->dados + f->tam) return 0;
                c0 = be32(g);
                c1 = be32(g + 4);
                g0 = be32(g + 8);
                if ((uint32_t)codigo < c0) return 0;
                if ((uint32_t)codigo <= c1)
                    return (int)(g0 + ((uint32_t)codigo - c0));
            }
            return 0;
        }
        default: break;
    }
    return 0;
}

/* ---- la tabla name ---- */

/* Saca el nombre de la fuente de la tabla 'name'. Se prefiere el nombre
 * PostScript (6), luego el completo (4) y por ultimo el de la familia (1). */
static void tt_nome(pdf_fonte_t *f)
{
    const uint8_t *nm;
    int tam, n, i;
    int melhor_id = 0, melhor_plat = -1, melhor_enc = 0;
    int melhor_off = 0, melhor_len = 0;

    if (!tt_tabela(f, "name", &nm, &tam)) return;
    if (tam < 6) return;
    n = be16(nm + 2);
    if (n < 0 || 6 + n * 12 > tam) return;
    /* Los desplazamientos de cada cadena son relativos al sitio donde
     * empiezan las cadenas, que es lo que dice el campo stringOffset. */
    if (6 + n * 12 > tam) return;

    for (i = 0; i < n; i++) {
        const uint8_t *e = nm + 6 + i * 12;
        int plat = be16(e), enc = be16(e + 2);
        int id = be16(e + 6);
        int len = be16(e + 8), off = be16(e + 10);
        int pontos;

        if (id != 6 && id != 1 && id != 4) continue;
        if (len <= 0 || len >= FONE_NOME_MAX) continue;
        off += be16(nm + 4);      /* el comienzo de las cadenas */
        if (off + len > tam) continue;
        if (plat != 3 && plat != 0 && plat != 1) continue;

        pontos = (id == 6) ? 3 : ((id == 4) ? 2 : 1);
        if (pontos > melhor_id) {
            melhor_id = pontos;
            melhor_plat = plat;
            melhor_enc = enc;
            melhor_off = off;
            melhor_len = len;
        }
    }
    if (melhor_id == 0) return;

    {
        char buf[FONE_NOME_MAX];
        int j, k = 0;
        bool utf16 = (melhor_plat == 3 &&
                      (melhor_enc == 1 || melhor_enc == 10));

        for (j = 0; j < melhor_len && k < FONE_NOME_MAX - 1; j++) {
            uint8_t b = nm[melhor_off + j];
            if (utf16 && (j & 1)) continue;   /* el byte alto del UTF-16 */
            buf[k++] = (char)b;
        }
        buf[k] = '\0';
        if (k > 0) copiar_nome(f->nome, FONE_NOME_MAX, buf);
    }
}

/* ---- la tabla post (nombres de los glifos) ---- */

/* Los nombres estándar de Macintosh, en el orden que usa la tabla post. */
static const char *const tt_nomes_padrao[258] = {
    ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl",
    "numbersign", "dollar", "percent", "ampersand", "quotesingle",
    "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period",
    "slash", "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater",
    "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
    "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "braceleft", "bar", "braceright", "asciitilde", "Adieresis", "Aring",
    "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute",
    "agrave", "acircumflex", "adieresis", "atilde", "aring", "ccedilla",
    "eacute", "egrave", "ecircumflex", "edieresis", "iacute", "igrave",
    "icircumflex", "idieresis", "ntilde", "oacute", "ograve", "ocircumflex",
    "odieresis", "otilde", "uacute", "ugrave", "ucircumflex", "udieresis",
    "dagger", "degree", "cent", "sterling", "section", "bullet", "paragraph",
    "germandbls", "registered", "copyright", "trademark", "acute",
    "dieresis", "notequal", "AE", "Oslash", "infinity", "plusminus",
    "lessequal", "greaterequal", "yen", "mu", "partialdiff", "summation",
    "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega",
    "ae", "oslash", "questiondown", "exclamdown", "logicalnot", "radical",
    "florin", "approxequal", "Delta", "guillemotleft", "guillemotright",
    "ellipsis", "nonbreakingspace", "Agrave", "Atilde", "Otilde", "OE", "oe",
    "endash", "emdash", "quotedblleft", "quotedblright", "quoteleft",
    "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis", "fraction",
    "currency", "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl",
    "periodcentered", "quotesinglbase", "quotedblbase", "perthousand",
    "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave", "Iacute",
    "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple",
    "Ograve", "Uacute", "Ucircumflex", "Ugrave", "dotlessi", "circumflex",
    "tilde", "macron", "breve", "dotaccent", "ring", "cedilla",
    "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron",
    "scaron", "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute",
    "yacute", "Thorn", "thorn", "minus", "multiply", "onesuperior",
    "twosuperior", "threesuperior", "onehalf", "onequarter", "threequarters",
    "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla", "scedilla",
    "Cacute", "cacute", "Ccaron", "ccaron", "dcroat"
};

/* Lee la tabla post (formato 2.0) y saca el nombre de cada glifo. */
static void tt_post(pdf_fonte_t *f)
{
    const uint8_t *po;
    int tam, ng, i, n;
    const uint8_t *p;

    if (!tt_tabela(f, "post", &po, &tam)) return;
    if (tam < 34) return;
    if (be32(po) != 0x00020000u) return;

    ng = be16(po + 32);
    if (ng <= 0) return;
    if (34 + ng * 2 > tam) return;

    f->tt_post = (char **)aloca_zerado((size_t)ng * sizeof(char *));
    if (!f->tt_post) return;
    f->tt_post_n = ng;

    p = po + 34 + ng * 2;
    for (i = 0; i < ng; i++) {
        int idx = be16(po + 34 + i * 2);
        const char *nome = NULL;

        if (idx < 258) {
            nome = tt_nomes_padrao[idx];
        } else if (p < po + tam) {
            int c = *p;
            if (c + 1 + p <= po + tam) {
                nome = (const char *)(p + 1);
                p += 1 + c;
            }
        }
        if (!nome) {
            f->tt_post[i] = (char *)aloca(8);
            if (f->tt_post[i]) {
                memcpy(f->tt_post[i], "glifo", 5);
                f->tt_post[i][5] = '\0';
            }
            continue;
        }
        n = 0;
        while (nome[n] && n < 63) n++;
        f->tt_post[i] = (char *)aloca((size_t)n + 1);
        if (!f->tt_post[i]) continue;
        memcpy(f->tt_post[i], nome, (size_t)n);
        f->tt_post[i][n] = '\0';
    }
}

/* Los nombres estándar de Adobe y su carácter Unicode. Sirven para
 * resolver las /Differences cuando la fuente no trae los nombres. */
static const struct {
    const char *nome;
    uint16_t    uni;
} tt_unicode[] = {
    {"zero", 0x30}, {"one", 0x31}, {"two", 0x32}, {"three", 0x33}, {"four", 0x34},
    {"five", 0x35}, {"six", 0x36}, {"seven", 0x37}, {"eight", 0x38}, {"nine", 0x39},
    {"Euro", 0x20ac}, {"Zcaron", 0x17d}, {"zcaron", 0x17e}, {"guilsinglleft", 0x2039},
    {"guilsinglright", 0x203a}, {"circumflex", 0x2c6}, {"tilde", 0x2dc}, {"brokenbar", 0xa6},
    {"dieresis", 0xa8}, {"macron", 0xaf}, {"acute", 0xb4}, {"cedilla", 0xb8},
    {"onesuperior", 0xb9}, {"twosuperior", 0xb2}, {"threesuperior", 0xb3},
    {"periodcentered", 0xb7}, {"ff", 0xfb00}, {"ffi", 0xfb03}, {"ffl", 0xfb04},
    {"quoteright", 0x2019}, {"quoteleft", 0x2018},
    {"space", 0x20}, {"exclam", 0x21}, {"quotedbl", 0x22}, {"numbersign", 0x23},
    {"dollar", 0x24}, {"percent", 0x25}, {"ampersand", 0x26}, {"quotesingle", 0x27},
    {"parenleft", 0x28}, {"parenright", 0x29}, {"asterisk", 0x2a}, {"plus", 0x2b},
    {"comma", 0x2c}, {"hyphen", 0x2d}, {"period", 0x2e}, {"slash", 0x2f},
    {"colon", 0x3a}, {"semicolon", 0x3b}, {"less", 0x3c}, {"equal", 0x3d},
    {"greater", 0x3e}, {"question", 0x3f}, {"at", 0x40}, {"bracketleft", 0x5b},
    {"backslash", 0x5c}, {"bracketright", 0x5d}, {"asciicircum", 0x5e},
    {"underscore", 0x5f}, {"grave", 0x60}, {"braceleft", 0x7b}, {"bar", 0x7c},
    {"braceright", 0x7d}, {"asciitilde", 0x7e},
    {"Aacute", 0xc1}, {"Acircumflex", 0xc2}, {"Adieresis", 0xc4}, {"Agrave", 0xc0},
    {"Aring", 0xc5}, {"Atilde", 0xc3}, {"Ccedilla", 0xc7}, {"Eacute", 0xc9},
    {"Ecircumflex", 0xca}, {"Edieresis", 0xcb}, {"Egrave", 0xc8}, {"Iacute", 0xcd},
    {"Icircumflex", 0xce}, {"Idieresis", 0xcf}, {"Igrave", 0xcc}, {"Ntilde", 0xd1},
    {"Oacute", 0xd3}, {"Ocircumflex", 0xd4}, {"Odieresis", 0xd6}, {"Ograve", 0xd2},
    {"Otilde", 0xd5}, {"Scaron", 0x160}, {"Uacute", 0xda}, {"Ucircumflex", 0xdb},
    {"Udieresis", 0xdc}, {"Ugrave", 0xd9}, {"Yacute", 0xdd}, {"Ydieresis", 0x178},
    {"aacute", 0xe1}, {"acircumflex", 0xe2}, {"adieresis", 0xe4}, {"agrave", 0xe0},
    {"aring", 0xe5}, {"atilde", 0xe3}, {"ccedilla", 0xe7}, {"eacute", 0xe9},
    {"ecircumflex", 0xea}, {"edieresis", 0xeb}, {"egrave", 0xe8}, {"iacute", 0xed},
    {"icircumflex", 0xee}, {"idieresis", 0xef}, {"igrave", 0xec}, {"ntilde", 0xf1},
    {"oacute", 0xf3}, {"ocircumflex", 0xf4}, {"odieresis", 0xf6}, {"ograve", 0xf2},
    {"otilde", 0xf5}, {"scaron", 0x161}, {"uacute", 0xfa}, {"ucircumflex", 0xfb},
    {"udieresis", 0xfc}, {"ugrave", 0xf9}, {"yacute", 0xfd}, {"ydieresis", 0xff},
    {"AE", 0xc6}, {"ae", 0xe6}, {"OE", 0x152}, {"oe", 0x153}, {"Oslash", 0xd8},
    {"oslash", 0xf8}, {"germandbls", 0xdf}, {"Eth", 0xd0}, {"eth", 0xf0},
    {"Thorn", 0xde}, {"thorn", 0xfe}, {"dotlessi", 0x131}, {"Lslash", 0x141},
    {"lslash", 0x142}, {"minus", 0x2212}, {"multiply", 0xd7}, {"divide", 0xf7},
    {"copyright", 0xa9}, {"registered", 0xae}, {"trademark", 0x2122},
    {"degree", 0xb0}, {"plusminus", 0xb1}, {"paragraph", 0xb6}, {"section", 0xa7},
    {"bullet", 0x2022}, {"ellipsis", 0x2026}, {"emdash", 0x2014}, {"endash", 0x2013},
    {"quotedblleft", 0x201c}, {"quotedblright", 0x201d}, {"quoteleft", 0x2018},
    {"quoteright", 0x2019}, {"quotesinglbase", 0x201a}, {"quotedblbase", 0x201e},
    {"dagger", 0x2020}, {"daggerdbl", 0x2021}, {"perthousand", 0x2030},
    {"exclamdown", 0xa1}, {"questiondown", 0xbf}, {"guillemotleft", 0xab},
    {"guillemotright", 0xbb}, {"logicalnot", 0xac}, {"currency", 0xa4},
    {"sterling", 0xa3}, {"yen", 0xa5}, {"cent", 0xa2}, {"florin", 0x192},
    {"radical", 0x221a}, {"approxequal", 0x2248}, {"Delta", 0x2206},
    {"Omega", 0x2126}, {"mu", 0xb5}, {"partialdiff", 0x2202}, {"summation", 0x2211},
    {"product", 0x220f}, {"pi", 0x3c0}, {"integral", 0x222b},
    {"ordfeminine", 0xaa}, {"ordmasculine", 0xba}, {"nonbreakingspace", 0xa0},
    {"fi", 0xfb01}, {"fl", 0xfb02}, {"onehalf", 0xbd}, {"onequarter", 0xbc},
    {"threequarters", 0xbe}, {"fraction", 0x2044}, {"lozenge", 0x25ca},
    {"nbspace", 0xa0}, {"sfthyphen", 0xad}, {"middot", 0xb7}, {"Delta2", 0x2206}
};

/* El carácter Unicode de un nombre de glifo estándar (o -1 si no se sabe). */
static int tt_nome_unicode(const char *nome)
{
    int i;

    if (!nome || !nome[0]) return -1;
    if (nome[1] == '\0') return (uint8_t)nome[0];

    for (i = 0; i < (int)(sizeof(tt_unicode) / sizeof(tt_unicode[0])); i++) {
        const char *a = tt_unicode[i].nome;
        const char *b = nome;
        while (*a && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return tt_unicode[i].uni;
    }
    /* Los nombres tipo 'uniXXXX' y 'uXXXX' llevan el código dentro. */
    if ((nome[0] == 'u' || nome[0] == 'U') && nome[1] == 'n' && nome[2] == 'i') {
        int v = 0, i2;
        for (i2 = 3; nome[i2]; i2++) {
            int c = (uint8_t)nome[i2], d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return -1;
            v = v * 16 + d;
            if (v > 0xffff) return -1;
        }
        return v;
    }
    return -1;
}

/* El indice de glifo de un nombre de glifo. */
static int tt_gid_por_nome(pdf_fonte_t *f, const char *nome)
{
    int i, uni;

    if (!nome || !nome[0]) return 0;

    for (i = 0; i < f->tt_post_n; i++) {
        if (f->tt_post[i] && igual(f->tt_post[i], nome) == 0) return i;
    }

    /* Sin tabla post: se busca el carácter en el cmap. */
    uni = tt_nome_unicode(nome);
    if (uni < 0) return 0;
    return tt_cmap_gid(f, uni);
}

/* El ancho de avance de un glifo (en 1/1000 de em). */
static int tt_avanca(pdf_fonte_t *f, int gid)
{
    int i, u;

    if (gid < 0 || f->tt_hmetrics <= 0 || !f->tt_hmtx) return 0;
    i = gid;
    if (i >= f->tt_hmetrics) i = f->tt_hmetrics - 1;
    if (4 + i * 4 + 1 >= f->tam) return 0;
    u = be16(f->tt_hmtx + i * 4);
    if (f->tt_upem <= 0) return u;
    return (int)((double)u * 1000.0 / (double)f->tt_upem + 0.5);
}

/* ---- los contornos ---- */

/* Una línea con la transformación aplicada. */
static void tt_linha(pdf_caminho_t *cam, const mat2_t *m,
                     double x0, double y0, double x1, double y1)
{
    double a, b, c, d;

    mat2_aplica(m, x0, y0, &a, &b);
    mat2_aplica(m, x1, y1, &c, &d);
    pdf_caminho_linha(cam, a, b, c, d);
}

/* Una curva cuadrática convertida en cúbica (son lo mismo). */
static void tt_cubica(pdf_caminho_t *cam, const mat2_t *m,
                      double x0, double y0, double qx, double qy,
                      double x1, double y1)
{
    double cx1 = x0 + (2.0 / 3.0) * (qx - x0);
    double cy1 = y0 + (2.0 / 3.0) * (qy - y0);
    double cx2 = x1 + (2.0 / 3.0) * (qx - x1);
    double cy2 = y1 + (2.0 / 3.0) * (qy - y1);
    double ax, ay, bx, by, tx, ty, ux, uy;

    mat2_aplica(m, x0, y0, &ax, &ay);
    mat2_aplica(m, cx1, cy1, &bx, &by);
    mat2_aplica(m, cx2, cy2, &tx, &ty);
    mat2_aplica(m, x1, y1, &ux, &uy);
    pdf_caminho_cubica(cam, ax, ay, bx, by, tx, ty, ux, uy);
}

/* Dibuja un contorno simple de TrueType. */
static bool tt_contorno_simples(const uint8_t *p, int tam, int ncont,
                                 const mat2_t *m, pdf_caminho_t *cam)
{
    uint16_t *fin;
    int npts, i, pos;
    uint8_t *flags;
    int *xs, *ys;
    int x = 0, y = 0;
    int *ox, *oy, *oo;      /* los puntos del contorno, ya en orden */
    int lista = 0;          /* cuantos puntos hay en la lista */

    if (10 + ncont * 2 > tam) return false;
    fin = (uint16_t *)aloca((size_t)ncont * sizeof(uint16_t) + 2);
    if (!fin) return false;
    for (i = 0; i < ncont; i++) fin[i] = be16(p + 10 + i * 2);
    npts = fin[ncont - 1] + 1;

    pos = 10 + ncont * 2;
    if (pos + 2 > tam) { libera(fin); return false; }
    pos += 2 + be16(p + pos);        /* las instrucciones del hinting */

    flags = (uint8_t *)aloca((size_t)npts + 1);
    xs = (int *)aloca((size_t)npts * sizeof(int) + 4);
    ys = (int *)aloca((size_t)npts * sizeof(int) + 4);
    if (!flags || !xs || !ys) {
        libera(fin);
        if (flags) libera(flags);
        if (xs) libera(xs);
        if (ys) libera(ys);
        return false;
    }

    /* Las banderas de cada punto (con repetición). */
    for (i = 0; i < npts; ) {
        uint8_t fl;
        if (pos >= tam) goto erro;
        fl = p[pos++];
        flags[i++] = fl;
        if (fl & 0x08) {          /* el flag se repite */
            int rep;
            if (pos >= tam) goto erro;
            rep = p[pos++];
            while (rep-- > 0 && i < npts) flags[i++] = fl;
        }
    }
    /* Las coordenadas en X (las diferencias son acumulativas). */
    for (i = 0; i < npts; i++) {
        uint8_t fl = flags[i];
        if (fl & 0x02) {
            int d;
            if (pos >= tam) goto erro;
            d = p[pos++];
            x += (fl & 0x10) ? d : -d;
        } else if (!(fl & 0x10)) {
            if (pos + 1 >= tam) goto erro;
            x += sbe16(p + pos);
            pos += 2;
        }
        xs[i] = x;
    }
    /* Las coordenadas en Y. */
    for (i = 0; i < npts; i++) {
        uint8_t fl = flags[i];
        if (fl & 0x04) {
            int d;
            if (pos >= tam) goto erro;
            d = p[pos++];
            y += (fl & 0x20) ? d : -d;
        } else if (!(fl & 0x20)) {
            if (pos + 1 >= tam) goto erro;
            y += sbe16(p + pos);
            pos += 2;
        }
        ys[i] = y;
    }

    /* Los puntos, en el orden en que se dibujan. */
    ox = (int *)aloca((size_t)(npts + 2) * sizeof(int) + 4);
    oy = (int *)aloca((size_t)(npts + 2) * sizeof(int) + 4);
    oo = (int *)aloca((size_t)(npts + 2) * sizeof(int) + 4);
    if (!ox || !oy || !oo) {
        libera(fin); libera(flags); libera(xs); libera(ys);
        if (ox) libera(ox);
        if (oy) libera(oy);
        if (oo) libera(oo);
        return false;
    }

    i = 0;
    while (i < ncont) {
        int primeiro = (i == 0) ? 0 : fin[i - 1] + 1;  /* el primer punto */
        int ultimo = fin[i];          /* indice del ultimo */
        int n = ultimo - primeiro + 1;
        int k, npts_lista, base = lista;
        double cx, cy;
        int pend = -1;                 /* punto fuera de curva pendiente */

        if (n <= 0) { i++; continue; }

        /* El contorno tiene que empezar en un punto en curva: si no, se
         * usa el punto medio entre el ultimo y el primero. */
        if (flags[primeiro] & 0x01) {
            npts_lista = n;
            for (k = 0; k < n; k++) {
                ox[lista] = xs[primeiro + k];
                oy[lista] = ys[primeiro + k];
                oo[lista] = flags[primeiro + k] & 0x01;
                lista++;
            }
        } else if (flags[ultimo] & 0x01) {
            /* El ultimo punto esta en curva: el contorno empieza por ahi. */
            npts_lista = n;
            ox[lista] = xs[ultimo]; oy[lista] = ys[ultimo]; oo[lista] = 1;
            lista++;
            for (k = 0; k < n - 1; k++) {
                ox[lista] = xs[primeiro + k];
                oy[lista] = ys[primeiro + k];
                oo[lista] = flags[primeiro + k] & 0x01;
                lista++;
            }
        } else {
            /* Los dos extremos estan fuera de curva: se inserta el punto
             * medio, que es el punto de partida. */
            npts_lista = n + 1;
            ox[lista] = (xs[ultimo] + xs[primeiro]) / 2;
            oy[lista] = (ys[ultimo] + ys[primeiro]) / 2;
            oo[lista] = 1;
            lista++;
            for (k = 0; k < n; k++) {
                ox[lista] = xs[primeiro + k];
                oy[lista] = ys[primeiro + k];
                oo[lista] = flags[primeiro + k] & 0x01;
                lista++;
            }
        }

        /* Se dibuja el contorno. */
        cx = ox[base];
        cy = oy[base];
        for (k = 1; k < npts_lista; k++) {
            int idx = base + k;
            if (oo[idx]) {
                if (pend >= 0) {
                    tt_cubica(cam, m, cx, cy, ox[pend], oy[pend],
                              ox[idx], oy[idx]);
                    cx = ox[idx]; cy = oy[idx];
                    pend = -1;
                } else {
                    tt_linha(cam, m, cx, cy, ox[idx], oy[idx]);
                    cx = ox[idx]; cy = oy[idx];
                }
            } else {
                if (pend >= 0) {
                    double mx = (ox[pend] + ox[idx]) / 2.0;
                    double my = (oy[pend] + oy[idx]) / 2.0;
                    tt_cubica(cam, m, cx, cy, ox[pend], oy[pend], mx, my);
                    cx = mx; cy = my;
                }
                pend = idx;
            }
        }
        /* Cierre del contorno. */
        if (pend >= 0)
            tt_cubica(cam, m, cx, cy, ox[pend], oy[pend], ox[base], oy[base]);
        else
            tt_linha(cam, m, cx, cy, ox[base], oy[base]);

        i++;
    }

    libera(fin); libera(flags); libera(xs); libera(ys);
    libera(ox); libera(oy); libera(oo);
    return true;

erro:
    libera(fin);
    if (flags) libera(flags);
    if (xs) libera(xs);
    if (ys) libera(ys);
    return false;
}

/* Dibuja un glifo, que puede ser simple o compuesto. */
static bool tt_glifo_caminho(pdf_fonte_t *f, int gid, const mat2_t *m,
                             pdf_caminho_t *cam, int prof)
{
    uint32_t off, fim;
    const uint8_t *p;
    int tam;

    if (prof > 5) return false;
    if (gid < 0 || gid >= f->tt_glyfs) return false;
    if (!f->tt_loca || !f->tt_glyf) return false;

    if (f->tt_loca_longo) {
        if (8 + gid * 4 + 7 >= f->tam) return false;
        off = be32(f->tt_loca + gid * 4);
        fim = be32(f->tt_loca + gid * 4 + 4);
    } else {
        if (4 + gid * 4 + 3 >= f->tam) return false;
        off = (uint32_t)be16(f->tt_loca + gid * 2) * 2u;
        fim = (uint32_t)be16(f->tt_loca + gid * 2 + 2) * 2u;
    }
    if (fim <= off) return true;           /* glifo vacio (un espacio) */
    if (off >= (uint32_t)f->tt_glyf_tam) return false;

    p = f->tt_glyf + off;
    tam = (int)(fim - off);
    if (tam > f->tt_glyf_tam - (int)off) tam = f->tt_glyf_tam - (int)off;
    if (tam < 10) return false;

    {
        int ncont = sbe16(p);
        if (ncont >= 0) {
            return tt_contorno_simples(p, tam, ncont, m, cam);
        } else {
            /* Glifo compuesto: una lista de componentes con su matriz. */
            int pos = 10;
            for (;;) {
                uint16_t flags, gi;
                int arg1, arg2;
                mat2_t comp, total;

                if (pos + 4 > tam) return false;
                flags = be16(p + pos);
                gi = be16(p + pos + 2);
                pos += 4;

                if (flags & 0x0001) {
                    if (pos + 4 > tam) return false;
                    arg1 = sbe16(p + pos);
                    arg2 = sbe16(p + pos + 2);
                    pos += 4;
                } else {
                    if (pos + 2 > tam) return false;
                    arg1 = (int)(int8_t)p[pos];
                    arg2 = (int)(int8_t)p[pos + 1];
                    pos += 2;
                }

                comp.a = 1; comp.b = 0; comp.c = 0; comp.d = 1;
                comp.e = 0; comp.f = 0;
                if (flags & 0x0008) {
                    if (pos + 2 > tam) return false;
                    comp.a = comp.d = tt_f2dot14(p + pos);
                    pos += 2;
                } else if (flags & 0x0040) {
                    if (pos + 4 > tam) return false;
                    comp.a = tt_f2dot14(p + pos);
                    comp.d = tt_f2dot14(p + pos + 2);
                    pos += 4;
                } else if (flags & 0x0080) {
                    if (pos + 8 > tam) return false;
                    comp.a = tt_f2dot14(p + pos);
                    comp.b = tt_f2dot14(p + pos + 2);
                    comp.c = tt_f2dot14(p + pos + 4);
                    comp.d = tt_f2dot14(p + pos + 6);
                    pos += 8;
                }
                if (flags & 0x0002) {
                    comp.e = arg1;
                    comp.f = arg2;
                }

                mat2_comp(&total, m, &comp);
                if (!tt_glifo_caminho(f, gi, &total, cam, prof + 1)) return false;

                if (!(flags & 0x0020)) break;
            }
            return true;
        }
    }
}

/* ---- la carga ---- */

static bool tt_carregar(pdf_fonte_t *f)
{
    const uint8_t *t;
    int tam;

    if (!tt_tabela(f, "head", &t, &tam) || tam < 54) return false;
    f->tt_upem = be16(t + 18);
    if (f->tt_upem <= 0) f->tt_upem = 1000;
    f->tt_loca_longo = sbe16(t + 50);

    if (!tt_tabela(f, "maxp", &t, &tam) || tam < 6) return false;
    f->tt_glyfs = be16(t + 4);

    if (!tt_tabela(f, "hhea", &t, &tam) || tam < 36) return false;
    f->ascendente = (int)((double)sbe16(t + 4) * 1000.0 / f->tt_upem);
    f->descendente = (int)((double)sbe16(t + 6) * 1000.0 / f->tt_upem);
    f->tt_hmetrics = be16(t + 34);

    if (!tt_tabela(f, "hmtx", &f->tt_hmtx, &tam)) return false;

    if (!tt_tabela(f, "loca", &f->tt_loca, &tam)) return false;
    if (!tt_tabela(f, "glyf", &f->tt_glyf, &f->tt_glyf_tam)) return false;

    if (!tt_elegir_cmap(f)) return false;

    tt_nome(f);
    if (!f->nome[0]) copiar_nome(f->nome, FONE_NOME_MAX, "TrueType");
    tt_post(f);

    return true;
}

/* ---- el glifo ---- */

/* El indice de glifo de un código de caracter. */
static int tt_gid(pdf_fonte_t *f, int codigo)
{
    int c = codigo;

    if (f->dif[codigo] && f->dif[codigo][0]) {
        int g = tt_gid_por_nome(f, f->dif[codigo]);
        if (g > 0) return g;
    }
    if (f->tt_simbolo && c >= 0xf000 && c <= 0xf0ff) c -= 0xf000;
    return tt_cmap_gid(f, c);
}

static bool tt_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g)
{
    mat2_t raiz;
    int gid;

    if (codigo < 0 || codigo >= FONE_MAX_COD) return false;

    gid = tt_gid(f, codigo);
    g->largura = tt_avanca(f, gid);
    if (gid <= 0) return false;

    raiz.a = 1000.0 / (double)f->tt_upem;
    raiz.b = 0; raiz.c = 0;
    raiz.d = 1000.0 / (double)f->tt_upem;
    raiz.e = 0; raiz.f = 0;

    return tt_glifo_caminho(f, gid, &raiz, &g->contorno, 0);
}

/* El ancho de un código. */
static int tt_largura(pdf_fonte_t *f, int codigo)
{
    int gid;

    if (codigo < 0 || codigo >= FONE_MAX_COD) return 0;
    gid = tt_gid(f, codigo);
    return tt_avanca(f, gid);
}
/* Las 391 cadenas estándar del CFF (las que usan los SID de 0 a 390). */
static const char *const cff_cadenas_padrao[391] = {
    ".notdef", "space", "exclam", "quotedbl", "numbersign", "dollar",
    "percent", "ampersand", "quoteright", "parenleft", "parenright", "asterisk",
    "plus", "comma", "hyphen", "period", "slash", "zero",
    "one", "two", "three", "four", "five", "six",
    "seven", "eight", "nine", "colon", "semicolon", "less",
    "equal", "greater", "question", "at", "A", "B",
    "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N",
    "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "bracketleft", "backslash", "bracketright", "asciicircum", "underscore", "quoteleft",
    "a", "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k", "l",
    "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x",
    "y", "z", "braceleft", "bar", "braceright", "asciitilde",
    "exclamdown", "cent", "sterling", "fraction", "yen", "florin",
    "section", "currency", "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",
    "guilsinglright", "fi", "fl", "endash", "dagger", "daggerdbl",
    "periodcentered", "paragraph", "bullet", "quotesinglbase", "quotedblbase", "quotedblright",
    "guillemotright", "ellipsis", "perthousand", "questiondown", "grave", "acute",
    "circumflex", "tilde", "macron", "breve", "dotaccent", "dieresis",
    "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "emdash",
    "AE", "ordfeminine", "Lslash", "Oslash", "OE", "ordmasculine",
    "ae", "dotlessi", "lslash", "oslash", "oe", "germandbls",
    "onesuperior", "logicalnot", "mu", "trademark", "Eth", "onehalf",
    "plusminus", "Thorn", "onequarter", "divide", "brokenbar", "degree",
    "thorn", "threequarters", "twosuperior", "registered", "minus", "eth",
    "multiply", "threesuperior", "copyright", "Aacute", "Acircumflex", "Adieresis",
    "Agrave", "Aring", "Atilde", "Ccedilla", "Eacute", "Ecircumflex",
    "Edieresis", "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave",
    "Ntilde", "Oacute", "Ocircumflex", "Odieresis", "Ograve", "Otilde",
    "Scaron", "Uacute", "Ucircumflex", "Udieresis", "Ugrave", "Yacute",
    "Ydieresis", "Zcaron", "aacute", "acircumflex", "adieresis", "agrave",
    "aring", "atilde", "ccedilla", "eacute", "ecircumflex", "edieresis",
    "egrave", "iacute", "icircumflex", "idieresis", "igrave", "ntilde",
    "oacute", "ocircumflex", "odieresis", "ograve", "otilde", "scaron",
    "uacute", "ucircumflex", "udieresis", "ugrave", "yacute", "ydieresis",
    "zcaron", "exclamsmall", "Hungarumlautsmall", "dollaroldstyle", "dollarsuperior", "ampersandsmall",
    "Acutesmall", "parenleftsuperior", "parenrightsuperior", "twodotenleader", "onedotenleader", "zerooldstyle",
    "oneoldstyle", "twooldstyle", "threeoldstyle", "fouroldstyle", "fiveoldstyle", "sixoldstyle",
    "sevenoldstyle", "eightoldstyle", "nineoldstyle", "commasuperior", "threequartersemdash", "periodsuperior",
    "questionsmall", "asuperior", "bsuperior", "centsuperior", "dsuperior", "esuperior",
    "isuperior", "lsuperior", "msuperior", "nsuperior", "osuperior", "rsuperior",
    "ssuperior", "tsuperior", "ff", "ffi", "ffl", "parenleftinferior",
    "parenrightinferior", "Circumflexsmall", "hyphensuperior", "Gravesmall", "Asmall", "Bsmall",
    "Csmall", "Dsmall", "Esmall", "Fsmall", "Gsmall", "Hsmall",
    "Ismall", "Jsmall", "Ksmall", "Lsmall", "Msmall", "Nsmall",
    "Osmall", "Psmall", "Qsmall", "Rsmall", "Ssmall", "Tsmall",
    "Usmall", "Vsmall", "Wsmall", "Xsmall", "Ysmall", "Zsmall",
    "colonmonetary", "onefitted", "rupiah", "Tildesmall", "exclamdownsmall", "centoldstyle",
    "Lslashsmall", "Scaronsmall", "Zcaronsmall", "Dieresissmall", "Brevesmall", "Caronsmall",
    "Dotaccentsmall", "Macronsmall", "figuredash", "hypheninferior", "Ogoneksmall", "Ringsmall",
    "Cedillasmall", "questiondownsmall", "oneeighth", "threeeighths", "fiveeighths", "seveneighths",
    "onethird", "twothirds", "zerosuperior", "foursuperior", "fivesuperior", "sixsuperior",
    "sevensuperior", "eightsuperior", "ninesuperior", "zeroinferior", "oneinferior", "twoinferior",
    "threeinferior", "fourinferior", "fiveinferior", "sixinferior", "seveninferior", "eightinferior",
    "nineinferior", "centinferior", "dollarinferior", "periodinferior", "commainferior", "Agravesmall",
    "Aacutesmall", "Acircumflexsmall", "Atildesmall", "Adieresissmall", "Aringsmall", "AEsmall",
    "Ccedillasmall", "Egravesmall", "Eacutesmall", "Ecircumflexsmall", "Edieresissmall", "Igravesmall",
    "Iacutesmall", "Icircumflexsmall", "Idieresissmall", "Ethsmall", "Ntildesmall", "Ogravesmall",
    "Oacutesmall", "Ocircumflexsmall", "Otildesmall", "Odieresissmall", "OEsmall", "Oslashsmall",
    "Ugravesmall", "Uacutesmall", "Ucircumflexsmall", "Udieresissmall", "Yacutesmall", "Thornsmall",
    "Ydieresissmall", "001.000", "001.001", "001.002", "001.003", "Black",
    "Bold", "Book", "Light", "Medium", "Regular", "Roman",
    "Semibold",
};

/* La codificación estándar de Adobe: código -> SID. */
static const int16_t cff_encoding_padrao[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
    77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 93, 94, 95, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 96, 97, 98, 99, 100, 101, 102,
    103, 104, 105, 106, 107, 108, 109, 110, 0, 111, 112, 113,
    114, 0, 115, 116, 117, 118, 119, 120, 121, 122, 0, 123,
    0, 124, 125, 126, 127, 128, 129, 130, 131, 0, 132, 133,
    0, 134, 135, 136, 137, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 138, 0, 139,
    0, 0, 0, 0, 140, 141, 142, 143, 0, 0, 0, 0,
    0, 144, 0, 0, 0, 145, 0, 0, 146, 147, 148, 149,
    0, 0, 0, 0,
};
/* ==================================================================== CFF ===
 *
 * El programa de fuente CFF (FontFile3 con subtipo Type1C, o la tabla 'CFF '
 * de un OpenType) es un binario con:
 *
 *   cabecera    version, tamaño de la cabecera y tamaño de los desplazamientos
 *   Name INDEX  el nombre de la fuente
 *   Top DICT    el diccionario principal
 *   String INDEX las cadenas propias de la fuente
 *   Global Subr INDEX
 *
 * Un INDEX es una lista de objetos: un contador, el tamaño de los
 * desplazamientos y la tabla de desplazamientos, seguida de los datos.
 *
 * Los glifos son charstrings de tipo 2: una pila de números y operadores que
 * dibujan el contorno. Los subrs (locales y globales) se llaman con el sesgo
 * que dicta el número de subrs.
 */

/* Lee un INDEX. */
static uint32_t cff_idx_off(const cff_idx_t *ix, int i);

static bool cff_idx_le(const uint8_t *p, int max, cff_idx_t *ix)
{
    int count, offsize;

    ix->p = p;
    ix->n = 0;
    ix->offsize = 0;
    ix->base = p + 2;
    if (max < 2) return false;
    count = be16(p);
    ix->n = count;
    if (count == 0) return true;
    if (max < 3) return false;
    offsize = p[2];
    if (offsize < 1 || offsize > 4) return false;
    if (3 + (count + 1) * offsize > max) return false;
    ix->offsize = offsize;
    /* Los desplazamientos empiezan en 1: cuentan desde el byte ANTERIOR a
     * los datos, asi que 'base' es ese byte (el objeto i va en base+off[i]). */
    ix->base = p + 2 + (count + 1) * offsize;
    if (cff_idx_off(ix, 0) != 1) return false;
    return true;
}

/* El desplazamiento del objeto i. */
static uint32_t cff_idx_off(const cff_idx_t *ix, int i)
{
    const uint8_t *e = ix->p + 3 + i * ix->offsize;
    uint32_t v = 0;
    int k;

    for (k = 0; k < ix->offsize; k++) v = (v << 8) | e[k];
    return v;
}

/* El objeto i del INDEX (devuelve NULL si no cabe). */
static const uint8_t *cff_idx_obj(const cff_idx_t *ix, int i, int *tam,
                                  int max)
{
    uint32_t a, b;

    if (i < 0 || i >= ix->n) return NULL;
    a = cff_idx_off(ix, i);
    b = cff_idx_off(ix, i + 1);
    if (b < a) return NULL;
    if (max > 0 && (int)b > max) return NULL;
    if (tam) *tam = (int)(b - a);
    return ix->base + a;
}

/* ---- los diccionarios ---- */

/* Una entrada de diccionario: el operador y sus operandos. */
typedef struct {
    int    op;
    int    n;
    double v[6];
} cff_ent_t;

/* Convierte un texto numerico (el formato de los reales del CFF) en numero. */
static double cff_atof(const char *s)
{
    double val = 0.0;
    int neg = 0, expo = 0, negexp = 0;

    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0 + (double)(*s - '0');
        s++;
    }
    if (*s == '.') {
        double f = 0.1;
        s++;
        while (*s >= '0' && *s <= '9') {
            val += (double)(*s - '0') * f;
            f *= 0.1;
            s++;
        }
    }
    if (*s == 'E' || *s == 'e') {
        s++;
        if (*s == '-') { negexp = 1; s++; }
        while (*s >= '0' && *s <= '9') {
            expo = expo * 10 + (*s - '0');
            s++;
        }
    }
    while (expo > 0) {
        if (negexp) val /= 10.0; else val *= 10.0;
        expo--;
    }
    return neg ? -val : val;
}

/* Lee un numero real del CFF (los nibbles). */
static bool cff_real(const uint8_t *p, int *pos, int tam, double *val)
{
    char buf[64];
    int n = 0, fim = 0;

    while (!fim && *pos < tam) {
        uint8_t b = p[*pos];
        int k;

        (*pos)++;
        for (k = 0; k < 2 && !fim; k++) {
            int nib = (k == 0) ? (b >> 4) : (b & 0x0f);
            switch (nib) {
                case 0xa: if (n < 60) buf[n++] = '.'; break;
                case 0xb: if (n < 60) buf[n++] = 'E'; break;
                case 0xc: if (n < 60) { buf[n++] = 'E'; buf[n++] = '-'; } break;
                case 0xe: if (n < 60) buf[n++] = '-'; break;
                case 0xd: fim = 1; break;
                case 0xf: fim = 1; break;
                default:
                    if (nib >= 0 && nib <= 9 && n < 60)
                        buf[n++] = (char)('0' + nib);
                    break;
            }
        }
    }
    buf[n] = '\0';
    *val = cff_atof(buf);
    return true;
}

/* Lee un diccionario: devuelve cuantas entradas tiene. */
static int cff_dict(const uint8_t *p, int tam, cff_ent_t *ent, int max)
{
    int pos = 0, n = 0;

    while (pos < tam && n < max) {
        double ops[48];
        int nop = 0;
        int op;

        /* Los operandos. */
        while (pos < tam) {
            int b0 = p[pos];
            double v;

            if (b0 == 28) {
                if (pos + 2 >= tam) return n;
                v = sbe16(p + pos + 1);
                pos += 3;
            } else if (b0 == 29) {
                if (pos + 4 >= tam) return n;
                v = (double)sbe32(p + pos + 1);
                pos += 5;
            } else if (b0 == 30) {
                pos++;
                if (!cff_real(p, &pos, tam, &v)) return n;
            } else if (b0 >= 32 && b0 <= 246) {
                v = (double)(b0 - 139);
                pos++;
            } else if (b0 >= 247 && b0 <= 250) {
                if (pos + 1 >= tam) return n;
                v = (double)((b0 - 247) * 256 + p[pos + 1] + 108);
                pos += 2;
            } else if (b0 >= 251 && b0 <= 254) {
                if (pos + 1 >= tam) return n;
                v = (double)(-((b0 - 251) * 256) - p[pos + 1] - 108);
                pos += 2;
            } else {
                break;      /* es un operador */
            }
            if (nop < 48) ops[nop++] = v;
        }
        if (pos >= tam) break;

        /* El operador. */
        op = p[pos++];
        if (op == 12) {
            if (pos >= tam) break;
            op = 1200 + p[pos++];
        }
        ent[n].op = op;
        ent[n].n = nop;
        {
            int i;
            for (i = 0; i < nop && i < 6; i++) ent[n].v[i] = ops[i];
            for (i = nop; i < 6; i++) ent[n].v[i] = 0.0;
        }
        n++;
    }
    return n;
}

/* ---- el intérprete de charstrings de tipo 2 ---- */

typedef struct {
    const uint8_t *cs;
    int      n;
    int      pos;
    double   st[64];
    int      sp;
    double   x, y;
    pdf_caminho_t *cam;         /* NULL si solo se quiere el ancho */
    double   larg;              /* el ancho, en unidades de fuente */
    int      tem_larg;
    int      hints;             /* numero de stems vistos */
    cff_idx_t subrs;
    int      tem_subrs;
    cff_idx_t gsubrs;
    const mat2_t *m;
    double   ix, iy;            /* el comienzo del tramo actual */
    int      aberto;            /* hay un tramo sin cerrar */
    int      ancho_visto;       /* ya paso el primer operador (el del ancho) */
    double   larg_nom;          /* nominalWidthX */
    int      fim;               /* endchar */
} cs2_t;

static void cs2_linha(cs2_t *c, double x0, double y0, double x1, double y1)
{
    double a, b, d, e;

    if (!c->cam) return;
    mat2_aplica(c->m, x0, y0, &a, &b);
    mat2_aplica(c->m, x1, y1, &d, &e);
    pdf_caminho_linha(c->cam, a, b, d, e);
}

static void cs2_cubica(cs2_t *c, double x0, double y0,
                       double c1x, double c1y, double c2x, double c2y,
                       double x1, double y1)
{
    double a, b, d, e, f, g, h, i;

    if (!c->cam) return;
    mat2_aplica(c->m, x0, y0, &a, &b);
    mat2_aplica(c->m, c1x, c1y, &d, &e);
    mat2_aplica(c->m, c2x, c2y, &f, &g);
    mat2_aplica(c->m, x1, y1, &h, &i);
    pdf_caminho_cubica(c->cam, a, b, d, e, f, g, h, i);
}

/* El sesgo de los subrs, segun cuantos hay. */
static int cs2_sesgo(int n)
{
    if (n < 1240) return 107;
    if (n < 33900) return 1131;
    return 32768;
}

/* Lee un numero del charstring. */
static bool cs2_num(cs2_t *c, int b0, double *v)
{
    if (b0 <= 246) { *v = (double)(b0 - 139); return true; }
    if (b0 <= 250) {
        if (c->pos >= c->n) return false;
        *v = (double)((b0 - 247) * 256 + c->cs[c->pos++] + 108);
        return true;
    }
    if (b0 <= 254) {
        if (c->pos >= c->n) return false;
        *v = (double)(-((b0 - 251) * 256) - c->cs[c->pos++] - 108);
        return true;
    }
    if (b0 == 28) {
        if (c->pos + 1 >= c->n) return false;
        *v = (double)sbe16(c->cs + c->pos);
        c->pos += 2;
        return true;
    }
    if (b0 == 255) {
        if (c->pos + 3 >= c->n) return false;
        *v = (double)sbe32(c->cs + c->pos) / 65536.0;
        c->pos += 4;
        return true;
    }
    return false;
}

/* El flex (12 35), hflex (12 34), hflex1 (12 36) y flex1 (12 37): dos
 * curvas seguidas (Adobe TN 5177, apendice E). */
static void cs2_flex(cs2_t *c, int tipo)
{
    double d[14];
    double x = c->x, y = c->y;
    double p[7][2];
    int i;

    for (i = 0; i < 14; i++) d[i] = (i < c->sp) ? c->st[i] : 0.0;
    switch (tipo) {
        case 0:     /* flex: dx1 dy1 ... dx6 dy6 fd */
            if (c->sp < 12) { c->sp = 0; return; }
            for (i = 0; i < 6; i++) {
                x += d[i * 2];
                y += d[i * 2 + 1];
                p[i + 1][0] = x; p[i + 1][1] = y;
            }
            break;
        case 1:     /* hflex: dx1 dx2 dy2 dx3 dx4 dx5 dx6 */
            if (c->sp < 7) { c->sp = 0; return; }
            x += d[0];             p[1][0] = x; p[1][1] = y;
            x += d[1]; y += d[2];  p[2][0] = x; p[2][1] = y;
            x += d[3];             p[3][0] = x; p[3][1] = y;
            x += d[4];             p[4][0] = x; p[4][1] = y;
            x += d[5]; y = c->y;   p[5][0] = x; p[5][1] = y;
            x += d[6];             p[6][0] = x; p[6][1] = y;
            break;
        case 2:     /* hflex1: dx1 dy1 dx2 dy2 dx3 dx4 dx5 dy5 dx6 */
            if (c->sp < 9) { c->sp = 0; return; }
            x += d[0]; y += d[1];  p[1][0] = x; p[1][1] = y;
            x += d[2]; y += d[3];  p[2][0] = x; p[2][1] = y;
            x += d[4];             p[3][0] = x; p[3][1] = y;
            x += d[5];             p[4][0] = x; p[4][1] = y;
            x += d[6]; y += d[7];  p[5][0] = x; p[5][1] = y;
            x += d[8]; y = c->y;   p[6][0] = x; p[6][1] = y;
            break;
        default: {  /* flex1: dx1 dy1 ... dx5 dy5 d6 */
            if (c->sp < 11) { c->sp = 0; return; }
            for (i = 0; i < 5; i++) {
                x += d[i * 2];
                y += d[i * 2 + 1];
                p[i + 1][0] = x; p[i + 1][1] = y;
            }
            double dx = x - c->x, dy = y - c->y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx > dy) { x += d[10]; y = c->y; }
            else { x = c->x; y += d[10]; }
            p[6][0] = x; p[6][1] = y;
            break;
        }
    }
    cs2_cubica(c, c->x, c->y, p[1][0], p[1][1], p[2][0], p[2][1], p[3][0], p[3][1]);
    cs2_cubica(c, p[3][0], p[3][1], p[4][0], p[4][1], p[5][0], p[5][1], p[6][0], p[6][1]);
    c->x = p[6][0];
    c->y = p[6][1];
    c->sp = 0;
}

/* Cierra el tramo abierto (en Type 2 los contornos se cierran solos al
 * empezar otro o al terminar el glifo). */
static void cs2_fechar(cs2_t *c)
{
    if (c->aberto && (c->x != c->ix || c->y != c->iy))
        cs2_linha(c, c->x, c->y, c->ix, c->iy);
    c->aberto = 0;
}

/* El ancho va delante de los argumentos del primer operador que limpia la
 * pila, si sobra uno: 'normal' es cuantos argumentos lleva sin ancho (o -1
 * si van por pares). */
static void cs2_ancho(cs2_t *c, int normal)
{
    if (c->ancho_visto) return;
    c->ancho_visto = 1;
    bool sobra = (normal < 0) ? (c->sp % 2 == 1) : (c->sp > normal);
    if (sobra && c->sp > 0) {
        c->larg = c->st[0] + c->larg_nom;
        c->tem_larg = 1;
        for (int i = 1; i < c->sp; i++) c->st[i - 1] = c->st[i];
        c->sp--;
    }
}

static void cs2_mover(cs2_t *c, double dx, double dy)
{
    cs2_fechar(c);
    c->x += dx;
    c->y += dy;
    c->ix = c->x;
    c->iy = c->y;
}

static void cs2_ate(cs2_t *c, double x, double y)
{
    cs2_linha(c, c->x, c->y, x, y);
    c->x = x;
    c->y = y;
    c->aberto = 1;
}

static void cs2_curva(cs2_t *c, double c1x, double c1y, double c2x, double c2y, double x, double y)
{
    cs2_cubica(c, c->x, c->y, c1x, c1y, c2x, c2y, x, y);
    c->x = x;
    c->y = y;
    c->aberto = 1;
}

/* Ejecuta un charstring (o un subr). */
static bool cs2_executa(cs2_t *c, const uint8_t *cs, int n, int prof)
{
    const uint8_t *g_cs = c->cs;
    int g_pos = c->pos, g_n = c->n;
    int i;

    if (prof > 10) return false;
    c->cs = cs;
    c->n = n;
    c->pos = 0;

    while (c->pos < c->n && !c->fim) {
        int op = c->cs[c->pos++];

        if (op >= 32 || op == 28) {
            double v;
            if (!cs2_num(c, op, &v)) break;
            if (c->sp < 48) c->st[c->sp++] = v;
            continue;
        }

        switch (op) {
            case 1:     /* hstem */
            case 3:     /* vstem */
            case 18:    /* hstemhm */
            case 23:    /* vstemhm */
                cs2_ancho(c, -1);
                c->hints += c->sp / 2;
                c->sp = 0;
                break;
            case 19:    /* hintmask */
            case 20:    /* cntrmask */
                /* Los argumentos que haya delante son un vstem implicito. */
                cs2_ancho(c, -1);
                c->hints += c->sp / 2;
                c->sp = 0;
                c->pos += (c->hints + 7) / 8;
                break;
            case 21:    /* rmoveto */
                cs2_ancho(c, 2);
                if (c->sp >= 2) cs2_mover(c, c->st[c->sp - 2], c->st[c->sp - 1]);
                c->sp = 0;
                break;
            case 22:    /* hmoveto */
                cs2_ancho(c, 1);
                if (c->sp >= 1) cs2_mover(c, c->st[c->sp - 1], 0.0);
                c->sp = 0;
                break;
            case 4:     /* vmoveto */
                cs2_ancho(c, 1);
                if (c->sp >= 1) cs2_mover(c, 0.0, c->st[c->sp - 1]);
                c->sp = 0;
                break;
            case 5:     /* rlineto */
                for (i = 0; i + 1 < c->sp; i += 2)
                    cs2_ate(c, c->x + c->st[i], c->y + c->st[i + 1]);
                c->sp = 0;
                break;
            case 6:     /* hlineto */
            case 7:     /* vlineto */
                {
                    int horiz = (op == 6);
                    for (i = 0; i < c->sp; i++) {
                        if (horiz) cs2_ate(c, c->x + c->st[i], c->y);
                        else cs2_ate(c, c->x, c->y + c->st[i]);
                        horiz = !horiz;
                    }
                }
                c->sp = 0;
                break;
            case 8:     /* rrcurveto */
                for (i = 0; i + 5 < c->sp; i += 6) {
                    double c1x = c->x + c->st[i], c1y = c->y + c->st[i + 1];
                    double c2x = c1x + c->st[i + 2], c2y = c1y + c->st[i + 3];
                    cs2_curva(c, c1x, c1y, c2x, c2y, c2x + c->st[i + 4], c2y + c->st[i + 5]);
                }
                c->sp = 0;
                break;
            case 24:    /* rcurveline */
                for (i = 0; i + 5 < c->sp - 2; i += 6) {
                    double c1x = c->x + c->st[i], c1y = c->y + c->st[i + 1];
                    double c2x = c1x + c->st[i + 2], c2y = c1y + c->st[i + 3];
                    cs2_curva(c, c1x, c1y, c2x, c2y, c2x + c->st[i + 4], c2y + c->st[i + 5]);
                }
                if (c->sp >= 2 && i + 1 < c->sp)
                    cs2_ate(c, c->x + c->st[i], c->y + c->st[i + 1]);
                c->sp = 0;
                break;
            case 25:    /* rlinecurve */
                for (i = 0; i + 1 < c->sp - 6; i += 2)
                    cs2_ate(c, c->x + c->st[i], c->y + c->st[i + 1]);
                if (i + 5 < c->sp) {
                    double c1x = c->x + c->st[i], c1y = c->y + c->st[i + 1];
                    double c2x = c1x + c->st[i + 2], c2y = c1y + c->st[i + 3];
                    cs2_curva(c, c1x, c1y, c2x, c2y, c2x + c->st[i + 4], c2y + c->st[i + 5]);
                }
                c->sp = 0;
                break;
            case 26:    /* vvcurveto: dx1? {dya dxb dyb dyc}+ */
                i = 0;
                {
                    double dx1 = 0.0;
                    if (c->sp % 4 == 1) dx1 = c->st[i++];
                    for (; i + 3 < c->sp; i += 4) {
                        double c1x = c->x + dx1, c1y = c->y + c->st[i];
                        double c2x = c1x + c->st[i + 1], c2y = c1y + c->st[i + 2];
                        cs2_curva(c, c1x, c1y, c2x, c2y, c2x, c2y + c->st[i + 3]);
                        dx1 = 0.0;
                    }
                }
                c->sp = 0;
                break;
            case 27:    /* hhcurveto: dy1? {dxa dxb dyb dxc}+ */
                i = 0;
                {
                    double dy1 = 0.0;
                    if (c->sp % 4 == 1) dy1 = c->st[i++];
                    for (; i + 3 < c->sp; i += 4) {
                        double c1x = c->x + c->st[i], c1y = c->y + dy1;
                        double c2x = c1x + c->st[i + 1], c2y = c1y + c->st[i + 2];
                        cs2_curva(c, c1x, c1y, c2x, c2y, c2x + c->st[i + 3], c2y);
                        dy1 = 0.0;
                    }
                }
                c->sp = 0;
                break;
            case 30:    /* vhcurveto */
            case 31:    /* hvcurveto */
                {
                    /* Las curvas alternan; si sobra un argumento, es el
                     * desplazamiento final de la ultima en el otro eje. */
                    int horiz = (op == 31);
                    i = 0;
                    while (i + 3 < c->sp) {
                        bool ultima = (i + 4 >= c->sp - 1);
                        double extra = (ultima && c->sp - i == 5) ? c->st[i + 4] : 0.0;
                        double c1x, c1y, c2x, c2y, fx, fy;
                        if (horiz) {
                            c1x = c->x + c->st[i]; c1y = c->y;
                            c2x = c1x + c->st[i + 1]; c2y = c1y + c->st[i + 2];
                            fy = c2y + c->st[i + 3]; fx = c2x + extra;
                        } else {
                            c1x = c->x; c1y = c->y + c->st[i];
                            c2x = c1x + c->st[i + 1]; c2y = c1y + c->st[i + 2];
                            fx = c2x + c->st[i + 3]; fy = c2y + extra;
                        }
                        cs2_curva(c, c1x, c1y, c2x, c2y, fx, fy);
                        i += 4;
                        horiz = !horiz;
                    }
                }
                c->sp = 0;
                break;
            case 10:    /* callsubr */
            case 29:    /* callgsubr */
                {
                    const cff_idx_t *ix;
                    const uint8_t *sc;
                    int sn, k;

                    if (c->sp < 1) break;
                    k = (int)c->st[--c->sp];
                    if (op == 10) {
                        if (!c->tem_subrs) break;
                        ix = &c->subrs;
                    } else {
                        ix = &c->gsubrs;
                    }
                    k += cs2_sesgo(ix->n);
                    if (k < 0 || k >= ix->n) break;
                    sc = cff_idx_obj(ix, k, &sn, 0);
                    if (!sc) break;
                    cs2_executa(c, sc, sn, prof + 1);
                }
                break;
            case 11:    /* return */
                c->cs = g_cs;
                c->pos = g_pos;
                c->n = g_n;
                return true;
            case 14:    /* endchar (con 4 argumentos seria un seac: no se dibuja el acento) */
                cs2_ancho(c, (c->sp >= 4) ? 4 : 0);
                cs2_fechar(c);
                c->sp = 0;
                c->fim = 1;
                break;
            case 12: {  /* operadores de dos bytes */
                int op2;
                if (c->pos >= c->n) break;
                op2 = c->cs[c->pos++];
                switch (op2) {
                    case 34: cs2_flex(c, 1); c->aberto = 1; break;    /* hflex */
                    case 35: cs2_flex(c, 0); c->aberto = 1; break;    /* flex */
                    case 36: cs2_flex(c, 2); c->aberto = 1; break;    /* hflex1 */
                    case 37: cs2_flex(c, 3); c->aberto = 1; break;    /* flex1 */
                    default: c->sp = 0; break;
                }
                break;
            }
            default:
                c->sp = 0;
                break;
        }
    }

    c->cs = g_cs;
    c->pos = g_pos;
    c->n = g_n;
    return true;
}

/* ---- la carga ---- */

/* El nombre de un SID (las cadenas estándar o las de la fuente). */
static const char *cff_sid_nome(pdf_fonte_t *f, int sid, cff_idx_t *strings)
{
    int tam;
    const uint8_t *p;
    static char buf[128];

    if (sid < 0) return NULL;
    if (sid < 391) return cff_cadenas_padrao[sid];
    p = cff_idx_obj(strings, sid - 391, &tam, f->cff_tam);
    if (!p) return NULL;
    {
        int n = tam < 127 ? tam : 127;
        memcpy(buf, p, (size_t)n);
        buf[n] = '\0';
    }
    return buf;
}

/* El final de un INDEX (donde empieza el siguiente). */
static const uint8_t *cff_idx_fim(const cff_idx_t *ix)
{
    if (ix->n == 0) return ix->base;
    return ix->base + cff_idx_off(ix, ix->n);
}

static bool cff_carregar(pdf_fonte_t *f)
{
    const uint8_t *base = f->dados;
    int tam = f->tam;
    cff_idx_t nome, top, strings, gsubrs;
    cff_ent_t ent[32];
    int nent, i;
    int off_chars = 0, off_priv = 0, tam_priv = 0;
    int off_charset = 0, off_encoding = 0;
    int fd_array = 0, fd_select = 0;
    double matriz[6] = { 0.001, 0, 0, 0.001, 0, 0 };
    double bbox[2] = { 0, 0 };
    bool tem_bbox = false;
    bool tem_matriz = false;
    const uint8_t *p;
    int t;
    int nchars;
    int *sids;

    /* Si viene dentro de un OpenType, se coge la tabla 'CFF '. */
    {
        const uint8_t *tab;
        int ttam;
        if (tt_tabela(f, "CFF ", &tab, &ttam) && ttam > 4) {
            base = tab;
            tam = ttam;
            f->cff_tam = ttam;
        }
    }
    f->cff_tam = tam;
    f->cff_mat[0] = 1.0; f->cff_mat[1] = 0.0;
    f->cff_mat[2] = 0.0; f->cff_mat[3] = 1.0;
    f->cff_mat[4] = 0.0; f->cff_mat[5] = 0.0;
    for (i = 0; i < FONE_MAX_COD; i++) f->cff_cod[i] = -1;

    if (tam < 4) return false;
    if (base[0] != 1) return false;
    if (base[2] < 4 || base[2] > tam) return false;

    p = base + base[2];
    if (!cff_idx_le(p, tam - base[2], &nome)) return false;
    p = cff_idx_fim(&nome);
    if (p > base + tam) return false;
    if (!cff_idx_le(p, tam - (int)(p - base), &top)) return false;
    p = cff_idx_fim(&top);
    if (p > base + tam) return false;
    if (!cff_idx_le(p, tam - (int)(p - base), &strings)) return false;
    p = cff_idx_fim(&strings);
    if (p > base + tam) return false;
    if (!cff_idx_le(p, tam - (int)(p - base), &gsubrs)) return false;

    /* El nombre de la fuente. */
    {
        const uint8_t *nn = cff_idx_obj(&nome, 0, &t, tam);
        if (nn) {
            char buf[FONE_NOME_MAX];
            int n = t < FONE_NOME_MAX - 1 ? t : FONE_NOME_MAX - 1;
            memcpy(buf, nn, (size_t)n);
            buf[n] = '\0';
            copiar_nome(f->nome, FONE_NOME_MAX, buf);
        }
    }

    /* El diccionario principal. */
    p = cff_idx_obj(&top, 0, &t, tam);
    if (!p) return false;
    nent = cff_dict(p, t, ent, 32);
    for (i = 0; i < nent; i++) {
        switch (ent[i].op) {
            case 17: off_chars = (int)ent[i].v[0]; break;
            case 18:
                tam_priv = (int)ent[i].v[0];
                off_priv = (int)ent[i].v[1];
                break;
            case 15: off_charset = (int)ent[i].v[0]; break;
            case 16: off_encoding = (int)ent[i].v[0]; break;
            case 5:
                if (ent[i].n >= 4) {
                    bbox[0] = ent[i].v[1];
                    bbox[1] = ent[i].v[3];
                    tem_bbox = true;
                }
                break;
            case 1207:
                if (ent[i].n >= 6 && ent[i].v[0] != 0.0) {
                    for (t = 0; t < 6; t++) matriz[t] = ent[i].v[t];
                    tem_matriz = true;
                }
                break;
            case 1230: f->cff_cid = 1; break;
            case 1236: fd_array = (int)ent[i].v[0]; break;
            case 1237: fd_select = (int)ent[i].v[0]; break;
            default: break;
        }
    }
    (void)tem_matriz;   /* sin /FontMatrix vale la de por defecto (1/1000) */
    /* La matriz del CFF lleva las unidades de la fuente a em; aqui se pasa
     * a 1/1000 de em. */
    for (i = 0; i < 6; i++) f->cff_mat[i] = matriz[i] * 1000.0;
    f->cff_escala = f->cff_mat[0];
    if (tem_bbox && bbox[1] > bbox[0]) {
        f->ascendente = (int)(bbox[1] * f->cff_mat[3]);
        f->descendente = (int)(bbox[0] * f->cff_mat[3]);
    }

    if (off_chars <= 0 || off_chars >= tam) return false;

    /* Los charstrings. */
    if (!cff_idx_le(base + off_chars, tam - off_chars, &f->cff_chars_idx)) return false;
    nchars = f->cff_chars_idx.n;
    if (nchars <= 0) return false;
    f->cff_gsubrs_idx = gsubrs;

    /* El diccionario privado. */
    f->cff_larg_def = 0;
    f->cff_larg_nom = 0;
    if (off_priv > 0 && off_priv < tam) {
        cff_ent_t pent[16];
        int npent;
        int off_subrs = 0;
        const uint8_t *pp = base + off_priv;
        int pmax = tam - off_priv;
        if (tam_priv > 0 && tam_priv < pmax) pmax = tam_priv;

        npent = cff_dict(pp, pmax, pent, 16);
        for (i = 0; i < npent; i++) {
            switch (pent[i].op) {
                case 19: off_subrs = (int)pent[i].v[0]; break;
                case 20: f->cff_larg_def = (int)pent[i].v[0]; break;
                case 21: f->cff_larg_nom = (int)pent[i].v[0]; break;
                default: break;
            }
        }
        if (off_subrs > 0 && off_priv + off_subrs < tam) {
            if (cff_idx_le(base + off_priv + off_subrs,
                           tam - off_priv - off_subrs, &f->cff_subrs_idx))
                f->cff_tem_subrs = 1;
        }
    }

    /* Los nombres de los glifos (el charset). */
    sids = (int *)aloca((size_t)nchars * sizeof(int) + 4);
    if (!sids) return false;
    for (i = 0; i < nchars; i++) sids[i] = i;   /* por defecto, el SID es el gid */
    if (off_charset > 2 && off_charset < tam) {
        /* El charset da el SID (o el CID) de cada glifo desde el 1. */
        const uint8_t *cs = base + off_charset;
        int resto = tam - off_charset;
        int fmt = cs[0];
        int n = 1, pos = 1;
        if (fmt == 0) {
            while (n < nchars && pos + 2 <= resto) {
                sids[n++] = be16(cs + pos);
                pos += 2;
            }
        } else if (fmt == 1 || fmt == 2) {
            int tam_resto = (fmt == 1) ? 1 : 2;
            while (n < nchars && pos + 2 + tam_resto <= resto) {
                int primeiro = be16(cs + pos);
                int sobra = (fmt == 1) ? cs[pos + 2] : be16(cs + pos + 2);
                pos += 2 + tam_resto;
                for (i = 0; i <= sobra && n < nchars; i++) sids[n++] = primeiro + i;
            }
        }
    }
    f->cff_charset = (char **)aloca_zerado((size_t)nchars * sizeof(char *));
    if (!f->cff_charset) { libera(sids); return false; }
    f->cff_ncharset = nchars;
    for (i = 0; i < nchars; i++) {
        const char *nm = cff_sid_nome(f, sids[i], &strings);
        int n;
        if (!nm) continue;
        n = 0;
        while (nm[n] && n < 63) n++;
        f->cff_charset[i] = (char *)aloca((size_t)n + 1);
        if (!f->cff_charset[i]) continue;
        memcpy(f->cff_charset[i], nm, (size_t)n);
        f->cff_charset[i][n] = '\0';
    }

    /* La codificacion: el SID de cada codigo. */
    {
        int enc_sid[256];
        for (i = 0; i < 256; i++) enc_sid[i] = cff_encoding_padrao[i];
        int enc_gid[256];
        for (i = 0; i < 256; i++) enc_gid[i] = -1;
        if (off_encoding > 1 && off_encoding < tam) {
            /* Una codificacion propia: dice que codigo tiene cada glifo
             * (el glifo i+1 lleva el codigo i-esimo), no SID. */
            const uint8_t *en = base + off_encoding;
            int resto = tam - off_encoding;
            int fmt = en[0] & 0x7f;
            for (i = 0; i < 256; i++) enc_sid[i] = 0;
            if (fmt == 0 && resto >= 2) {
                int ncod = en[1];
                for (i = 0; i < ncod && 2 + i < resto; i++)
                    if (i + 1 < nchars) enc_gid[en[2 + i]] = i + 1;
            } else if (fmt == 1 && resto >= 2) {
                int nrang = en[1], g = 1;
                for (i = 0; i < nrang && 2 + i * 2 + 1 < resto; i++) {
                    int primeiro = en[2 + i * 2], sobra = en[3 + i * 2];
                    for (int k = 0; k <= sobra && primeiro + k < 256 && g < nchars; k++)
                        enc_gid[primeiro + k] = g++;
                }
            }
        }
        for (i = 0; i < 256; i++)
            if (enc_gid[i] >= 0) f->cff_cod[i] = (int16_t)enc_gid[i];
        /* El codigo apunta al glifo cuyo charset tiene el mismo SID. */
        for (i = 0; i < 256; i++) {
            int g;
            if (enc_sid[i] <= 0 || f->cff_cod[i] >= 0) continue;
            for (g = 0; g < nchars; g++) {
                if (sids[g] == enc_sid[i]) { f->cff_cod[i] = (int16_t)g; break; }
            }
        }
    }

    /* En una fuente CID el charset da el CID de cada glifo. */
    f->cff_nglifos = nchars;
    if (f->cff_cid) {
        int maxcid = 0;
        for (i = 0; i < nchars; i++) if (sids[i] > maxcid) maxcid = sids[i];
        if (maxcid < 65536) {
            f->cff_cid_gid = (uint16_t *)aloca_zerado((size_t)(maxcid + 1) * sizeof(uint16_t));
            if (f->cff_cid_gid) {
                f->cff_cid_max = maxcid;
                for (i = nchars - 1; i >= 1; i--) f->cff_cid_gid[sids[i]] = (uint16_t)i;
            }
        }
    }
    libera(sids);

    /* Las fuentes CID: el FDArray y el FDSelect. */
    if (fd_array > 0 && fd_array < tam) {
        cff_idx_t fdarray;
        if (cff_idx_le(base + fd_array, tam - fd_array, &fdarray)) {
            int nfd = fdarray.n;
            if (nfd > 32) nfd = 32;
            f->cff_nfd = nfd;
            for (i = 0; i < nfd; i++) {
                const uint8_t *dp = cff_idx_obj(&fdarray, i, &t, tam);
                cff_ent_t dent[16];
                int nd, k;
                int off_subrs = 0, priv = 0, priv_tam = 0;
                if (!dp) continue;
                nd = cff_dict(dp, t, dent, 16);
                for (k = 0; k < nd; k++) {
                    if (dent[k].op == 18) {
                        priv_tam = (int)dent[k].v[0];
                        priv = (int)dent[k].v[1];
                    }
                }
                f->cff_fd_larg_def[i] = 0;
                f->cff_fd_larg_nom[i] = 0;
                if (priv > 0 && priv < tam) {
                    cff_ent_t pent[16];
                    int np;
                    int pmax = tam - priv;
                    if (priv_tam > 0 && priv_tam < pmax) pmax = priv_tam;
                    np = cff_dict(base + priv, pmax, pent, 16);
                    for (k = 0; k < np; k++) {
                        if (pent[k].op == 19) off_subrs = (int)pent[k].v[0];
                        else if (pent[k].op == 20) f->cff_fd_larg_def[i] = (int)pent[k].v[0];
                        else if (pent[k].op == 21) f->cff_fd_larg_nom[i] = (int)pent[k].v[0];
                    }
                    if (off_subrs > 0 && priv + off_subrs < tam) {
                        cff_idx_t si;
                        if (cff_idx_le(base + priv + off_subrs,
                                       tam - priv - off_subrs, &si))
                        {
                            f->cff_fd_subrs[i] = si;
                            f->cff_fd_tem_subrs[i] = 1;
                        }
                    }
                }
            }
        }
    }
    if (fd_select > 0 && fd_select < tam) {
        const uint8_t *fs = base + fd_select;
        f->cff_fdselect = fs;
        f->cff_fdselect_fmt = fs[0];
    }

    if (!f->nome[0]) copiar_nome(f->nome, FONE_NOME_MAX, "CFF");
    return true;
}

/* El indice de glifo de un codigo. */
static int cff_gid(pdf_fonte_t *f, int codigo)
{
    if (codigo < 0 || codigo >= FONE_MAX_COD) return -1;
    if (f->dif[codigo] && f->dif[codigo][0]) {
        int i;
        for (i = 0; i < f->cff_ncharset; i++) {
            if (f->cff_charset[i] &&
                igual(f->cff_charset[i], f->dif[codigo]) == 0)
                return i;
        }
    }
    return f->cff_cod[codigo];
}

/* El FD (fuentes CID) de un glifo, segun el FDSelect. */
static int cff_fd_do_glifo(pdf_fonte_t *f, int gid)
{
    const uint8_t *fs = f->cff_fdselect;
    const uint8_t *fim = f->dados + f->tam;
    int fd = 0;
    if (!fs) return 0;
    if (f->cff_fdselect_fmt == 0) {
        if (fs + 1 + gid < fim) fd = fs[1 + gid];
    } else if (f->cff_fdselect_fmt == 3 && fs + 3 <= fim) {
        int nr = be16(fs + 1);
        for (int i = 0; i < nr; i++) {
            const uint8_t *r = fs + 3 + i * 3;
            if (r + 5 > fim) break;
            if (gid >= be16(r) && gid < be16(r + 3)) { fd = r[2]; break; }
        }
    }
    if (fd < 0 || fd >= f->cff_nfd) fd = 0;
    return fd;
}

/* Ejecuta un charstring del CFF. */
static void cff_executa(pdf_fonte_t *f, int gid, pdf_caminho_t *cam,
                        double *larg)
{
    cs2_t c;
    const uint8_t *cs;
    int n = 0;
    mat2_t m;

    cs = cff_idx_obj(&f->cff_chars_idx, gid, &n, f->cff_tam);
    if (!cs) {
        if (larg) *larg = 0.0;
        return;
    }

    m.a = f->cff_mat[0]; m.b = f->cff_mat[1];
    m.c = f->cff_mat[2]; m.d = f->cff_mat[3];
    m.e = f->cff_mat[4]; m.f = f->cff_mat[5];

    memset(&c, 0, sizeof(c));
    c.cs = cs; c.n = n; c.pos = 0;
    c.x = 0; c.y = 0;
    c.cam = cam;
    c.subrs = f->cff_subrs_idx;
    c.tem_subrs = f->cff_tem_subrs;
    c.gsubrs = f->cff_gsubrs_idx;
    c.m = &m;
    c.larg_nom = f->cff_larg_nom;
    double larg_def = f->cff_larg_def;
    if (f->cff_nfd > 0) {
        /* Fuente CID: los subrs locales y los anchos son los de su FD. */
        int fd = cff_fd_do_glifo(f, gid);
        c.subrs = f->cff_fd_subrs[fd];
        c.tem_subrs = f->cff_fd_tem_subrs[fd];
        c.larg_nom = f->cff_fd_larg_nom[fd];
        larg_def = f->cff_fd_larg_def[fd];
    }

    cs2_executa(&c, cs, n, 0);
    if (!c.fim) cs2_fechar(&c);   /* sin endchar: se cierra igual */

    if (larg) {
        if (c.tem_larg) *larg = c.larg * f->cff_escala;
        else *larg = larg_def * f->cff_escala;
    }
}

static bool cff_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g)
{
    int gid;
    double larg = 0.0;

    gid = cff_gid(f, codigo);
    if (gid < 0) {
        g->largura = 0;
        return false;
    }
    cff_executa(f, gid, &g->contorno, &larg);
    g->largura = (int)(larg + 0.5);
    return true;
}

static int cff_largura(pdf_fonte_t *f, int codigo)
{
    int gid;
    double larg = 0.0;

    gid = cff_gid(f, codigo);
    if (gid < 0) return 0;
    cff_executa(f, gid, NULL, &larg);
    return (int)(larg + 0.5);
}
/* ================================================================== Type 1 ===
 *
 * El programa de fuente Type 1 (FontFile) es PostScript:
 *
 *   la parte clara   /FontName, /FontMatrix, /FontBBox, /Encoding...
 *   el bloque eexec  cifrado: ahi estan el diccionario privado, los subrs y
 *                    los charstrings, cada uno con el formato
 *                    "/nombre longitud RD <bytes> ND".
 *   cleartomark       el final.
 *
 * El bloque eexec va cifrado con el cifrado propio de Type 1 (Adobe Type 1
 * Font Format, cap. 7): r = 55665, c1 = 52845, c2 = 22719, y los cuatro
 * primeros bytes descifrados se tiran. Puede venir en binario (lo normal
 * en un PDF) o en hexadecimal (PFA). Cada charstring y cada subr van
 * cifrados otra vez, con r = 4330, y llevan lenIV bytes (4) delante.
 *
 * Los charstrings son de tipo 1: una pila de numeros y operadores. Los
 * callothersubr son el truco de los hints: el 0 y el 2 son el flex (dos
 * curvas) y el 3 es el reemplazo de hints.
 */

/* El cifrado de Type 1 (eexec con r = 55665, charstrings con r = 4330). */
static void t1_r_cifra(uint8_t *d, int n, int r)
{
    int i;

    /* Descifra en su sitio: la clave avanza con el byte CIFRADO. */
    for (i = 0; i < n; i++) {
        uint8_t cifrado = d[i];
        d[i] = (uint8_t)(cifrado ^ (r >> 8));
        r = (int)(((uint32_t)(cifrado + r) * 52845u + 22719u) & 0xffffu);
    }
}

/* ---- el intérprete de charstrings de tipo 1 ---- */

typedef struct {
    const uint8_t *cs;
    int      n;
    int      pos;
    double   st[64];
    int      sp;
    double   x, y;
    double   ix, iy;          /* el comienzo del tramo actual */
    pdf_caminho_t *cam;
    double   larg;            /* el ancho, en unidades de fuente */
    int      tem_larg;
    double   sbx;             /* el desplazamiento lateral */
    pdf_fonte_t *f;
    double   mat[6];
    double   flexx[7], flexy[7];
    int      nflex;
    int      flex_ativo;
    double   flex_x0, flex_y0;    /* el punto antes del flex */
    double   ps[16];              /* la pila de PostScript (callothersubr/pop) */
    int      nps;
    double   ox, oy;              /* desplazamiento (el acento de un seac) */
} t1_t;

static void t1_linha(t1_t *c, double x0, double y0, double x1, double y1)
{
    double a, b, d, e;

    if (!c->cam) return;
    x0 += c->ox; y0 += c->oy; x1 += c->ox; y1 += c->oy;
    a = c->mat[0] * x0 + c->mat[2] * y0 + c->mat[4];
    b = c->mat[1] * x0 + c->mat[3] * y0 + c->mat[5];
    d = c->mat[0] * x1 + c->mat[2] * y1 + c->mat[4];
    e = c->mat[1] * x1 + c->mat[3] * y1 + c->mat[5];
    pdf_caminho_linha(c->cam, a, b, d, e);
}

static void t1_cubica(t1_t *c, double x0, double y0,
                      double c1x, double c1y, double c2x, double c2y,
                      double x1, double y1)
{
    double a, b, d, e, f, g, h, i;

    if (!c->cam) return;
    x0 += c->ox; y0 += c->oy; c1x += c->ox; c1y += c->oy;
    c2x += c->ox; c2y += c->oy; x1 += c->ox; y1 += c->oy;
    a = c->mat[0] * x0 + c->mat[2] * y0 + c->mat[4];
    b = c->mat[1] * x0 + c->mat[3] * y0 + c->mat[5];
    d = c->mat[0] * c1x + c->mat[2] * c1y + c->mat[4];
    e = c->mat[1] * c1x + c->mat[3] * c1y + c->mat[5];
    f = c->mat[0] * c2x + c->mat[2] * c2y + c->mat[4];
    g = c->mat[1] * c2x + c->mat[3] * c2y + c->mat[5];
    h = c->mat[0] * x1 + c->mat[2] * y1 + c->mat[4];
    i = c->mat[1] * x1 + c->mat[3] * y1 + c->mat[5];
    pdf_caminho_cubica(c->cam, a, b, d, e, f, g, h, i);
}

/* Lee un numero del charstring de tipo 1. */
static bool t1_num(t1_t *c, int b0, double *v)
{
    if (b0 <= 246) { *v = (double)(b0 - 139); return true; }
    if (b0 <= 250) {
        if (c->pos >= c->n) return false;
        *v = (double)((b0 - 247) * 256 + c->cs[c->pos++] + 108);
        return true;
    }
    if (b0 <= 254) {
        if (c->pos >= c->n) return false;
        *v = (double)(-((b0 - 251) * 256) - c->cs[c->pos++] - 108);
        return true;
    }
    if (b0 == 255) {
        if (c->pos + 3 >= c->n) return false;
        *v = (double)sbe32(c->cs + c->pos);
        c->pos += 4;
        return true;
    }
    return false;
}

/* El nombre de un codigo en la codificacion estandar de Adobe. */
static const char *t1_nombre_estandar(int codigo)
{
    int sid;

    if (codigo < 0 || codigo > 255) return NULL;
    sid = cff_encoding_padrao[codigo];
    if (sid <= 0 || sid >= 391) return NULL;
    return cff_cadenas_padrao[sid];
}

/* Ejecuta un charstring de tipo 1. */
static bool t1_executa(t1_t *c, const uint8_t *cs, int n, int prof)
{
    const uint8_t *g_cs = c->cs;
    int g_pos = c->pos, g_n = c->n;

    if (prof > 10) return false;
    c->cs = cs;
    c->n = n;
    c->pos = 0;

    while (c->pos < c->n) {
        int op = c->cs[c->pos++];

        if (op >= 32) {
            double v;
            if (!t1_num(c, op, &v)) break;
            if (c->sp < 64) c->st[c->sp++] = v;
            continue;
        }

        switch (op) {
            case 1:     /* hstem */
            case 3:     /* vstem */
                c->sp = 0;
                break;
            case 4:     /* vmoveto */
                if (c->sp >= 1) {
                    c->y += c->st[0];
                    if (!c->flex_ativo) { c->ix = c->x; c->iy = c->y; }
                }
                c->sp = 0;
                break;
            case 21:    /* rmoveto */
                if (c->sp >= 2) {
                    c->x += c->st[0];
                    c->y += c->st[1];
                    if (!c->flex_ativo) { c->ix = c->x; c->iy = c->y; }
                }
                c->sp = 0;
                break;
            case 22:    /* hmoveto */
                if (c->sp >= 1) {
                    c->x += c->st[0];
                    if (!c->flex_ativo) { c->ix = c->x; c->iy = c->y; }
                }
                c->sp = 0;
                break;
            case 5:     /* rlineto */
                {
                    int i;
                    for (i = 0; i + 1 < c->sp; i += 2) {
                        t1_linha(c, c->x, c->y, c->x + c->st[i], c->y + c->st[i + 1]);
                        c->x += c->st[i];
                        c->y += c->st[i + 1];
                    }
                }
                c->sp = 0;
                break;
            case 6:     /* hlineto */
            case 7:     /* vlineto */
                {
                    int i;
                    int horiz = (op == 6);
                    for (i = 0; i < c->sp; i++) {
                        if (horiz) {
                            t1_linha(c, c->x, c->y, c->x + c->st[i], c->y);
                            c->x += c->st[i];
                        } else {
                            t1_linha(c, c->x, c->y, c->x, c->y + c->st[i]);
                            c->y += c->st[i];
                        }
                        horiz = !horiz;
                    }
                }
                c->sp = 0;
                break;
            case 8:     /* rrcurveto */
                {
                    int i;
                    for (i = 0; i + 5 < c->sp; i += 6) {
                        double c1x = c->x + c->st[i], c1y = c->y + c->st[i + 1];
                        double c2x = c1x + c->st[i + 2], c2y = c1y + c->st[i + 3];
                        double fx = c2x + c->st[i + 4], fy = c2y + c->st[i + 5];
                        t1_cubica(c, c->x, c->y, c1x, c1y, c2x, c2y, fx, fy);
                        c->x = fx; c->y = fy;
                    }
                }
                c->sp = 0;
                break;
            case 30:    /* vhcurveto */
            case 31:    /* hvcurveto */
                {
                    int i = 0;
                    int horiz = (op == 31);
                    while (i + 3 < c->sp) {
                        double c1x, c1y, c2x, c2y, fx, fy;
                        if (horiz) {
                            c1x = c->x + c->st[i]; c1y = c->y;
                            c2x = c1x + c->st[i + 1]; c2y = c1y + c->st[i + 2];
                            fx = c2x; fy = c2y + c->st[i + 3];
                        } else {
                            c1x = c->x; c1y = c->y + c->st[i];
                            c2x = c1x + c->st[i + 1]; c2y = c1y + c->st[i + 2];
                            fx = c2x + c->st[i + 3]; fy = c2y;
                        }
                        t1_cubica(c, c->x, c->y, c1x, c1y, c2x, c2y, fx, fy);
                        c->x = fx; c->y = fy;
                        i += 4;
                        horiz = !horiz;
                    }
                }
                c->sp = 0;
                break;
            case 9:     /* closepath */
                /* Al contrario que en PostScript, el closepath de Type 1 NO
                 * mueve el punto actual: el rmoveto siguiente es relativo al
                 * punto de antes del closepath. */
                t1_linha(c, c->x, c->y, c->ix, c->iy);
                c->sp = 0;
                break;
            case 10:    /* callsubr */
                {
                    int i;
                    if (c->sp < 1) break;
                    i = (int)c->st[--c->sp];
                    if (i < 0 || i >= c->f->t1_subrs) break;
                    if (!c->f->t1_chars) break;
                    {
                        const uint8_t *sub = c->f->t1_subrs_dados[i];
                        int sn = c->f->t1_subrs_tam[i];
                        if (sub) t1_executa(c, sub, sn, prof + 1);
                    }
                }
                break;
            case 11:    /* return */
                c->cs = g_cs; c->pos = g_pos; c->n = g_n;
                return true;
            case 13:    /* hsbw */
                if (c->sp >= 2) {
                    c->sbx = c->st[0];
                    c->larg = c->st[1];
                    c->tem_larg = 1;
                    c->x = c->sbx;
                    c->y = 0;
                    c->ix = c->x; c->iy = c->y;
                }
                c->sp = 0;
                break;
            case 14:    /* endchar */
                c->sp = 0;
                c->cs = g_cs; c->pos = g_pos; c->n = g_n;
                return true;
            case 12: {  /* operadores de dos bytes */
                int op2;
                if (c->pos >= c->n) break;
                op2 = c->cs[c->pos++];
                switch (op2) {
                    case 0:     /* dotsection */
                        c->sp = 0;
                        break;
                    case 1:     /* vstem3 */
                    case 2:     /* hstem3 */
                        c->sp = 0;
                        break;
                    case 6: {   /* seac: un caracter base y un acento encima */
                        double asb = 0, adx = 0, ady = 0;
                        int bchar = 0, achar = 0;
                        if (c->sp >= 5) {
                            asb = c->st[c->sp - 5];
                            adx = c->st[c->sp - 4];
                            ady = c->st[c->sp - 3];
                            bchar = (int)c->st[c->sp - 2];
                            achar = (int)c->st[c->sp - 1];
                        }
                        c->sp = 0;
                        double larg = c->larg, sbx = c->sbx;
                        int tem = c->tem_larg;
                        for (int parte = 0; parte < 2; parte++) {
                            const char *nm = t1_nombre_estandar(parte ? achar : bchar);
                            int g = -1, k;
                            if (!nm) continue;
                            for (k = 0; k < c->f->t1_n; k++) {
                                if (c->f->t1_nomes[k] && igual(c->f->t1_nomes[k], nm) == 0) {
                                    g = k;
                                    break;
                                }
                            }
                            if (g < 0 || !c->f->t1_chars[g]) continue;
                            /* El origen del acento va en (adx - asb, ady)
                             * respecto del base (su hsbw le suma su sbx). */
                            c->ox = parte ? adx - asb : 0.0;
                            c->oy = parte ? ady : 0.0;
                            t1_executa(c, c->f->t1_chars[g], c->f->t1_tam[g], prof + 1);
                        }
                        c->ox = c->oy = 0.0;
                        c->larg = larg;
                        c->sbx = sbx;
                        c->tem_larg = tem;
                        /* El seac termina el glifo (como un endchar). */
                        c->cs = g_cs; c->pos = g_pos; c->n = g_n;
                        return true;
                    }
                    case 7:     /* sbw */
                        if (c->sp >= 4) {
                            c->sbx = c->st[0];
                            c->larg = c->st[2];
                            c->tem_larg = 1;
                            c->x = c->sbx;
                            c->y = 0;
                            c->ix = c->x; c->iy = c->y;
                        }
                        c->sp = 0;
                        break;
                    case 12:    /* div */
                        if (c->sp >= 2) {
                            double a = c->st[c->sp - 2], b = c->st[c->sp - 1];
                            c->st[c->sp - 2] = (b != 0.0) ? a / b : 0.0;
                            c->sp--;
                        }
                        break;
                    case 16: {  /* callothersubr */
                        int nargs = 0, sub = -1;
                        double args[16];
                        if (c->sp >= 2) {
                            sub = (int)c->st[c->sp - 1];
                            nargs = (int)c->st[c->sp - 2];
                            c->sp -= 2;
                            if (nargs > c->sp) nargs = c->sp;
                            if (nargs < 0) nargs = 0;
                            if (nargs > 16) nargs = 16;
                            for (int k = 0; k < nargs; k++) args[k] = c->st[c->sp - nargs + k];
                            c->sp -= nargs;
                        }
                        c->nps = 0;
                        switch (sub) {
                            case 1:     /* empieza el flex */
                                c->nflex = 0;
                                c->flex_ativo = 1;
                                c->flex_x0 = c->x;
                                c->flex_y0 = c->y;
                                break;
                            case 2:     /* anade un punto del flex */
                                if (c->flex_ativo && c->nflex < 7) {
                                    c->flexx[c->nflex] = c->x;
                                    c->flexy[c->nflex] = c->y;
                                    c->nflex++;
                                }
                                break;
                            case 0:     /* termina el flex: dos curvas */
                                if (c->flex_ativo && c->nflex == 7) {
                                    /* El punto 0 es solo de referencia. */
                                    t1_cubica(c, c->flex_x0, c->flex_y0,
                                              c->flexx[1], c->flexy[1],
                                              c->flexx[2], c->flexy[2],
                                              c->flexx[3], c->flexy[3]);
                                    t1_cubica(c, c->flexx[3], c->flexy[3],
                                              c->flexx[4], c->flexy[4],
                                              c->flexx[5], c->flexy[5],
                                              c->flexx[6], c->flexy[6]);
                                    c->x = c->flexx[6];
                                    c->y = c->flexy[6];
                                }
                                c->flex_ativo = 0;
                                /* Los dos "pop" que siguen dan x e y para el
                                 * setcurrentpoint. */
                                c->ps[0] = c->y;
                                c->ps[1] = c->x;
                                c->nps = 2;
                                break;
                            case 3:     /* reemplazo de hints: "pop callsubr"
                                         * debe llamar al subr 3 (que no hace nada) */
                                c->ps[0] = 3.0;
                                c->nps = 1;
                                break;
                            default:    /* los demas devuelven sus argumentos */
                                for (int k = 0; k < nargs; k++) c->ps[k] = args[nargs - 1 - k];
                                c->nps = nargs;
                                break;
                        }
                        break;
                    }
                    case 17:    /* pop: de la pila de PostScript a la nuestra */
                        if (c->nps > 0 && c->sp < 64) c->st[c->sp++] = c->ps[--c->nps];
                        break;
                    case 33:    /* setcurrentpoint */
                        if (c->sp >= 2) {
                            c->x = c->st[c->sp - 2];
                            c->y = c->st[c->sp - 1];
                            c->sp -= 2;
                        }
                        break;
                    default:
                        c->sp = 0;
                        break;
                }
                break;
            }
            default:
                c->sp = 0;
                break;
        }
    }

    c->cs = g_cs; c->pos = g_pos; c->n = g_n;
    return true;
}

/* ---- la carga ---- */

/* Busca un texto dentro de un buffer. */
static const uint8_t *t1_busca(const uint8_t *d, int n, const char *txt)
{
    int lt = 0, i;

    while (txt[lt]) lt++;
    if (lt > n) return NULL;
    for (i = 0; i + lt <= n; i++) {
        if (memcmp(d + i, txt, (size_t)lt) == 0) return d + i;
    }
    return NULL;
}

/* Saca un numero de un texto (como mucho hasta 'fim'). */
static bool t1_le_num_lim(const char **p, const char *fim, double *v)
{
    const char *s = *p;
    double val = 0.0;
    int neg = 0, tem = 0;

    while (s < fim && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    if (s < fim && (*s == '-' || *s == '+')) { neg = (*s == '-'); s++; }
    while (s < fim && *s >= '0' && *s <= '9') {
        val = val * 10.0 + (double)(*s - '0');
        s++;
        tem = 1;
    }
    if (s < fim && *s == '.') {
        double f = 0.1;
        s++;
        while (s < fim && *s >= '0' && *s <= '9') {
            val += (double)(*s - '0') * f;
            f *= 0.1;
            s++;
            tem = 1;
        }
    }
    if (!tem) return false;
    *v = neg ? -val : val;
    *p = s;
    return true;
}

static bool t1_espaco(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == 0;
}

/* Tras "<longitud>" viene la palabra que lee los bytes binarios: "RD" o
 * "-|" (las dos formas que usan las fuentes reales) y UN espacio. Devuelve
 * la posicion de los bytes o -1. */
static int t1_depois_rd(const uint8_t *d, int n, int i)
{
    while (i < n && t1_espaco(d[i])) i++;
    if (i + 2 > n) return -1;
    if (!((d[i] == 'R' && d[i + 1] == 'D') || (d[i] == '-' && d[i + 1] == '|'))) return -1;
    i += 2;
    if (i >= n || !t1_espaco(d[i])) return -1;
    return i + 1;
}

/* Copia un charstring descifrado (r = 4330) y sin los lenIV bytes del
 * principio. Con lenIV = -1 los charstrings van en claro. */
static uint8_t *t1_charstring(const uint8_t *src, int len, int liniv, int *tam)
{
    uint8_t *cs = (uint8_t *)aloca((size_t)len + 1);
    if (!cs) return NULL;
    memcpy(cs, src, (size_t)len);
    if (liniv >= 0) {
        t1_r_cifra(cs, len, 4330);
        if (liniv > len) liniv = len;
        memmove(cs, cs + liniv, (size_t)(len - liniv));
        len -= liniv;
    }
    *tam = len;
    return cs;
}

/* Los Subrs y los CharStrings de la parte privada (ya descifrada). */
static int t1_extrai(const uint8_t *d, int n, int liniv, pdf_fonte_t *f)
{
    const uint8_t *cs_ini = t1_busca(d, n, "/CharStrings");
    const uint8_t *sb_ini = t1_busca(d, n, "/Subrs");
    int i;

    /* Los Subrs: "/Subrs <n> array" y luego "dup <i> <len> RD <bytes> NP". */
    if (sb_ini && (!cs_ini || sb_ini < cs_ini)) {
        const char *p = (const char *)sb_ini + 6;
        double nsub = 0;
        if (t1_le_num_lim(&p, (const char *)d + n, &nsub) && nsub > 0 && nsub < 65536) {
            int total = (int)nsub;
            int fim = cs_ini ? (int)(cs_ini - d) : n;
            f->t1_subrs_dados = (const uint8_t **)aloca_zerado((size_t)total * sizeof(uint8_t *));
            f->t1_subrs_tam = (int *)aloca_zerado((size_t)total * sizeof(int));
            if (!f->t1_subrs_dados || !f->t1_subrs_tam) return 0;
            f->t1_subrs = total;
            i = (int)((const uint8_t *)p - d);
            while (i + 3 < fim) {
                if (memcmp(d + i, "dup", 3) != 0 || (i > 0 && !t1_espaco(d[i - 1]) && d[i - 1] != '}')) {
                    i++;
                    continue;
                }
                const char *q = (const char *)d + i + 3;
                double idx, len;
                if (!t1_le_num_lim(&q, (const char *)d + fim, &idx) ||
                    !t1_le_num_lim(&q, (const char *)d + fim, &len)) { i += 3; continue; }
                int pos = t1_depois_rd(d, fim, (int)(q - (const char *)d));
                if (pos < 0 || len < 0 || pos + (int)len > n) { i += 3; continue; }
                int k = (int)idx;
                if (k >= 0 && k < total && !f->t1_subrs_dados[k]) {
                    int t = 0;
                    f->t1_subrs_dados[k] = t1_charstring(d + pos, (int)len, liniv, &t);
                    f->t1_subrs_tam[k] = t;
                }
                i = pos + (int)len;
            }
        }
    }

    /* Los CharStrings: "/<nombre> <len> RD <bytes> ND". */
    if (!cs_ini) return 0;
    int cap = 64, nchar = 0;
    f->t1_nomes = (char **)aloca_zerado((size_t)cap * sizeof(char *));
    f->t1_chars = (uint8_t **)aloca_zerado((size_t)cap * sizeof(uint8_t *));
    f->t1_tam = (int *)aloca_zerado((size_t)cap * sizeof(int));
    if (!f->t1_nomes || !f->t1_chars || !f->t1_tam) return 0;
    i = (int)(cs_ini - d) + 12;
    while (i < n) {
        if (d[i] != '/') {
            /* "end" cierra el diccionario de los CharStrings. */
            if (nchar > 0 && i + 3 <= n && memcmp(d + i, "end", 3) == 0 &&
                t1_espaco(d[i - 1]) && (i + 3 == n || t1_espaco(d[i + 3]))) break;
            i++;
            continue;
        }
        char nome[64];
        int ln = 0;
        int j = i + 1;
        while (j < n && ln < 63 && !t1_espaco(d[j]) && d[j] != '/') nome[ln++] = (char)d[j++];
        nome[ln] = '\0';
        const char *q = (const char *)d + j;
        double len;
        if (!t1_le_num_lim(&q, (const char *)d + n, &len)) { i = j; continue; }
        int pos = t1_depois_rd(d, n, (int)(q - (const char *)d));
        if (pos < 0 || len < 0 || pos + (int)len > n) { i = j; continue; }
        if (nchar >= cap) {
            int ncap = cap * 2;
            char **nn = (char **)realoca(f->t1_nomes, (size_t)ncap * sizeof(char *));
            if (!nn) break;
            f->t1_nomes = nn;
            uint8_t **nc = (uint8_t **)realoca(f->t1_chars, (size_t)ncap * sizeof(uint8_t *));
            if (!nc) break;
            f->t1_chars = nc;
            int *nt = (int *)realoca(f->t1_tam, (size_t)ncap * sizeof(int));
            if (!nt) break;
            f->t1_tam = nt;
            cap = ncap;
        }
        f->t1_nomes[nchar] = (char *)aloca((size_t)ln + 1);
        if (f->t1_nomes[nchar]) memcpy(f->t1_nomes[nchar], nome, (size_t)ln + 1);
        int t = 0;
        f->t1_chars[nchar] = t1_charstring(d + pos, (int)len, liniv, &t);
        f->t1_tam[nchar] = f->t1_chars[nchar] ? t : 0;
        f->t1_n = ++nchar;
        i = pos + (int)len;
    }
    return nchar;
}

static int t1_hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool t1_carregar(pdf_fonte_t *f)
{
    const uint8_t *d = f->dados;
    int n = f->tam;
    int liniv = 4;
    int i;

    for (i = 0; i < FONE_MAX_COD; i++) f->t1_cod[i] = -1;

    /* Un PFB lleva el programa en segmentos; se juntan. */
    if (n > 1 && d[0] == 0x80) {
        int ini = 0;
        int tam = 0;
        uint8_t *juntado = (uint8_t *)aloca((size_t)n + 16);
        if (!juntado) return false;
        while (ini + 6 <= n && d[ini] == 0x80) {
            int tipo = d[ini + 1];
            if (tipo == 3) break;
            int len = (int)le32(d + ini + 2);
            ini += 6;
            if (len < 0 || ini + len > n) len = n - ini;
            memcpy(juntado + tam, d + ini, (size_t)len);
            tam += len;
            ini += len;
        }
        libera(f->dados);
        d = juntado;
        n = tam;
        f->dados = juntado;
        f->tam = tam;
    }

    /* La parte clara acaba en "eexec"; lo que sigue va cifrado (r = 55665),
     * en binario (lo normal en un PDF) o en hexadecimal (PFA). */
    const uint8_t *eexec = t1_busca(d, n, "eexec");
    if (!eexec) return false;
    const uint8_t *limpo = d;
    int nlimpo = (int)(eexec - d);
    int ini = nlimpo + 5;
    while (ini < n && (d[ini] == ' ' || d[ini] == '\t' || d[ini] == '\r' || d[ini] == '\n')) ini++;
    int ncifrado = n - ini;
    if (ncifrado <= 4) return false;

    uint8_t *desc = (uint8_t *)aloca((size_t)ncifrado + 1);
    if (!desc) return false;
    bool hex = true;
    for (i = 0; i < 4; i++) if (t1_hex(d[ini + i]) < 0) hex = false;
    int ndesc = 0;
    if (hex) {
        int alto = -1;
        for (i = ini; i < n; i++) {
            int v = t1_hex(d[i]);
            if (v < 0) {
                if (t1_espaco(d[i])) continue;
                break;
            }
            if (alto < 0) alto = v;
            else { desc[ndesc++] = (uint8_t)(alto * 16 + v); alto = -1; }
        }
    } else {
        memcpy(desc, d + ini, (size_t)ncifrado);
        ndesc = ncifrado;
    }
    t1_r_cifra(desc, ndesc, 55665);
    /* Los cuatro primeros bytes son aleatorios. */
    uint8_t *priv = desc + 4;
    int npriv = ndesc - 4;
    priv[npriv] = 0;

    /* El lenIV (por defecto 4; -1 = charstrings sin cifrar). */
    {
        const uint8_t *lv = t1_busca(priv, npriv, "/lenIV");
        if (lv) {
            double num;
            const char *p = (const char *)lv + 6;
            if (t1_le_num_lim(&p, (const char *)priv + npriv, &num) && num >= -1 && num < 16)
                liniv = (int)num;
        }
    }

    if (t1_extrai(priv, npriv, liniv, f) <= 0) {
        libera(desc);
        return false;
    }
    libera(desc);

    /* El nombre, la matriz y el alto de la fuente (en la parte clara). */
    {
        const uint8_t *fn = t1_busca(limpo, nlimpo, "/FontName");
        if (fn) {
            int j = (int)(fn - limpo) + 9;
            char buf[FONE_NOME_MAX];
            int k = 0;
            while (j < nlimpo && t1_espaco(limpo[j])) j++;
            if (j < nlimpo && limpo[j] == '/') j++;
            while (j < nlimpo && k < FONE_NOME_MAX - 1 && !t1_espaco(limpo[j]) && limpo[j] != '/')
                buf[k++] = (char)limpo[j++];
            buf[k] = '\0';
            if (k) copiar_nome(f->nome, FONE_NOME_MAX, buf);
        }
    }
    f->t1_mat[0] = 1.0; f->t1_mat[1] = 0.0; f->t1_mat[2] = 0.0;
    f->t1_mat[3] = 1.0; f->t1_mat[4] = 0.0; f->t1_mat[5] = 0.0;
    {
        const uint8_t *fm = t1_busca(limpo, nlimpo, "/FontMatrix");
        if (fm) {
            const char *p = (const char *)fm + 11;
            const char *fim = (const char *)limpo + nlimpo;
            double m[6] = {0.001, 0, 0, 0.001, 0, 0};
            int k = 0;
            while (k < 6) {
                double v;
                while (p < fim && (*p == ' ' || *p == '[' || *p == '{' || *p == '\n' || *p == '\r')) p++;
                if (p >= fim || *p == ']' || *p == '}') break;
                if (!t1_le_num_lim(&p, fim, &v)) break;
                m[k++] = v;
            }
            /* Se pasa a 1/1000 de em (la unidad de los glifos del motor). */
            if (k == 6 && m[0] != 0.0)
                for (i = 0; i < 6; i++) f->t1_mat[i] = m[i] * 1000.0;
        }
    }
    f->t1_escala = f->t1_mat[0];
    {
        const uint8_t *fb = t1_busca(limpo, nlimpo, "/FontBBox");
        if (fb) {
            const char *p = (const char *)fb + 9;
            const char *fim = (const char *)limpo + nlimpo;
            double v[4] = {0, 0, 0, 0};
            int k = 0;
            while (k < 4) {
                double x;
                while (p < fim && (*p == ' ' || *p == '{' || *p == '[' || *p == '\n' || *p == '\r')) p++;
                if (p >= fim || *p == '}' || *p == ']') break;
                if (!t1_le_num_lim(&p, fim, &x)) break;
                v[k++] = x;
            }
            if (k >= 4 && v[3] > v[1]) {
                f->ascendente = (int)(v[3] * f->t1_escala);
                f->descendente = (int)(v[1] * f->t1_escala);
            }
        }
    }

    /* La codificacion propia: StandardEncoding o "dup <codigo> /<nombre> put". */
    {
        const char *nomes_cod[FONE_MAX_COD];
        char *propios[FONE_MAX_COD];
        for (i = 0; i < FONE_MAX_COD; i++) {
            nomes_cod[i] = t1_nombre_estandar(i);
            propios[i] = NULL;
        }
        const uint8_t *en = t1_busca(limpo, nlimpo, "/Encoding");
        if (en) {
            int j = (int)(en - limpo) + 9;
            while (j < nlimpo && t1_espaco(limpo[j])) j++;
            if (!(j + 16 <= nlimpo && memcmp(limpo + j, "StandardEncoding", 16) == 0)) {
                for (i = 0; i < FONE_MAX_COD; i++) nomes_cod[i] = NULL;
                const uint8_t *fim_enc = t1_busca(limpo + j, nlimpo - j, "readonly def");
                if (!fim_enc) fim_enc = t1_busca(limpo + j, nlimpo - j, " def");
                int lim = fim_enc ? (int)(fim_enc - limpo) : nlimpo;
                while (j < lim) {
                    const uint8_t *dup = t1_busca(limpo + j, lim - j, "dup ");
                    if (!dup) break;
                    const char *p = (const char *)dup + 4;
                    const char *fim = (const char *)limpo + lim;
                    double num;
                    j = (int)(dup - limpo) + 4;
                    if (!t1_le_num_lim(&p, fim, &num)) continue;
                    while (p < fim && *p == ' ') p++;
                    if (p >= fim || *p != '/') continue;
                    p++;
                    char nome[64];
                    int k = 0;
                    while (p < fim && !t1_espaco(*p) && *p != '/' && k < 63) nome[k++] = *p++;
                    nome[k] = '\0';
                    int cod = (int)num;
                    if (cod >= 0 && cod < FONE_MAX_COD && !propios[cod]) {
                        propios[cod] = (char *)aloca((size_t)k + 1);
                        if (propios[cod]) {
                            memcpy(propios[cod], nome, (size_t)k + 1);
                            nomes_cod[cod] = propios[cod];
                        }
                    }
                    j = (int)(p - (const char *)limpo);
                }
            }
        }
        /* El codigo apunta al charstring con ese nombre. */
        for (i = 0; i < FONE_MAX_COD; i++) {
            if (!nomes_cod[i]) continue;
            for (int k = 0; k < f->t1_n; k++) {
                if (f->t1_nomes[k] && igual(f->t1_nomes[k], nomes_cod[i]) == 0) {
                    f->t1_cod[i] = k;
                    break;
                }
            }
        }
        for (i = 0; i < FONE_MAX_COD; i++) if (propios[i]) libera(propios[i]);
    }

    if (!f->nome[0]) copiar_nome(f->nome, FONE_NOME_MAX, "Type1");
    return true;
}

/* El indice del charstring de un codigo. */
static int t1_indice(pdf_fonte_t *f, int codigo)
{
    if (codigo < 0 || codigo >= FONE_MAX_COD) return -1;
    if (f->dif[codigo] && f->dif[codigo][0]) {
        int i;
        for (i = 0; i < f->t1_n; i++) {
            if (f->t1_nomes[i] &&
                igual(f->t1_nomes[i], f->dif[codigo]) == 0)
                return i;
        }
        return -1;
    }
    return f->t1_cod[codigo];
}

/* Ejecuta un charstring del Type 1. */
static void t1_executa_glyph(pdf_fonte_t *f, int idx, pdf_caminho_t *cam,
                             double *larg)
{
    t1_t c;
    int i;

    if (idx < 0 || idx >= f->t1_n || !f->t1_chars[idx]) {
        if (larg) *larg = 0.0;
        return;
    }
    memset(&c, 0, sizeof(c));
    c.cs = f->t1_chars[idx];
    c.n = f->t1_tam[idx];
    c.pos = 0;
    c.cam = cam;
    c.f = f;
    for (i = 0; i < 6; i++) c.mat[i] = f->t1_mat[i];

    t1_executa(&c, f->t1_chars[idx], f->t1_tam[idx], 0);

    if (larg) {
        if (c.tem_larg) *larg = c.larg * f->t1_escala;
        else *larg = 0.0;
    }
}

static bool t1_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g)
{
    int idx;
    double larg = 0.0;

    idx = t1_indice(f, codigo);
    if (idx < 0) {
        g->largura = 0;
        return false;
    }
    t1_executa_glyph(f, idx, &g->contorno, &larg);
    g->largura = (int)(larg + 0.5);
    return true;
}

static int t1_largura(pdf_fonte_t *f, int codigo)
{
    int idx;
    double larg = 0.0;

    idx = t1_indice(f, codigo);
    if (idx < 0) return 0;
    t1_executa_glyph(f, idx, NULL, &larg);
    return (int)(larg + 0.5);
}
/* =========================================================== fuentes básicas ===
 *
 * Las 14 fuentes basicas (Helvetica, Times, Courier y sus variantes, mas
 * Symbol y ZapfDingbats) no van incrustadas en el PDF: hay que dibujarlas
 * con una fuente propia.
 *
 * Aqui hay una fuente de trazos sencilla: cada letra es una lista de plumas
 * en un lenguaje facilito:
 *
 *   M x y       mueve el lapiz (empieza un trazo)
 *   L x y       linea hasta
 *   C x1 y1 x2 y2 x y   curva cubica
 *
 * Las coordenadas van en 1/1000 de em, con la linea base en 0 y las mayusculas
 * a 700. Como el rasterizador rellena los caminos, cada tramo de pluma se
 * convierte en un cuadrilatero con el grosor de la letra: asi se ve el
 * trazo. Los anchos son los de las fuentes originales (Helvetica, Times,
 * Courier), que son publicos.
 */

#define BASICA_SANS   0
#define BASICA_SERIF  1
#define BASICA_DING   2

/* Los anchos de las familias (codigos del 32 al 126). */
static const int16_t basica_larg_helv[95] = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584
};

static const int16_t basica_larg_helv_bold[95] = {
    278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333,
    278, 278, 556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333,
    584, 584, 584, 611, 975, 722, 722, 722, 722, 667, 611, 778, 722, 278,
    556, 722, 611, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944,
    667, 667, 611, 333, 278, 333, 584, 556, 333, 556, 611, 556, 611, 556,
    333, 611, 611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556,
    333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584
};

static const int16_t basica_larg_times[95] = {
    250, 333, 408, 500, 500, 833, 778, 180, 333, 333, 500, 564, 250, 333,
    250, 278, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 278, 278,
    564, 564, 564, 444, 921, 722, 667, 667, 722, 611, 556, 722, 722, 333,
    389, 722, 611, 889, 722, 722, 556, 722, 667, 556, 611, 722, 722, 944,
    722, 722, 611, 333, 278, 333, 469, 500, 333, 444, 500, 444, 500, 444,
    333, 500, 500, 278, 278, 500, 278, 778, 500, 500, 500, 500, 333, 389,
    278, 500, 500, 722, 500, 500, 444, 480, 200, 480, 541
};

static const int16_t basica_larg_times_bold[95] = {
    250, 333, 555, 500, 500, 1000, 833, 278, 333, 333, 500, 570, 250, 333,
    250, 278, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 333, 333,
    570, 570, 570, 500, 930, 722, 667, 722, 722, 667, 611, 778, 778, 389,
    500, 778, 667, 944, 722, 778, 611, 778, 722, 556, 667, 722, 722, 1000,
    722, 722, 667, 333, 278, 333, 581, 500, 333, 500, 556, 444, 556, 444,
    333, 500, 556, 278, 333, 556, 278, 833, 556, 500, 556, 556, 444, 389,
    333, 556, 500, 722, 500, 500, 444, 394, 220, 394, 520
};

/* Las formas de las letras, del 32 al 126. */
static const char *const basica_sans[95] = {
    "",
    "M 100 700 L 100 200 M 95 60 L 105 60",
    "M 80 700 L 80 560 M 180 700 L 180 560",
    "M 140 700 L 100 0 M 320 700 L 280 0 M 60 460 L 380 460 M 40 240 L 360 240",
    "M 250 720 L 250 -20 M 380 540 C 380 660 100 700 100 520 C 100 400 380 340 380 200 C 380 30 100 60 80 160",
    "M 80 700 L 420 0 M 150 520 C 70 520 70 700 150 700 C 230 700 230 520 150 520 M 350 180 C 270 180 270 0 350 0 C 430 0 430 180 350 180",
    "M 420 0 C 300 0 60 130 60 330 C 60 470 190 490 190 620 C 190 700 100 700 100 630 C 100 510 420 420 420 200 C 420 60 300 0 210 0",
    "M 100 700 L 100 560",
    "M 180 700 C 60 500 60 200 180 0",
    "M 80 700 C 200 500 200 200 80 0",
    "M 150 620 L 150 320 M 40 470 L 260 470 M 75 380 L 225 560",
    "M 60 350 L 500 350 M 280 550 L 280 150",
    "M 120 60 L 60 -120",
    "M 60 330 L 400 330",
    "M 95 60 L 105 60",
    "M 40 -50 L 360 750",
    "M 280 700 C 100 700 100 0 280 0 C 460 0 460 700 280 700",
    "M 120 560 L 280 700 L 280 0 M 120 0 L 440 0",
    "M 80 560 C 80 700 460 720 460 520 C 460 350 100 260 80 0 L 470 0",
    "M 90 620 C 160 720 430 700 430 560 C 430 430 250 400 250 400 C 250 400 460 400 460 230 C 460 30 180 20 90 120",
    "M 340 0 L 340 700 L 60 200 L 460 200",
    "M 430 700 L 140 700 L 110 400 C 200 450 460 430 460 220 C 460 40 190 0 90 110",
    "M 400 620 C 340 700 140 720 110 480 C 90 340 110 0 290 0 C 450 0 470 240 320 280 C 200 310 120 260 110 200",
    "M 80 700 L 460 700 L 200 0",
    "M 280 700 C 140 700 140 400 280 400 C 420 400 420 700 280 700 M 280 400 C 110 400 110 0 280 0 C 450 0 450 400 280 400",
    "M 160 80 C 220 0 420 -20 450 220 C 470 360 450 700 270 700 C 110 700 90 460 240 420 C 360 390 440 440 450 500",
    "M 95 480 L 105 480 M 95 60 L 105 60",
    "M 95 480 L 105 480 M 120 60 L 60 -120",
    "M 400 550 L 100 330 L 400 110",
    "M 80 420 L 480 420 M 80 240 L 480 240",
    "M 100 550 L 400 330 L 100 110",
    "M 80 560 C 80 700 440 720 440 550 C 440 420 250 400 250 220 M 245 60 L 255 60",
    "M 400 400 C 400 300 300 260 260 340 C 220 420 260 500 340 480 C 400 460 420 400 420 340 L 420 200 C 420 60 200 0 120 100 C 40 200 80 600 260 640 C 360 660 420 620 460 560",
    "M 20 0 L 300 700 L 580 0 M 130 220 L 470 220",
    "M 90 0 L 90 700 L 350 700 C 510 700 510 400 350 400 L 90 400 M 350 400 C 530 400 530 0 350 0 L 90 0",
    "M 460 560 C 400 700 110 730 100 350 C 90 0 400 -30 460 140",
    "M 90 0 L 90 700 L 320 700 C 520 700 520 0 320 0 L 90 0",
    "M 460 700 L 90 700 L 90 0 L 460 0 M 90 370 L 390 370",
    "M 460 700 L 90 700 L 90 0 M 90 370 L 390 370",
    "M 460 560 C 400 700 110 730 100 350 C 90 0 470 -30 470 160 L 470 300 L 300 300",
    "M 90 0 L 90 700 M 510 0 L 510 700 M 90 360 L 510 360",
    "M 200 0 L 200 700 M 80 700 L 320 700 M 80 0 L 320 0",
    "M 380 700 L 380 180 C 380 -20 150 -30 110 130",
    "M 90 0 L 90 700 M 480 700 L 90 320 M 250 440 L 500 0",
    "M 90 700 L 90 0 L 470 0",
    "M 80 0 L 80 700 L 330 100 L 580 700 L 580 0",
    "M 90 0 L 90 700 L 510 0 L 510 700",
    "M 300 700 C 90 700 90 0 300 0 C 510 0 510 700 300 700",
    "M 90 0 L 90 700 L 350 700 C 530 700 530 380 350 380 L 90 380",
    "M 300 700 C 90 700 90 0 300 0 C 510 0 510 700 300 700 M 330 200 L 500 -60",
    "M 90 0 L 90 700 L 340 700 C 520 700 520 400 340 400 L 90 400 M 320 400 L 520 0",
    "M 460 580 C 460 700 120 730 100 570 C 80 430 430 400 450 220 C 470 30 130 20 90 140",
    "M 60 700 L 500 700 M 280 700 L 280 0",
    "M 90 700 L 90 200 C 90 -30 510 -30 510 200 L 510 700",
    "M 60 700 L 300 0 L 540 700",
    "M 40 700 L 200 0 L 320 520 L 440 0 L 600 700",
    "M 80 700 L 500 0 M 500 700 L 80 0",
    "M 80 700 L 300 350 L 520 700 M 300 350 L 300 0",
    "M 80 700 L 490 700 L 80 0 L 490 0",
    "M 180 700 L 90 700 L 90 0 L 180 0",
    "M 40 750 L 360 -50",
    "M 90 700 L 180 700 L 180 0 L 90 0",
    "M 100 500 L 280 700 L 460 500",
    "M 40 -100 L 520 -100",
    "M 90 700 L 190 600",
    "M 90 400 C 150 500 380 500 380 320 L 380 0 M 380 320 C 380 120 160 40 100 140",
    "M 90 730 L 90 0 M 90 330 C 130 480 380 500 380 250 C 380 20 130 30 90 170",
    "M 380 400 C 320 500 100 500 100 250 C 100 20 320 0 380 100",
    "M 380 730 L 380 0 M 380 330 C 340 480 90 500 90 250 C 90 20 340 30 380 170",
    "M 90 250 L 390 250 C 390 480 100 520 90 270 C 90 120 200 30 300 40",
    "M 380 700 C 300 740 180 720 180 600 L 180 0 M 60 490 L 340 490",
    "M 380 500 L 380 -60 C 380 -180 200 -200 110 -120 M 380 330 C 340 480 90 500 90 250 C 90 20 340 30 380 170",
    "M 90 730 L 90 0 M 90 320 C 150 480 380 500 380 300 L 380 0",
    "M 180 500 L 180 0 M 180 640 L 180 630",
    "M 230 500 L 230 -60 C 230 -160 120 -180 70 -140 M 230 640 L 230 630",
    "M 90 730 L 90 0 M 350 500 L 90 200 M 200 280 L 370 0",
    "M 150 730 L 150 100 C 150 20 200 -10 260 0",
    "M 80 500 L 80 0 M 80 340 C 110 490 250 500 250 330 L 250 0 M 250 340 C 280 490 420 500 420 330 L 420 0",
    "M 90 500 L 90 0 M 90 320 C 150 480 380 500 380 300 L 380 0",
    "M 240 500 C 100 500 100 0 240 0 C 380 0 380 500 240 500",
    "M 90 500 L 90 -200 M 90 330 C 130 480 380 500 380 250 C 380 20 130 30 90 170",
    "M 380 500 L 380 -200 M 380 330 C 340 480 90 500 90 250 C 90 20 340 30 380 170",
    "M 90 500 L 90 0 M 90 300 C 140 470 300 500 360 460",
    "M 370 420 C 370 510 110 520 100 420 C 90 330 370 320 380 220 C 390 110 120 110 90 180",
    "M 180 700 L 180 120 C 180 20 260 -10 340 10 M 60 490 L 340 490",
    "M 90 500 L 90 180 C 90 20 320 20 380 160 M 380 500 L 380 0",
    "M 70 500 L 230 0 L 390 500",
    "M 50 500 L 160 0 L 250 380 L 340 0 L 450 500",
    "M 80 500 L 380 0 M 380 500 L 80 0",
    "M 70 500 L 240 -20 M 390 500 L 180 -200",
    "M 80 500 L 370 500 L 80 0 L 370 0",
    "M 220 700 L 150 700 L 150 400 L 90 350 L 150 300 L 150 0 L 220 0",
    "M 150 750 L 150 -150",
    "M 80 700 L 150 700 L 150 400 L 210 350 L 150 300 L 150 0 L 80 0",
    "M 80 300 C 140 380 200 240 280 320 C 340 380 400 300 460 330"
};

/* Las formas con remate (las de Times). Lo que no esta aqui es igual que
 * en la fuente sin remate. */
static const struct {
    char        cod;
    const char *forma;
} basica_serif[] = {
    { 'A', "M 20 0 L 300 700 L 580 0 M 130 220 L 470 220 M 20 0 L 120 0 M 580 0 L 480 0" },
    { 'E', "M 460 700 L 90 700 L 90 0 L 460 0 M 90 370 L 390 370 M 30 700 L 150 700 M 30 0 L 150 0 M 400 700 L 500 700 M 400 0 L 500 0" },
    { 'F', "M 460 700 L 90 700 L 90 0 M 90 370 L 390 370 M 30 700 L 150 700 M 30 0 L 150 0" },
    { 'H', "M 90 0 L 90 700 M 510 0 L 510 700 M 90 360 L 510 360 M 30 700 L 150 700 M 30 0 L 150 0 M 450 700 L 570 700 M 450 0 L 570 0" },
    { 'J', "M 380 700 L 380 180 C 380 -20 150 -30 110 130" },
    { 'K', "M 90 0 L 90 700 M 480 700 L 90 320 M 250 440 L 500 0 M 30 700 L 150 700 M 30 0 L 150 0" },
    { 'L', "M 90 700 L 90 0 L 470 0 M 30 700 L 150 700 M 30 0 L 200 0" },
    { 'M', "M 80 0 L 80 700 L 330 100 L 580 700 L 580 0 M 30 700 L 130 700 M 30 0 L 130 0 M 530 700 L 630 700 M 530 0 L 630 0" },
    { 'N', "M 90 0 L 90 700 L 510 0 L 510 700 M 30 700 L 150 700 M 30 0 L 150 0 M 450 700 L 570 700 M 450 0 L 570 0" },
    { 'Q', "M 300 700 C 90 700 90 0 300 0 C 510 0 510 700 300 700 M 330 200 L 500 -60" },
    { 'R', "M 90 0 L 90 700 L 340 700 C 520 700 520 400 340 400 L 90 400 M 320 400 L 520 0 M 30 700 L 150 700 M 30 0 L 150 0" },
    { 'T', "M 60 700 L 500 700 M 280 700 L 280 0 M 30 700 L 130 700 M 430 700 L 530 700" },
    { 'V', "M 60 700 L 300 0 L 540 700 M 20 700 L 120 700 M 480 700 L 580 700" },
    { 'W', "M 40 700 L 200 0 L 320 520 L 440 0 L 600 700 M 0 700 L 100 700 M 540 700 L 640 700" },
    { 'X', "M 80 700 L 500 0 M 500 700 L 80 0 M 40 700 L 140 700 M 440 700 L 540 700 M 40 0 L 140 0 M 440 0 L 540 0" },
    { 'Y', "M 80 700 L 300 350 L 520 700 M 300 350 L 300 0 M 40 700 L 140 700 M 460 700 L 560 700" },
    { 'Z', "M 80 700 L 490 700 L 80 0 L 490 0 M 40 700 L 140 700 M 420 700 L 520 700 M 40 0 L 140 0 M 420 0 L 520 0" },
    { 'a', "M 90 400 C 150 500 380 500 380 320 L 380 0 M 380 320 C 380 120 160 40 100 140 M 90 400 L 90 60" },
    { 'e', "M 90 250 L 390 250 C 390 480 100 520 90 270 C 90 120 200 30 300 40" },
    { 'f', "M 380 700 C 300 740 180 720 180 600 L 180 0 M 60 490 L 340 490 M 120 700 L 240 700" },
    { 'g', "M 380 500 L 380 -60 C 380 -180 200 -200 110 -120 M 380 330 C 340 480 90 500 90 250 C 90 20 340 30 380 170" },
    { 'k', "M 90 730 L 90 0 M 350 500 L 90 200 M 200 280 L 370 0 M 30 730 L 150 730 M 30 0 L 150 0" },
    { 's', "M 370 420 C 370 510 110 520 100 420 C 90 330 370 320 380 220 C 390 110 120 110 90 180" },
    { 'y', "M 70 500 L 240 -20 M 390 500 L 180 -200" },
    { '1', "M 120 560 L 280 700 L 280 0 M 120 0 L 440 0 M 200 700 L 360 700" },
    { 0, NULL }
};

/* Los ZapfDingbats: unas formas sencillas que se reparten por el codigo. */
static const char *const basica_ding[5] = {
    "M 350 700 C 100 700 100 0 350 0 C 600 0 600 700 350 700",
    "M 100 0 L 600 0 L 600 700 L 100 700 L 100 0",
    "M 80 0 L 620 700 M 620 0 L 80 700",
    "M 350 700 L 620 350 L 350 0 L 80 350 L 350 700",
    "M 80 350 L 620 350 M 350 700 L 350 0"
};

/* Las 14 fuentes basicas. */
typedef struct {
    const char *nome;
    int    forma;        /* BASICA_SANS, BASICA_SERIF o BASICA_DING */
    int    familia;      /* 0 = helv, 1 = helv bold, 2 = times, 3 = times
                          *   bold, 4 = courier */
    int    espessura;    /* grosor del trazo */
    double inclinacao;   /* la inclinacion de las oblicuas */
} basica_t;

static const basica_t basicas[] = {
    { "Helvetica",           BASICA_SANS,  0,  90, 0.0 },
    { "Helvetica-Bold",      BASICA_SANS,  1, 140, 0.0 },
    { "Helvetica-Oblique",   BASICA_SANS,  0,  90, 0.21 },
    { "Helvetica-BoldOblique", BASICA_SANS, 1, 140, 0.21 },
    { "Times-Roman",         BASICA_SERIF, 2,  80, 0.0 },
    { "Times-Bold",          BASICA_SERIF, 3, 120, 0.0 },
    { "Times-Italic",        BASICA_SERIF, 2,  80, 0.21 },
    { "Times-BoldItalic",    BASICA_SERIF, 3, 120, 0.21 },
    { "Courier",             BASICA_SANS,  4,  90, 0.0 },
    { "Courier-Bold",        BASICA_SANS,  4, 140, 0.0 },
    { "Courier-Oblique",     BASICA_SANS,  4,  90, 0.21 },
    { "Courier-BoldOblique", BASICA_SANS,  4, 140, 0.21 },
    { "Symbol",              BASICA_SANS,  0,  90, 0.0 },
    { "ZapfDingbats",        BASICA_DING,  5,  90, 0.0 }
};

#define NBASICAS ((int)(sizeof(basicas) / sizeof(basicas[0])))

/* La raiz cuadrada (no hay math.h). */
static double base_raiz(double x)
{
    double r;
    int i;

    if (x <= 0.0) return 0.0;
    r = x > 1.0 ? x : 1.0;
    for (i = 0; i < 40; i++) r = 0.5 * (r + x / r);
    return r;
}

/* Un tramo de pluma se convierte en un cuadrilatero con grosor. */
static void base_segmento(pdf_caminho_t *cam, double x0, double y0,
                          double x1, double y1, double esp, double incl)
{
    double dx = x1 - x0, dy = y1 - y0;
    double l = base_raiz(dx * dx + dy * dy);
    double nx, ny;
    double ax, ay, bx, by, cx, cy, dx2, dy2;
    double area;

    if (l < 0.0001) return;

    nx = -dy / l * (esp * 0.5);
    ny =  dx / l * (esp * 0.5);

    ax = x0 + nx; ay = y0 + ny;
    bx = x1 + nx; by = y1 + ny;
    cx = x1 - nx; cy = y1 - ny;
    dx2 = x0 - nx; dy2 = y0 - ny;

    /* Todos los cuadrilateros tienen que girar igual, para que al rellenar
     * se sumen y no se resten. */
    area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (area < 0.0) {
        double tx = ax, ty = ay;
        ax = cx; ay = cy;
        cx = tx; cy = ty;
        tx = bx; ty = by;
        bx = dx2; by = dy2;
        dx2 = tx; dy2 = ty;
    }

    /* La inclinacion de las oblicuas. */
    if (incl != 0.0) {
        ax += incl * ay; bx += incl * by; cx += incl * cy; dx2 += incl * dy2;
    }

    pdf_caminho_linha(cam, ax, ay, bx, by);
    pdf_caminho_linha(cam, bx, by, cx, cy);
    pdf_caminho_linha(cam, cx, cy, dx2, dy2);
    pdf_caminho_linha(cam, dx2, dy2, ax, ay);
}

/* Lee un numero del lenguaje de las formas. */
static bool base_le_num(const char **p, double *v)
{
    const char *s = *p;
    double val = 0.0;
    int neg = 0, tem = 0;

    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0 + (double)(*s - '0');
        s++;
        tem = 1;
    }
    if (!tem) return false;
    *v = neg ? -val : val;
    *p = s;
    return true;
}

/* Dibuja una letra a partir de su forma. */
static void base_desenha(const char *forma, double esp, double incl,
                         pdf_caminho_t *cam)
{
    const char *p = forma;
    double cx = 0.0, cy = 0.0;

    while (*p) {
        char c = *p;

        if (c == 'M' || c == 'L' || c == 'C') {
            double v[6];
            int n = (c == 'C') ? 6 : 2;
            int i;

            p++;
            for (i = 0; i < n; i++) {
                if (!base_le_num(&p, &v[i])) return;
            }
            if (c == 'M') {
                cx = v[0];
                cy = v[1];
            } else if (c == 'L') {
                base_segmento(cam, cx, cy, v[0], v[1], esp, incl);
                cx = v[0];
                cy = v[1];
            } else {
                double x0 = cx, y0 = cy;
                int k;
                for (k = 1; k <= 8; k++) {
                    double t = (double)k / 8.0;
                    double mt = 1.0 - t;
                    double a = mt * mt * mt;
                    double b = 3.0 * mt * mt * t;
                    double d = 3.0 * mt * t * t;
                    double e = t * t * t;
                    double nx = a * x0 + b * v[0] + d * v[2] + e * v[4];
                    double ny = a * y0 + b * v[1] + d * v[3] + e * v[5];
                    base_segmento(cam, cx, cy, nx, ny, esp, incl);
                    cx = nx;
                    cy = ny;
                }
            }
            continue;
        }
        p++;
    }
}

/* El ancho de un codigo en una familia. */
static int base_largura(int familia, int codigo)
{
    if (familia == 4) return 600;            /* Courier: todo igual */
    if (codigo < 32 || codigo > 126) return 0;
    switch (familia) {
        case 0: return basica_larg_helv[codigo - 32];
        case 1: return basica_larg_helv_bold[codigo - 32];
        case 2: return basica_larg_times[codigo - 32];
        case 3: return basica_larg_times_bold[codigo - 32];
        default: return 0;
    }
}

/* La forma de un codigo. */
static const char *base_forma(int forma, int codigo)
{
    if (forma == BASICA_DING) {
        if (codigo < 32 || codigo > 126) return NULL;
        return basica_ding[(codigo - 32) % 5];
    }
    if (codigo < 32 || codigo > 126) return NULL;
    if (forma == BASICA_SERIF) {
        int i;
        for (i = 0; basica_serif[i].forma; i++) {
            if (basica_serif[i].cod == (char)codigo) return basica_serif[i].forma;
        }
    }
    return basica_sans[codigo - 32];
}

/* Carga una de las 14 fuentes basicas. */
pdf_fonte_t *pdf_fonte_basica(const char *nome)
{
    pdf_fonte_t *f;
    int i;

    if (!nome) return NULL;
    for (i = 0; i < NBASICAS; i++) {
        if (igual(nome, basicas[i].nome) == 0) break;
    }
    if (i >= NBASICAS) return NULL;

    f = (pdf_fonte_t *)aloca_zerado(sizeof(pdf_fonte_t));
    if (!f) return NULL;
    f->tipo = 0;
    f->basica = i + 1;
    f->b_espessura = basicas[i].espessura;
    f->b_inclina = basicas[i].inclinacao;
    f->ascendente = 718;
    f->descendente = -207;
    copiar_nome(f->nome, FONE_NOME_MAX, basicas[i].nome);
    return f;
}

/* El glifo de una fuente basica. */
static bool basica_glifo(pdf_fonte_t *f, int codigo, pdf_glifo_t *g)
{
    const basica_t *b;
    const char *forma;

    if (f->basica < 1 || f->basica > NBASICAS) return false;
    b = &basicas[f->basica - 1];

    g->largura = base_largura(b->familia, codigo);

    if (f->dif[codigo] && f->dif[codigo][0]) {
        /* Con diferencias solo se cambia el ancho: la forma es la misma. */
        return true;
    }

    forma = base_forma(b->forma, codigo);
    if (!forma) return false;
    if (!forma[0]) return true;       /* el espacio */

    base_desenha(forma, (double)f->b_espessura, f->b_inclina,
                 &g->contorno);
    return true;
}

/* El ancho de una fuente basica. */
static int basica_largura(pdf_fonte_t *f, int codigo)
{
    const basica_t *b;

    if (f->basica < 1 || f->basica > NBASICAS) return 0;
    b = &basicas[f->basica - 1];
    return base_largura(b->familia, codigo);
}

/* El glifo de un CID (fuentes Type0). En TrueType el llamante ya ha pasado
 * el CID por el /CIDToGIDMap, asi que aqui llega el numero de glifo; en un
 * CFF con CID se busca en su charset; en un CFF sin CID, CID = glifo. */
bool pdf_fonte_glifo_cid(pdf_fonte_t *f, int cid, pdf_glifo_t *glifo)
{
    if (!f || !glifo || cid < 0) return false;
    memset(glifo, 0, sizeof(pdf_glifo_t));
    if (f->tipo == PDF_FONTE_TRUETYPE) {
        mat2_t raiz;
        int gid = cid;
        if (gid <= 0 || gid >= f->tt_glyfs) return false;
        glifo->largura = tt_avanca(f, gid);
        raiz.a = 1000.0 / (double)f->tt_upem;
        raiz.b = 0; raiz.c = 0;
        raiz.d = 1000.0 / (double)f->tt_upem;
        raiz.e = 0; raiz.f = 0;
        return tt_glifo_caminho(f, gid, &raiz, &glifo->contorno, 0);
    }
    if (f->tipo == PDF_FONTE_CFF) {
        int gid = cid;
        double larg = 0.0;
        if (f->cff_cid_gid) gid = (cid <= f->cff_cid_max) ? f->cff_cid_gid[cid] : 0;
        if (gid <= 0 || gid >= f->cff_nglifos) return false;
        cff_executa(f, gid, &glifo->contorno, &larg);
        glifo->largura = (int)(larg + 0.5);
        return true;
    }
    return false;
}

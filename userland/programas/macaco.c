// SPDX-License-Identifier: GPL-2.0-or-later
/* macaco [ficheiro] - la hoja de cálculo del SaguiOS (mejora 078, D140).
 *
 * "Macaco eXcel Sheet": una hoja de cálculo minimalista, con los gráficos de
 * barras y de tarta y con sus propios ficheros .mxs (texto legible).
 *
 * Lo que hay dentro:
 *   - la cuadrícula de celdas, con desplazamiento y selección por el teclado
 *     y por el ratón; el valor de la celda activa se ve en la barra de
 *     fórmulas, como en cualquier hoja de cálculo;
 *   - las fórmulas de siempre, con "=": =A1+B2, =SUM(A1:A10), =AVERAGE(...),
 *     =MIN, =MAX, =COUNT, =IF, =ROUND, =ABS, =PRODUCT y la referencia a otra
 *     hoja con Hoja2!A1;
 *   - los gráficos (barras verticales, barras horizontales y tarta), dibujados
 *     en otra ventana, con su leyenda;
 *   - los ficheros .mxs: abrir, guardar y guardar como.
 *
 * Todo en C freestanding, sin libc. Los cálculos son en punto fijo entero
 * (centésimas), porque la libsagui se compila sin coma flotante (D076); la
 * división en 64 bits está escrita a mano, sin libgcc (que los programas no
 * llevan).
 */
#include "sagui.h"
#include "gui.h"
#include "pdf_escribir.h"

/* Teclas de función que la ABI no nombra (D293). */
#ifndef TECLA_F2
#define TECLA_F2 0x10B
#define TECLA_F4 0x10D
#endif

/* ------------------------------------------------------------ constantes */

#define MAC_MAX_LINHAS   1000   /* filas por hoja */
#define MAC_MAX_COLUNAS  100    /* columnas por hoja */
#define MAC_MAX_FOLHAS   8      /* hojas por libro */
#define MAC_NOME_FOLHA   24     /* caracteres del nombre de una hoja */
#define MAC_TEXTO_CEL    256    /* caracteres del contenido de una celda */
#define MAC_PROF_MAX     24     /* profundidad de la evaluación de fórmulas */
#define MAC_SERIE_MAX    64     /* series de un gráfico */
#define MAC_UNDO_MAX     24     /* pasos de deshacer */

/* ----flags de la celda ---- */
#define CEL_NUMERO   0x01   /* el contenido (o el resultado) es un número */
#define CEL_TEXTO    0x02   /* el contenido (o el resultado) es texto */
#define CEL_FORMULA  0x04   /* el texto es una fórmula */
#define CEL_CALC     0x08   /* se está evaluando (para detectar ciclos) */

/* ----geometría de la ventana ---- */
#define MENU_A         20    /* barra de menús */
#define FERR_A         28    /* cada barra de herramientas */
#define MAC_BARRA      (MENU_A + 2 * FERR_A)   /* menús + barras Estándar y Formato */
#define MAC_FORMULA    24    /* barra de fórmulas */
#define MAC_CABECALH   18    /* cabecera de las columnas */
#define ESTADO_A       22    /* barra de estado */
#define MAC_RODAPE     (22 + ESTADO_A)   /* pestañas de hojas + barra de estado */
#define MAC_LARGC      40    /* ancho de la franja de los números de fila */
#define MAC_ALTLINHA   20    /* alto de una fila */
#define MAC_LARGCOL    72    /* ancho de una columna, por omisión */

/* ----tipos de gráfico ---- */
#define GRAF_BARRAS      0   /* barras verticales */
#define GRAF_BARRAS_H    1   /* barras horizontales */
#define GRAF_TARTA       2   /* tarta (reparto por porcentaje) */

/* ------------------------------------------------------------ estructura */

/* Formato de una celda (D293). Todo a cero = formato por omisión; las celdas guardan solo un
 * índice a la tabla de formatos del libro (como los "XF" de Excel), así que 100000 celdas con
 * el mismo formato cuestan un byte cada una. */
#define NF_GERAL    0   /* General */
#define NF_NUMERO   1   /* número con "dec" decimales */
#define NF_MOEDA    2   /* moneda con miles y "dec" decimales */
#define NF_PERCENT  3   /* porcentaje */
#define NF_MILHAR   4   /* con separador de miles */
#define AL_AUTO     0
#define AL_ESQ      1
#define AL_CENTRO   2
#define AL_DIR      3
#define EST_NEGRITA 1
#define EST_CURSIVA 2
#define EST_SUBLIN  4
#define BOR_CIMA    1
#define BOR_BAIXO   2
#define BOR_ESQ     4
#define BOR_DIR     8
#define REL_ATIVO   0x1000000u   /* bit de "hay relleno" en fmt_t.rel */
#define MAC_MAX_FMT 255
typedef struct {
    uint8_t  nf, dec, moeda, al;
    uint8_t  est, fam, tam, bor;     /* tam = 0: 10 pt */
    uint32_t cor;                    /* color del texto (0 = negro) */
    uint32_t rel;                    /* REL_ATIVO | RGB del relleno */
} fmt_t;

typedef struct {
    int32_t valor;      /* el número, en centésimas */
    uint8_t flags;      /* CEL_* */
    uint8_t fmt;        /* índice en livro.fmts (0 = por omisión) */
    char   *texto;      /* el contenido: literal o fórmula (NULL si vacía) */
} celula_t;

typedef struct {
    char      nome[MAC_NOME_FOLHA];
    int       linhas, colunas;         /* dimensiones usadas */
    celula_t *celulas;                 /* linhas * colunas */
    int       larg_col[MAC_MAX_COLUNAS];
} folha_t;

typedef struct {
    folha_t folha[MAC_MAX_FOLHAS];
    int     n;
    int     ativa;
    fmt_t   fmts[MAC_MAX_FMT + 1];   /* tabla de formatos; [0] = por omisión */
    int     n_fmts;
} livro_t;

/* El resultado de evaluar una expresión: un número o un texto. */
typedef struct {
    int32_t num;
    bool    eh_texto;
    bool    vazia;        /* la celda estaba vacía (COUNT no la cuenta) */
    char    texto[64];
} res_t;

/* Una referencia a una celda o a un intervalo. */
typedef struct {
    int  folha;
    int  l1, c1, l2, c2;
    bool intervalo;
} ref_t;

/* El estado del analizador de fórmulas. */
typedef struct {
    const char *p;
    int   folha;      /* la hoja donde está la fórmula */
    int   prof;       /* profundidad (para cortar los ciclos) */
    bool  erro;
} ps_t;

/* Un paso del deshacer. */
typedef struct {
    int      tipo;      /* 0 = una celda, 1 = la hoja entera */
    int      folha;
    int      l, c;
    char    *antigo;    /* el contenido anterior de la celda */
    int      linhas, colunas;
    celula_t *copia;    /* copia de la hoja (paso estructural) */
    int      larg_col[MAC_MAX_COLUNAS];
} undo_t;

/* El área de transferencia (del programa, no la del sistema: aquí se copian
 * celdas enteras, con sus fórmulas). */
typedef struct {
    int      linhas, colunas;
    char   **conteudos;
    uint8_t *fmts;          /* formatos de las celdas copiadas */
    bool     vazio;
} copia_t;

/* Una serie de un gráfico. */
typedef struct {
    char    rotulo[48];
    int32_t valor;
    uint32_t cor;
} serie_t;

/* ------------------------------------------------------------ estado */

static jan_t   *jan;
static jan_t   *jan_graf;
static livro_t  livro;

static int  cel_l, cel_c;      /* la celda activa */
static int  anc_l, anc_c;      /* el ancla de la selección */
static int  desl_x, desl_y;    /* desplazamiento de la cuadrícula, en píxeles */
static bool arrastrando;        /* el ratón está eligiendo el intervalo */
static bool editando;          /* la celda activa se está escribiendo */
static char edit_buf[MAC_TEXTO_CEL];
static int  edit_cur;
static char caminho[256];
static bool modificado;
static char estado[96];        /* el mensaje de la barra de abajo */
static bool sair_pedido;

static undo_t desfazer[MAC_UNDO_MAX];
static int    n_desfazer;
static copia_t copia;

static serie_t series[MAC_SERIE_MAX];
static int     n_series;
static int     tipo_grafico = GRAF_BARRAS;

static const uint32_t CORES_SERIE[8] = {
    0x2A6FD8, 0x0B7A3E, 0xD9822B, 0xC0392B,
    0x8E44AD, 0x16A085, 0xB7950B, 0x5D6D7E
};

/* ------------------------------------------------------------ utilidades */

/* Pone un texto en un sitio (no hay strcpy en la libsagui). */
static void por_aqui(char *dst, int max, const char *txt)
{
    copia_texto(dst, txt, (size_t)max);
}

/* División de un entero de 64 bits por otro de 64, sin libgcc (__divdi3 no
 * existe en los programas del SaguiOS). El cociente se trunca hacia cero. */
static int64_t div64_64(int64_t num, int64_t den)
{
    if (den == 0) return 0;
    bool neg = (num < 0) != (den < 0);
    uint64_t n = (num < 0) ? (uint64_t)(-num) : (uint64_t)num;
    uint64_t d = (den < 0) ? (uint64_t)(-den) : (uint64_t)den;
    uint32_t hi = (uint32_t)(n >> 32), lo = (uint32_t)n;
    uint64_t q = 0, r = 0;
    /* Los bits del dividendo salen de uno en uno; los desplazamientos son de
     * 32 bits (que el i386 hace sin ayuda) o de 64 bits por uno (que también
     * quedan en línea). */
    for (int b = 63; b >= 0; b--) {
        uint32_t bit = (b >= 32) ? ((hi >> (b - 32)) & 1u) : ((lo >> b) & 1u);
        r = (r << 1) | bit;
        q <<= 1;
        if (r >= d) { r -= d; q |= 1u; }
    }
    return neg ? -(int64_t)q : (int64_t)q;
}

static char *duplicar_texto(const char *s)
{
    if (!s) return NULL;
    size_t n = tam_texto(s);
    char *d = aloca(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

static res_t res_zero(void)
{
    res_t r;
    r.num = 0;
    r.eh_texto = false;
    r.vazia = true;
    r.texto[0] = 0;
    return r;
}

/* "A1", "B2"... a partir de los índices (empezando por cero). */
static void nome_ref(char *dst, int max, int l, int c)
{
    char letras[8];
    int n = 0, cc = c;
    do {
        letras[n++] = (char)('A' + cc % 26);
        cc = cc / 26 - 1;
    } while (cc >= 0 && n < 6);
    char tmp[8];
    for (int i = 0; i < n; i++) tmp[i] = letras[n - 1 - i];
    tmp[n] = 0;
    formatar_texto(dst, max, "%s%d", tmp, l + 1);
}

/* El nombre de una columna, sin el número de fila ("A", "B", ... "AA"). */
static void nome_coluna(char *dst, int max, int c)
{
    char letras[8];
    int n = 0, cc = c;
    do {
        letras[n++] = (char)('A' + cc % 26);
        cc = cc / 26 - 1;
    } while (cc >= 0 && n < 6);
    if (n > max - 1) n = max - 1;
    for (int i = 0; i < n; i++) dst[i] = letras[n - 1 - i];
    dst[n] = 0;
}

/* ------------------------------------------------------------ números */

/* Lee un número del principio del texto y devuelve cuántos caracteres usó
 * (0 si no hay número). El punto es el separador decimal. */
static int ler_numero(const char *s, int32_t *cent)
{
    int i = 0;
    bool neg = false;
    if (s[0] == '-' || s[0] == '+') { neg = s[0] == '-'; i = 1; }
    int ini = i;
    int64_t inteiro = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        inteiro = inteiro * 10 + (s[i] - '0');
        if (inteiro > 20000000) return 0;   /* fuera del alcance */
        i++;
    }
    if (i == ini) return 0;                /* no hay cifras */
    int32_t frac = 0;          /* hasta tres casas, para redondear */
    int nfrac = 0;
    if (s[i] == '.') {
        int j = i + 1, k = j;
        while (s[k] >= '0' && s[k] <= '9') k++;
        if (k > j) {                        /* hay cifras detrás del punto */
            int m = j;
            while (m < k && nfrac < 3) { frac = frac * 10 + (s[m] - '0'); nfrac++; m++; }
            while (nfrac < 3) { frac *= 10; nfrac++; }
            i = k;
        }
    }
    int64_t total = inteiro * 100 + (frac / 10);
    if ((frac % 10) >= 5) total++;          /* redondea */
    *cent = (int32_t)(neg ? -total : total);
    return i;
}

/* Convierte un texto en número (en centésimas). Devuelve false si no es un
 * número válido (es decir, si sobra algo detrás). */
static bool parse_numero(const char *s, int32_t *cent)
{
    int n = ler_numero(s, cent);
    return n > 0 && s[n] == 0;
}

/* El número en texto: "42", "42.5", "42.25". */
static void formatar_numero(char *dst, int max, int32_t cent)
{
    bool neg = cent < 0;
    int32_t v = neg ? -cent : cent;
    int32_t inteiro = v / 100, frac = v % 100;
    if (frac == 0)
        formatar_texto(dst, max, "%s%d", neg ? "-" : "", inteiro);
    else if (frac % 10 == 0)
        formatar_texto(dst, max, "%s%d.%d", neg ? "-" : "", inteiro, frac / 10);
    else
        formatar_texto(dst, max, "%s%d.%02d", neg ? "-" : "", inteiro, frac);
}

/* ------------------------------------------------------------ el libro */

static folha_t *folha_ativa(void) { return &livro.folha[livro.ativa]; }

static celula_t *celula_de(folha_t *fo, int l, int c)
{
    if (!fo || l < 0 || l >= fo->linhas || c < 0 || c >= fo->colunas) return NULL;
    return &fo->celulas[(size_t)l * fo->colunas + c];
}

static int procurar_folha(const char *nome)
{
    for (int f = 0; f < livro.n; f++) {
        const char *a = livro.folha[f].nome, *b = nome;
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
            if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
            if (ca != cb) break;
            a++; b++;
        }
        if (!*a && !*b) return f;
    }
    return -1;
}

/* Redimensiona la hoja, conservando el contenido. Sirve para crecer y para
 * cambiar el número de columnas (que cambia el paso de las filas). */
static bool folha_redimensionar(folha_t *fo, int linhas, int colunas)
{
    if (linhas < 1) linhas = 1;
    if (colunas < 1) colunas = 1;
    if (linhas > MAC_MAX_LINHAS) linhas = MAC_MAX_LINHAS;
    if (colunas > MAC_MAX_COLUNAS) colunas = MAC_MAX_COLUNAS;
    if (linhas == fo->linhas && colunas == fo->colunas) return true;
    celula_t *novo = aloca_zerado((size_t)linhas * colunas * sizeof(celula_t));
    if (!novo) return false;
    int copiar_l = (fo->linhas < linhas) ? fo->linhas : linhas;
    int copiar_c = (fo->colunas < colunas) ? fo->colunas : colunas;
    for (int l = 0; l < copiar_l; l++)
        for (int c = 0; c < copiar_c; c++) {
            celula_t *d = &novo[(size_t)l * colunas + c];
            celula_t *o = &fo->celulas[(size_t)l * fo->colunas + c];
            *d = *o;
            d->flags &= (uint8_t)~CEL_CALC;
        }
    /* Las celdas que quedan fuera sueltan su texto. */
    for (int l = 0; l < fo->linhas; l++)
        for (int c = 0; c < fo->colunas; c++) {
            bool fica = (l < copiar_l && c < copiar_c);
            if (!fica) {
                celula_t *o = &fo->celulas[(size_t)l * fo->colunas + c];
                if (o->texto) { libera(o->texto); o->texto = NULL; }
            }
        }
    libera(fo->celulas);
    fo->celulas = novo;
    fo->linhas = linhas;
    fo->colunas = colunas;
    return true;
}

/* Pone el contenido de una celda (texto, número o fórmula); los valores se
 * recalculan después. */
static bool celula_definir(folha_t *fo, int l, int c, const char *conteudo)
{
    if (l < 0 || l >= MAC_MAX_LINHAS || c < 0 || c >= MAC_MAX_COLUNAS) return false;
    if (!folha_redimensionar(fo, (l + 1 > fo->linhas) ? l + 1 : fo->linhas,
                             (c + 1 > fo->colunas) ? c + 1 : fo->colunas)) return false;
    celula_t *ce = celula_de(fo, l, c);
    if (!ce) return false;
    char *novo = NULL;
    int32_t v = 0;
    uint8_t flags = 0;
    if (conteudo && conteudo[0]) {
        int n = (int)tam_texto(conteudo);
        if (n > MAC_TEXTO_CEL - 1) n = MAC_TEXTO_CEL - 1;
        novo = aloca((size_t)n + 1);
        if (!novo) return false;
        memcpy(novo, conteudo, (size_t)n);
        novo[n] = 0;
        if (novo[0] == '=') flags = CEL_FORMULA;
        else if (parse_numero(novo, &v)) flags = CEL_NUMERO;
        else flags = CEL_TEXTO;
    }
    if (ce->texto) libera(ce->texto);
    ce->texto = novo;
    ce->valor = v;
    ce->flags = flags;
    return true;
}

static void folha_nova(const char *nome)
{
    if (livro.n >= MAC_MAX_FOLHAS) return;
    folha_t *fo = &livro.folha[livro.n];
    memset(fo, 0, sizeof *fo);
    copia_texto(fo->nome, nome, sizeof fo->nome);
    fo->linhas = 20;
    fo->colunas = 8;
    fo->celulas = aloca_zerado((size_t)fo->linhas * fo->colunas * sizeof(celula_t));
    for (int c = 0; c < fo->colunas; c++) fo->larg_col[c] = MAC_LARGCOL;
    livro.n++;
    livro.ativa = livro.n - 1;
}

/* ------------------------------------------------------------ fórmulas */

static res_t avaliar_expr(ps_t *ps);
static res_t avaliar_comparar(ps_t *ps);

/* El texto de una fórmula empieza por "=", que el analizador no necesita. */
static const char *inicio_formula(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '=') p++;
    return p;
}

/* El valor de una celda: si tiene fórmula, la evalúa (y guarda el
 * resultado en la celda). */
static res_t valor_celula(int f, int l, int c, int prof)
{
    celula_t *ce = celula_de(&livro.folha[f], l, c);
    if (!ce) return res_zero();
    if (ce->flags & CEL_FORMULA) {
        if ((ce->flags & CEL_CALC) || prof > MAC_PROF_MAX) return res_zero();   /* ciclo */
        ps_t ps;
        ps.p = inicio_formula(ce->texto ? ce->texto : "");
        ps.folha = f;
        ps.prof = prof + 1;
        ps.erro = false;
        ce->flags |= CEL_CALC;
        res_t r = avaliar_expr(&ps);
        ce->flags &= (uint8_t)~CEL_CALC;
        ce->valor = r.num;
        ce->flags = (uint8_t)((ce->flags & ~(CEL_NUMERO | CEL_TEXTO)) |
                              (r.eh_texto ? CEL_TEXTO : CEL_NUMERO));
        return r;
    }
    res_t r = res_zero();
    if (ce->flags & CEL_NUMERO) { r.num = ce->valor; r.vazia = false; return r; }
    if (ce->flags & CEL_TEXTO) {
        r.eh_texto = true;
        r.vazia = false;
        copia_texto(r.texto, ce->texto ? ce->texto : "", sizeof r.texto);
        return r;
    }
    return r;   /* vacía */
}

static void pular_espacos(ps_t *ps)
{
    while (*ps->p == ' ' || *ps->p == '\t') ps->p++;
}

/* Lee las letras de una columna ("A" = 0, "AA" = 26). */
static bool ler_coluna(const char **p, int *col)
{
    const char *q = *p;
    int c = 0, n = 0;
    if (*q == '$') q++;                    /* referencia absoluta de columna */
    while ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z')) {
        char ch = *q;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        c = c * 26 + (ch - 'A' + 1);
        if (c > MAC_MAX_COLUNAS) return false;
        q++;
        if (++n > 3) return false;
    }
    if (n == 0) return false;
    *col = c - 1;
    *p = q;
    return true;
}

static bool ler_numero_linha(const char **p, int *lin)
{
    const char *q = *p;
    int v = 0, n = 0;
    if (*q == '$') q++;                    /* referencia absoluta de fila */
    while (*q >= '0' && *q <= '9') {
        v = v * 10 + (*q - '0');
        if (v > MAC_MAX_LINHAS) return false;
        q++;
        if (++n > 4) return false;
    }
    if (n == 0) return false;
    *lin = v - 1;
    *p = q;
    return true;
}

/* Lee una referencia: [Hoja!]COLUMNA FILA[:COLUMNA FILA]. */
static bool parse_ref(ps_t *ps, ref_t *r)
{
    const char *ini = ps->p;
    int folha = ps->folha;

    if ((*ps->p >= 'A' && *ps->p <= 'Z') || (*ps->p >= 'a' && *ps->p <= 'z') || *ps->p == '_') {
        const char *q = ps->p;
        char nome[MAC_NOME_FOLHA];
        int n = 0;
        while ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') ||
               (*q >= '0' && *q <= '9') || *q == '_') {
            if (n < MAC_NOME_FOLHA - 1) nome[n++] = *q;
            q++;
        }
        nome[n] = 0;
        if (*q == '!') {
            int f = procurar_folha(nome);
            if (f < 0) { ps->erro = true; ps->p = ini; return false; }
            folha = f;
            ps->p = q + 1;
        }
    }

    int col, lin;
    if (!ler_coluna(&ps->p, &col)) { ps->p = ini; return false; }
    if (!ler_numero_linha(&ps->p, &lin)) { ps->p = ini; return false; }

    r->folha = folha;
    r->c1 = r->c2 = col;
    r->l1 = r->l2 = lin;
    r->intervalo = true;

    if (*ps->p == ':') {
        ps->p++;
        int col2, lin2;
        if (!ler_coluna(&ps->p, &col2) || !ler_numero_linha(&ps->p, &lin2)) {
            ps->erro = true;
            return false;
        }
        r->c2 = col2;
        r->l2 = lin2;
    }
    if (r->l1 > r->l2) { int t = r->l1; r->l1 = r->l2; r->l2 = t; }
    if (r->c1 > r->c2) { int t = r->c1; r->c1 = r->c2; r->c2 = t; }
    return true;
}

/* El valor de un intervalo usado como escalar: la celda de arriba a la
 * izquierda. */
static res_t valor_intervalo(ref_t *r, int prof)
{
    return valor_celula(r->folha, r->l1, r->c1, prof);
}

/* Suma los números de un intervalo (o de un escalar). */
static void agregar_numero(ref_t *r, res_t *v, int64_t *soma, int32_t *min, int32_t *max,
                           int *cont, int prof)
{
    if (!r->intervalo) {
        if (v->eh_texto || v->vazia) return;
        if (*soma > 2000000000LL - v->num) *soma = 2000000000LL;
        else *soma += v->num;
        if (v->num < *min) *min = v->num;
        if (v->num > *max) *max = v->num;
        (*cont)++;
        return;
    }
    for (int l = r->l1; l <= r->l2; l++)
        for (int c = r->c1; c <= r->c2; c++) {
            res_t x = valor_celula(r->folha, l, c, prof);
            if (x.eh_texto || x.vazia) continue;
            if (*soma > 2000000000LL - x.num) *soma = 2000000000LL;
            else *soma += x.num;
            if (x.num < *min) *min = x.num;
            if (x.num > *max) *max = x.num;
            (*cont)++;
        }
}

/* Un argumento de función: o es una referencia (y se guarda el intervalo),
 * o es una expresión (y se guarda el valor). */
static void parse_argumento(ps_t *ps, ref_t *r, res_t *v)
{
    ps_t copia = *ps;
    r->intervalo = false;
    r->folha = ps->folha;
    r->l1 = r->l2 = r->c1 = r->c2 = 0;
    *v = res_zero();
    if (parse_ref(ps, r)) {
        pular_espacos(ps);
        if (*ps->p == ',' || *ps->p == ')') return;   /* es de verdad una referencia */
    }
    *ps = copia;
    r->intervalo = false;      /* era una expresión, no un intervalo */
    *v = avaliar_expr(ps);
}

/* Redondea un número en centésimas a n casas decimales. */
static int32_t arredondar(int32_t x, int n)
{
    int escala = (n <= 0) ? 100 : (n == 1 ? 10 : 1);
    int32_t q = x / escala;
    int32_t r = x % escala;
    if (r < 0) r = -r;
    if (r * 2 >= escala) q += (x < 0) ? -1 : 1;
    return q * escala;
}

static bool nome_igual(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return false;
        a++; b++;
    }
    return !*a && !*b;
}

/* Llama a una función. */
static res_t chamar_funcao(ps_t *ps, const char *nome)
{
    res_t r = res_zero();
    ps->p++;   /* el '(' */
    pular_espacos(ps);

    if (nome_igual(nome, "IF")) {
        res_t cond = avaliar_expr(ps);
        pular_espacos(ps);
        if (*ps->p == ',') ps->p++;
        res_t a = avaliar_expr(ps);
        bool erro_a = ps->erro;
        pular_espacos(ps);
        if (*ps->p == ',') ps->p++;
        res_t b = avaliar_expr(ps);
        bool erro_b = ps->erro;
        pular_espacos(ps);
        if (*ps->p == ')') ps->p++;
        else ps->erro = true;
        /* Sólo cuenta la rama elegida; el error de la otra no pasa adelante. */
        if (cond.num != 0) { r = a; ps->erro = erro_a; }
        else { r = b; ps->erro = erro_b; }
        return r;
    }

    ref_t  refs[8];
    res_t  vals[8];
    int    n = 0;
    bool   fecha = false;
    if (*ps->p != ')') {
        while (n < 8) {
            parse_argumento(ps, &refs[n], &vals[n]);
            n++;
            pular_espacos(ps);
            if (*ps->p == ',') { ps->p++; continue; }
            break;
        }
    }
    pular_espacos(ps);
    if (*ps->p == ')') { ps->p++; fecha = true; }
    else ps->erro = true;
    if (!fecha) return r;

    int64_t soma = 0;
    int32_t vmin = 2000000000, vmax = -2000000000;
    int cont = 0;

    if (nome_igual(nome, "SUM") || nome_igual(nome, "AVERAGE") || nome_igual(nome, "AVG") ||
        nome_igual(nome, "MIN") || nome_igual(nome, "MAX") || nome_igual(nome, "COUNT")) {
        for (int i = 0; i < n; i++) {
            if (nome_igual(nome, "COUNT")) {
                /* COUNT sólo cuenta las celdas con número. */
                if (refs[i].intervalo) {
                    for (int l = refs[i].l1; l <= refs[i].l2; l++)
                        for (int c = refs[i].c1; c <= refs[i].c2; c++) {
                            res_t x = valor_celula(refs[i].folha, l, c, ps->prof);
                            if (!x.eh_texto && !x.vazia) cont++;
                        }
                } else if (!vals[i].eh_texto && !vals[i].vazia) cont++;
                continue;
            }
            agregar_numero(&refs[i], &vals[i], &soma, &vmin, &vmax, &cont, ps->prof);
        }
        if (nome_igual(nome, "SUM")) { r.num = (int32_t)soma; r.vazia = false; return r; }
        if (nome_igual(nome, "COUNT")) { r.num = cont * 100; r.vazia = false; return r; }
        if (nome_igual(nome, "MIN")) { r.num = (cont > 0) ? vmin : 0; r.vazia = false; return r; }
        if (nome_igual(nome, "MAX")) { r.num = (cont > 0) ? vmax : 0; r.vazia = false; return r; }
        /* AVERAGE: divide la suma por las celdas, redondeando. */
        if (cont == 0) { r.num = 0; r.vazia = false; return r; }
        int64_t q = div64_64(soma, cont);
        int64_t resto = soma - q * (int64_t)cont;
        if (resto < 0) resto = -resto;
        if (resto * 2 >= cont) q += (soma < 0) ? -1 : 1;
        r.num = (int32_t)q;
        r.vazia = false;
        return r;
    }

    if (nome_igual(nome, "PRODUCT")) {
        int64_t prod = 100;    /* 1,00 en centésimas */
        for (int i = 0; i < n; i++) {
            if (refs[i].intervalo) {
                for (int l = refs[i].l1; l <= refs[i].l2; l++)
                    for (int c = refs[i].c1; c <= refs[i].c2; c++) {
                        res_t v = valor_celula(refs[i].folha, l, c, ps->prof);
                        if (v.eh_texto || v.vazia) continue;
                        if (prod > div64_64(900000000000000LL, v.num ? v.num : 1))
                            prod = 900000000000000LL;
                        else
                            prod = div64_64(prod * v.num, 100);
                    }
            } else {
                if (vals[i].eh_texto || vals[i].vazia) continue;
                if (prod > div64_64(900000000000000LL, vals[i].num ? vals[i].num : 1))
                    prod = 900000000000000LL;
                else
                    prod = div64_64(prod * vals[i].num, 100);
            }
        }
        r.num = (int32_t)prod;
        r.vazia = false;
        return r;
    }

    if (nome_igual(nome, "ABS")) {
        res_t v = vals[0];
        if (refs[0].intervalo) v = valor_intervalo(&refs[0], ps->prof);
        r.num = v.eh_texto ? 0 : (v.num < 0 ? -v.num : v.num);
        r.vazia = false;
        return r;
    }

    if (nome_igual(nome, "ROUND")) {
        res_t v = vals[0];
        if (refs[0].intervalo) v = valor_intervalo(&refs[0], ps->prof);
        int casas = 0;
        if (n > 1) {
            res_t d = vals[1];
            if (refs[1].intervalo) d = valor_intervalo(&refs[1], ps->prof);
            if (!d.eh_texto) casas = d.num / 100;
        }
        r.num = arredondar(v.eh_texto ? 0 : v.num, casas);
        r.vazia = false;
        return r;
    }

    ps->erro = true;   /* función desconocida */
    return r;
}

static res_t avaliar_primario(ps_t *ps)
{
    res_t r = res_zero();
    pular_espacos(ps);
    char c = *ps->p;

    if (c == '(') {
        ps->p++;
        r = avaliar_expr(ps);
        pular_espacos(ps);
        if (*ps->p == ')') ps->p++; else ps->erro = true;
        return r;
    }
    if (c == '"') {
        ps->p++;
        char buf[64];
        int n = 0;
        while (*ps->p && *ps->p != '"' && n < 63) buf[n++] = *ps->p++;
        buf[n] = 0;
        if (*ps->p == '"') ps->p++;
        r.eh_texto = true;
        r.vazia = false;
        copia_texto(r.texto, buf, sizeof r.texto);
        return r;
    }
    if (c == '-' || c == '+') {
        ps->p++;
        r = avaliar_primario(ps);
        if (!r.eh_texto && c == '-') r.num = -r.num;
        return r;
    }
    if ((c >= '0' && c <= '9') || c == '.') {
        int32_t v = 0;
        int n = ler_numero(ps->p, &v);
        if (n > 0) {
            ps->p += n;
            r.num = v;
            r.vazia = false;
            return r;
        }
    }
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$') {
        ps_t copia = *ps;
        ref_t rf;
        if (parse_ref(ps, &rf)) return valor_intervalo(&rf, ps->prof);
        *ps = copia;
        /* ¿Es el nombre de una función? */
        char nome[24];
        int n = 0;
        while ((*ps->p >= 'A' && *ps->p <= 'Z') || (*ps->p >= 'a' && *ps->p <= 'z')) {
            if (n < 23) nome[n++] = *ps->p;
            ps->p++;
        }
        nome[n] = 0;
        if (n > 0 && *ps->p == '(') return chamar_funcao(ps, nome);
        ps->erro = true;
        return r;
    }
    ps->erro = true;
    return r;
}

static res_t avaliar_unario(ps_t *ps)
{
    pular_espacos(ps);
    if (*ps->p == '-') {
        ps->p++;
        res_t r = avaliar_unario(ps);
        if (!r.eh_texto) r.num = -r.num;
        return r;
    }
    if (*ps->p == '+') { ps->p++; return avaliar_unario(ps); }
    return avaliar_primario(ps);
}

static res_t avaliar_termo(ps_t *ps)
{
    res_t r = avaliar_unario(ps);
    for (;;) {
        pular_espacos(ps);
        char c = *ps->p;
        if (c != '*' && c != '/') break;
        ps->p++;
        res_t d = avaliar_unario(ps);
        if (r.eh_texto || d.eh_texto) { r.num = 0; r.eh_texto = false; continue; }
        /* En centésimas: el producto se divide entre 100 (para quitar las dos
         * escalas) y la división multiplica por 100 (para volver a ponerla:
         * los dos operandos van en centésimas, pero el cociente es un
         * cociente de cantidades, no de centésimas). */
        if (c == '*') r.num = (int32_t)div64_64((int64_t)r.num * d.num, 100);
        else if (d.num == 0) r.num = 0;
        else r.num = (int32_t)div64_64((int64_t)r.num * 100, d.num);
    }
    return r;
}

/* Las comparaciones (=, <>, <, >, <=, >=): devuelven 1,00 si son ciertas y
 * 0 si no (como en cualquier hoja de cálculo). */
static res_t avaliar_comparar(ps_t *ps)
{
    res_t a = avaliar_termo(ps);
    pular_espacos(ps);
    char c1 = ps->p[0], c2 = ps->p[1];
    int tipo;      /* 1 = <, 2 = >, 3 = <=, 4 = >=, 5 = =, 6 = <> */
    if (c1 == '<' && c2 == '=') { tipo = 3; ps->p += 2; }
    else if (c1 == '>' && c2 == '=') { tipo = 4; ps->p += 2; }
    else if (c1 == '<' && c2 == '>') { tipo = 6; ps->p += 2; }
    else if (c1 == '<') { tipo = 1; ps->p += 1; }
    else if (c1 == '>') { tipo = 2; ps->p += 1; }
    else if (c1 == '=') { tipo = 5; ps->p += 1; }
    else return a;
    res_t b = avaliar_termo(ps);
    if (a.eh_texto || b.eh_texto) { a.num = 0; a.eh_texto = false; return a; }
    int cmp = (a.num < b.num) ? -1 : (a.num > b.num) ? 1 : 0;
    bool certo = false;
    switch (tipo) {
    case 1: certo = cmp < 0; break;
    case 2: certo = cmp > 0; break;
    case 3: certo = cmp <= 0; break;
    case 4: certo = cmp >= 0; break;
    case 5: certo = cmp == 0; break;
    case 6: certo = cmp != 0; break;
    default: break;
    }
    res_t r = res_zero();
    r.num = certo ? 100 : 0;    /* VERDADERO = 1,00 */
    r.vazia = false;
    return r;
}

static res_t avaliar_expr(ps_t *ps)
{
    res_t r = avaliar_comparar(ps);
    for (;;) {
        pular_espacos(ps);
        char c = *ps->p;
        if (c != '+' && c != '-') break;
        ps->p++;
        res_t d = avaliar_comparar(ps);
        if (r.eh_texto || d.eh_texto) { r.num = 0; r.eh_texto = false; continue; }
        r.num = (c == '+') ? (r.num + d.num) : (r.num - d.num);
    }
    return r;
}

/* Evalúa todas las fórmulas del libro y guarda los resultados en las
 * celdas. */
static void recalcular(void)
{
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        for (int l = 0; l < fo->linhas; l++)
            for (int c = 0; c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!(ce->flags & CEL_FORMULA)) continue;
                ps_t ps;
                ps.p = inicio_formula(ce->texto ? ce->texto : "");
                ps.folha = f;
                ps.prof = 1;
                ps.erro = false;
                ce->flags |= CEL_CALC;
                res_t r = avaliar_expr(&ps);
                ce->flags &= (uint8_t)~CEL_CALC;
                ce->valor = r.num;
                ce->flags = (uint8_t)((ce->flags & ~(CEL_NUMERO | CEL_TEXTO)) |
                                      (r.eh_texto ? CEL_TEXTO : CEL_NUMERO));
            }
    }
}

/* ------------------------------------------------------------ deshacer */

static void desfazer_limpar(void)
{
    for (int i = 0; i < n_desfazer; i++) {
        if (desfazer[i].antigo) { libera(desfazer[i].antigo); desfazer[i].antigo = NULL; }
        if (desfazer[i].copia) {
            int n = desfazer[i].linhas * desfazer[i].colunas;
            for (int k = 0; k < n; k++)
                if (desfazer[i].copia[k].texto) libera(desfazer[i].copia[k].texto);
            libera(desfazer[i].copia);
            desfazer[i].copia = NULL;
        }
    }
    n_desfazer = 0;
}

/* Descarta el paso más antiguo (la pila no crece sin límite). */
static void desfazer_antiguo(void)
{
    if (n_desfazer == 0) return;
    if (desfazer[0].antigo) { libera(desfazer[0].antigo); desfazer[0].antigo = NULL; }
    if (desfazer[0].copia) {
        int n = desfazer[0].linhas * desfazer[0].colunas;
        for (int k = 0; k < n; k++) if (desfazer[0].copia[k].texto) libera(desfazer[0].copia[k].texto);
        libera(desfazer[0].copia);
        desfazer[0].copia = NULL;
    }
    for (int i = 1; i < n_desfazer; i++) desfazer[i - 1] = desfazer[i];
    n_desfazer--;
}

static void desfazer_guardar_celula(int f, int l, int c, const char *antigo)
{
    if (n_desfazer >= MAC_UNDO_MAX) desfazer_antiguo();
    undo_t *u = &desfazer[n_desfazer++];
    memset(u, 0, sizeof *u);
    u->tipo = 0;
    u->folha = f;
    u->l = l;
    u->c = c;
    u->antigo = duplicar_texto(antigo);
}

/* Guarda la hoja entera, para deshacer las operaciones estructurales. */
static void desfazer_guardar_folha(int f)
{
    if (n_desfazer >= MAC_UNDO_MAX) desfazer_antiguo();
    folha_t *fo = &livro.folha[f];
    undo_t *u = &desfazer[n_desfazer++];
    memset(u, 0, sizeof *u);
    u->tipo = 1;
    u->folha = f;
    u->linhas = fo->linhas;
    u->colunas = fo->colunas;
    for (int c = 0; c < MAC_MAX_COLUNAS; c++) u->larg_col[c] = fo->larg_col[c];
    int n = fo->linhas * fo->colunas;
    u->copia = aloca_zerado((size_t)(n ? n : 1) * sizeof(celula_t));
    if (!u->copia) { n_desfazer--; return; }
    for (int k = 0; k < n; k++) {
        u->copia[k] = fo->celulas[k];
        u->copia[k].texto = duplicar_texto(fo->celulas[k].texto);
    }
}

static bool desfazer_refazer(void)
{
    if (n_desfazer == 0) return false;
    undo_t *u = &desfazer[--n_desfazer];
    if (u->tipo == 0) {
        folha_t *fo = &livro.folha[u->folha];
        celula_definir(fo, u->l, u->c, u->antigo ? u->antigo : "");
    } else {
        folha_t *fo = &livro.folha[u->folha];
        /* Suelta el contenido actual y mete la copia. */
        for (int k = 0; k < fo->linhas * fo->colunas; k++)
            if (fo->celulas[k].texto) { libera(fo->celulas[k].texto); fo->celulas[k].texto = NULL; }
        libera(fo->celulas);
        fo->linhas = u->linhas;
        fo->colunas = u->colunas;
        fo->celulas = u->copia;
        u->copia = NULL;
        for (int c = 0; c < MAC_MAX_COLUNAS; c++) fo->larg_col[c] = u->larg_col[c];
    }
    if (u->antigo) { libera(u->antigo); u->antigo = NULL; }
    return true;
}

/* ------------------------------------------------------------ copiar y pegar */

static void copia_limpar(void)
{
    if (copia.conteudos) {
        for (int i = 0; i < copia.linhas * copia.colunas; i++)
            if (copia.conteudos[i]) libera(copia.conteudos[i]);
        libera(copia.conteudos);
        copia.conteudos = NULL;
    }
    if (copia.fmts) { libera(copia.fmts); copia.fmts = NULL; }
    copia.linhas = copia.colunas = 0;
    copia.vazio = true;
}

static void intervalo_selecionado(int *l1, int *c1, int *l2, int *c2)
{
    *l1 = (anc_l < cel_l) ? anc_l : cel_l;
    *l2 = (anc_l < cel_l) ? cel_l : anc_l;
    *c1 = (anc_c < cel_c) ? anc_c : cel_c;
    *c2 = (anc_c < cel_c) ? cel_c : anc_c;
}

static void copiar_intervalo(void)
{
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    folha_t *fo = folha_ativa();
    copia_limpar();
    copia.linhas = l2 - l1 + 1;
    copia.colunas = c2 - c1 + 1;
    copia.conteudos = aloca_zerado((size_t)copia.linhas * copia.colunas * sizeof(char *));
    copia.fmts = aloca_zerado((size_t)copia.linhas * copia.colunas);
    if (!copia.conteudos || !copia.fmts) { copia_limpar(); copia.vazio = true; return; }
    for (int l = 0; l < copia.linhas; l++)
        for (int c = 0; c < copia.colunas; c++) {
            celula_t *ce = celula_de(fo, l1 + l, c1 + c);
            copia.fmts[l * copia.colunas + c] = ce ? ce->fmt : 0;
            copia.conteudos[l * copia.colunas + c] = duplicar_texto(ce && ce->texto ? ce->texto : "");
        }
    copia.vazio = false;
    char ref[16];
    nome_ref(ref, sizeof ref, l1, c1);
    registrar("macaco: copiadas %d filas x %d columnas desde %s", copia.linhas, copia.colunas, ref);
}

static void colar_intervalo(void)
{
    if (copia.vazio || !copia.conteudos) return;
    folha_t *fo = folha_ativa();
    desfazer_guardar_folha(livro.ativa);
    for (int l = 0; l < copia.linhas; l++)
        for (int c = 0; c < copia.colunas; c++) {
            const char *t = copia.conteudos[l * copia.colunas + c];
            celula_definir(fo, cel_l + l, cel_c + c, t ? t : "");
            celula_t *nc = celula_de(fo, cel_l + l, cel_c + c);
            if (nc && copia.fmts) nc->fmt = copia.fmts[l * copia.colunas + c];
        }
    modificado = true;
    recalcular();
    registrar("macaco: pegadas %d filas x %d columnas en %d,%d",
              copia.linhas, copia.colunas, cel_l + 1, cel_c + 1);
}

/* ------------------------------------------------------------ filas y columnas */

static bool linha_inserir(folha_t *fo, int l)
{
    if (fo->linhas + 1 > MAC_MAX_LINHAS) return false;
    if (!folha_redimensionar(fo, fo->linhas + 1, fo->colunas)) return false;
    size_t passo = (size_t)fo->colunas * sizeof(celula_t);
    memmove(&fo->celulas[(size_t)(l + 1) * fo->colunas], &fo->celulas[(size_t)l * fo->colunas],
            (size_t)(fo->linhas - l) * passo);
    for (int c = 0; c < fo->colunas; c++) {
        celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
        if (ce->texto) { libera(ce->texto); ce->texto = NULL; }
        ce->valor = 0;
        ce->flags = 0;
        ce->fmt = 0;
    }
    return true;
}

static bool linha_apagar(folha_t *fo, int l)
{
    if (fo->linhas <= 1) return false;
    for (int c = 0; c < fo->colunas; c++) {
        celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
        if (ce->texto) { libera(ce->texto); ce->texto = NULL; }
    }
    size_t passo = (size_t)fo->colunas * sizeof(celula_t);
    memmove(&fo->celulas[(size_t)l * fo->colunas], &fo->celulas[(size_t)(l + 1) * fo->colunas],
            (size_t)(fo->linhas - l - 1) * passo);
    for (int c = 0; c < fo->colunas; c++) {
        celula_t *ce = &fo->celulas[(size_t)(fo->linhas - 1) * fo->colunas + c];
        ce->texto = NULL;
        ce->valor = 0;
        ce->flags = 0;
        ce->fmt = 0;
    }
    fo->linhas--;
    return true;
}

static bool coluna_inserir(folha_t *fo, int c)
{
    if (fo->colunas + 1 > MAC_MAX_COLUNAS) return false;
    int nc = fo->colunas + 1;
    celula_t *novo = aloca_zerado((size_t)fo->linhas * nc * sizeof(celula_t));
    if (!novo) return false;
    for (int l = 0; l < fo->linhas; l++)
        for (int i = 0; i < fo->colunas; i++) {
            int d = (i < c) ? i : i + 1;
            novo[(size_t)l * nc + d] = fo->celulas[(size_t)l * fo->colunas + i];
        }
    libera(fo->celulas);
    fo->celulas = novo;
    fo->colunas = nc;
    return true;
}

static bool coluna_apagar(folha_t *fo, int c)
{
    if (fo->colunas <= 1) return false;
    int nc = fo->colunas - 1;
    celula_t *novo = aloca_zerado((size_t)fo->linhas * (nc ? nc : 1) * sizeof(celula_t));
    if (!novo) return false;
    for (int l = 0; l < fo->linhas; l++)
        for (int i = 0; i < fo->colunas; i++) {
            if (i == c) {
                celula_t *o = &fo->celulas[(size_t)l * fo->colunas + i];
                if (o->texto) { libera(o->texto); o->texto = NULL; }
                continue;
            }
            int d = (i < c) ? i : i - 1;
            novo[(size_t)l * nc + d] = fo->celulas[(size_t)l * fo->colunas + i];
        }
    libera(fo->celulas);
    fo->celulas = novo;
    fo->colunas = nc;
    return true;
}

/* ------------------------------------------------------------ ordenar */

/* Ordena las filas del intervalo por la primera columna (los números antes
 * que el texto; el texto en orden alfabético). */
static void ordenar_intervalo(bool crescente)
{
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    folha_t *fo = folha_ativa();
    int linhas = l2 - l1 + 1;
    if (linhas < 2) return;

    int cols = c2 - c1 + 1;
    char **conteudos = aloca_zerado((size_t)linhas * cols * sizeof(char *));
    int32_t *valores = aloca_zerado((size_t)linhas * sizeof(int32_t));
    bool *numeros = aloca_zerado((size_t)linhas * sizeof(bool));
    if (!conteudos || !valores || !numeros) {
        if (conteudos) libera(conteudos);
        if (valores) libera(valores);
        if (numeros) libera(numeros);
        return;
    }
    for (int l = 0; l < linhas; l++) {
        celula_t *ce = celula_de(fo, l1 + l, c1);
        valores[l] = (ce && (ce->flags & CEL_NUMERO)) ? ce->valor : 0;
        numeros[l] = ce && (ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO);
        for (int c = 0; c < cols; c++) {
            celula_t *x = celula_de(fo, l1 + l, c1 + c);
            conteudos[l * cols + c] = duplicar_texto(x && x->texto ? x->texto : "");
        }
    }
    /* Ordenación por inserción (es estable). */
    for (int a = 1; a < linhas; a++) {
        int i = a;
        while (i > 0) {
            int j = i - 1;
            bool menor;
            if (numeros[j] != numeros[i]) menor = numeros[i];   /* los números primero */
            else if (numeros[i]) menor = valores[i] < valores[j];
            else menor = compara_texto(conteudos[j * cols], conteudos[i * cols]) > 0;
            if (!menor) break;
            /* cambia las filas j e i */
            int32_t tv = valores[j]; valores[j] = valores[i]; valores[i] = tv;
            bool tn = numeros[j]; numeros[j] = numeros[i]; numeros[i] = tn;
            for (int c = 0; c < cols; c++) {
                char *ts = conteudos[j * cols + c]; conteudos[j * cols + c] = conteudos[i * cols + c];
                conteudos[i * cols + c] = ts;
            }
            i--;
        }
    }
    if (!crescente) {
        /* Da la vuelta al orden. */
        for (int a = 0; a < linhas / 2; a++) {
            int b = linhas - 1 - a;
            int32_t tv = valores[a]; valores[a] = valores[b]; valores[b] = tv;
            bool tn = numeros[a]; numeros[a] = numeros[b]; numeros[b] = tn;
            for (int c = 0; c < cols; c++) {
                char *ts = conteudos[a * cols + c]; conteudos[a * cols + c] = conteudos[b * cols + c];
                conteudos[b * cols + c] = ts;
            }
        }
    }
    for (int l = 0; l < linhas; l++)
        for (int c = 0; c < cols; c++) {
            char *t = conteudos[l * cols + c];
            conteudos[l * cols + c] = NULL;
            celula_definir(fo, l1 + l, c1 + c, t ? t : "");
            if (t) libera(t);
        }
    libera(conteudos);
    libera(valores);
    libera(numeros);
    modificado = true;
    recalcular();
    registrar("macaco: intervalo ordenado (%s)", crescente ? "A-Z" : "Z-A");
}

/* ------------------------------------------------------------ ficheros .mxs */

/* Las palabras clave del formato .mxs. */
#define MAC_KW_HOJA    "hoja "
#define MAC_KW_CELDA   "celda "
#define MAC_KW_LARGU   "larguras "

/* El formato es texto, para poder mirarse con cualquier editor:
 *
 *   MXS 1
 *   hoja "Hoja1" 20 8
 *   larguras 72,72,72
 *   celda A1 42
 *   celda B1 =SUM(A1:A10)
 *   celda C1 "un texto"
 *
 * La primera línea es la versión; cada hoja dice su nombre y sus dimensiones;
 * después, el contenido celda a celda (el número, la fórmula o el texto entre
 * comillas). */

static void txt_add(char **buf, int *tam, int *cap, const char *s)
{
    int n = (int)tam_texto(s);
    if (*tam + n + 1 > *cap) {
        int nova = *cap ? *cap * 2 : 4096;
        while (nova < *tam + n + 1) nova *= 2;
        char *b = realoca(*buf, (size_t)nova);
        if (!b) return;
        *buf = b;
        *cap = nova;
    }
    memcpy(*buf + *tam, s, (size_t)n);
    *tam += n;
    (*buf)[*tam] = 0;
}

static void txt_addf(char **buf, int *tam, int *cap, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    txt_add(buf, tam, cap, tmp);
}

/* El nombre de la hoja, entre comillas, con las comillas de dentro
 * escapadas. */
static void txt_nome(char *dst, int max, const char *nome)
{
    int n = 0;
    dst[n++] = '"';
    for (int i = 0; nome[i] && n < max - 3; i++) {
        if (nome[i] == '"') { dst[n++] = '\\'; if (n < max - 3) dst[n++] = '"'; }
        else dst[n++] = nome[i];
    }
    dst[n++] = '"';
    dst[n] = 0;
}

static int fmt_internar_livro(const fmt_t *f)
{
    if (livro.n_fmts < 1) livro.n_fmts = 1;
    for (int i = 0; i < livro.n_fmts; i++)
        if (memcmp(&livro.fmts[i], f, sizeof *f) == 0) return i;
    if (livro.n_fmts > MAC_MAX_FMT) return -1;
    livro.fmts[livro.n_fmts] = *f;
    return livro.n_fmts++;
}

static const fmt_t *fmt_de_livro(const celula_t *ce)
{
    static const fmt_t padrao;
    if (!ce || ce->fmt == 0 || ce->fmt >= livro.n_fmts) return &padrao;
    return &livro.fmts[ce->fmt];
}

static bool guardar_ficheiro(const char *destino)
{
    char *buf = NULL;
    int tam = 0, cap = 0;
    txt_add(&buf, &tam, &cap, "MXS 1\n");

    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        char nome[MAC_NOME_FOLHA + 4];
        txt_nome(nome, sizeof nome, fo->nome);
        txt_addf(&buf, &tam, &cap, "hoja %s %d %d\n", nome, fo->linhas, fo->colunas);
        /* Los anchos de las columnas, para que la hoja vuelva igual. */
        txt_add(&buf, &tam, &cap, "larguras ");
        for (int c = 0; c < fo->colunas; c++)
            txt_addf(&buf, &tam, &cap, "%s%d", c ? "," : "", fo->larg_col[c]);
        txt_add(&buf, &tam, &cap, "\n");
        for (int l = 0; l < fo->linhas; l++)
            for (int c = 0; c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!ce->texto) continue;
                char ref[16];
                nome_ref(ref, sizeof ref, l, c);
                if (ce->flags & CEL_TEXTO) {
                    char t[MAC_TEXTO_CEL + 4];
                    int n = 0;
                    t[n++] = '"';
                    for (int i = 0; ce->texto[i] && n < MAC_TEXTO_CEL + 2; i++) {
                        if (ce->texto[i] == '"' || ce->texto[i] == '\\') t[n++] = '\\';
                        t[n++] = ce->texto[i];
                    }
                    t[n++] = '"';
                    t[n] = 0;
                    txt_addf(&buf, &tam, &cap, "celda %s %s\n", ref, t);
                } else
                    txt_addf(&buf, &tam, &cap, "celda %s %s\n", ref, ce->texto);
            }
        /* Los formatos de celda (D293): una línea por celda con formato; los lectores antiguos las ignoran. */
        for (int l = 0; l < fo->linhas; l++)
            for (int c = 0; c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!ce->fmt) continue;
                const fmt_t *ft = fmt_de_livro(ce);
                char ref[16];
                nome_ref(ref, sizeof ref, l, c);
                txt_addf(&buf, &tam, &cap,
                         "formato %s nf=%d dec=%d mo=%d al=%d est=%d fam=%d tam=%d bor=%d cor=%x rel=%x\n",
                         ref, ft->nf, ft->dec, ft->moeda, ft->al, ft->est, ft->fam, ft->tam, ft->bor,
                         (unsigned)ft->cor, (unsigned)ft->rel);
            }
    }
    if (!buf) return false;

    int fd = abrir(destino, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        libera(buf);
        return false;
    }
    int r = escrever_tudo(fd, buf, (unsigned)tam);
    fechar(fd);
    libera(buf);
    if (r != tam) return false;
    return true;
}

/* Lee el fichero entero. */
static char *ler_ficheiro(const char *nome_ficheiro, int *tam)
{
    info_arquivo_t info;
    if (info_arquivo(nome_ficheiro, &info) < 0) return NULL;
    int n = (int)info.tamanho;
    if (n < 0) n = 0;
    int fd = abrir(nome_ficheiro, ABRIR_LER);
    if (fd < 0) return NULL;
    char *buf = aloca((size_t)n + 1);
    if (!buf) { fechar(fd); return NULL; }
    int lidos = 0, r;
    while (lidos < n && (r = ler(fd, buf + lidos, (unsigned)(n - lidos))) > 0) lidos += r;
    fechar(fd);
    buf[lidos] = 0;
    *tam = lidos;
    return buf;
}

/* Corta una línea del texto (sin el fin de línea). */
static char *linha_separada(char **p)
{
    char *ini = *p;
    if (!*ini) return NULL;
    char *fim = acha_char(ini, '\n');
    if (fim) { *fim = 0; *p = fim + 1; }
    else *p = ini + tam_texto(ini);
    /* Quita el \r del final (ficheros de Windows). */
    int n = (int)tam_texto(ini);
    if (n > 0 && ini[n - 1] == '\r') ini[n - 1] = 0;
    return ini;
}

/* El texto entre comillas, con las barras invertidas quitadas. */
static bool ler_aspas(const char **p, char *dst, int max)
{
    if (**p != '"') return false;
    (*p)++;
    int n = 0;
    while (**p && **p != '"') {
        char c = **p;
        if (c == '\\' && (*p)[1]) { (*p)++; c = **p; }
        if (n < max - 1) dst[n++] = c;
        (*p)++;
    }
    dst[n] = 0;
    if (**p == '"') (*p)++;
    return true;
}

/* ------------------------------------------------------------ CSV (D293) */

static bool termina_en(const char *t, const char *suf)
{
    int a = (int)tam_texto(t), b = (int)tam_texto(suf);
    if (a < b) return false;
    for (int i = 0; i < b; i++) {
        char c = t[a - b + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != suf[i]) return false;
    }
    return true;
}

static bool campo_precisa_comillas(const char *t)
{
    for (; *t; t++) if (*t == ',' || *t == '"' || *t == '\n' || *t == ';') return true;
    return false;
}

/* Escribe la hoja como CSV: separador coma, comillas dobles, números sin formato (con punto
 * decimal) y fórmulas con su resultado. */
static char *csv_texto(folha_t *fo, int *tam)
{
    char *buf = NULL;
    int n = 0, cap = 0;
    int ul = -1, uc = -1;
    for (int l = 0; l < fo->linhas; l++)
        for (int c = 0; c < fo->colunas; c++)
            if (fo->celulas[(size_t)l * fo->colunas + c].texto) { if (l > ul) ul = l; if (c > uc) uc = c; }
    for (int l = 0; l <= ul; l++) {
        for (int c = 0; c <= uc; c++) {
            celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
            char v[MAC_TEXTO_CEL + 8];
            v[0] = 0;
            if (ce->texto) {
                if ((ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO)) formatar_numero(v, sizeof v, ce->valor);
                else copia_texto(v, ce->texto, sizeof v);
            }
            if (c) txt_add(&buf, &n, &cap, ",");
            if (campo_precisa_comillas(v)) {
                char q[2 * MAC_TEXTO_CEL + 4];
                int k = 0;
                q[k++] = '"';
                for (int i = 0; v[i] && k < (int)sizeof q - 3; i++) { if (v[i] == '"') q[k++] = '"'; q[k++] = v[i]; }
                q[k++] = '"';
                q[k] = 0;
                txt_add(&buf, &n, &cap, q);
            } else txt_add(&buf, &n, &cap, v);
        }
        txt_add(&buf, &n, &cap, "\n");
    }
    if (!buf) { buf = aloca(1); if (buf) buf[0] = 0; }
    *tam = n;
    return buf;
}

static bool escrever_buffer(const char *destino, const void *d, int n)
{
    int fd = abrir(destino, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    int r = escrever_tudo(fd, d, (unsigned)n);
    fechar(fd);
    return r == n;
}

static bool csv_gravar(const char *destino)
{
    int n;
    char *t = csv_texto(folha_ativa(), &n);
    if (!t) return false;
    bool ok = escrever_buffer(destino, t, n);
    libera(t);
    return ok;
}

/* Lee un CSV en la hoja, empezando en (l0, c0). Detecta el separador (coma, punto y coma o
 * tabulador) por la primera línea y convierte UTF-8 a Latin-1. Devuelve las celdas leídas. */
static int csv_ler(folha_t *fo, const char *nome_ficheiro, int l0, int c0)
{
    int tam = 0;
    char *buf = ler_ficheiro(nome_ficheiro, &tam);
    if (!buf) return -1;
    tam = (int)utf8_para_latin1(buf, (size_t)tam, NULL);
    buf[tam] = 0;
    int nc = 0, np = 0, nt = 0;
    bool aspas = false;
    for (int i = 0; i < tam && buf[i] != '\n'; i++) {
        if (buf[i] == '"') aspas = !aspas;
        if (aspas) continue;
        if (buf[i] == ',') nc++; else if (buf[i] == ';') np++; else if (buf[i] == '\t') nt++;
    }
    char sep = (np > nc && np >= nt) ? ';' : (nt > nc && nt > np) ? '\t' : ',';
    int l = 0, c = 0, lidas = 0;
    int i = 0;
    while (i < tam) {
        char campo[MAC_TEXTO_CEL];
        int k = 0;
        if (buf[i] == '"') {
            i++;
            while (i < tam) {
                if (buf[i] == '"') { if (i + 1 < tam && buf[i + 1] == '"') { if (k < MAC_TEXTO_CEL - 1) campo[k++] = '"'; i += 2; continue; } i++; break; }
                if (k < MAC_TEXTO_CEL - 1) campo[k++] = buf[i] == '\n' ? ' ' : buf[i];
                i++;
            }
            while (i < tam && buf[i] != sep && buf[i] != '\n') i++;
        } else {
            while (i < tam && buf[i] != sep && buf[i] != '\n') { if (k < MAC_TEXTO_CEL - 1 && buf[i] != '\r') campo[k++] = buf[i]; i++; }
        }
        campo[k] = 0;
        if (campo[0] && celula_definir(fo, l0 + l, c0 + c, campo)) lidas++;
        if (i < tam && buf[i] == sep) { c++; i++; continue; }
        /* fin de línea (o de fichero) */
        if (i < tam && buf[i] == '\n') i++;
        l++;
        c = 0;
        if (l0 + l >= MAC_MAX_LINHAS) break;
    }
    libera(buf);
    return lidas;
}

static bool carregar_ficheiro(const char *nome_ficheiro)
{
    int tam = 0;
    if (termina_en(nome_ficheiro, ".csv")) {
        /* Un CSV se abre como libro de una hoja. */
        for (int f = 0; f < livro.n; f++) {
            folha_t *fo = &livro.folha[f];
            for (int k = 0; k < fo->linhas * fo->colunas; k++)
                if (fo->celulas[k].texto) { libera(fo->celulas[k].texto); fo->celulas[k].texto = NULL; }
            if (fo->celulas) { libera(fo->celulas); fo->celulas = NULL; }
        }
        memset(&livro, 0, sizeof livro);
        livro.n_fmts = 1;
        desfazer_limpar();
        copia_limpar();
        folha_nova("Hoja1");
        int n = csv_ler(&livro.folha[0], nome_ficheiro, 0, 0);
        if (n < 0) return false;
        livro.ativa = 0;
        recalcular();
        registrar("macaco: leídas %d celdas de CSV", n);
        return true;
    }
    char *buf = ler_ficheiro(nome_ficheiro, &tam);
    if (!buf) return false;

    /* Libro nuevo. */
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        for (int k = 0; k < fo->linhas * fo->colunas; k++)
            if (fo->celulas[k].texto) { libera(fo->celulas[k].texto); fo->celulas[k].texto = NULL; }
        if (fo->celulas) { libera(fo->celulas); fo->celulas = NULL; }
    }
    memset(&livro, 0, sizeof livro);
    livro.n_fmts = 1;
    desfazer_limpar();
    copia_limpar();

    char *p = buf;
    char *lin = linha_separada(&p);
    if (!lin || !comeca_com(lin, "MXS ")) { libera(buf); return false; }
    int versao = texto_para_int(lin + 4);
    if (versao < 1) { libera(buf); return false; }

    int folha_atual = -1;
    int n_celulas = 0;
    while ((lin = linha_separada(&p)) != NULL) {
        if (!lin[0]) continue;
        if (comeca_com(lin, MAC_KW_HOJA)) {
            const char *q = lin + (int)sizeof(MAC_KW_HOJA) - 1;
            char nome[MAC_NOME_FOLHA];
            if (!ler_aspas(&q, nome, sizeof nome)) { libera(buf); return false; }
            int li = texto_para_int(q);
            const char *r = acha_char(q, ' ');
            int co = 0;
            if (r) co = texto_para_int(r + 1);
            if (li < 1) li = 1;
            if (co < 1) co = 1;
            if (li > MAC_MAX_LINHAS) li = MAC_MAX_LINHAS;
            if (co > MAC_MAX_COLUNAS) co = MAC_MAX_COLUNAS;
            if (livro.n >= MAC_MAX_FOLHAS) break;
            folha_t *fo = &livro.folha[livro.n++];
            memset(fo, 0, sizeof *fo);
            copia_texto(fo->nome, nome, sizeof fo->nome);
            fo->linhas = li;
            fo->colunas = co;
            fo->celulas = aloca_zerado((size_t)li * co * sizeof(celula_t));
            for (int c = 0; c < fo->colunas; c++) fo->larg_col[c] = MAC_LARGCOL;
            folha_atual = livro.n - 1;
        } else if (comeca_com(lin, MAC_KW_CELDA) && folha_atual >= 0) {
            const char *q = lin + (int)sizeof(MAC_KW_CELDA) - 1;
            /* La referencia: las letras de la columna y el número de la fila. */
            int col, lin2;
            if (!ler_coluna(&q, &col) || !ler_numero_linha(&q, &lin2)) continue;
            while (*q == ' ') q++;
            char conteudo[MAC_TEXTO_CEL];
            if (*q == '"') {
                if (!ler_aspas(&q, conteudo, sizeof conteudo)) continue;
            } else {
                int n = (int)tam_texto(q);
                if (n > MAC_TEXTO_CEL - 1) n = MAC_TEXTO_CEL - 1;
                memcpy(conteudo, q, (size_t)n);
                conteudo[n] = 0;
            }
            folha_t *fo = &livro.folha[folha_atual];
            if (celula_definir(fo, lin2, col, conteudo)) n_celulas++;
        } else if (comeca_com(lin, "formato ") && folha_atual >= 0) {
            const char *q = lin + 8;
            int col, lin2;
            if (!ler_coluna(&q, &col) || !ler_numero_linha(&q, &lin2)) continue;
            fmt_t ft;
            memset(&ft, 0, sizeof ft);
            while (*q) {
                while (*q == ' ') q++;
                const char *k = q;
                while (*q && *q != '=' && *q != ' ') q++;
                if (*q != '=') break;
                int kl = (int)(q - k);
                q++;
                unsigned v = 0;
                const char *vi = q;
                bool hex = (kl == 3 && (memcmp(k, "cor", 3) == 0 || memcmp(k, "rel", 3) == 0));
                while ((*q >= '0' && *q <= '9') || (hex && ((*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F')))) {
                    int d = (*q <= '9') ? *q - '0' : ((*q | 32) - 'a' + 10);
                    v = v * (hex ? 16u : 10u) + (unsigned)d;
                    q++;
                }
                if (q == vi) break;
                if (kl == 2 && memcmp(k, "nf", 2) == 0) ft.nf = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "dec", 3) == 0) ft.dec = (uint8_t)v;
                else if (kl == 2 && memcmp(k, "mo", 2) == 0) ft.moeda = (uint8_t)v;
                else if (kl == 2 && memcmp(k, "al", 2) == 0) ft.al = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "est", 3) == 0) ft.est = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "fam", 3) == 0) ft.fam = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "tam", 3) == 0) ft.tam = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "bor", 3) == 0) ft.bor = (uint8_t)v;
                else if (kl == 3 && memcmp(k, "cor", 3) == 0) ft.cor = v;
                else if (kl == 3 && memcmp(k, "rel", 3) == 0) ft.rel = v;
            }
            folha_t *fo = &livro.folha[folha_atual];
            if (!folha_redimensionar(fo, lin2 + 1 > fo->linhas ? lin2 + 1 : fo->linhas, col + 1 > fo->colunas ? col + 1 : fo->colunas)) continue;
            int idx = fmt_internar_livro(&ft);
            celula_t *ce = celula_de(fo, lin2, col);
            if (ce && idx >= 0) ce->fmt = (uint8_t)idx;
        } else if (comeca_com(lin, MAC_KW_LARGU) && folha_atual >= 0) {
            const char *q = lin + (int)sizeof(MAC_KW_LARGU) - 1;
            folha_t *fo = &livro.folha[folha_atual];
            int c = 0;
            while (*q && c < MAC_MAX_COLUNAS) {
                int v = texto_para_int(q);
                if (v < 16) v = 16;
                if (v > 400) v = 400;
                fo->larg_col[c++] = v;
                while (*q && *q != ',') q++;
                if (*q == ',') q++;
            }
        }
    }
    libera(buf);
    if (livro.n == 0) return false;
    livro.ativa = 0;
    recalcular();
    registrar("macaco: leídas %d celdas de %d hojas", n_celulas, livro.n);
    return true;
}

/* ------------------------------------------------------------ gráficos */

/* El seno de un ángulo en milésimas de grado, en diezmilésimas (1/10000).
 * Usa la fórmula de Bhaskara, que sólo necesita enteros. */
static int16_t tabela_sen[91];
static bool tabela_pronta;

static void preparar_senos(void)
{
    if (tabela_pronta) return;
    for (int g = 0; g <= 90; g++) {
        int num = 4 * g * (180 - g);
        int den = 40500 - g * (180 - g);
        if (den <= 0) den = 1;
        tabela_sen[g] = (int16_t)(num * 10000 / den);
    }
    tabela_sen[90] = 10000;
    tabela_pronta = true;
}

static int sen_md(int md)
{
    while (md < 0) md += 360000;
    md %= 360000;
    /* Entre 180° y 360° el seno es negativo: sin(360° - x) = -sin(x). */
    bool neg = false;
    if (md > 180000) { md = 360000 - md; neg = true; }
    /* El segundo cuadrante (90°..180°) se refleja sobre los 90°, y el
     * tercero (180°..270°), ya reducido, también: sin esto la tabla de
     * senos se leería fuera de rango. */
    if (md > 90000) md = 180000 - md;
    int g = md / 1000, frac = md % 1000;
    int s0 = tabela_sen[g];
    int s1 = tabela_sen[(g < 90) ? g + 1 : 90];
    int v = s0 + ((s1 - s0) * frac) / 1000;
    return neg ? -v : v;
}

/* El punto del círculo (ángulo en milésimas de grado, desde las tres). */
static void ponto_circulo(int cx, int cy, int r, int md, int *x, int *y)
{
    int s = sen_md(md);
    int c = sen_md(90000 - md);
    *x = cx + (r * c) / 10000;
    *y = cy + (r * s) / 10000;
}

/* Rellena un polígono por líneas horizontales (regla del par-impar), como
 * hace el lienzo de la libsagui. */
static void preencher_poligono(jan_t *g, const int *pts, int n, uint32_t cor)
{
    static int xs[300];
    if (n < 3) return;
    int ymin = pts[1], ymax = pts[1];
    for (int i = 1; i < n; i++) {
        int v = pts[i * 2 + 1];
        if (v < ymin) ymin = v;
        if (v > ymax) ymax = v;
    }
    for (int fy = ymin; fy <= ymax; fy++) {
        int nxs = 0;
        for (int i = 0, k = n - 1; i < n; k = i++) {
            int xi = pts[i * 2], yi = pts[i * 2 + 1];
            int xk = pts[k * 2], yk = pts[k * 2 + 1];
            if ((yi > fy) != (yk > fy))
                if (nxs < 300) xs[nxs++] = xi + (fy - yi) * (xk - xi) / (yk - yi);
        }
        for (int a = 1; a < nxs; a++) {          /* ordenación por inserción */
            int v = xs[a], b = a - 1;
            while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; }
            xs[b + 1] = v;
        }
        for (int a = 0; a + 1 < nxs; a += 2) {
            int x0 = xs[a], x1 = xs[a + 1];
            if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
            g_retangulo(g, x0, fy, x1 - x0, 1, cor);
        }
    }
}

/* Toma los datos del intervalo seleccionado: las etiquetas son las de la
 * primera columna y los valores los de la segunda (si no hay segunda, los
 * valores son los de la primera y las etiquetas son los números de fila). */
static bool recolher_dados(void)
{
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    folha_t *fo = folha_ativa();
    n_series = 0;
    int col_val = (c2 > c1) ? c1 + 1 : c1;
    for (int l = l1; l <= l2 && n_series < MAC_SERIE_MAX; l++) {
        serie_t *s = &series[n_series];
        char t[48];
        t[0] = 0;
        if (c2 > c1) {
            celula_t *ce = celula_de(fo, l, c1);
            if (ce && (ce->flags & CEL_TEXTO) && !(ce->flags & CEL_FORMULA))
                copia_texto(t, ce->texto, sizeof t);
            else if (ce && (ce->flags & CEL_NUMERO))
                formatar_numero(t, sizeof t, ce->valor);
        }
        if (!t[0]) formatar_texto(t, sizeof t, "%d", l + 1);
        copia_texto(s->rotulo, t, sizeof s->rotulo);
        celula_t *cv = celula_de(fo, l, col_val);
        s->valor = (cv && (cv->flags & CEL_NUMERO)) ? cv->valor : 0;
        s->cor = CORES_SERIE[n_series % 8];
        n_series++;
    }
    return n_series > 0;
}

static const char *nome_grafico(void)
{
    if (tipo_grafico == GRAF_TARTA) return "Gráfico de tarta";
    if (tipo_grafico == GRAF_BARRAS_H) return "Gráfico de barras horizontales";
    return "Gráfico de barras";
}

static int legenda_altura(int larg)
{
    if (n_series <= 0) return 0;
    int cols = larg / 150;
    if (cols < 1) cols = 1;
    int linhas = (n_series + cols - 1) / cols;
    return linhas * 20 + 8;
}

static void grafico_desenhar(void)
{
    jan_t *g = jan_graf;
    if (!g) return;
    preparar_senos();
    g_retangulo(g, 0, 0, g->larg, g->alt, COR_FUNDO);

    char tit[80];
    formatar_texto(tit, sizeof tit, "Macaco - %s", nome_grafico());
    g_texto(g, 12, 8, tit, COR_TEXTO);

    int alt_legenda = legenda_altura(g->larg);
    int ax = 48, ay = 34;
    int aw = g->larg - 60, ah = g->alt - ay - alt_legenda - 14;
    if (aw < 20) aw = 20;
    if (ah < 20) ah = 20;

    /* Los valores extremos, para escalar el gráfico. */
    int32_t vmax = 0, vmin = 0;
    for (int i = 0; i < n_series; i++) {
        if (series[i].valor > vmax) vmax = series[i].valor;
        if (series[i].valor < vmin) vmin = series[i].valor;
    }
    int32_t span = vmax - vmin;
    if (span <= 0) span = 1;

    if (tipo_grafico == GRAF_TARTA) {
        int raio = (aw < ah ? aw : ah) / 2 - 10;
        if (raio < 10) raio = 10;
        int cx = ax + aw / 2, cy = ay + ah / 2;
        int64_t total = 0;
        for (int i = 0; i < n_series; i++) if (series[i].valor > 0) total += series[i].valor;
        if (total <= 0) {
            g_texto(g, ax + 10, ay + 10, "Sin valores para el gráfico", COR_TEXTO_FRACO);
        } else {
            static int pontos[600];
            int ang = -90000;    /* empieza a las doce */
            for (int i = 0; i < n_series; i++) {
                if (series[i].valor <= 0) continue;
                int pedaco = (int)div64_64((int64_t)series[i].valor * 360000, total);
                if (pedaco <= 0) continue;
                int np = 0;
                pontos[np * 2] = cx; pontos[np * 2 + 1] = cy; np++;
                int passos = pedaco / 3000 + 1;
                for (int p = 0; p <= passos; p++) {
                    int a2 = ang + (int)div64_64((int64_t)pedaco * p, passos);
                    int px, py;
                    ponto_circulo(cx, cy, raio, a2, &px, &py);
                    pontos[np * 2] = px; pontos[np * 2 + 1] = py; np++;
                    if (np >= 290) break;
                }
                preencher_poligono(g, pontos, np, series[i].cor);
                for (int p = 1; p < np; p++)
                    g_linha(g, pontos[(p - 1) * 2], pontos[(p - 1) * 2 + 1],
                            pontos[p * 2], pontos[p * 2 + 1], 0x2C3E50);
                ang += pedaco;
            }
        }
    } else if (tipo_grafico == GRAF_BARRAS) {
        int base = ay + ah - (int)div64_64((int64_t)(0 - vmin) * ah, span);
        int n = n_series > 0 ? n_series : 1;
        int larg_disp = aw / n;
        for (int i = 0; i < n_series; i++) {
            int x = ax + i * larg_disp + 3;
            int w = larg_disp - 6;
            if (w < 2) w = 2;
            int h = (int)div64_64((int64_t)series[i].valor * ah, span);
            if (h < 0) h = -h;
            int y = (series[i].valor >= 0) ? base - h : base;
            g_retangulo(g, x, y, w, h, series[i].cor);
            g_moldura(g, x, y, w, h, 0x2C3E50);
            char vt[24];
            formatar_numero(vt, sizeof vt, series[i].valor);
            int tx = x + (w - g_largura_texto(vt)) / 2;
            if (tx < ax) tx = ax;
            g_texto(g, tx, (series[i].valor >= 0 ? y - 18 : y + h + 2), vt, COR_TEXTO);
        }
        g_linha(g, ax, base, ax + aw, base, 0x6A737C);
    } else {
        int esq = ax;
        int n = n_series > 0 ? n_series : 1;
        int alt_disp = ah / n;
        for (int i = 0; i < n_series; i++) {
            int y = ay + i * alt_disp + 3;
            int h = alt_disp - 6;
            if (h < 2) h = 2;
            int w = (int)div64_64((int64_t)series[i].valor * aw, span);
            if (w < 0) w = -w;
            g_retangulo(g, esq, y, w, h, series[i].cor);
            g_moldura(g, esq, y, w, h, 0x2C3E50);
            char vt[24];
            formatar_numero(vt, sizeof vt, series[i].valor);
            int tx = esq + w + 4;
            if (tx + g_largura_texto(vt) > ax + aw) tx = esq + w - g_largura_texto(vt) - 4;
            g_texto(g, tx, y + (h - LETRA_A) / 2, vt, COR_TEXTO);
        }
        g_linha(g, esq, ay, esq, ay + ah, 0x6A737C);
    }

    /* La leyenda. */
    int64_t total = 0;
    for (int i = 0; i < n_series; i++) if (series[i].valor > 0) total += series[i].valor;
    int cols = g->larg / 150;
    if (cols < 1) cols = 1;
    int ly = g->alt - alt_legenda + 2;
    for (int i = 0; i < n_series; i++) {
        int x = 12 + (i % cols) * 150;
        int y = ly + (i / cols) * 20;
        g_retangulo(g, x, y + 2, 14, 14, series[i].cor);
        g_moldura(g, x, y + 2, 14, 14, 0x2C3E50);
        char t[80];
        char vt[24];
        formatar_numero(vt, sizeof vt, series[i].valor);
        if (tipo_grafico == GRAF_TARTA && total > 0)
            formatar_texto(t, sizeof t, "%s %d%%", series[i].rotulo,
                           (int)div64_64((int64_t)series[i].valor * 100, total));
        else
            formatar_texto(t, sizeof t, "%s %s", series[i].rotulo, vt);
        g_texto_n(g, x + 20, y + 2, t, 17, COR_TEXTO);
    }
    jan_mostrar(g);
}

/* Abre (o vuelve a dibujar) la ventana del gráfico. */
static void abrir_grafico(int tipo)
{
    if (!recolher_dados()) {
        por_aqui(estado, sizeof estado, "No hay datos para el gráfico");
        return;
    }
    tipo_grafico = tipo;
    char tit[80];
    formatar_texto(tit, sizeof tit, "Macaco - %s", nome_grafico());
    if (!jan_graf) {
        jan_graf = jan_abrir(tit, 640, 460);
        if (!jan_graf) {
            por_aqui(estado, sizeof estado, "No se ha podido abrir la ventana del gráfico");
            return;
        }
    } else {
        titulo_janela(jan_graf->id, tit);
    }
    grafico_desenhar();
    registrar("macaco: gráfico %s con %d series", nome_grafico(), n_series);
}

/* ------------------------------------------------------------ formatos de celda (D293) */

static const char *const MOEDAS[3] = { "$", "EUR ", "\xA3" };
static const char *const NOMES_FONTE[3] = { "Arial", "Times New Roman", "Courier New" };
static const uint32_t PALETA_COR[9] = {
    0x000000, 0xFF0000, 0x008000, 0x0000FF, 0x000080, 0x800000, 0x808080, 0xFF8000, 0xFFFFFF
};
static const char *const NOMES_COR[9] = {
    "Negro", "Rojo", "Verde", "Azul", "Azul oscuro", "Rojo oscuro", "Gris", "Naranja", "Blanco"
};
static const uint32_t PALETA_REL[9] = {
    0xFFFF00, 0xFFC0C0, 0xC0FFC0, 0xC0C0FF, 0xFFE0A0, 0xC0FFFF, 0xFFC0FF, 0xE0E0E0, 0x00FFFF
};
static const char *const NOMES_REL[9] = {
    "Amarillo", "Rosa", "Verde claro", "Azul claro", "Naranja claro", "Cian claro", "Lila", "Gris claro", "Cian"
};

#define fmt_internar fmt_internar_livro
#define fmt_de fmt_de_livro

/* Texto de un número (en centésimas) según su formato. */
static void fmt_numero(char *dst, int max, int32_t cent, const fmt_t *f)
{
    if (f->nf == NF_GERAL) { formatar_numero(dst, max, cent); return; }
    bool neg = cent < 0;
    int32_t v = neg ? -cent : cent;
    int dec = f->dec > 4 ? 4 : f->dec;
    int64_t ip;
    int frac = 0;
    if (dec == 0) { ip = (v + 50) / 100; }
    else if (dec == 1) { int t = (v + 5) / 10; ip = t / 10; frac = t % 10; }
    else { ip = v / 100; frac = v % 100; }
    char dig[24];
    formatar_texto(dig, sizeof dig, "%d", (int)ip);
    char com[40];
    int n = 0, len = (int)tam_texto(dig);
    bool miles = (f->nf == NF_MOEDA || f->nf == NF_MILHAR);
    for (int i = 0; i < len; i++) {
        com[n++] = dig[i];
        int resto = len - 1 - i;
        if (miles && resto > 0 && resto % 3 == 0) com[n++] = ',';
    }
    com[n] = 0;
    char f2[16];
    f2[0] = 0;
    if (dec == 1) formatar_texto(f2, sizeof f2, ".%d", frac);
    else if (dec >= 2) {
        formatar_texto(f2, sizeof f2, ".%02d", frac);
        for (int k = 2; k < dec; k++) copia_texto(f2 + tam_texto(f2), "0", 2);
    }
    if (f->nf == NF_PERCENT) {
        /* 0.25 se guarda como 25 centésimas: ya es "25 %". */
        formatar_texto(dst, max, "%s%d%%", neg ? "-" : "", (int)(v / 1));
        if (dec > 0) {
            char z[8];
            z[0] = '.';
            int k = 1;
            for (; k <= dec && k < 7; k++) z[k] = '0';
            z[k] = 0;
            formatar_texto(dst, max, "%s%d%s%%", neg ? "-" : "", (int)v, z);
        }
        return;
    }
    formatar_texto(dst, max, "%s%s%s%s", neg ? "-" : "", f->nf == NF_MOEDA ? MOEDAS[f->moeda < 3 ? f->moeda : 0] : "", com, f2);
}

/* El texto que se ve en la celda. */
static void celula_exibida(char *dst, int max, celula_t *ce)
{
    if (!ce) { dst[0] = 0; return; }
    if (ce->flags & CEL_TEXTO) {
        copia_texto(dst, ce->texto ? ce->texto : "", (size_t)max);
        return;
    }
    if (ce->flags & CEL_NUMERO) { fmt_numero(dst, max, ce->valor, fmt_de(ce)); return; }
    dst[0] = 0;
}

/* El contenido de la celda (la fórmula o el literal), para la barra de fórmulas. */
static void celula_conteudo(char *dst, int max, celula_t *ce)
{
    if (!ce) { dst[0] = 0; return; }
    copia_texto(dst, ce->texto ? ce->texto : "", (size_t)max);
}

/* ------------------------------------------------------------ geometría y estado de la interfaz */

static int off_col[MAC_MAX_COLUNAS + 1];

static void calcular_offsets(void)
{
    folha_t *fo = folha_ativa();
    off_col[0] = 0;
    for (int c = 0; c < fo->colunas; c++) off_col[c + 1] = off_col[c] + fo->larg_col[c];
}

static int x_coluna(int c) { return MAC_LARGC + off_col[c] - desl_x; }
static int y_linha(int l) { return MAC_BARRA + MAC_FORMULA + MAC_CABECALH + l * MAC_ALTLINHA - desl_y; }
static int grelha_y(void) { return MAC_BARRA + MAC_FORMULA + MAC_CABECALH; }
static int grelha_a(void) { return jan->alt - grelha_y() - MAC_RODAPE; }

#define Y_FERR1  MENU_A
#define Y_FERR2  (MENU_A + FERR_A)
#define Y_FORM   MAC_BARRA
#define Y_PEST   (jan->alt - MAC_RODAPE)
#define Y_ESTADO (jan->alt - ESTADO_A)

enum {
    A_NOVO = 1, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, A_PDF, A_CSV_IMP, A_CSV_EXP, A_SAIR,
    A_DESFAZER, A_CORTAR, A_COPIAR, A_COLAR, A_BORRAR, A_BORRAR_FMT, A_TUDO, A_IR,
    A_LINHA_INS, A_LINHA_DEL, A_COL_INS, A_COL_DEL, A_FOLHA_NOVA, A_FOLHA_DEL, A_FOLHA_REN,
    A_GRAF_B, A_GRAF_H, A_GRAF_T,
    A_CELULAS, A_NEGRITA, A_CURSIVA, A_SUBLIN, A_ESQ, A_CENTRO, A_DIR,
    A_MOEDA, A_PERCENT, A_MILHAR, A_DEC_MAIS, A_DEC_MENOS, A_BORDAS, A_RELLENO, A_COR_TXT,
    A_LARG, A_ORD_A, A_ORD_Z, A_SOMA, A_RECALC, A_GRADE, A_FORMULAS, A_ACERCA, A_IGUAL,
    /* elecciones de colores y bordes (menús desplegables de los botones) */
    A_REL_0 = 100, A_COR_0 = 120, A_BOR_0 = 140
};

#define COD_REL   8201
#define COD_COR   8202
#define COD_BOR   8203
#define COD_PEST  8204
#define COD_CTX   8205

static barra_menu_t   menu;
static barra_ferr_t   ferr1, ferr2;
static barra_item_t   it1[20], it2[24];
static combo_t        c_fonte, c_tam;
static barra_estado_t barra_est;
static char           ult_titulo[160];
static uint32_t       cor_rel_atual = 0xFFFF00, cor_txt_atual = 0xFF0000;
static bool           grade_visivel = true;
static bool           mostrar_formulas;
static bool           nome_foco;           /* se está escribiendo en el cuadro de nombre */
static char           nome_buf[24];
static int            redim_col = -1, redim_x0, redim_w0;   /* arrastre del borde de una columna */
static int            pest_ctx = -1;       /* hoja sobre la que se abrió el menú de pestañas */

/* ------------------------------------------------------------ iconos propios de Macaco */

#define R(x_, y_, w_, h_, c_) g_retangulo(j, x + (x_), y + (y_), (w_), (h_), (c_))

/* Letras y cifras de 3x5 para los iconos (cada fila es un patrón de 3 bits). */
static void mini(jan_t *j, int x, int y, const char *fil, uint32_t cor)
{
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (fil[r * 3 + c] == '#') g_retangulo(j, x + c, y + r, 1, 1, cor);
}
static const char G_A[] = ".#.#.####.##.#";
static const char G_Z[] = "###..#.#.#..###";
static const char G_0[] = "####.##.##.####";

static void ic_soma(jan_t *j, int x, int y, bool a)
{
    (void)a;
    R(3, 1, 10, 2, W98_NEGRO);
    g_linha(j, x + 3, y + 2, x + 8, y + 8, W98_NEGRO); g_linha(j, x + 4, y + 2, x + 9, y + 8, W98_NEGRO);
    g_linha(j, x + 8, y + 8, x + 3, y + 14, W98_NEGRO); g_linha(j, x + 9, y + 8, x + 4, y + 14, W98_NEGRO);
    R(3, 13, 10, 2, W98_NEGRO);
    R(12, 3, 1, 2, W98_NEGRO); R(12, 11, 1, 2, W98_NEGRO);
}
static void flecha_ord(jan_t *j, int x, int y, bool baixo)
{
    R(11, 2, 2, 11, 0x000080);
    if (baixo) { R(9, 10, 6, 1, 0x000080); R(10, 11, 4, 1, 0x000080); R(11, 12, 2, 1, 0x000080); }
    else { R(9, 5, 6, 1, 0x000080); R(10, 4, 4, 1, 0x000080); R(11, 3, 2, 1, 0x000080); }
}
static void ic_ord_a(jan_t *j, int x, int y, bool a)
{
    (void)a;
    mini(j, x + 2, y + 1, G_A, W98_NEGRO); mini(j, x + 2, y + 9, G_Z, W98_NEGRO);
    flecha_ord(j, x, y, true);
}
static void ic_ord_z(jan_t *j, int x, int y, bool a)
{
    (void)a;
    mini(j, x + 2, y + 1, G_Z, W98_NEGRO); mini(j, x + 2, y + 9, G_A, W98_NEGRO);
    flecha_ord(j, x, y, true);
}
static void ic_grafico(jan_t *j, int x, int y, bool a)
{
    (void)a;
    R(1, 1, 1, 13, W98_NEGRO); R(1, 14, 14, 1, W98_NEGRO);
    R(3, 8, 3, 6, 0x0000FF); R(7, 3, 3, 11, 0xC00000); R(11, 6, 3, 8, 0x008000);
    R(3, 8, 3, 1, W98_LUZ); R(7, 3, 3, 1, W98_LUZ); R(11, 6, 3, 1, W98_LUZ);
}
static void ic_dec_mais(jan_t *j, int x, int y, bool a)
{
    (void)a;
    mini(j, x + 1, y + 3, G_0, W98_NEGRO); mini(j, x + 5, y + 3, G_0, W98_NEGRO);
    R(10, 5, 5, 1, 0x000080); R(12, 3, 1, 5, 0x000080);
    R(1, 10, 3, 1, 0x000080); R(4, 9, 1, 3, 0x000080); R(5, 10, 8, 1, 0x000080);
}
static void ic_dec_menos(jan_t *j, int x, int y, bool a)
{
    (void)a;
    mini(j, x + 1, y + 3, G_0, W98_NEGRO);
    R(10, 5, 5, 1, 0x000080);
    R(1, 10, 3, 1, 0x000080); R(4, 9, 1, 3, 0x000080); R(5, 10, 8, 1, 0x000080);
    mini(j, x + 9, y + 8, G_0, W98_NEGRO);
}
static void ic_bordas(jan_t *j, int x, int y, bool a)
{
    (void)a;
    R(2, 2, 12, 1, W98_NEGRO); R(2, 13, 12, 1, W98_NEGRO); R(2, 2, 1, 12, W98_NEGRO); R(13, 2, 1, 12, W98_NEGRO);
    R(8, 3, 1, 10, W98_SOMBRA); R(3, 8, 10, 1, W98_SOMBRA);
}
static void ic_relleno(jan_t *j, int x, int y, bool a)
{
    (void)a;
    g_linha(j, x + 2, y + 6, x + 7, y + 1, W98_NEGRO); g_linha(j, x + 2, y + 6, x + 7, y + 11, W98_NEGRO);
    g_linha(j, x + 7, y + 1, x + 12, y + 6, W98_NEGRO); g_linha(j, x + 7, y + 11, x + 12, y + 6, W98_NEGRO);
    for (int k = 0; k < 3; k++) g_linha(j, x + 4 + k, y + 7 - k, x + 10 - k, y + 7 - k, 0xC0C0C0);
    R(13, 8, 2, 3, 0x0000FF);
    R(1, 13, 14, 3, cor_rel_atual); g_moldura(j, x + 1, y + 13, 14, 3, W98_SOMBRA);
}
static void ic_cor_txt(jan_t *j, int x, int y, bool a)
{
    (void)a;
    R(4, 1, 2, 3, W98_NEGRO); R(10, 1, 2, 3, W98_NEGRO);
    g_linha(j, x + 7, y + 1, x + 3, y + 11, W98_NEGRO); g_linha(j, x + 8, y + 1, x + 12, y + 11, W98_NEGRO);
    R(5, 7, 6, 2, W98_NEGRO);
    R(1, 13, 14, 3, cor_txt_atual); g_moldura(j, x + 1, y + 13, 14, 3, W98_SOMBRA);
}
static void ic_igual(jan_t *j, int x, int y, bool a)
{
    (void)a;
    R(3, 5, 9, 2, W98_NEGRO); R(3, 9, 9, 2, W98_NEGRO);
}

static void itens_iniciar(void)
{
#define IT(id_, ic_, tx_, neg_, con_, lg_) { id_, ic_, tx_, neg_, con_, false, false, lg_ }
    static const barra_item_t u1[16] = {
        IT(A_NOVO, ic98_novo, NULL, false, false, 0), IT(A_ABRIR, ic98_abrir, NULL, false, false, 0),
        IT(A_GUARDAR, ic98_guardar, NULL, false, false, 0), IT(A_PDF, ic98_pdf, NULL, false, false, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_CORTAR, ic98_cortar, NULL, false, false, 0), IT(A_COPIAR, ic98_copiar, NULL, false, false, 0),
        IT(A_COLAR, ic98_colar, NULL, false, false, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_DESFAZER, ic98_desfazer, NULL, false, false, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_SOMA, ic_soma, NULL, false, false, 0), IT(A_ORD_A, ic_ord_a, NULL, false, false, 0),
        IT(A_ORD_Z, ic_ord_z, NULL, false, false, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_GRAF_B, ic_grafico, NULL, false, false, 0),
    };
    static const barra_item_t u2[20] = {
        IT(-1, NULL, NULL, false, false, 146), IT(-1, NULL, NULL, false, false, 58),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_NEGRITA, NULL, "N", true, true, 0), IT(A_CURSIVA, NULL, "K", false, true, 0),
        IT(A_SUBLIN, NULL, "S", false, true, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_ESQ, ic98_esq, NULL, false, true, 0), IT(A_CENTRO, ic98_centro, NULL, false, true, 0),
        IT(A_DIR, ic98_dir, NULL, false, true, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_MOEDA, NULL, "$", false, false, 0), IT(A_PERCENT, NULL, "%", false, false, 0),
        IT(A_MILHAR, NULL, "000", false, false, 32), IT(A_DEC_MAIS, ic_dec_mais, NULL, false, false, 0),
        IT(A_DEC_MENOS, ic_dec_menos, NULL, false, false, 0),
        IT(0, NULL, NULL, false, false, 0),
        IT(A_BORDAS, ic_bordas, NULL, false, false, 0), IT(A_RELLENO, ic_relleno, NULL, false, false, 0),
        IT(A_COR_TXT, ic_cor_txt, NULL, false, false, 0),
    };
#undef IT
    memcpy(it1, u1, sizeof u1);
    memcpy(it2, u2, sizeof u2);
}

static void menus_atualizar(void);

static void layout(void)
{
    int l = jan->larg;
    menus_atualizar();               /* rellena nombres y n antes de colocar los menús */
    barra_menu_iniciar(&menu, 0, 0, l);
    barra_ferr_iniciar(&ferr1, 0, Y_FERR1, l, it1, 16);
    barra_ferr_iniciar(&ferr2, 0, Y_FERR2, l, it2, 20);
    static const char *const tams_txt[] = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "22", "24", "28", "36", "48", "72" };
    combo_iniciar(&c_fonte, barra_ferr_x(&ferr2, 0), Y_FERR2 + 3, 140, NOMES_FONTE, 3, false, 7301);
    combo_iniciar(&c_tam, barra_ferr_x(&ferr2, 1), Y_FERR2 + 3, 52, tams_txt, 15, true, 7302);
    static const int larg[4] = { 260, 150, 150, 100 };
    barra_estado_iniciar(&barra_est, 0, Y_ESTADO, l, 4, larg);
}

/* ------------------------------------------------------------ menús (barra y contextuales) */

typedef struct { const char *t; int a; } mi_t;

static const mi_t M_ARQUIVO[] = {
    { "Nuevo\tCtrl+N", A_NOVO }, { "Abrir...\tCtrl+O", A_ABRIR }, { "-", 0 },
    { "Guardar\tCtrl+S", A_GUARDAR }, { "Guardar como...", A_GUARDAR_COMO }, { "-", 0 },
    { "Importar CSV...", A_CSV_IMP }, { "Exportar CSV...", A_CSV_EXP },
    { "Exportar a PDF...\tCtrl+P", A_PDF }, { "-", 0 }, { "Salir", A_SAIR },
};
static const mi_t M_EDICAO[] = {
    { "Deshacer\tCtrl+Z", A_DESFAZER }, { "-", 0 },
    { "Cortar\tCtrl+X", A_CORTAR }, { "Copiar\tCtrl+C", A_COPIAR }, { "Pegar\tCtrl+V", A_COLAR }, { "-", 0 },
    { "Borrar contenido\tSupr", A_BORRAR }, { "Borrar formato", A_BORRAR_FMT }, { "-", 0 },
    { "Seleccionar todo\tCtrl+A", A_TUDO }, { "Ir a...\tCtrl+G", A_IR },
};
static const mi_t M_VER[] = {
    { "Líneas de la cuadrícula", A_GRADE }, { "Mostrar fórmulas en las celdas", A_FORMULAS },
};
static const mi_t M_INSERIR[] = {
    { "Fila", A_LINHA_INS }, { "Columna", A_COL_INS }, { "Hoja", A_FOLHA_NOVA }, { "-", 0 },
    { "Gráfico de barras", A_GRAF_B }, { "Gráfico de barras horizontales", A_GRAF_H }, { "Gráfico de tarta", A_GRAF_T },
};
static const mi_t M_FORMATO[] = {
    { "Celdas...\tCtrl+1", A_CELULAS }, { "-", 0 },
    { "Negrita\tCtrl+B", A_NEGRITA }, { "Cursiva\tCtrl+I", A_CURSIVA }, { "Subrayado\tCtrl+U", A_SUBLIN }, { "-", 0 },
    { "Estilo de moneda", A_MOEDA }, { "Estilo porcentual", A_PERCENT }, { "Separador de miles", A_MILHAR },
    { "Más decimales", A_DEC_MAIS }, { "Menos decimales", A_DEC_MENOS }, { "-", 0 },
    { "Ancho de las columnas...", A_LARG }, { "Cambiar nombre de la hoja...", A_FOLHA_REN },
};
static const mi_t M_FERRAMENTAS[] = {
    { "Recalcular", A_RECALC }, { "Autosuma", A_SOMA },
};
static const mi_t M_DADOS[] = {
    { "Ordenar de A a Z", A_ORD_A }, { "Ordenar de Z a A", A_ORD_Z }, { "-", 0 },
    { "Importar CSV...", A_CSV_IMP }, { "Exportar CSV...", A_CSV_EXP },
};
static const mi_t M_AJUDA[] = { { "Acerca de Macaco", A_ACERCA } };

static const mi_t M_CONTEXTO[] = {
    { "Cortar\tCtrl+X", A_CORTAR }, { "Copiar\tCtrl+C", A_COPIAR }, { "Pegar\tCtrl+V", A_COLAR }, { "-", 0 },
    { "Insertar fila", A_LINHA_INS }, { "Eliminar fila", A_LINHA_DEL },
    { "Insertar columna", A_COL_INS }, { "Eliminar columna", A_COL_DEL }, { "-", 0 },
    { "Borrar contenido\tSupr", A_BORRAR }, { "Formato de celdas...\tCtrl+1", A_CELULAS }, { "-", 0 },
    { "Ordenar de A a Z", A_ORD_A }, { "Ordenar de Z a A", A_ORD_Z }, { "-", 0 },
    { "Gráfico de barras", A_GRAF_B }, { "Gráfico de barras horizontales", A_GRAF_H }, { "Gráfico de tarta", A_GRAF_T },
    { "-", 0 }, { "Ancho de las columnas...", A_LARG },
};
static const mi_t M_PESTANA[] = {
    { "Insertar hoja", A_FOLHA_NOVA }, { "Eliminar hoja", A_FOLHA_DEL }, { "Cambiar nombre...", A_FOLHA_REN },
};

#define NM(v) ((int)(sizeof(v) / sizeof((v)[0])))
static const struct { const char *nome; const mi_t *l; int n; } MENUS[8] = {
    { "Archivo", M_ARQUIVO, NM(M_ARQUIVO) }, { "Edición", M_EDICAO, NM(M_EDICAO) }, { "Ver", M_VER, NM(M_VER) },
    { "Insertar", M_INSERIR, NM(M_INSERIR) }, { "Formato", M_FORMATO, NM(M_FORMATO) },
    { "Herramientas", M_FERRAMENTAS, NM(M_FERRAMENTAS) }, { "Datos", M_DADOS, NM(M_DADOS) },
    { "Ayuda", M_AJUDA, NM(M_AJUDA) },
};
static char menu_buf[8][900];

static bool tem_selecao(void) { return anc_l != cel_l || anc_c != cel_c; }

/* ¿Está activada la acción en este momento? */
static bool acao_ativa(int a)
{
    switch (a) {
    case A_DESFAZER: return n_desfazer > 0;
    case A_COLAR: return !copia.vazio;
    default: return true;
    }
}

static void menu_construir(char *dst, int max, const mi_t *l, int n)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        const char *t = l[i].t;
        bool off = l[i].a && !acao_ativa(l[i].a);
        bool marca = (l[i].a == A_GRADE && grade_visivel) || (l[i].a == A_FORMULAS && mostrar_formulas);
        int tl = (int)tam_texto(t);
        if (k + tl + 6 >= max) break;
        if (i) dst[k++] = '\n';
        if (off) dst[k++] = '~';
        if (marca) { dst[k++] = '*'; dst[k++] = ' '; }
        memcpy(dst + k, t, (size_t)tl);
        k += tl;
    }
    dst[k] = 0;
}

static void menus_atualizar(void)
{
    for (int i = 0; i < 8; i++) {
        menu_construir(menu_buf[i], sizeof menu_buf[i], MENUS[i].l, MENUS[i].n);
        menu.nomes[i] = MENUS[i].nome;
        menu.itens[i] = menu_buf[i];
    }
    menu.n = 8;
}

static void menu_lista(int x, int y, const mi_t *l, int n, int codigo)
{
    static char buf[1200];
    menu_construir(buf, sizeof buf, l, n);
    menu_contextual(jan->id, x, y, buf, codigo);
}

static int acao_de_linha(const mi_t *l, int n, int linha)
{
    return (linha >= 0 && linha < n) ? l[linha].a : 0;
}

/* ------------------------------------------------------------ dibujo de la hoja */

static uint32_t mezclar(uint32_t a, uint32_t b)
{
    return (((a & 0xFF0000) + (b & 0xFF0000)) / 2 & 0xFF0000) | (((a & 0xFF00) + (b & 0xFF00)) / 2 & 0xFF00) |
           (((a & 0xFF) + (b & 0xFF)) / 2 & 0xFF);
}

/* Dibuja un texto de celda con su formato: la fuente de pantalla es de mapa de bits, así que
 * el tamaño se consigue ampliando por vecino más próximo y la cursiva inclinando las filas. */
#define TXT_CLAVE 0x010203
static uint32_t txt_buf[256 * 8 * 17];

static void texto_celda(int x, int y, int w, int h, const char *t, const fmt_t *f, bool numero, uint32_t cor)
{
    int tam = f->tam ? f->tam : 10;
    int n = (int)tam_texto(t);
    if (n > 250) n = 250;
    if (n == 0) return;
    int al = f->al ? f->al : (numero ? AL_DIR : AL_ESQ);
    bool negr = (f->est & EST_NEGRITA) != 0, curs = (f->est & EST_CURSIVA) != 0, sub = (f->est & EST_SUBLIN) != 0;
    int dw = n * LETRA_L * tam / 10, dh = LETRA_A * tam / 10;
    if (dw > w - 4) {                       /* recorta por la derecha */
        n = (w - 4) * 10 / (LETRA_L * tam);
        if (n <= 0) return;
        dw = n * LETRA_L * tam / 10;
    }
    int tx = (al == AL_ESQ) ? x + 3 : (al == AL_DIR ? x + w - 3 - dw : x + (w - dw) / 2);
    int ty = y + (h - dh) / 2;
    if (tam == 10 && !curs) {
        g_texto_n(jan, tx, ty, t, n, cor);
        if (negr) g_texto_n(jan, tx + 1, ty, t, n, cor);
        if (sub) g_retangulo(jan, tx, ty + dh - 1, dw + (negr ? 1 : 0), 1, cor);
        return;
    }
    int sw = n * LETRA_L + 1;
    jan_t sj;
    sj.id = -1; sj.px = txt_buf; sj.larg = sw; sj.alt = LETRA_A; sj.estilo = 0; sj.min_l = sj.min_a = 0;
    for (int i = 0; i < sw * LETRA_A; i++) txt_buf[i] = TXT_CLAVE;
    uint32_t c2 = (cor == TXT_CLAVE) ? 0 : cor;
    g_texto_n(&sj, 0, 0, t, n, c2);
    if (negr) g_texto_n(&sj, 1, 0, t, n, c2);
    if (sub) g_retangulo(&sj, 0, LETRA_A - 1, sw, 1, c2);
    int dw2 = sw * tam / 10;
    for (int dy = 0; dy < dh; dy++) {
        int py = ty + dy;
        if (py < y || py >= y + h || py < 0 || py >= jan->alt) continue;
        int sy = dy * LETRA_A / dh;
        int desl = curs ? (dh - 1 - dy) / 4 : 0;
        for (int dx = 0; dx < dw2; dx++) {
            int px = tx + dx + desl;
            if (px < x || px >= x + w || px < 0 || px >= jan->larg) continue;
            uint32_t v = txt_buf[sy * sw + dx * 10 / tam];
            if (v != TXT_CLAVE) jan->px[py * jan->larg + px] = v;
        }
    }
}

static bool na_selecao(int l, int c)
{
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    return l >= l1 && l <= l2 && c >= c1 && c <= c2;
}

static void desenhar_grelha(void)
{
    folha_t *fo = folha_ativa();
    int gy = grelha_y(), ga = grelha_a();
    int gl = jan->larg - MAC_LARGC;
    calcular_offsets();
    int ncol = fo->colunas;

    g_retangulo(jan, MAC_LARGC, gy, gl, ga, COR_BRANCO);
    /* Rango de filas visible. */
    int l_ini = desl_y / MAC_ALTLINHA;
    int l_fim = (desl_y + ga) / MAC_ALTLINHA + 1;
    if (l_fim > fo->linhas) l_fim = fo->linhas;

    /* Fondos: relleno propio, o el tinte de la selección (la celda activa queda blanca). */
    for (int l = l_ini; l < l_fim; l++) {
        int y = y_linha(l);
        for (int c = 0; c < ncol; c++) {
            int x = x_coluna(c), lc = fo->larg_col[c];
            if (x + lc <= MAC_LARGC) continue;
            if (x >= jan->larg) break;
            celula_t *ce = &fo->celulas[(size_t)l * ncol + c];
            const fmt_t *f = fmt_de(ce);
            uint32_t fundo = (f->rel & REL_ATIVO) ? (f->rel & 0xFFFFFF) : COR_BRANCO;
            bool ativa = (l == cel_l && c == cel_c);
            if (tem_selecao() && !ativa && na_selecao(l, c)) fundo = mezclar(fundo, 0x6080D0);
            if (fundo != COR_BRANCO) g_retangulo(jan, x, y, lc, MAC_ALTLINHA, fundo);
        }
    }

    /* Las líneas de la cuadrícula. */
    if (grade_visivel) {
        for (int c = 0; c <= ncol; c++) {
            int x = x_coluna(c);
            if (x < MAC_LARGC || x > jan->larg) continue;
            g_linha(jan, x, gy, x, gy + ga - 1, 0xC0C0C0);
        }
        for (int l = l_ini; l <= l_fim; l++) {
            int y = y_linha(l);
            if (y < gy || y >= gy + ga) continue;
            g_linha(jan, MAC_LARGC, y, jan->larg, y, 0xC0C0C0);
        }
    }

    /* Los valores. */
    for (int l = l_ini; l < l_fim; l++) {
        int y = y_linha(l);
        for (int c = 0; c < ncol; c++) {
            int x = x_coluna(c);
            int lcol = fo->larg_col[c];
            if (x + lcol <= MAC_LARGC) continue;
            if (x >= jan->larg) break;
            celula_t *ce = &fo->celulas[(size_t)l * ncol + c];
            const fmt_t *f = fmt_de(ce);
            char t[MAC_TEXTO_CEL];
            if (editando && l == cel_l && c == cel_c) {
                copia_texto(t, edit_buf, sizeof t);
                texto_celda(x, y, lcol, MAC_ALTLINHA, t, f, false, f->cor);
                continue;
            }
            if (mostrar_formulas && (ce->flags & CEL_FORMULA)) copia_texto(t, ce->texto, sizeof t);
            else celula_exibida(t, sizeof t, ce);
            bool numero = (ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO) && !(mostrar_formulas && (ce->flags & CEL_FORMULA));
            if (t[0]) texto_celda(x, y, lcol, MAC_ALTLINHA, t, f, numero, f->cor);
            /* bordes de la celda */
            if (f->bor) {
                if (f->bor & BOR_CIMA) g_linha(jan, x, y, x + lcol - 1, y, W98_NEGRO);
                if (f->bor & BOR_BAIXO) g_linha(jan, x, y + MAC_ALTLINHA - 1, x + lcol - 1, y + MAC_ALTLINHA - 1, W98_NEGRO);
                if (f->bor & BOR_ESQ) g_linha(jan, x, y, x, y + MAC_ALTLINHA - 1, W98_NEGRO);
                if (f->bor & BOR_DIR) g_linha(jan, x + lcol - 1, y, x + lcol - 1, y + MAC_ALTLINHA - 1, W98_NEGRO);
            }
        }
    }

    /* El marco de la selección (negro y grueso, como en Excel) y el cursor de edición. */
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    int sx = x_coluna(c1), sw = off_col[c2 + 1] - off_col[c1];
    int sy = y_linha(l1), sh = (l2 - l1 + 1) * MAC_ALTLINHA;
    if (sw > 0 && sh > 0) {
        int y0 = sy, y1 = sy + sh;
        if (y0 < gy) y0 = gy;
        if (y1 > gy + ga) y1 = gy + ga;
        if (y1 > y0 && sx < jan->larg && sx + sw > MAC_LARGC) {
            g_moldura(jan, sx - 1, y0 - 1, sw + 2, y1 - y0 + 2, W98_NEGRO);
            g_moldura(jan, sx, y0, sw, y1 - y0, W98_NEGRO);
        }
    }
    if (editando) {
        int ax = x_coluna(cel_c), ay = y_linha(cel_l);
        const fmt_t *f = fmt_de(celula_de(fo, cel_l, cel_c));
        int tam = f->tam ? f->tam : 10;
        int cx = ax + 3 + edit_cur * LETRA_L * tam / 10;
        g_retangulo(jan, cx, ay + 2, 1, MAC_ALTLINHA - 4, W98_NEGRO);
    }

    /* La cabecera de las columnas: botones biselados, con la seleccionada hundida. */
    int hy = gy - MAC_CABECALH;
    g_retangulo(jan, 0, hy, jan->larg, MAC_CABECALH, W98_FACE);
    for (int c = 0; c < ncol; c++) {
        int x = x_coluna(c), lcol = fo->larg_col[c];
        if (x + lcol <= MAC_LARGC) continue;
        if (x >= jan->larg) break;
        bool sel = c >= c1 && c <= c2;
        g_retangulo(jan, x, hy, lcol, MAC_CABECALH, W98_FACE);
        g_relevo(jan, x, hy, lcol, MAC_CABECALH, sel);
        char nome[8];
        nome_coluna(nome, sizeof nome, c);
        int tx = x + (lcol - g_largura_texto(nome)) / 2 + (sel ? 1 : 0);
        g_texto(jan, tx, hy + 1 + (sel ? 1 : 0), nome, W98_NEGRO);
        if (sel) g_texto(jan, tx + 1, hy + 1 + 1, nome, W98_NEGRO);
    }
    g_retangulo(jan, 0, hy, MAC_LARGC, MAC_CABECALH, W98_FACE);
    g_bisel(jan, 0, hy, MAC_LARGC, MAC_CABECALH, false);

    /* La cabecera de las filas. */
    g_retangulo(jan, 0, gy, MAC_LARGC, ga, W98_FACE);
    for (int l = l_ini; l < l_fim; l++) {
        int y = y_linha(l);
        if (y + MAC_ALTLINHA <= gy || y >= gy + ga) continue;
        bool sel = l >= l1 && l <= l2;
        g_retangulo(jan, 0, y, MAC_LARGC, MAC_ALTLINHA, W98_FACE);
        g_relevo(jan, 0, y, MAC_LARGC, MAC_ALTLINHA, sel);
        char n[8];
        formatar_texto(n, sizeof n, "%d", l + 1);
        int tx = (MAC_LARGC - g_largura_texto(n)) / 2 + (sel ? 1 : 0);
        g_texto(jan, tx, y + (MAC_ALTLINHA - LETRA_A) / 2 + (sel ? 1 : 0), n, W98_NEGRO);
    }
    /* La esquina izquierda de la cabecera cubre lo que desbordó por la izquierda. */
    g_retangulo(jan, 0, hy, MAC_LARGC, MAC_CABECALH, W98_FACE);
    g_relevo(jan, 0, hy, MAC_LARGC, MAC_CABECALH, false);
}

static void desenhar_formula(void)
{
    int y = Y_FORM;
    g_retangulo(jan, 0, y, jan->larg, MAC_FORMULA, W98_FACE);
    /* La caja del nombre (la referencia de la celda activa). */
    char ref[24];
    if (nome_foco) copia_texto(ref, nome_buf, sizeof ref);
    else nome_ref(ref, sizeof ref, cel_l, cel_c);
    g_retangulo(jan, 4, y + 2, 76, MAC_FORMULA - 4, COR_BRANCO);
    g_bisel(jan, 4, y + 2, 76, MAC_FORMULA - 4, true);
    g_texto(jan, 8, y + 4, ref, W98_NEGRO);
    if (nome_foco) g_retangulo(jan, 8 + (int)tam_texto(ref) * LETRA_L, y + 4, 1, LETRA_A, W98_NEGRO);
    /* El botón "=" (editar fórmula). */
    g_retangulo(jan, 84, y + 2, 22, MAC_FORMULA - 4, W98_FACE);
    g_bisel(jan, 84, y + 2, 22, MAC_FORMULA - 4, false);
    ic_igual(jan, 87, y + 4, true);
    /* La caja del contenido. */
    int cx = 110, cw = jan->larg - cx - 4;
    if (cw < 20) cw = 20;
    g_retangulo(jan, cx, y + 2, cw, MAC_FORMULA - 4, COR_BRANCO);
    g_bisel(jan, cx, y + 2, cw, MAC_FORMULA - 4, true);
    char texto[MAC_TEXTO_CEL];
    if (editando) copia_texto(texto, edit_buf, sizeof texto);
    else celula_conteudo(texto, sizeof texto, celula_de(folha_ativa(), cel_l, cel_c));
    int maxc = (cw - 8) / LETRA_L;
    if (maxc > MAC_TEXTO_CEL - 1) maxc = MAC_TEXTO_CEL - 1;
    g_texto_n(jan, cx + 4, y + 4, texto, maxc, W98_NEGRO);
    if (editando) g_retangulo(jan, cx + 4 + edit_cur * LETRA_L, y + 4, 1, LETRA_A, W98_NEGRO);
}

static int larg_pestana(int f) { return g_largura_texto(livro.folha[f].nome) + 22; }

static void desenhar_pestanas(void)
{
    int y = Y_PEST;
    g_retangulo(jan, 0, y, jan->larg, MAC_RODAPE - ESTADO_A, W98_FACE);
    g_retangulo(jan, 0, y, jan->larg, 1, W98_SOMBRA);
    int x = 6;
    for (int f = 0; f < livro.n; f++) {
        int l = larg_pestana(f);
        if (x + l > jan->larg - 30) break;
        bool ativa = f == livro.ativa;
        int y0 = y + (ativa ? 0 : 1);
        g_retangulo(jan, x, y0, l, MAC_RODAPE - ESTADO_A - (ativa ? 1 : 2), ativa ? COR_BRANCO : W98_FACE);
        g_linha(jan, x, y0, x, y0 + 17, W98_NEGRO);                        /* lado izquierdo */
        g_linha(jan, x + l - 1, y0, x + l - 1, y0 + 17, W98_NEGRO);         /* lado derecho */
        g_linha(jan, x, y0 + 17, x + l - 1, y0 + 17, W98_NEGRO);            /* cierra por abajo */
        if (ativa) g_linha(jan, x + 1, y, x + l - 2, y, COR_BRANCO);        /* se funde con la hoja */
        g_texto(jan, x + 11, y0 + 1, livro.folha[f].nome, W98_NEGRO);
        if (ativa) g_texto(jan, x + 12, y0 + 1, livro.folha[f].nome, W98_NEGRO);
        x += l;
    }
    if (x + 24 < jan->larg) {                                  /* botón de hoja nueva */
        g_retangulo(jan, x + 4, y + 2, 20, 17, W98_FACE);
        g_bisel(jan, x + 4, y + 2, 20, 17, false);
        g_texto(jan, x + 10, y + 2, "+", W98_NEGRO);
    }
}

static void estatisticas(int32_t *soma, int *cont, int *numeros)
{
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    folha_t *fo = folha_ativa();
    int64_t s = 0;
    int n = 0, t = 0;
    if ((int64_t)(l2 - l1 + 1) * (c2 - c1 + 1) <= 40000) {
        for (int l = l1; l <= l2 && l < fo->linhas; l++)
            for (int c = c1; c <= c2 && c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!ce->texto) continue;
                t++;
                if ((ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO)) { s += ce->valor; n++; }
            }
    }
    if (s > 2000000000LL) s = 2000000000LL;
    if (s < -2000000000LL) s = -2000000000LL;
    *soma = (int32_t)s;
    *cont = t;
    *numeros = n;
}

static const char *nome_curto(void)
{
    if (!caminho[0]) return "Libro1";
    const char *b = acha_ultimo_char(caminho, '/');
    return b ? b + 1 : caminho;
}

static void titulo_atualizar(void)
{
    char t[200];
    formatar_texto(t, sizeof t, "Macaco - %s%s", nome_curto(), modificado ? " *" : "");
    if (compara_texto(t, ult_titulo) == 0) return;
    copia_texto(ult_titulo, t, sizeof ult_titulo);
    titulo_janela(jan->id, t);
}

static void estado_atualizar(void)
{
    barra_estado_texto(&barra_est, 0, editando ? "Modo edición" : (estado[0] ? estado : "Listo"));
    char t[64];
    if (tem_selecao()) {
        int32_t soma; int cont, nums;
        estatisticas(&soma, &cont, &nums);
        char v[32];
        formatar_numero(v, sizeof v, soma);
        formatar_texto(t, sizeof t, "SUMA=%s", v);
        barra_estado_texto(&barra_est, 1, nums ? t : "");
        if (nums) {
            formatar_numero(v, sizeof v, (int32_t)div64_64((int64_t)soma, nums));
            formatar_texto(t, sizeof t, "PROMEDIO=%s", v);
        } else t[0] = 0;
        barra_estado_texto(&barra_est, 2, t);
        formatar_texto(t, sizeof t, "CUENTA=%d", cont);
        barra_estado_texto(&barra_est, 3, cont ? t : "");
    } else {
        barra_estado_texto(&barra_est, 1, "");
        barra_estado_texto(&barra_est, 2, "");
        barra_estado_texto(&barra_est, 3, "");
    }
    barra_estado_desenhar(jan, &barra_est);
}

/* Refleja el formato de la celda activa en las barras. */
static void ui_atualizar(void)
{
    const fmt_t *f = fmt_de(celula_de(folha_ativa(), cel_l, cel_c));
    barra_ferr_marcar(&ferr2, A_NEGRITA, (f->est & EST_NEGRITA) != 0);
    barra_ferr_marcar(&ferr2, A_CURSIVA, (f->est & EST_CURSIVA) != 0);
    barra_ferr_marcar(&ferr2, A_SUBLIN, (f->est & EST_SUBLIN) != 0);
    barra_ferr_marcar(&ferr2, A_ESQ, f->al == AL_ESQ);
    barra_ferr_marcar(&ferr2, A_CENTRO, f->al == AL_CENTRO);
    barra_ferr_marcar(&ferr2, A_DIR, f->al == AL_DIR);
    barra_ferr_desenhar(jan, &ferr2);
    if (!c_fonte.foco) combo_definir(&c_fonte, NOMES_FONTE[f->fam < 3 ? f->fam : 0]);
    combo_desenhar(jan, &c_fonte);
    if (!c_tam.foco) { char t[8]; formatar_texto(t, sizeof t, "%d", f->tam ? f->tam : 10); combo_definir(&c_tam, t); }
    combo_desenhar(jan, &c_tam);
}

static void desenhar_tudo(void)
{
    g_retangulo(jan, 0, 0, jan->larg, jan->alt, W98_FACE);
    menus_atualizar();
    barra_menu_desenhar(jan, &menu);
    barra_ferr_desenhar(jan, &ferr1);
    desenhar_formula();
    desenhar_grelha();
    desenhar_pestanas();
    ui_atualizar();
    estado_atualizar();
    titulo_atualizar();
    jan_mostrar(jan);
}

/* ------------------------------------------------------------ la edición */

static void comecar_edicao(const char *inicial)
{
    folha_t *fo = folha_ativa();
    celula_t *ce = celula_de(fo, cel_l, cel_c);
    editando = true;
    if (inicial) {
        copia_texto(edit_buf, inicial, sizeof edit_buf);
        edit_cur = (int)tam_texto(inicial);
    } else {
        celula_conteudo(edit_buf, sizeof edit_buf, ce);
        edit_cur = (int)tam_texto(edit_buf);
    }
}

static void confirmar_edicao(void)
{
    if (!editando) return;
    editando = false;
    folha_t *fo = folha_ativa();
    celula_t *ce = celula_de(fo, cel_l, cel_c);
    char antigo[MAC_TEXTO_CEL];
    celula_conteudo(antigo, sizeof antigo, ce);
    if (compara_texto(antigo, edit_buf) == 0) return;   /* no ha cambiado nada */
    desfazer_guardar_celula(livro.ativa, cel_l, cel_c, antigo);
    celula_definir(fo, cel_l, cel_c, edit_buf);
    modificado = true;
    recalcular();
    char ref[16], val[32];
    nome_ref(ref, sizeof ref, cel_l, cel_c);
    celula_t *novo = celula_de(fo, cel_l, cel_c);
    formatar_numero(val, sizeof val, novo ? novo->valor : 0);
    if (novo && (novo->flags & CEL_FORMULA))
        registrar("macaco: celula %s = %s (%s)", ref, val, novo->texto);
    else if (novo && (novo->flags & CEL_TEXTO))
        registrar("macaco: celula %s = %s", ref, novo->texto);
    else
        registrar("macaco: celula %s = %s", ref, val);
}

static void cancelar_edicao(void)
{
    editando = false;
}

/* F4: da la vuelta a las referencias absolutas de la que está bajo el cursor
 * (A1 -> $A$1 -> A$1 -> $A1 -> A1). */
static void alternar_absoluta(void)
{
    int n = (int)tam_texto(edit_buf);
    int a = edit_cur, b = edit_cur;
    while (a > 0 && ((edit_buf[a - 1] >= 'A' && edit_buf[a - 1] <= 'Z') || (edit_buf[a - 1] >= 'a' && edit_buf[a - 1] <= 'z') ||
                     (edit_buf[a - 1] >= '0' && edit_buf[a - 1] <= '9') || edit_buf[a - 1] == '$')) a--;
    while (b < n && ((edit_buf[b] >= 'A' && edit_buf[b] <= 'Z') || (edit_buf[b] >= 'a' && edit_buf[b] <= 'z') ||
                     (edit_buf[b] >= '0' && edit_buf[b] <= '9') || edit_buf[b] == '$')) b++;
    char tok[24];
    int tl = b - a;
    if (tl <= 0 || tl > 12) return;
    memcpy(tok, edit_buf + a, (size_t)tl);
    tok[tl] = 0;
    int i = 0;
    bool ac = false, al = false;
    if (tok[i] == '$') { ac = true; i++; }
    int ls = i;
    while ((tok[i] >= 'A' && tok[i] <= 'Z') || (tok[i] >= 'a' && tok[i] <= 'z')) i++;
    int le = i;
    if (le == ls) return;
    if (tok[i] == '$') { al = true; i++; }
    int ds = i;
    while (tok[i] >= '0' && tok[i] <= '9') i++;
    if (i == ds || tok[i]) return;
    char letras[8], digs[8];
    memcpy(letras, tok + ls, (size_t)(le - ls)); letras[le - ls] = 0;
    memcpy(digs, tok + ds, (size_t)(i - ds)); digs[i - ds] = 0;
    /* ciclo */
    if (!ac && !al) { ac = true; al = true; }
    else if (ac && al) { ac = false; al = true; }
    else if (!ac && al) { ac = true; al = false; }
    else { ac = false; al = false; }
    char novo[32];
    formatar_texto(novo, sizeof novo, "%s%s%s%s", ac ? "$" : "", letras, al ? "$" : "", digs);
    int nl = (int)tam_texto(novo);
    if (n - tl + nl >= MAC_TEXTO_CEL) return;
    memmove(edit_buf + a + nl, edit_buf + b, (size_t)(n - b + 1));
    memcpy(edit_buf + a, novo, (size_t)nl);
    edit_cur = a + nl;
}

/* ------------------------------------------------------------ desplazamiento */

static void assure_visivel(void)
{
    folha_t *fo = folha_ativa();
    calcular_offsets();
    int ga = grelha_a();
    /* La columna. */
    if (cel_c < fo->colunas) {
        int x = off_col[cel_c], fim = off_col[cel_c + 1];
        if (x < desl_x) desl_x = x;
        else if (fim > desl_x + (jan->larg - MAC_LARGC)) desl_x = fim - (jan->larg - MAC_LARGC);
    }
    if (desl_x < 0) desl_x = 0;
    /* La fila. */
    int y = cel_l * MAC_ALTLINHA, fim = y + MAC_ALTLINHA;
    if (y < desl_y) desl_y = y;
    else if (fim > desl_y + ga) desl_y = fim - ga;
    if (desl_y < 0) desl_y = 0;
}

static void mover_para(int l, int c, bool extender)
{
    if (l < 0) l = 0;
    if (c < 0) c = 0;
    if (l >= MAC_MAX_LINHAS) l = MAC_MAX_LINHAS - 1;
    if (c >= MAC_MAX_COLUNAS) c = MAC_MAX_COLUNAS - 1;
    if (!extender) {
        anc_l = l;
        anc_c = c;
    }
    /* Con Mayús el ancla se queda donde estaba y solo se mueve el extremo activo. */
    cel_l = l;
    cel_c = c;
    assure_visivel();
}

/* La esquina de la zona con datos (para el Ctrl+setas). */
static void limite_dados(int *l, int *c, bool *achou)
{
    folha_t *fo = folha_ativa();
    int ml = 0, mc = 0;
    bool ache = false;
    for (int i = 0; i < fo->linhas; i++)
        for (int k = 0; k < fo->colunas; k++) {
            celula_t *ce = &fo->celulas[(size_t)i * fo->colunas + k];
            if (!ce->texto) continue;
            ache = true;
            if (i > ml) ml = i;
            if (k > mc) mc = k;
        }
    *l = ml;
    *c = mc;
    *achou = ache;
}

/* ------------------------------------------------------------ acciones */

static bool guardar_como(void);

/* Diálogos de archivo (D272): abrir ve todo; guardar pone la extensión .mxs. */
#define FILTROS_MACACO_ABRIR "Libro Macaco (*.mxs)|*.mxs|Texto CSV (*.csv)|*.csv|Todos los archivos (*.*)|*"
#define FILTROS_MACACO_GUARDAR "Libro Macaco (*.mxs)|*.mxs"
#define FILTROS_CSV "Texto CSV (*.csv)|*.csv"
#define FILTROS_PDF "Documento PDF (*.pdf)|*.pdf"

static void libro_vaciar(void)
{
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        for (int k = 0; k < fo->linhas * fo->colunas; k++)
            if (fo->celulas[k].texto) { libera(fo->celulas[k].texto); fo->celulas[k].texto = NULL; }
        if (fo->celulas) { libera(fo->celulas); fo->celulas = NULL; }
    }
    memset(&livro, 0, sizeof livro);
    livro.n_fmts = 1;
    desfazer_limpar();
    copia_limpar();
}

static bool abrir_ficheiro(void)
{
    char c[256];
    copia_texto(c, caminho[0] ? caminho : "/tmp/livro.mxs", sizeof c);
    if (!dialogo_abrir("Abrir", FILTROS_MACACO_ABRIR, c, c, sizeof c) || !c[0])
        return false;
    if (!carregar_ficheiro(c)) {
        char m[200];
        formatar_texto(m, sizeof m, "No se ha podido abrir\n%s", c);
        mensagem("Macaco", m);
        return false;
    }
    copia_texto(caminho, c, sizeof caminho);
    modificado = false;
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    por_aqui(estado, sizeof estado, "Archivo cargado");
    registrar("macaco: abierto %s", caminho);
    return true;
}

static bool guardar(void)
{
    if (!caminho[0]) return guardar_como();
    if (!guardar_ficheiro(caminho)) {
        char m[200];
        formatar_texto(m, sizeof m, "No se ha podido guardar\n%s", caminho);
        mensagem("Macaco", m);
        return false;
    }
    modificado = false;
    por_aqui(estado, sizeof estado, "Guardado");
    registrar("macaco: guardado %s", caminho);
    return true;
}

static bool guardar_como(void)
{
    char c[256];
    copia_texto(c, caminho[0] ? caminho : "/tmp/livro.mxs", sizeof c);
    if (!dialogo_guardar("Guardar como", FILTROS_MACACO_GUARDAR, c, c, sizeof c) || !c[0])
        return false;
    copia_texto(caminho, c, sizeof caminho);
    return guardar();
}

static void novo_livro(void)
{
    libro_vaciar();
    folha_nova("Hoja1");
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    caminho[0] = 0;
    modificado = false;
    por_aqui(estado, sizeof estado, "Libro nuevo");
    registrar("macaco: libro nuevo");
}

static void inserir_linha(void)
{
    folha_t *fo = folha_ativa();
    desfazer_guardar_folha(livro.ativa);
    if (!linha_inserir(fo, cel_l)) {
        por_aqui(estado, sizeof estado, "No hay espacio para más líneas");
        return;
    }
    modificado = true;
    recalcular();
    registrar("macaco: insertada la línea %d", cel_l + 1);
}

static void apagar_linha(void)
{
    folha_t *fo = folha_ativa();
    if (fo->linhas <= 1) {
        por_aqui(estado, sizeof estado, "La hoja sólo tiene una línea");
        return;
    }
    desfazer_guardar_folha(livro.ativa);
    if (!linha_apagar(fo, cel_l)) return;
    if (cel_l >= fo->linhas) cel_l = fo->linhas - 1;
    anc_l = cel_l;
    modificado = true;
    recalcular();
    registrar("macaco: borrada la línea %d", cel_l + 1);
}

static void inserir_coluna(void)
{
    folha_t *fo = folha_ativa();
    desfazer_guardar_folha(livro.ativa);
    if (!coluna_inserir(fo, cel_c)) {
        por_aqui(estado, sizeof estado, "No hay espacio para más columnas");
        return;
    }
    modificado = true;
    recalcular();
    registrar("macaco: insertada la columna %d", cel_c + 1);
}

static void apagar_coluna(void)
{
    folha_t *fo = folha_ativa();
    if (fo->colunas <= 1) {
        por_aqui(estado, sizeof estado, "La hoja sólo tiene una columna");
        return;
    }
    desfazer_guardar_folha(livro.ativa);
    if (!coluna_apagar(fo, cel_c)) return;
    if (cel_c >= fo->colunas) cel_c = fo->colunas - 1;
    anc_c = cel_c;
    modificado = true;
    recalcular();
    registrar("macaco: borrada la columna %d", cel_c + 1);
}

static void ajustar_larguras(void)
{
    char c[32];
    c[0] = 0;
    if (!pedir_texto("Ancho de las columnas", "Ancho en píxeles (de 16 a 400):", c, sizeof c) || !c[0])
        return;
    int v = texto_para_int(c);
    if (v < 16) v = 16;
    if (v > 400) v = 400;
    folha_t *fo = folha_ativa();
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    bool todas = (l1 == 0 && c1 == 0 && l2 == fo->linhas - 1 && c2 == fo->colunas - 1);
    for (int c = 0; c < fo->colunas; c++)
        if (todas || (c >= c1 && c <= c2)) fo->larg_col[c] = v;
    modificado = true;
    registrar("macaco: anchos de las columnas = %d", v);
}

static void nova_folha(void)
{
    if (livro.n >= MAC_MAX_FOLHAS) {
        por_aqui(estado, sizeof estado, "Ya hay suficientes hojas");
        return;
    }
    char nome[MAC_NOME_FOLHA];
    int k = livro.n + 1;
    do {
        formatar_texto(nome, sizeof nome, "Hoja%d", k++);
    } while (procurar_folha(nome) >= 0);
    folha_nova(nome);
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    modificado = true;
    registrar("macaco: nueva hoja %s", nome);
}

static void apagar_folha(int f)
{
    if (livro.n <= 1) { por_aqui(estado, sizeof estado, "El libro necesita al menos una hoja"); return; }
    if (f < 0 || f >= livro.n) return;
    char m[120];
    formatar_texto(m, sizeof m, "¿Eliminar la hoja %s?", livro.folha[f].nome);
    if (!confirmar("Macaco", m)) return;
    folha_t *fo = &livro.folha[f];
    for (int k = 0; k < fo->linhas * fo->colunas; k++)
        if (fo->celulas[k].texto) libera(fo->celulas[k].texto);
    if (fo->celulas) libera(fo->celulas);
    for (int i = f; i < livro.n - 1; i++) livro.folha[i] = livro.folha[i + 1];
    memset(&livro.folha[livro.n - 1], 0, sizeof(folha_t));
    livro.n--;
    desfazer_limpar();
    if (livro.ativa >= livro.n) livro.ativa = livro.n - 1;
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    modificado = true;
    recalcular();
    registrar("macaco: hoja eliminada");
}

static void renomear_folha(int f)
{
    if (f < 0 || f >= livro.n) return;
    char nome[MAC_NOME_FOLHA];
    copia_texto(nome, livro.folha[f].nome, sizeof nome);
    if (!pedir_texto("Cambiar nombre de la hoja", "Nombre nuevo:", nome, sizeof nome) || !nome[0]) return;
    int o = procurar_folha(nome);
    if (o >= 0 && o != f) { mensagem("Macaco", "Ya existe una hoja con ese nombre."); return; }
    copia_texto(livro.folha[f].nome, nome, sizeof livro.folha[f].nome);
    modificado = true;
    registrar("macaco: hoja renombrada a %s", nome);
}

/* ---- formato de las celdas seleccionadas ---- */

enum { FO_NF, FO_DEC, FO_AL, FO_EST_LIGAR, FO_EST_DESLIGAR, FO_COR, FO_REL, FO_BOR, FO_TAM, FO_FAM, FO_LIMPAR, FO_MASC };

static void aplicar_formato_celula(celula_t *ce, int op, uint32_t v, const fmt_t *completo)
{
    fmt_t f = *fmt_de(ce);
    switch (op) {
    case FO_NF: f.nf = (uint8_t)v; if ((v == NF_MOEDA) && f.dec == 0) f.dec = 2; break;
    case FO_DEC: {
        int d = (int)f.dec + (int)(int32_t)v;
        if (d < 0) d = 0;
        if (d > 4) d = 4;
        if (f.nf == NF_GERAL) f.nf = NF_NUMERO;
        f.dec = (uint8_t)d;
        break;
    }
    case FO_AL: f.al = (uint8_t)v; break;
    case FO_EST_LIGAR: f.est |= (uint8_t)v; break;
    case FO_EST_DESLIGAR: f.est &= (uint8_t)~v; break;
    case FO_COR: f.cor = v; break;
    case FO_REL: f.rel = v; break;
    case FO_BOR: f.bor = (uint8_t)v; break;
    case FO_TAM: f.tam = (uint8_t)(v == 10 ? 0 : v); break;
    case FO_FAM: f.fam = (uint8_t)v; break;
    case FO_LIMPAR: memset(&f, 0, sizeof f); break;
    case FO_MASC:
        if (v & 1) { f.nf = completo->nf; f.dec = completo->dec; f.moeda = completo->moeda; }
        if (v & 2) f.al = completo->al;
        if (v & 4) f.fam = completo->fam;
        if (v & 8) f.est = completo->est;
        if (v & 16) f.tam = completo->tam;
        if (v & 32) f.cor = completo->cor;
        if (v & 64) f.rel = completo->rel;
        if (v & 128) f.bor = completo->bor;
        break;
    }
    int idx = fmt_internar(&f);
    if (idx < 0) { por_aqui(estado, sizeof estado, "Demasiados formatos distintos"); return; }
    ce->fmt = (uint8_t)idx;
}

static void formato_aplicar(int op, uint32_t v, const fmt_t *completo)
{
    folha_t *fo = folha_ativa();
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    desfazer_guardar_folha(livro.ativa);
    if (!folha_redimensionar(fo, l2 + 1 > fo->linhas ? l2 + 1 : fo->linhas, c2 + 1 > fo->colunas ? c2 + 1 : fo->colunas))
        return;
    for (int l = l1; l <= l2; l++)
        for (int c = c1; c <= c2; c++)
            aplicar_formato_celula(celula_de(fo, l, c), op, v, completo);
    modificado = true;
}

static void alternar_estilo(unsigned bit)
{
    const fmt_t *f = fmt_de(celula_de(folha_ativa(), cel_l, cel_c));
    formato_aplicar((f->est & bit) ? FO_EST_DESLIGAR : FO_EST_LIGAR, bit, NULL);
}

static void bordas_aplicar(int escolha)
{
    /* 0 ninguno, 1 contorno, 2 todos, 3 abajo, 4 arriba, 5 izquierda, 6 derecha */
    folha_t *fo = folha_ativa();
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    desfazer_guardar_folha(livro.ativa);
    if (!folha_redimensionar(fo, l2 + 1 > fo->linhas ? l2 + 1 : fo->linhas, c2 + 1 > fo->colunas ? c2 + 1 : fo->colunas))
        return;
    for (int l = l1; l <= l2; l++)
        for (int c = c1; c <= c2; c++) {
            celula_t *ce = celula_de(fo, l, c);
            unsigned b = fmt_de(ce)->bor;
            switch (escolha) {
            case 0: b = 0; break;
            case 1:
                if (l == l1) b |= BOR_CIMA;
                if (l == l2) b |= BOR_BAIXO;
                if (c == c1) b |= BOR_ESQ;
                if (c == c2) b |= BOR_DIR;
                break;
            case 2: b = BOR_CIMA | BOR_BAIXO | BOR_ESQ | BOR_DIR; break;
            case 3: if (l == l2) b |= BOR_BAIXO; break;
            case 4: if (l == l1) b |= BOR_CIMA; break;
            case 5: if (c == c1) b |= BOR_ESQ; break;
            case 6: if (c == c2) b |= BOR_DIR; break;
            }
            aplicar_formato_celula(ce, FO_BOR, b, NULL);
        }
    modificado = true;
}

static void apagar_conteudo(void)
{
    folha_t *fo = folha_ativa();
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    desfazer_guardar_folha(livro.ativa);
    for (int l = l1; l <= l2; l++)
        for (int c = c1; c <= c2; c++) celula_definir(fo, l, c, "");
    modificado = true;
    recalcular();
    registrar("macaco: borradas las celdas");
}

static void cortar(void)
{
    copiar_intervalo();
    apagar_conteudo();
    registrar("macaco: cortadas las celdas");
}

static void autosoma(void)
{
    folha_t *fo = folha_ativa();
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    desfazer_guardar_folha(livro.ativa);
    char f[64], a[16], b[16];
    if (l1 == l2 && c1 == c2) {
        /* Una sola celda: suma los números contiguos de arriba (o, si no hay, de la izquierda). */
        int k = l1 - 1;
        while (k >= 0) {
            celula_t *ce = celula_de(fo, k, c1);
            if (!ce || !(ce->flags & CEL_NUMERO) || (ce->flags & CEL_TEXTO)) break;
            k--;
        }
        if (k < l1 - 1) {
            nome_ref(a, sizeof a, k + 1, c1);
            nome_ref(b, sizeof b, l1 - 1, c1);
        } else {
            int q = c1 - 1;
            while (q >= 0) {
                celula_t *ce = celula_de(fo, l1, q);
                if (!ce || !(ce->flags & CEL_NUMERO) || (ce->flags & CEL_TEXTO)) break;
                q--;
            }
            if (q == c1 - 1) { por_aqui(estado, sizeof estado, "No hay números que sumar"); n_desfazer--; return; }
            nome_ref(a, sizeof a, l1, q + 1);
            nome_ref(b, sizeof b, l1, c1 - 1);
        }
        formatar_texto(f, sizeof f, "=SUM(%s:%s)", a, b);
        celula_definir(fo, l1, c1, f);
    } else {
        /* Un rango: pone la suma de cada columna en la fila de debajo. */
        for (int c = c1; c <= c2; c++) {
            nome_ref(a, sizeof a, l1, c);
            nome_ref(b, sizeof b, l2, c);
            formatar_texto(f, sizeof f, "=SUM(%s:%s)", a, b);
            celula_definir(fo, l2 + 1, c, f);
        }
    }
    modificado = true;
    recalcular();
    registrar("macaco: autosuma");
}

static void ir_para(const char *t)
{
    const char *q = t;
    int c, l;
    if (!ler_coluna(&q, &c) || !ler_numero_linha(&q, &l)) { por_aqui(estado, sizeof estado, "Referencia no válida"); return; }
    mover_para(l, c, false);
    if (*q == ':') {
        q++;
        int c2, l2;
        if (ler_coluna(&q, &c2) && ler_numero_linha(&q, &l2)) { cel_l = l2; cel_c = c2; }
    }
    assure_visivel();
}

static void selecionar_tudo(void)
{
    folha_t *fo = folha_ativa();
    anc_l = 0; anc_c = 0;
    cel_l = fo->linhas - 1; cel_c = fo->colunas - 1;
}

static void desfazer_acao(void)
{
    if (desfazer_refazer()) { modificado = true; recalcular(); por_aqui(estado, sizeof estado, "Deshecho"); }
    else por_aqui(estado, sizeof estado, "Nada para deshacer");
}

static void acerca(void)
{
    mensagem("Acerca de Macaco", "Macaco - hoja de cálculo de SaguiOS\nEstilo Office clásico (mejora 117).\nFormatos: .mxs, CSV y PDF.");
}


/* ------------------------------------------------------------ exportar a PDF (escritor de la libsagui) */

#define PDF_MARGEN 850
#define PDF_PAPEL_L 11906
#define PDF_PAPEL_A 16838
#define PDF_RODAPE 500

static int pdf_alt_linha(folha_t *fo, int l)
{
    int h = MAC_ALTLINHA * 15;
    for (int c = 0; c < fo->colunas; c++) {
        const fmt_t *f = fmt_de(&fo->celulas[(size_t)l * fo->colunas + c]);
        int t = (f->tam ? f->tam : 10) * 24;
        if (t > h) h = t;
    }
    return h;
}

/* Genera el PDF de la hoja activa: las filas bajan por páginas y, al acabar, se sigue con el
 * siguiente bloque de columnas (como "abajo, luego a la derecha" de Excel). Cada celda lleva su
 * relleno, bordes, fuente (Helvetica/Times/Courier), tamaño, estilo, color y alineación. */
static bool pdf_gerar(const char *titulo, uint8_t **dados, int *n)
{
    folha_t *fo = folha_ativa();
    int ul = -1, uc = -1;
    for (int l = 0; l < fo->linhas; l++)
        for (int c = 0; c < fo->colunas; c++) {
            celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
            if (ce->texto || ce->fmt) { if (l > ul) ul = l; if (c > uc) uc = c; }
        }
    if (ul < 0) { ul = 0; uc = 0; }
    pdfw_t *w = pdfw_criar(PDFW_A4);
    if (!w) return false;
    pdfw_titulo(w, titulo);
    int util_l = PDF_PAPEL_L - 2 * PDF_MARGEN;
    int util_a = PDF_PAPEL_A - 2 * PDF_MARGEN - PDF_RODAPE;
    int pagina_n = 0;
    int c0 = 0;
    while (c0 <= uc) {
        int c1 = c0, ac = 0;
        while (c1 <= uc) {
            int lc = fo->larg_col[c1] * 15;
            if (ac + lc > util_l && c1 > c0) break;
            ac += lc;
            c1++;
        }
        c1--;                                   /* último de este bloque de columnas */
        int l0 = 0;
        while (l0 <= ul) {
            int l1 = l0, aa = 0;
            while (l1 <= ul) {
                int h = pdf_alt_linha(fo, l1);
                if (aa + h > util_a && l1 > l0) break;
                aa += h;
                l1++;
            }
            l1--;
            if (!pdfw_pagina(w)) { pdfw_liberar(w); return false; }
            pagina_n++;
            int y = PDF_MARGEN;
            for (int l = l0; l <= l1; l++) {
                int h = pdf_alt_linha(fo, l);
                int x = PDF_MARGEN;
                for (int c = c0; c <= c1; c++) {
                    int lc = fo->larg_col[c] * 15;
                    celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                    const fmt_t *f = fmt_de(ce);
                    if (f->rel & REL_ATIVO) pdfw_retangulo(w, x, y, lc, h, f->rel & 0xFFFFFF);
                    char t[MAC_TEXTO_CEL];
                    celula_exibida(t, sizeof t, ce);
                    int nn = (int)tam_texto(t);
                    if (nn > 0) {
                        int tam = (f->tam ? f->tam : 10) * 20;
                        int fam = f->fam == 1 ? PDFW_SERIF : f->fam == 2 ? PDFW_MONO : PDFW_SANS;
                        int est = ((f->est & EST_NEGRITA) ? PDFW_NEGRITA : 0) | ((f->est & EST_CURSIVA) ? PDFW_CURSIVA : 0);
                        while (nn > 0 && pdfw_largura_texto(fam, est, tam, t, nn) > lc - 90) nn--;
                        bool numero = (ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO);
                        int al = f->al ? f->al : (numero ? AL_DIR : AL_ESQ);
                        int tw = pdfw_largura_texto(fam, est, tam, t, nn);
                        int tx = (al == AL_ESQ) ? x + 45 : (al == AL_DIR ? x + lc - 45 - tw : x + (lc - tw) / 2);
                        int base = y + (h + tam * 7 / 10) / 2;
                        if (nn > 0) pdfw_texto(w, tx, base, fam, est, tam, f->cor, t, nn, 0);
                        if (nn > 0 && (f->est & EST_SUBLIN)) pdfw_linha(w, tx, base + 25, tx + tw, base + 25, 8, f->cor);
                    }
                    if (f->bor & BOR_CIMA) pdfw_linha(w, x, y, x + lc, y, 10, 0);
                    if (f->bor & BOR_BAIXO) pdfw_linha(w, x, y + h, x + lc, y + h, 10, 0);
                    if (f->bor & BOR_ESQ) pdfw_linha(w, x, y, x, y + h, 10, 0);
                    if (f->bor & BOR_DIR) pdfw_linha(w, x + lc, y, x + lc, y + h, 10, 0);
                    x += lc;
                }
                y += h;
            }
            char rod[96];
            formatar_texto(rod, sizeof rod, "%s - página %d", fo->nome, pagina_n);
            pdfw_texto(w, PDF_MARGEN, PDF_PAPEL_A - PDF_MARGEN + 200, PDFW_SANS, 0, 160, 0x808080, rod, (int)tam_texto(rod), 0);
            l0 = l1 + 1;
        }
        c0 = c1 + 1;
    }
    bool ok = pdfw_finalizar(w, dados, n);
    pdfw_liberar(w);
    return ok;
}

static bool pdf_exportar(const char *destino, const char *titulo)
{
    uint8_t *d;
    int n;
    if (!pdf_gerar(titulo, &d, &n)) return false;
    bool ok = escrever_buffer(destino, d, n);
    libera(d);
    if (ok) registrar("macaco: pdf exportado %s (%d bytes)", destino, n);
    return ok;
}

static void trocar_extensao(char *c, int max, const char *ext)
{
    char *p = acha_ultimo_char(c, '.');
    char *b = acha_ultimo_char(c, '/');
    if (p && (!b || p > b)) *p = 0;
    copia_texto(c + tam_texto(c), ext, (size_t)(max - (int)tam_texto(c)));
}

static void exportar_pdf_dialogo(void)
{
    char c[256];
    if (caminho[0]) { copia_texto(c, caminho, sizeof c); trocar_extensao(c, sizeof c, ".pdf"); }
    else copia_texto(c, "/home/libro.pdf", sizeof c);
    if (!dialogo_guardar("Exportar a PDF", FILTROS_PDF, c, c, sizeof c) || !c[0]) return;
    if (!pdf_exportar(c, nome_curto())) { mensagem("Macaco", "No se ha podido exportar el PDF."); return; }
    por_aqui(estado, sizeof estado, "PDF exportado");
}

static void csv_exportar_dialogo(void)
{
    char c[256];
    if (caminho[0]) { copia_texto(c, caminho, sizeof c); trocar_extensao(c, sizeof c, ".csv"); }
    else copia_texto(c, "/home/libro.csv", sizeof c);
    if (!dialogo_guardar("Exportar CSV", FILTROS_CSV, c, c, sizeof c) || !c[0]) return;
    if (!csv_gravar(c)) { mensagem("Macaco", "No se ha podido guardar el CSV."); return; }
    por_aqui(estado, sizeof estado, "CSV exportado");
    registrar("macaco: csv exportado %s", c);
}

static void csv_importar_dialogo(void)
{
    char c[256];
    copia_texto(c, "/home/libro.csv", sizeof c);
    if (!dialogo_abrir("Importar CSV", FILTROS_CSV "|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) return;
    folha_t *fo = folha_ativa();
    desfazer_guardar_folha(livro.ativa);
    int n = csv_ler(fo, c, cel_l, cel_c);
    if (n < 0) { mensagem("Macaco", "No se ha podido leer el CSV."); n_desfazer--; return; }
    modificado = true;
    recalcular();
    por_aqui(estado, sizeof estado, "CSV importado");
    registrar("macaco: csv importado %s (%d celdas)", c, n);
}

/* Sin ventana, para las pruebas: macaco --convertir entrada salida (.mxs/.csv -> .mxs/.csv/.pdf). */
static int modo_converter(const char *entrada, const char *saida)
{
    if (!carregar_ficheiro(entrada)) { registrar("macaco: no se ha leído %s", entrada); return 1; }
    bool ok;
    if (termina_en(saida, ".pdf")) ok = pdf_exportar(saida, acha_ultimo_char(saida, '/') ? acha_ultimo_char(saida, '/') + 1 : saida);
    else if (termina_en(saida, ".csv")) ok = csv_gravar(saida);
    else ok = guardar_ficheiro(saida);
    registrar("macaco: convertido %s -> %s %s", entrada, saida, ok ? "bien" : "ERROR");
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------ diálogo Formato de celdas */

#define M_NUM 1
#define M_AL  2
#define M_FAM 4
#define M_EST 8
#define M_TAM 16
#define M_COR 32
#define M_REL 64
#define M_BOR 128

static const int TAMS_DLG[15] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 36, 48, 72 };

static void icone_cor_txt(jan_t *j, int x, int y, int i)
{
    if (i >= 0 && i < 9) { g_retangulo(j, x + 1, y + 2, 14, 12, PALETA_COR[i]); g_moldura(j, x + 1, y + 2, 14, 12, W98_SOMBRA); }
}
static void icone_cor_rel(jan_t *j, int x, int y, int i)
{
    if (i >= 0 && i < 9) { g_retangulo(j, x + 1, y + 2, 14, 12, PALETA_REL[i]); g_moldura(j, x + 1, y + 2, 14, 12, W98_SOMBRA); }
    else g_moldura(j, x + 1, y + 2, 14, 12, W98_SOMBRA);
}

static void dialogo_celulas(void)
{
    static char it_cat[5][64] = { "General", "Número", "Moneda", "Porcentaje", "Miles" };
    static char it_mo[3][64] = { "$ (dólar)", "EUR (euro)", "£ (libra)" };
    static char it_al[4][64] = { "General", "Izquierda", "Centro", "Derecha" };
    static char it_fam[3][64] = { "Arial", "Times New Roman", "Courier New" };
    static char it_est[4][64] = { "Normal", "Cursiva", "Negrita", "Negrita cursiva" };
    static char it_tam[15][64], it_cor[9][64], it_rel[10][64];
    for (int i = 0; i < 15; i++) formatar_texto(it_tam[i], 64, "%d", TAMS_DLG[i]);
    for (int i = 0; i < 9; i++) copia_texto(it_cor[i], NOMES_COR[i], 64);
    for (int i = 0; i < 9; i++) copia_texto(it_rel[i], NOMES_REL[i], 64);
    copia_texto(it_rel[9], "Sin relleno", 64);

    celula_t *ativa = celula_de(folha_ativa(), cel_l, cel_c);
    fmt_t ini = *fmt_de(ativa);
    jan_t *j = jan_abrir("Formato de celdas", 440, 410);
    if (!j) return;
    jan_estilo(j, JAN_DIALOGO, 0, 0);

    int sel_tam = 2, sel_cor = -1, sel_rel = 9;
    for (int i = 0; i < 15; i++) if (TAMS_DLG[i] == (ini.tam ? ini.tam : 10)) sel_tam = i;
    for (int i = 0; i < 9; i++) if (PALETA_COR[i] == ini.cor) { sel_cor = i; break; }
    for (int i = 0; i < 9; i++) if ((ini.rel & REL_ATIVO) && PALETA_REL[i] == (ini.rel & 0xFFFFFF)) sel_rel = i;
    if (sel_cor < 0) sel_cor = 0;

    lista_t l_cat = { 24, 62, 150, 90, it_cat, 5, ini.nf < 5 ? ini.nf : 0, 0, NULL, NULL };
    lista_t l_mo = { 200, 128, 140, 62, it_mo, 3, ini.moeda < 3 ? ini.moeda : 0, 0, NULL, NULL };
    lista_t l_al = { 24, 62, 150, 90, it_al, 4, ini.al < 4 ? ini.al : 0, 0, NULL, NULL };
    lista_t l_fam = { 24, 62, 150, 82, it_fam, 3, ini.fam < 3 ? ini.fam : 0, 0, NULL, NULL };
    lista_t l_est = { 184, 62, 130, 82, it_est, 4, ((ini.est & EST_NEGRITA) ? 2 : 0) | ((ini.est & EST_CURSIVA) ? 1 : 0), 0, NULL, NULL };
    lista_t l_tam = { 324, 62, 80, 82, it_tam, 15, sel_tam, 0, NULL, NULL };
    lista_t l_cor = { 24, 176, 190, 100, it_cor, 9, sel_cor, 0, icone_cor_txt, NULL };
    lista_t l_rel = { 24, 62, 200, 190, it_rel, 10, sel_rel, 0, icone_cor_rel, NULL };
    for (lista_t *l = &l_cat; l <= &l_rel; l++) {
        int vis = (l->a - 4) / LISTA_ALTURA_ITEM;
        if (l->sel >= vis) l->topo = l->sel - vis + 1;
    }
    caixa_t dec;
    memset(&dec, 0, sizeof dec);
    dec.x = 200; dec.y = 80; dec.l = 50;
    char t[16];
    formatar_texto(t, sizeof t, "%d", ini.dec);
    caixa_definir(&dec, t);
    casilla_t c_sub = { 200, 182, (ini.est & EST_SUBLIN) != 0 };
    casilla_t c_b[4] = {
        { 24, 70, (ini.bor & BOR_CIMA) != 0 }, { 24, 96, (ini.bor & BOR_BAIXO) != 0 },
        { 24, 122, (ini.bor & BOR_ESQ) != 0 }, { 24, 148, (ini.bor & BOR_DIR) != 0 },
    };
    static const char *const rot_b[4] = { "Borde superior", "Borde inferior", "Borde izquierdo", "Borde derecho" };
    botao_t b_ning = { 220, 66, 150, 24, "Ninguno", false }, b_cont = { 220, 96, 150, 24, "Contorno", false },
            b_todos = { 220, 126, 150, 24, "Todos", false };
    botao_t ok = { 240, 372, 90, 26, "Aceptar", false }, cancel = { 340, 372, 90, 26, "Cancelar", false };
    pestanas_t ps = { 8, 8, 424, 5, 0, { "Número", "Alineación", "Fuente", "Bordes", "Tramas" } };
    int foco_dec = 0;
    bool aceptar = false, redib = true;
    evento_t e;

    /* El valor de muestra: el de la celda activa si es un número. */
    int32_t amostra_v = (ativa && (ativa->flags & CEL_NUMERO) && !(ativa->flags & CEL_TEXTO)) ? ativa->valor : 123456;

    for (;;) {
        if (redib) {
            g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE);
            pestanas_desenhar(j, &ps);
            g_bisel(j, 8, 30, 424, 330, false);
            switch (ps.sel) {
            case 0: {
                rotulo(j, 24, 44, "Categoría:", W98_NEGRO);
                lista_en_marco(j, &l_cat);
                rotulo(j, 200, 62, "Posiciones decimales:", W98_NEGRO);
                caixa_desenhar(j, &dec);
                rotulo(j, 200, 110, "Símbolo:", W98_NEGRO);
                lista_en_marco(j, &l_mo);
                rotulo(j, 24, 190, "Muestra:", W98_NEGRO);
                fmt_t m = ini;
                m.nf = (uint8_t)l_cat.sel;
                m.dec = (uint8_t)texto_para_int(dec.texto);
                m.moeda = (uint8_t)(l_mo.sel < 0 ? 0 : l_mo.sel);
                char v[64];
                fmt_numero(v, sizeof v, amostra_v, &m);
                g_retangulo(j, 24, 210, 380, 28, COR_BRANCO);
                g_bisel(j, 22, 208, 384, 32, true);
                g_texto(j, 32, 216, v, W98_NEGRO);
                break;
            }
            case 1:
                rotulo(j, 24, 44, "Horizontal:", W98_NEGRO);
                lista_en_marco(j, &l_al);
                break;
            case 2:
                rotulo(j, 24, 44, "Fuente:", W98_NEGRO); rotulo(j, 184, 44, "Estilo:", W98_NEGRO); rotulo(j, 324, 44, "Tamaño:", W98_NEGRO);
                lista_en_marco(j, &l_fam); lista_en_marco(j, &l_est); lista_en_marco(j, &l_tam);
                rotulo(j, 24, 158, "Color:", W98_NEGRO);
                lista_en_marco(j, &l_cor);
                casilla98(j, &c_sub, "Subrayado");
                rotulo(j, 200, 214, "En pantalla se ven el tamaño, el", W98_NEGRO);
                rotulo(j, 200, 232, "estilo y el color; la familia solo", W98_NEGRO);
                rotulo(j, 200, 250, "cambia en el PDF.", W98_NEGRO);
                break;
            case 3:
                for (int i = 0; i < 4; i++) casilla98(j, &c_b[i], rot_b[i]);
                boton98(j, &b_ning); boton98(j, &b_cont); boton98(j, &b_todos);
                break;
            case 4:
                rotulo(j, 24, 44, "Color de fondo:", W98_NEGRO);
                lista_en_marco(j, &l_rel);
                break;
            }
            boton98(j, &ok); boton98(j, &cancel);
            jan_mostrar(j);
            redib = false;
        }
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (boton98_tratar(j, &ok, &e)) { aceptar = true; break; }
        if (boton98_tratar(j, &cancel, &e)) break;
        if (pestanas_tratar(j, &ps, &e)) { redib = true; continue; }
        if (e.tipo == EV_TECLA && e.tecla == '\n' && !foco_dec) { aceptar = true; break; }
        switch (ps.sel) {
        case 0:
            if (e.tipo == EV_CLIQUE) foco_dec = (e.x >= dec.x && e.x < dec.x + dec.l && e.y >= dec.y - 2 && e.y < dec.y + 24);
            dec.foco = foco_dec;
            if (e.tipo == EV_TECLA && foco_dec) { if (caixa_tratar(j, &dec, &e)) foco_dec = 0; redib = true; break; }
            if (lista_tratar(j, &l_cat, &e) != LISTA_NADA) redib = true;
            if (lista_tratar(j, &l_mo, &e) != LISTA_NADA && e.tipo == EV_CLIQUE) redib = true;
            break;
        case 1: if (lista_tratar(j, &l_al, &e) != LISTA_NADA) redib = true; break;
        case 2:
            if (casilla98_tratar(j, &c_sub, "Subrayado", &e)) redib = true;
            if (lista_tratar(j, &l_fam, &e) != LISTA_NADA) redib = true;
            if (lista_tratar(j, &l_est, &e) != LISTA_NADA) redib = true;
            if (lista_tratar(j, &l_tam, &e) != LISTA_NADA) redib = true;
            if (lista_tratar(j, &l_cor, &e) != LISTA_NADA) redib = true;
            break;
        case 3:
            for (int i = 0; i < 4; i++) if (casilla98_tratar(j, &c_b[i], rot_b[i], &e)) redib = true;
            if (boton98_tratar(j, &b_ning, &e)) { for (int i = 0; i < 4; i++) c_b[i].marcada = false; redib = true; }
            if (boton98_tratar(j, &b_cont, &e) || boton98_tratar(j, &b_todos, &e)) { for (int i = 0; i < 4; i++) c_b[i].marcada = true; redib = true; }
            break;
        case 4: if (lista_tratar(j, &l_rel, &e) != LISTA_NADA) redib = true; break;
        }
    }
    jan_fechar(j);
    if (!aceptar) return;

    /* Aplica solo lo que el usuario cambió (como Excel), sobre toda la selección. */
    fmt_t r = ini;
    unsigned mask = 0;
    int d = texto_para_int(dec.texto);
    if (d < 0) d = 0;
    if (d > 4) d = 4;
    if (l_cat.sel != ini.nf || d != ini.dec || (l_cat.sel == NF_MOEDA && l_mo.sel != ini.moeda)) {
        r.nf = (uint8_t)l_cat.sel; r.dec = (uint8_t)d; r.moeda = (uint8_t)(l_mo.sel < 0 ? 0 : l_mo.sel);
        mask |= M_NUM;
    }
    if (l_al.sel != ini.al) { r.al = (uint8_t)l_al.sel; mask |= M_AL; }
    if (l_fam.sel != ini.fam) { r.fam = (uint8_t)l_fam.sel; mask |= M_FAM; }
    uint8_t est = (uint8_t)(((l_est.sel & 2) ? EST_NEGRITA : 0) | ((l_est.sel & 1) ? EST_CURSIVA : 0) | (c_sub.marcada ? EST_SUBLIN : 0));
    if (est != ini.est) { r.est = est; mask |= M_EST; }
    uint8_t tam = (uint8_t)(TAMS_DLG[l_tam.sel < 0 ? 2 : l_tam.sel] == 10 ? 0 : TAMS_DLG[l_tam.sel < 0 ? 2 : l_tam.sel]);
    if (tam != ini.tam) { r.tam = tam; mask |= M_TAM; }
    if (l_cor.sel >= 0 && PALETA_COR[l_cor.sel] != ini.cor) { r.cor = PALETA_COR[l_cor.sel]; mask |= M_COR; }
    uint32_t rel = (l_rel.sel >= 0 && l_rel.sel < 9) ? (REL_ATIVO | PALETA_REL[l_rel.sel]) : 0;
    if (rel != ini.rel) { r.rel = rel; mask |= M_REL; }
    uint8_t bor = (uint8_t)((c_b[0].marcada ? BOR_CIMA : 0) | (c_b[1].marcada ? BOR_BAIXO : 0) | (c_b[2].marcada ? BOR_ESQ : 0) | (c_b[3].marcada ? BOR_DIR : 0));
    if (bor != ini.bor) { r.bor = bor; mask |= M_BOR; }
    if (!mask) return;
    formato_aplicar(FO_MASC, mask, &r);
    registrar("macaco: formato de celdas aplicado (máscara %x)", mask);
}

static void executar_acao(int a)
{
    switch (a) {
    case A_NOVO:
        if (modificado && !confirmar("Macaco", "El libro tiene cambios sin guardar.\n¿Descartarlos?")) return;
        novo_livro(); break;
    case A_ABRIR: abrir_ficheiro(); break;
    case A_GUARDAR: guardar(); break;
    case A_GUARDAR_COMO: guardar_como(); break;
    case A_PDF: exportar_pdf_dialogo(); break;
    case A_CSV_IMP: csv_importar_dialogo(); break;
    case A_CSV_EXP: csv_exportar_dialogo(); break;
    case A_SAIR: sair_pedido = true; break;
    case A_DESFAZER: desfazer_acao(); break;
    case A_CORTAR: cortar(); break;
    case A_COPIAR: copiar_intervalo(); break;
    case A_COLAR: colar_intervalo(); break;
    case A_BORRAR: apagar_conteudo(); break;
    case A_BORRAR_FMT: formato_aplicar(FO_LIMPAR, 0, NULL); break;
    case A_TUDO: selecionar_tudo(); break;
    case A_IR: {
        char t[24];
        t[0] = 0;
        if (pedir_texto("Ir a", "Referencia (A1 o A1:C3):", t, sizeof t) && t[0]) ir_para(t);
        break;
    }
    case A_LINHA_INS: inserir_linha(); break;
    case A_LINHA_DEL: apagar_linha(); break;
    case A_COL_INS: inserir_coluna(); break;
    case A_COL_DEL: apagar_coluna(); break;
    case A_FOLHA_NOVA: nova_folha(); break;
    case A_FOLHA_DEL: apagar_folha(pest_ctx >= 0 ? pest_ctx : livro.ativa); pest_ctx = -1; break;
    case A_FOLHA_REN: renomear_folha(pest_ctx >= 0 ? pest_ctx : livro.ativa); pest_ctx = -1; break;
    case A_GRAF_B: abrir_grafico(GRAF_BARRAS); break;
    case A_GRAF_H: abrir_grafico(GRAF_BARRAS_H); break;
    case A_GRAF_T: abrir_grafico(GRAF_TARTA); break;
    case A_CELULAS: dialogo_celulas(); break;
    case A_NEGRITA: alternar_estilo(EST_NEGRITA); break;
    case A_CURSIVA: alternar_estilo(EST_CURSIVA); break;
    case A_SUBLIN: alternar_estilo(EST_SUBLIN); break;
    case A_ESQ: formato_aplicar(FO_AL, AL_ESQ, NULL); break;
    case A_CENTRO: formato_aplicar(FO_AL, AL_CENTRO, NULL); break;
    case A_DIR: formato_aplicar(FO_AL, AL_DIR, NULL); break;
    case A_MOEDA: formato_aplicar(FO_NF, NF_MOEDA, NULL); break;
    case A_PERCENT: formato_aplicar(FO_NF, NF_PERCENT, NULL); break;
    case A_MILHAR: formato_aplicar(FO_NF, NF_MILHAR, NULL); break;
    case A_DEC_MAIS: formato_aplicar(FO_DEC, 1, NULL); break;
    case A_DEC_MENOS: formato_aplicar(FO_DEC, (uint32_t)-1, NULL); break;
    case A_BORDAS: {
        static const char *const t = "Sin bordes\nContorno\nTodos los bordes\nBorde inferior\nBorde superior\nBorde izquierdo\nBorde derecho";
        menu_contextual(jan->id, barra_ferr_x(&ferr2, 17), Y_FERR2 + FERR_A, t, COD_BOR);
        break;
    }
    case A_RELLENO: {
        char t[400];
        int k = 0;
        for (int i = 0; i < 9; i++) k += formatar_texto(t + k, (int)sizeof t - k, "%s\n", NOMES_REL[i]);
        copia_texto(t + k, "Sin relleno", (size_t)((int)sizeof t - k));
        menu_contextual(jan->id, barra_ferr_x(&ferr2, 18), Y_FERR2 + FERR_A, t, COD_REL);
        break;
    }
    case A_COR_TXT: {
        char t[400];
        int k = 0;
        for (int i = 0; i < 9; i++) k += formatar_texto(t + k, (int)sizeof t - k, "%s%s", NOMES_COR[i], i < 8 ? "\n" : "");
        menu_contextual(jan->id, barra_ferr_x(&ferr2, 19), Y_FERR2 + FERR_A, t, COD_COR);
        break;
    }
    case A_LARG: ajustar_larguras(); break;
    case A_ORD_A: ordenar_intervalo(true); break;
    case A_ORD_Z: ordenar_intervalo(false); break;
    case A_SOMA: autosoma(); break;
    case A_RECALC: recalcular(); por_aqui(estado, sizeof estado, "Recalculado"); break;
    case A_GRADE: grade_visivel = !grade_visivel; break;
    case A_FORMULAS: mostrar_formulas = !mostrar_formulas; break;
    case A_ACERCA: acerca(); break;
    case A_IGUAL:
        if (!editando) comecar_edicao("=");
        break;
    default: break;
    }
}

/* ------------------------------------------------------------ eventos */

static void tratar_tecla_edicao(const evento_t *e)
{
    unsigned t = e->tecla;
    int n = (int)tam_texto(edit_buf);
    if (t == '\n') { confirmar_edicao(); mover_para(cel_l + 1, cel_c, false); return; }
    if (t == '\t') { confirmar_edicao(); mover_para(cel_l, cel_c + 1, false); return; }
    if (t == 27 || t == TECLA_ESC) { cancelar_edicao(); return; }
    if (t == TECLA_F4) { alternar_absoluta(); return; }
    if (t == TECLA_CIMA) { confirmar_edicao(); mover_para(cel_l - 1, cel_c, false); return; }
    if (t == TECLA_BAIXO) { confirmar_edicao(); mover_para(cel_l + 1, cel_c, false); return; }
    if (t == '\b') {
        if (edit_cur > 0) {
            for (int i = edit_cur - 1; i < n; i++) edit_buf[i] = edit_buf[i + 1];
            edit_cur--;
        }
        return;
    }
    if (t == TECLA_DELETE) {
        if (edit_cur < n)
            for (int i = edit_cur; i < n; i++) edit_buf[i] = edit_buf[i + 1];
        return;
    }
    if (t == TECLA_ESQUERDA) { if (edit_cur > 0) edit_cur--; return; }
    if (t == TECLA_DIREITA) { if (edit_cur < n) edit_cur++; return; }
    if (t == TECLA_HOME) { edit_cur = 0; return; }
    if (t == TECLA_END) { edit_cur = n; return; }
    if (t >= 0x20 && t < 0x100 && t != 0x7F && n < MAC_TEXTO_CEL - 1) {
        for (int i = n; i > edit_cur; i--) edit_buf[i] = edit_buf[i - 1];
        edit_buf[edit_cur] = (char)t;
        edit_cur++;
        edit_buf[n + 1] = 0;
    }
}

static void tratar_tecla_nome(const evento_t *e)
{
    unsigned t = e->tecla;
    int n = (int)tam_texto(nome_buf);
    if (t == '\n') { nome_foco = false; ir_para(nome_buf); return; }
    if (t == 27 || t == TECLA_ESC) { nome_foco = false; return; }
    if (t == '\b') { if (n > 0) nome_buf[n - 1] = 0; return; }
    if (t >= 0x20 && t < 0x7F && n < (int)sizeof nome_buf - 1) { nome_buf[n] = (char)t; nome_buf[n + 1] = 0; }
}

static void tratar_tecla(const evento_t *e)
{
    unsigned t = e->tecla;
    int mod = (int)e->modificadores;
    bool ctrl = (mod & 2) != 0;
    bool shift = (mod & 1) != 0;

    if (nome_foco) { tratar_tecla_nome(e); return; }
    if (editando) { tratar_tecla_edicao(e); return; }

    if (ctrl) {
        switch (t) {
        case 19: if (shift) guardar_como(); else guardar(); return;      /* Ctrl+S */
        case 15: abrir_ficheiro(); return;                               /* Ctrl+O */
        case 14: executar_acao(A_NOVO); return;                               /* Ctrl+N */
        case 16: exportar_pdf_dialogo(); return;                         /* Ctrl+P */
        case 26: desfazer_acao(); return;                                /* Ctrl+Z */
        case 3: copiar_intervalo(); return;                              /* Ctrl+C */
        case 22: colar_intervalo(); return;                              /* Ctrl+V */
        case 24: cortar(); return;                                       /* Ctrl+X */
        case 1: selecionar_tudo(); return;                               /* Ctrl+A */
        case 7: executar_acao(A_IR); return;                                  /* Ctrl+G */
        case 2: alternar_estilo(EST_NEGRITA); return;                    /* Ctrl+B */
        case 21: alternar_estilo(EST_SUBLIN); return;                    /* Ctrl+U */
        case '1': dialogo_celulas(); return;                             /* Ctrl+1 */
        default: break;
        }
        if (t == 9) { alternar_estilo(EST_CURSIVA); return; }            /* Ctrl+I */
    }

    switch (t) {
    case TECLA_F2: comecar_edicao(NULL); return;
    case TECLA_CIMA:
        if (ctrl) {
            int l, c; bool achou;
            limite_dados(&l, &c, &achou);
            mover_para(cel_l > 0 ? 0 : (achou ? l : 0), cel_c, shift);
        } else mover_para(cel_l - 1, cel_c, shift);
        return;
    case TECLA_BAIXO:
        if (ctrl) {
            int l, c; bool achou;
            limite_dados(&l, &c, &achou);
            mover_para(achou ? l : cel_l, cel_c, shift);
        } else mover_para(cel_l + 1, cel_c, shift);
        return;
    case TECLA_ESQUERDA:
        if (ctrl) {
            int l, c; bool achou;
            limite_dados(&l, &c, &achou);
            mover_para(cel_l, cel_c > 0 ? 0 : (achou ? c : 0), shift);
        } else mover_para(cel_l, cel_c - 1, shift);
        return;
    case TECLA_DIREITA:
        if (ctrl) {
            int l, c; bool achou;
            limite_dados(&l, &c, &achou);
            mover_para(cel_l, achou ? c : cel_c, shift);
        } else mover_para(cel_l, cel_c + 1, shift);
        return;
    case TECLA_HOME: mover_para(ctrl ? 0 : cel_l, 0, shift); return;
    case TECLA_END: {
        int l, c; bool achou;
        limite_dados(&l, &c, &achou);
        mover_para(ctrl && achou ? l : cel_l, achou ? c : cel_c, shift);
        return;
    }
    case TECLA_PGUP: mover_para(cel_l - (grelha_a() / MAC_ALTLINHA), cel_c, shift); return;
    case TECLA_PGDN: mover_para(cel_l + (grelha_a() / MAC_ALTLINHA), cel_c, shift); return;
    case TECLA_DELETE:
    case '\b': apagar_conteudo(); return;
    case '\n': comecar_edicao(NULL); return;
    case '\t': mover_para(cel_l, shift ? cel_c - 1 : cel_c + 1, false); return;
    default: break;
    }

    /* Una tecla normal: empieza a escribir. */
    if (t >= 0x20 && t < 0x100 && t != 0x7F && !ctrl) {
        char ch[2];
        ch[0] = (char)t;
        ch[1] = 0;
        comecar_edicao(ch);
    }
}

/* La celda que hay en un punto de la ventana (sin mover nada). */
static void celula_em(int mx, int my, int *l, int *c)
{
    folha_t *fo = folha_ativa();
    int gy = grelha_y();
    *l = (my - gy + desl_y) / MAC_ALTLINHA;
    if (*l < 0) *l = 0;
    if (*l >= MAC_MAX_LINHAS) *l = MAC_MAX_LINHAS - 1;
    /* La columna: los anchos no son todos iguales. */
    *c = 0;
    int x = MAC_LARGC - desl_x;
    while (*c < fo->colunas && mx >= x + fo->larg_col[*c]) { x += fo->larg_col[*c]; (*c)++; }
    if (*c > fo->colunas) *c = fo->colunas;
    if (*c >= MAC_MAX_COLUNAS) *c = MAC_MAX_COLUNAS - 1;
}

static void clique_grelha(int mx, int my)
{
    int l, c;
    celula_em(mx, my, &l, &c);
    cel_l = l;
    cel_c = c;
    anc_l = l;
    anc_c = c;
    assure_visivel();
}

/* Durante el arrastre sólo se mueve la celda activa: el ancla se queda donde
 * se empezó, para que la selección sea el rango entero. */
static void arrastrar_para(int mx, int my)
{
    int l, c;
    celula_em(mx, my, &l, &c);
    cel_l = l;
    cel_c = c;
}

/* Clic en la cabecera de las columnas: borde (empieza a redimensionar), esquina (todo) o columna. */
static void clique_cabecalho(int mx, int my)
{
    folha_t *fo = folha_ativa();
    int hy = grelha_y() - MAC_CABECALH;
    if (my < hy || my >= grelha_y()) return;
    if (mx < MAC_LARGC) { selecionar_tudo(); return; }
    calcular_offsets();
    for (int c = 0; c < fo->colunas; c++) {
        int borde = x_coluna(c + 1);
        if (mx >= borde - 3 && mx <= borde + 3) {
            redim_col = c; redim_x0 = mx; redim_w0 = fo->larg_col[c];
            return;
        }
    }
    int c = 0;
    int x = MAC_LARGC - desl_x;
    while (c < fo->colunas && mx >= x + fo->larg_col[c]) { x += fo->larg_col[c]; c++; }
    if (c < fo->colunas) {
        cel_c = c;
        anc_c = c;
        cel_l = fo->linhas - 1;
        anc_l = 0;
        assure_visivel();
    }
}

static void clique_cabecalho_linha(int my)
{
    folha_t *fo = folha_ativa();
    int l = (my - grelha_y() + desl_y) / MAC_ALTLINHA;
    if (l < 0) l = 0;
    if (l >= fo->linhas) l = fo->linhas - 1;
    anc_l = l; cel_l = l;
    anc_c = 0; cel_c = fo->colunas - 1;
}

/* La pestaña (hoja) bajo una posición, o -1. -2 = el botón "+". */
static int pestana_em(int mx, int my)
{
    int y = Y_PEST;
    if (my < y || my >= y + MAC_RODAPE - ESTADO_A) return -1;
    int x = 6;
    for (int f = 0; f < livro.n; f++) {
        int l = larg_pestana(f);
        if (x + l > jan->larg - 30) break;
        if (mx >= x && mx < x + l) return f;
        x += l;
    }
    if (mx >= x + 4 && mx < x + 24) return -2;
    return -1;
}

static void clique_pestana(int p)
{
    if (p == -2) { nova_folha(); return; }
    if (p < 0) return;
    livro.ativa = p;
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    registrar("macaco: hoja %s", livro.folha[p].nome);
}

static void menu_contexto_grelha(int mx, int my)
{
    int l, c;
    celula_em(mx, my, &l, &c);
    if (!na_selecao(l, c)) {
        cel_l = l; cel_c = c; anc_l = l; anc_c = c;
        assure_visivel();
    }
    menu_lista(mx, my, M_CONTEXTO, NM(M_CONTEXTO), COD_CTX);
}

/* Elecciones de los menús desplegables propios. Devuelve true si el evento era de alguno. */
static bool tratar_menus_proprios(const evento_t *e)
{
    if (e->tipo != EV_MENU) return false;
    int cod = (int)e->tecla;
    int i = e->x;
    if (cod == COD_CTX) { if (i >= 0) executar_acao(acao_de_linha(M_CONTEXTO, NM(M_CONTEXTO), i)); return true; }
    if (cod == COD_PEST) { if (i >= 0) executar_acao(acao_de_linha(M_PESTANA, NM(M_PESTANA), i)); else pest_ctx = -1; return true; }
    if (cod == COD_REL) {
        if (i >= 0 && i < 9) { cor_rel_atual = PALETA_REL[i]; formato_aplicar(FO_REL, REL_ATIVO | PALETA_REL[i], NULL); }
        else if (i == 9) formato_aplicar(FO_REL, 0, NULL);
        return true;
    }
    if (cod == COD_COR) {
        if (i >= 0 && i < 9) { cor_txt_atual = PALETA_COR[i]; formato_aplicar(FO_COR, PALETA_COR[i], NULL); }
        return true;
    }
    if (cod == COD_BOR) { if (i >= 0) bordas_aplicar(i); return true; }
    return false;
}

static void tratar_evento(const evento_t *e)
{
    if (e->tipo == EV_FECHAR) return;
    if (e->tipo == EV_REDIMENSIONAR) { jan_redimensionar(jan, e); layout(); return; }

    /* menús y listas desplegables */
    if (e->tipo == EV_MENU) {
        int m = barra_menu_tratar(jan, &menu, e);
        if (m >= 0) { executar_acao(MENUS[m / 100].l[m % 100].a); return; }
        if (tratar_menus_proprios(e)) return;
        if (combo_tratar(jan, &c_fonte, e) == COMBO_SELECIONOU) { formato_aplicar(FO_FAM, (uint32_t)c_fonte.sel, NULL); return; }
        if (combo_tratar(jan, &c_tam, e) == COMBO_SELECIONOU) {
            formato_aplicar(FO_TAM, (uint32_t)texto_para_int(c_tam.texto), NULL);
            return;
        }
        return;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) barra_menu_tratar(jan, &menu, e);

    /* combos con foco: reciben el teclado */
    if (e->tipo == EV_TECLA && (c_tam.foco || c_fonte.foco)) {
        int r = combo_tratar(jan, c_tam.foco ? &c_tam : &c_fonte, e);
        if (r == COMBO_ENTROU) {
            int v = texto_para_int(c_tam.texto);
            if (v >= 1 && v <= 72) formato_aplicar(FO_TAM, (uint32_t)v, NULL);
        }
        return;
    }
    if (e->tipo == EV_CLIQUE) {
        combo_tratar(jan, &c_fonte, e);
        combo_tratar(jan, &c_tam, e);
    }

    /* barras de herramientas */
    int id = barra_ferr_tratar(jan, &ferr1, e);
    if (!id) id = barra_ferr_tratar(jan, &ferr2, e);
    if (id > 0) { confirmar_edicao(); executar_acao(id); return; }

    if (e->tipo == EV_RODA) {
        if (e->y >= grelha_y() && e->y < grelha_y() + grelha_a()) {
            desl_y += (int)e->cliques * MAC_ALTLINHA * RODA_LINHAS;
            if (desl_y < 0) desl_y = 0;
            folha_t *fo = folha_ativa();
            int max = fo->linhas * MAC_ALTLINHA - grelha_a();
            if (desl_y > max) desl_y = (max > 0) ? max : 0;
        }
        return;
    }

    if (e->tipo == EV_CLIQUE) {
        if (e->y < MAC_BARRA) return;
        /* Barra de fórmulas. */
        if (e->y < MAC_BARRA + MAC_FORMULA) {
            nome_foco = false;
            if (e->x >= 4 && e->x < 80) {
                confirmar_edicao();
                nome_foco = true;
                nome_buf[0] = 0;       /* el texto anterior se reemplaza al escribir, como en Excel */
            } else if (e->x >= 84 && e->x < 106) executar_acao(A_IGUAL);
            else if (e->x >= 110) { if (!editando) comecar_edicao(NULL); edit_cur = (int)tam_texto(edit_buf); }
            return;
        }
        nome_foco = false;
        /* El botón derecho en la cuadrícula abre el menú contextual. */
        if ((e->botoes & 2) && e->y >= grelha_y() && e->y < grelha_y() + grelha_a() && e->x >= MAC_LARGC) {
            confirmar_edicao();
            menu_contexto_grelha(e->x, e->y);
            return;
        }
        if (e->y >= Y_PEST && e->y < Y_ESTADO) {
            int p = pestana_em(e->x, e->y);
            confirmar_edicao();
            if (e->botoes & 2) {
                if (p >= 0) { pest_ctx = p; clique_pestana(p); menu_lista(e->x, Y_PEST - 4 * 20, M_PESTANA, NM(M_PESTANA), COD_PEST); }
            } else clique_pestana(p);
            return;
        }
        if (e->y >= Y_ESTADO) return;
        confirmar_edicao();
        if (e->y < grelha_y()) { clique_cabecalho(e->x, e->y); return; }
        if (e->botoes & 1) {
            if (e->x < MAC_LARGC) { clique_cabecalho_linha(e->y); return; }
            clique_grelha(e->x, e->y);
            arrastrando = true;
        }
        return;
    }

    if (e->tipo == EV_MOVER && redim_col >= 0 && (e->botoes & 1)) {
        int w = redim_w0 + e->x - redim_x0;
        if (w < 16) w = 16;
        if (w > 400) w = 400;
        folha_ativa()->larg_col[redim_col] = w;
        return;
    }
    if (e->tipo == EV_MOVER && arrastrando && (e->botoes & 1)) {
        arrastrar_para(e->x, e->y);
        return;
    }
    if (e->tipo == EV_SOLTOU) {
        if (arrastrando) assure_visivel();
        if (redim_col >= 0) { modificado = true; registrar("macaco: anchura de la columna %d = %d", redim_col + 1, folha_ativa()->larg_col[redim_col]); }
        arrastrando = false;
        redim_col = -1;
        return;
    }
    if (e->tipo == EV_TECLA) {
        /* Con las teclas de edición pendientes, el cuadro de nombre y la barra mandan. */
        tratar_tecla(e);
        return;
    }
}

/* ------------------------------------------------------------ modo de prueba */

/* Escribe en el registro el contenido del libro (para la prueba automática). */
static void dump(void)
{
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        registrar("macaco: hoja %s %dx%d", fo->nome, fo->linhas, fo->colunas);
        for (int l = 0; l < fo->linhas; l++)
            for (int c = 0; c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!ce->texto) continue;
                char ref[16], val[32];
                nome_ref(ref, sizeof ref, l, c);
                formatar_numero(val, sizeof val, ce->valor);
                if (ce->flags & CEL_FORMULA)
                    registrar("macaco: celula %s!%s = %s [%s]", fo->nome, ref, val, ce->texto);
                else if (ce->flags & CEL_TEXTO)
                    registrar("macaco: celula %s!%s = %s", fo->nome, ref, ce->texto);
                else
                    registrar("macaco: celula %s!%s = %s", fo->nome, ref, val);
            }
        /* los formatos, para la prueba */
        for (int l = 0; l < fo->linhas; l++)
            for (int c = 0; c < fo->colunas; c++) {
                celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
                if (!ce->fmt) continue;
                const fmt_t *ft = fmt_de(ce);
                char ref[16], vis[64];
                nome_ref(ref, sizeof ref, l, c);
                celula_exibida(vis, sizeof vis, ce);
                registrar("macaco: formato %s!%s nf=%d dec=%d al=%d est=%d tam=%d bor=%d cor=%x rel=%x vis=[%s]",
                          fo->nome, ref, ft->nf, ft->dec, ft->al, ft->est, ft->tam ? ft->tam : 10, ft->bor,
                          (unsigned)ft->cor, (unsigned)ft->rel, vis);
            }
    }
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    memset(&livro, 0, sizeof livro);
    livro.n_fmts = 1;
    /* El modo de prueba: carga el fichero y escribe las celdas en el registro. */
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 't') {
        if (argc < 3) { registrar("macaco: modo -t sin fichero"); return 1; }
        if (!carregar_ficheiro(argv[2])) {
            registrar("macaco: no se ha leído %s", argv[2]);
            return 1;
        }
        dump();
        registrar("macaco: pronto");
        return 0;
    }
    if (argc >= 4 && compara_texto(argv[1], "--convertir") == 0) return modo_converter(argv[2], argv[3]);

    folha_nova("Hoja1");
    if (argc > 1) {
        if (carregar_ficheiro(argv[1])) {
            copia_texto(caminho, argv[1], sizeof caminho);
            por_aqui(estado, sizeof estado, "Archivo cargado");
        } else {
            char m[200];
            formatar_texto(m, sizeof m, "No se ha podido abrir\n%s", argv[1]);
            mensagem("Macaco", m);
        }
    }

    jan = jan_abrir("Macaco - Libro1", 900, 600);
    if (!jan) { registrar("macaco: no se ha abierto la ventana"); return 1; }
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 640, 360);
    itens_iniciar();
    layout();
    por_aqui(estado, sizeof estado, "");
    registrar("macaco: pronto");
    desenhar_tudo();

    evento_t e;
    while (!sair_pedido) {
        int r = esperar_evento(jan->id, &e, 20);
        if (r == 1) {
            if (e.tipo == EV_FECHAR) {
                if (!modificado || confirmar("Macaco", "El libro tiene cambios sin guardar.\n¿Salir sin guardar?")) break;
                continue;
            }
            tratar_evento(&e);
            desenhar_tudo();
        }
        /* La ventana del gráfico, si está abierta. */
        if (jan_graf) {
            evento_t ge;
            while (proximo_evento(jan_graf->id, &ge, false) == 1) {
                if (ge.tipo == EV_FECHAR) {
                    jan_fechar(jan_graf);
                    jan_graf = NULL;
                    registrar("macaco: cerrada la ventana del gráfico");
                    break;
                }
                if (ge.tipo == EV_REDIMENSIONAR) { jan_redimensionar(jan_graf, &ge); grafico_desenhar(); }
            }
        }
    }
    registrar("macaco: cerrado");
    if (jan_graf) jan_fechar(jan_graf);
    jan_fechar(jan);
    return 0;
}

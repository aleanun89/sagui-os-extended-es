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
#define MAC_BARRA      32    /* barra de botones */
#define MAC_FORMULA    26    /* barra de fórmulas */
#define MAC_CABECALH   20    /* cabecera de las columnas */
#define MAC_RODAPE     22    /* barra de abajo (separadores de hoja) */
#define MAC_LARGC      46    /* ancho de la franja de los números de fila */
#define MAC_ALTLINHA   20    /* alto de una fila */
#define MAC_LARGCOL    72    /* ancho de una columna, por omisión */

/* ----tipos de gráfico ---- */
#define GRAF_BARRAS      0   /* barras verticales */
#define GRAF_BARRAS_H    1   /* barras horizontales */
#define GRAF_TARTA       2   /* tarta (reparto por porcentaje) */

/* ------------------------------------------------------------ estructura */

typedef struct {
    int32_t valor;      /* el número, en centésimas */
    uint8_t flags;      /* CEL_* */
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
    int    linhas, colunas;
    char **conteudos;
    bool   vazio;
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
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
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
    if (!copia.conteudos) { copia.vazio = true; return; }
    for (int l = 0; l < copia.linhas; l++)
        for (int c = 0; c < copia.colunas; c++) {
            celula_t *ce = celula_de(fo, l1 + l, c1 + c);
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

static bool carregar_ficheiro(const char *nome_ficheiro)
{
    int tam = 0;
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

/* ------------------------------------------------------------ dibujo */

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

/* El texto que se ve en la celda. */
static void celula_exibida(char *dst, int max, celula_t *ce)
{
    if (!ce) { dst[0] = 0; return; }
    if (ce->flags & CEL_TEXTO) {
        copia_texto(dst, ce->texto ? ce->texto : "", (size_t)max);
        return;
    }
    if (ce->flags & CEL_NUMERO) { formatar_numero(dst, max, ce->valor); return; }
    dst[0] = 0;
}

/* El contenido de la celda (la fórmula o el literal), para la barra de
 * fórmulas. */
static void celula_conteudo(char *dst, int max, celula_t *ce)
{
    if (!ce) { dst[0] = 0; return; }
    copia_texto(dst, ce->texto ? ce->texto : "", (size_t)max);
}

#define MAC_QTD_BT 6
static botao_t bt[MAC_QTD_BT];
static const char *const ROT_BT[MAC_QTD_BT] = { "Nuevo", "Abrir", "Guardar", "Deshacer", "Gráfico", "Opciones" };

static void desenhar_barra(void)
{
    g_retangulo(jan, 0, 0, jan->larg, MAC_BARRA, 0xE6E9EC);
    g_retangulo(jan, 0, MAC_BARRA - 1, jan->larg, 1, COR_BORDA);
    int x = 8;
    for (int i = 0; i < MAC_QTD_BT; i++) {
        int l = g_largura_texto(ROT_BT[i]) + 20;
        bt[i].x = x; bt[i].y = 6; bt[i].l = l; bt[i].a = 22;
        bt[i].texto = ROT_BT[i];
        /* OJO: el estado "apertado" no se toca aquí: lo pone el clic y lo
         * quita el soltar, y si se pusiera a false en cada dibujo el botón
         * nunca elegiría su acción. */
        botao_desenhar(jan, &bt[i]);
        x += l + 4;
    }
    char dica[32];
    formatar_texto(dica, sizeof dica, "Ctrl+S guarda");
    if (x + 16 < jan->larg - 12 - g_largura_texto(dica))
        g_texto(jan, jan->larg - 12 - g_largura_texto(dica), 9, dica, COR_TEXTO_FRACO);
}

static void desenhar_formula(void)
{
    int y = MAC_BARRA;
    g_retangulo(jan, 0, y, jan->larg, MAC_FORMULA, COR_FUNDO);
    g_retangulo(jan, 0, y + MAC_FORMULA - 1, jan->larg, 1, COR_BORDA);

    /* La caja del nombre (la referencia de la celda activa). */
    char ref[16];
    nome_ref(ref, sizeof ref, cel_l, cel_c);
    g_retangulo(jan, 6, y + 4, 58, MAC_FORMULA - 10, COR_BRANCO);
    g_moldura(jan, 6, y + 4, 58, MAC_FORMULA - 10, COR_BORDA);
    g_texto(jan, 12, y + 6, ref, COR_TEXTO);

    /* La caja del contenido. */
    int cx = 72, cw = jan->larg - cx - 8;
    if (cw < 20) cw = 20;
    g_retangulo(jan, cx, y + 4, cw, MAC_FORMULA - 10, COR_BRANCO);
    g_moldura(jan, cx, y + 4, cw, MAC_FORMULA - 10, COR_BORDA);
    char texto[MAC_TEXTO_CEL];
    if (editando) copia_texto(texto, edit_buf, sizeof texto);
    else celula_conteudo(texto, sizeof texto, celula_de(folha_ativa(), cel_l, cel_c));
    int maxc = (cw - 8) / LETRA_L;
    if (maxc > MAC_TEXTO_CEL - 1) maxc = MAC_TEXTO_CEL - 1;
    g_texto_n(jan, cx + 4, y + 6, texto, maxc, COR_TEXTO);
    if (editando) {
        int cx2 = cx + 4 + edit_cur * LETRA_L;
        g_retangulo(jan, cx2, y + 6, 1, LETRA_A, COR_SELECAO);
    }
}

static void desenhar_grelha(void)
{
    folha_t *fo = folha_ativa();
    int gy = grelha_y(), ga = grelha_a();
    int gl = jan->larg - MAC_LARGC;
    calcular_offsets();

    g_retangulo(jan, MAC_LARGC, gy, gl, ga, COR_BRANCO);

    /* El fondo de la selección. */
    int l1, c1, l2, c2;
    intervalo_selecionado(&l1, &c1, &l2, &c2);
    int sx = x_coluna(c1);
    int sw = off_col[c2 + 1] - off_col[c1];
    int sy = y_linha(l1);
    int sh = (l2 - l1 + 1) * MAC_ALTLINHA;
    if (sw > 0 && sh > 0) {
        int y0 = sy, y1 = sy + sh;
        if (y0 < gy) y0 = gy;
        if (y1 > gy + ga) y1 = gy + ga;
        if (y1 > y0) g_retangulo(jan, sx, y0, sw, y1 - y0, 0xDCE9F9);
    }

    /* Las líneas de la cuadrícula. */
    for (int c = 0; c <= fo->colunas; c++) {
        int x = x_coluna(c);
        if (x < MAC_LARGC || x > jan->larg) continue;
        g_linha(jan, x, gy, x, gy + ga, 0xD8DDE3);
    }
    for (int l = 0; l <= fo->linhas; l++) {
        int y = y_linha(l);
        if (y < gy || y > gy + ga) continue;
        g_linha(jan, MAC_LARGC, y, jan->larg, y, 0xD8DDE3);
    }

    /* Los valores. */
    for (int l = 0; l < fo->linhas; l++) {
        int y = y_linha(l);
        if (y + MAC_ALTLINHA <= gy) continue;
        if (y >= gy + ga) break;
        for (int c = 0; c < fo->colunas; c++) {
            int x = x_coluna(c);
            int lcol = fo->larg_col[c];
            if (x + lcol <= MAC_LARGC) continue;
            if (x >= jan->larg) break;
            celula_t *ce = &fo->celulas[(size_t)l * fo->colunas + c];
            char t[MAC_TEXTO_CEL];
            celula_exibida(t, sizeof t, ce);
            if (!t[0]) continue;
            bool numero = (ce->flags & CEL_NUMERO) && !(ce->flags & CEL_TEXTO);
            int maxc = (lcol - 8) / LETRA_L;
            if (maxc > MAC_TEXTO_CEL - 1) maxc = MAC_TEXTO_CEL - 1;
            int tx = numero ? (x + lcol - 4 - g_largura_texto(t)) : (x + 4);
            g_texto_n(jan, tx, y + (MAC_ALTLINHA - LETRA_A) / 2, t, maxc, COR_TEXTO);
        }
    }

    /* El marco de la selección y de la celda activa. */
    if (sw > 0 && sh > 0) {
        int y0 = sy, y1 = sy + sh;
        if (y0 < gy) y0 = gy;
        if (y1 > gy + ga) y1 = gy + ga;
        g_moldura(jan, sx, y0, sw, y1 - y0, COR_SELECAO);
    }
    int ax = x_coluna(cel_c), ay = y_linha(cel_l);
    if (ax + fo->larg_col[cel_c] > MAC_LARGC && ax < jan->larg &&
        ay + MAC_ALTLINHA > gy && ay < gy + ga) {
        g_moldura(jan, ax + 1, ay + 1, fo->larg_col[cel_c] - 2, MAC_ALTLINHA - 2, COR_SELECAO);
        g_moldura(jan, ax + 2, ay + 2, fo->larg_col[cel_c] - 4, MAC_ALTLINHA - 4, COR_SELECAO);
    }

    /* La cabecera de las columnas. */
    int hy = gy - MAC_CABECALH;
    g_retangulo(jan, 0, hy, jan->larg, MAC_CABECALH, 0xE6E9EC);
    g_retangulo(jan, 0, hy + MAC_CABECALH - 1, jan->larg, 1, COR_BORDA);
    for (int c = 0; c < fo->colunas; c++) {
        int x = x_coluna(c), lcol = fo->larg_col[c];
        if (x + lcol <= MAC_LARGC) continue;
        if (x >= jan->larg) break;
        char nome[8];
        nome_coluna(nome, sizeof nome, c);
        g_texto(jan, x + (lcol - g_largura_texto(nome)) / 2, hy + 2, nome, COR_TEXTO);
        if (c == cel_c) g_retangulo(jan, x, hy, lcol, MAC_CABECALH - 1, 0xD2D8DE);
    }
    g_retangulo(jan, 0, hy, MAC_LARGC, MAC_CABECALH, 0xD2D8DE);

    /* La cabecera de las filas. */
    g_retangulo(jan, 0, gy, MAC_LARGC, ga, 0xE6E9EC);
    for (int l = 0; l < fo->linhas; l++) {
        int y = y_linha(l);
        if (y + MAC_ALTLINHA <= gy) continue;
        if (y >= gy + ga) break;
        g_retangulo(jan, 0, y, MAC_LARGC, MAC_ALTLINHA, 0xE6E9EC);
        char n[8];
        formatar_texto(n, sizeof n, "%d", l + 1);
        g_texto(jan, (MAC_LARGC - g_largura_texto(n)) / 2, y + (MAC_ALTLINHA - LETRA_A) / 2, n, COR_TEXTO);
        if (l == cel_l) g_retangulo(jan, 0, y, MAC_LARGC, MAC_ALTLINHA, 0xD2D8DE);
    }
    g_linha(jan, MAC_LARGC - 1, gy, MAC_LARGC - 1, gy + ga, COR_BORDA);
}

static void desenhar_rodape(void)
{
    int y = jan->alt - MAC_RODAPE;
    g_retangulo(jan, 0, y, jan->larg, MAC_RODAPE, 0xE6E9EC);
    g_retangulo(jan, 0, y, jan->larg, 1, COR_BORDA);
    /* Los separadores de las hojas. */
    int x = 4;
    for (int f = 0; f < livro.n; f++) {
        int l = g_largura_texto(livro.folha[f].nome) + 20;
        if (x + l > jan->larg - 120) break;
        bool ativa = f == livro.ativa;
        g_retangulo(jan, x, y + 2, l, MAC_RODAPE - 4, ativa ? COR_BRANCO : 0xD2D8DE);
        g_moldura(jan, x, y + 2, l, MAC_RODAPE - 4, COR_BORDA);
        g_texto(jan, x + 8, y + 4, livro.folha[f].nome, ativa ? COR_TEXTO : COR_TEXTO_FRACO);
        x += l + 2;
    }
    /* El botón de hoja nueva. */
    if (x + 24 < jan->larg - 120) {
        g_retangulo(jan, x, y + 2, 24, MAC_RODAPE - 4, 0xD2D8DE);
        g_moldura(jan, x, y + 2, 24, MAC_RODAPE - 4, COR_BORDA);
        g_texto(jan, x + 8, y + 4, "+", COR_TEXTO);
    }
    /* El mensaje. */
    if (estado[0]) {
        int tx = jan->larg - 8 - g_largura_texto(estado);
        if (tx > x + 30) g_texto(jan, tx, y + 4, estado, COR_TEXTO_FRACO);
    }
}

static void desenhar_tudo(void)
{
    g_retangulo(jan, 0, 0, jan->larg, jan->alt, COR_FUNDO);
    desenhar_barra();
    desenhar_formula();
    desenhar_grelha();
    desenhar_rodape();
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
    if (extender) {
        if (anc_l != cel_l || anc_c != cel_c) { anc_l = cel_l; anc_c = cel_c; }
    } else {
        anc_l = l;
        anc_c = c;
    }
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

static bool abrir_ficheiro(void)
{
    char c[256];
    copia_texto(c, caminho[0] ? caminho : "/tmp/livro.mxs", sizeof c);
    if (!pedir_texto("Abrir", "Archivo .mxs (por ejemplo /tmp/livro.mxs):", c, sizeof c) || !c[0])
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
    if (!pedir_texto("Guardar como", "Guardar en (por ejemplo /tmp/livro.mxs):", c, sizeof c) || !c[0])
        return false;
    copia_texto(caminho, c, sizeof caminho);
    return guardar();
}

static void novo_livro(void)
{
    for (int f = 0; f < livro.n; f++) {
        folha_t *fo = &livro.folha[f];
        for (int k = 0; k < fo->linhas * fo->colunas; k++)
            if (fo->celulas[k].texto) { libera(fo->celulas[k].texto); fo->celulas[k].texto = NULL; }
        if (fo->celulas) { libera(fo->celulas); fo->celulas = NULL; }
    }
    memset(&livro, 0, sizeof livro);
    desfazer_limpar();
    copia_limpar();
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
    formatar_texto(nome, sizeof nome, "Hoja%d", livro.n + 1);
    folha_nova(nome);
    cel_l = cel_c = anc_l = anc_c = 0;
    desl_x = desl_y = 0;
    modificado = true;
    registrar("macaco: nueva hoja %s", nome);
}

/* El menú con todas las opciones. */
static void abrir_menu(int x, int y)
{
    char itens[1400];
    bool sel = (anc_l != cel_l || anc_c != cel_c);
    bool temcopia = !copia.vazio;
    formatar_texto(itens, sizeof itens,
        "Nuevo\n"
        "Abrir...\n"
        "Guardar\tCtrl+S\n"
        "Guardar como...\n"
        "-\n"
        "%sDeshacer\tCtrl+Z\n"
        "-\n"
        "%sCopiar\tCtrl+C\n"
        "%sCortar\tCtrl+X\n"
        "%sPegar\tCtrl+V\n"
        "-\n"
        "Insertar línea\n"
        "Borrar línea\n"
        "Insertar columna\n"
        "Borrar columna\n"
        "-\n"
        "Ordenar (A-Z)\n"
        "Ordenar (Z-A)\n"
        "-\n"
        "Ancho de las columnas...\n"
        "-\n"
        "Gráfico de barras\n"
        "Gráfico de barras horizontales\n"
        "Gráfico de tarta\n"
        "Nueva hoja",
        n_desfazer ? "" : "~", sel ? "" : "~", sel ? "" : "~", temcopia ? "" : "~");
    menu_contextual(jan->id, x, y, itens, 1);
}

static void executar_menu(int linha)
{
    switch (linha) {
    case 0: novo_livro(); break;
    case 1: abrir_ficheiro(); break;
    case 2: guardar(); break;
    case 3: guardar_como(); break;
    case 5:
        if (desfazer_refazer()) { modificado = true; recalcular(); por_aqui(estado, sizeof estado, "Deshecho"); }
        else por_aqui(estado, sizeof estado, "Nada para deshacer");
        break;
    case 7: copiar_intervalo(); break;
    case 8: {
        copiar_intervalo();
        /* Cortar: copia y luego borra. */
        folha_t *fo = folha_ativa();
        int l1, c1, l2, c2;
        intervalo_selecionado(&l1, &c1, &l2, &c2);
        desfazer_guardar_folha(livro.ativa);
        for (int l = l1; l <= l2; l++)
            for (int c = c1; c <= c2; c++) celula_definir(fo, l, c, "");
        modificado = true;
        recalcular();
        registrar("macaco: cortadas las celdas");
        break;
    }
    case 9: colar_intervalo(); break;
    case 11: inserir_linha(); break;
    case 12: apagar_linha(); break;
    case 13: inserir_coluna(); break;
    case 14: apagar_coluna(); break;
    case 16: ordenar_intervalo(true); break;
    case 17: ordenar_intervalo(false); break;
    case 19: ajustar_larguras(); break;
    case 21: abrir_grafico(GRAF_BARRAS); break;
    case 22: abrir_grafico(GRAF_BARRAS_H); break;
    case 23: abrir_grafico(GRAF_TARTA); break;
    case 24: nova_folha(); break;
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

static void tratar_tecla(const evento_t *e)
{
    unsigned t = e->tecla;
    int mod = (int)e->modificadores;
    bool ctrl = (mod & 2) != 0;
    bool shift = (mod & 1) != 0;

    if (editando) { tratar_tecla_edicao(e); return; }

    if (ctrl) {
        switch (t) {
        case 19: if (shift) guardar_como(); else guardar(); return;      /* Ctrl+S */
        case 15: abrir_ficheiro(); return;                               /* Ctrl+O */
        case 26:
            if (desfazer_refazer()) { modificado = true; recalcular(); por_aqui(estado, sizeof estado, "Deshecho"); }
            else por_aqui(estado, sizeof estado, "Nada para deshacer");
            return;                                                      /* Ctrl+Z */
        case 3: copiar_intervalo(); return;                              /* Ctrl+C */
        case 22: colar_intervalo(); return;                              /* Ctrl+V */
        case 24: executar_menu(8); return;                               /* Ctrl+X */
        default: break;
        }
    }

    switch (t) {
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
        mover_para(cel_l, achou ? c : cel_c, shift);
        return;
    }
    case TECLA_PGUP: mover_para(cel_l - (grelha_a() / MAC_ALTLINHA), cel_c, shift); return;
    case TECLA_PGDN: mover_para(cel_l + (grelha_a() / MAC_ALTLINHA), cel_c, shift); return;
    case TECLA_DELETE:
    case '\b': {
        folha_t *fo = folha_ativa();
        int l1, c1, l2, c2;
        intervalo_selecionado(&l1, &c1, &l2, &c2);
        desfazer_guardar_folha(livro.ativa);
        for (int l = l1; l <= l2; l++)
            for (int c = c1; c <= c2; c++) celula_definir(fo, l, c, "");
        modificado = true;
        recalcular();
        registrar("macaco: borradas las celdas");
        return;
    }
    case '\n': comecar_edicao(NULL); return;
    case '\t': mover_para(cel_l, cel_c + 1, false); return;
    default: break;
    }

    /* Una tecla normal: empieza a escribir. */
    if (t >= 0x20 && t < 0x100 && t != 0x7F) {
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

static void clique_cabecalho(int mx, int my)
{
    folha_t *fo = folha_ativa();
    int hy = grelha_y() - MAC_CABECALH;
    if (my < hy || my >= grelha_y()) return;
    if (mx >= MAC_LARGC) {
        /* Las columnas. */
        int c = 0;
        int x = MAC_LARGC - desl_x;
        while (c < fo->colunas && mx >= x + fo->larg_col[c]) { x += fo->larg_col[c]; c++; }
        if (c < fo->colunas) {
            cel_c = c;
            anc_c = c;
            cel_l = 0;
            anc_l = fo->linhas - 1;
            assure_visivel();
        }
        return;
    }
    /* Las filas. */
    int l = (my - hy + desl_y) / MAC_ALTLINHA;
    if (l < 0) l = 0;
    if (l >= fo->linhas) l = fo->linhas - 1;
    cel_l = l;
    anc_l = l;
    cel_c = 0;
    anc_c = fo->colunas - 1;
}

static void clique_rodape(int mx, int my)
{
    int y = jan->alt - MAC_RODAPE;
    if (my < y) return;
    int x = 4;
    for (int f = 0; f < livro.n; f++) {
        int l = g_largura_texto(livro.folha[f].nome) + 20;
        if (mx >= x && mx < x + l) {
            livro.ativa = f;
            cel_l = cel_c = anc_l = anc_c = 0;
            desl_x = desl_y = 0;
            registrar("macaco: hoja %s", livro.folha[f].nome);
            return;
        }
        x += l + 2;
    }
    if (mx >= x && mx < x + 24) nova_folha();
}

static void tratar_evento(const evento_t *e)
{
    /* Los botones se miran siempre: el clic los aprieta y el soltar dentro
     * es lo que elige la acción. */
    for (int i = 0; i < MAC_QTD_BT; i++)
        if (botao_tratar(jan, &bt[i], e)) {
            switch (i) {
            case 0: novo_livro(); break;
            case 1: abrir_ficheiro(); break;
            case 2: guardar(); break;
            case 3:
                if (desfazer_refazer()) { modificado = true; recalcular(); por_aqui(estado, sizeof estado, "Deshecho"); }
                else por_aqui(estado, sizeof estado, "Nada para deshacer");
                break;
            case 4: abrir_grafico(GRAF_BARRAS); break;
            case 5: abrir_menu(bt[i].x, bt[i].y + bt[i].a + 2); break;
            }
            return;
        }

    if (e->tipo == EV_FECHAR) return;
    if (e->tipo == EV_REDIMENSIONAR) { jan_redimensionar(jan, e); return; }

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
        /* El botón derecho en la cuadrícula abre el menú. Si el clic cae
         * dentro de la selección, la deja como estaba (como en Excel). */
        if ((e->botoes & 2) && e->y >= grelha_y() && e->y < grelha_y() + grelha_a()) {
            int l, c;
            celula_em(e->x, e->y, &l, &c);
            int l1, c1, l2, c2;
            intervalo_selecionado(&l1, &c1, &l2, &c2);
            if (l < l1 || l > l2 || c < c1 || c > c2) {
                cel_l = l;
                cel_c = c;
                anc_l = l;
                anc_c = c;
                assure_visivel();
            }
            abrir_menu(e->x, e->y);
            return;
        }
        if (e->y < MAC_BARRA) return;
        if (e->y < MAC_BARRA + MAC_FORMULA) {
            /* Clic en la barra de fórmulas: empieza a escribir. */
            comecar_edicao(NULL);
            return;
        }
        if (e->y < grelha_y()) { clique_cabecalho(e->x, e->y); return; }
        if (e->y >= jan->alt - MAC_RODAPE) { clique_rodape(e->x, e->y); return; }
        if (e->botoes & 1) {
            clique_grelha(e->x, e->y);
            arrastrando = true;
        }
        return;
    }

    if (e->tipo == EV_MOVER && arrastrando && (e->botoes & 1)) {
        arrastrar_para(e->x, e->y);
        return;
    }
    if (e->tipo == EV_SOLTOU) {
        if (arrastrando) assure_visivel();
        arrastrando = false;
        return;
    }
    if (e->tipo == EV_TECLA) { tratar_tecla(e); return; }
    if (e->tipo == EV_MENU) {
        if (e->x >= 0) executar_menu(e->x);
        return;
    }
}

/* ------------------------------------------------------------ modo de prueba */

/* Escribe en el registro el contenido del libro (para la prueba
 * automática). */
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
    }
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    /* El modo de prueba: carga el fichero y escribe las celdas en el
     * registro. */
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 't') {
        memset(&livro, 0, sizeof livro);
        if (argc < 3) { registrar("macaco: modo -t sin fichero"); return 1; }
        if (!carregar_ficheiro(argv[2])) {
            registrar("macaco: no se ha leído %s", argv[2]);
            return 1;
        }
        dump();
        registrar("macaco: pronto");
        return 0;
    }

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

    jan = jan_abrir("Macaco - sin nombre", 820, 560);
    if (!jan) { registrar("macaco: no se ha abierto la ventana"); return 1; }
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 520, 320);
    por_aqui(estado, sizeof estado, "Listo");
    registrar("macaco: pronto");
    desenhar_tudo();

    evento_t e;
    for (;;) {
        int r = esperar_evento(jan->id, &e, 20);
        if (r == 1) {
            if (e.tipo == EV_FECHAR) break;
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

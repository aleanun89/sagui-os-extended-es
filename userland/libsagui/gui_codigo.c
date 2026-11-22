// SPDX-License-Identifier: GPL-3.0-only
/* gui_codigo.c - editor de código con resaltado por tabla de reglas (D454).
 *
 * Dos piezas: (1) el resaltador, un escáner de una línea guiado por una tabla (edrealce_regras_t: comentarios, cadenas,
 * palabras clave, tipos, funciones y constantes, números, preprocesador) con un «estado» que atraviesa líneas (comentario
 * de bloque, cadena larga); las tablas incluidas son Lua y C. (2) El componente codigo_t: texto Latin-1 con números de
 * línea, una línea marcada como error, buscar (Ctrl+F: incremental, Intro siguiente, Mayús+Intro anterior), sangría
 * automática (conserva la de la línea, la aumenta tras then/do/else/function.../{ y la reduce al escribir end/else/until/}),
 * selección con ratón y teclado, portapapeles y deshacer/rehacer (Ctrl+Z, Ctrl+Y) con los tecleos seguidos fundidos en uno.
 * Mico (el editor de terminal) conserva su resaltador propio: este es el de los programas con ventana. */
#include "gui.h"

/* ------------------------------------------------------------ tablas de reglas */

static const char *const lua_chaves[] = { "and", "break", "do", "else", "elseif", "end", "for", "function", "goto", "if", "in",
    "local", "not", "or", "repeat", "return", "then", "until", "while", NULL };
static const char *const lua_tipos[] = { "table", "string", "math", "os", "io", "coroutine", "debug", "utf8", NULL };
static const char *const lua_funcoes[] = { "print", "pairs", "ipairs", "type", "tostring", "tonumber", "require", "pcall",
    "xpcall", "error", "assert", "setmetatable", "getmetatable", "select", "next", "rawget", "rawset", "rawequal", "rawlen",
    "load", "dofile", "unpack", "collectgarbage", NULL };
static const char *const lua_consts[] = { "nil", "true", "false", "self", NULL };

const edrealce_regras_t edrealce_lua = {
    .nome = "Lua", .linha_com = "--", .bloco_ini = "--[[", .bloco_fim = "]]", .cadeia_ini = "[[", .cadeia_fim = "]]",
    .aspas = "\"'", .escapes = true, .sem_caixa = false, .preproc = 0,
    .chaves = lua_chaves, .tipos = lua_tipos, .funcoes = lua_funcoes, .constantes = lua_consts,
};

static const char *const c_chaves[] = { "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
    "return", "goto", "sizeof", "typedef", "struct", "union", "enum", "static", "extern", "const", "volatile", "register",
    "inline", "auto", "restrict", NULL };
static const char *const c_tipos[] = { "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "bool",
    "_Bool", "size_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "FILE", NULL };
static const char *const c_consts[] = { "true", "false", "NULL", NULL };
static const char *const sem_funcoes[] = { NULL };

const edrealce_regras_t edrealce_c = {
    .nome = "C", .linha_com = "//", .bloco_ini = "/*", .bloco_fim = "*/", .cadeia_ini = NULL, .cadeia_fim = NULL,
    .aspas = "\"'", .escapes = true, .sem_caixa = false, .preproc = '#',
    .chaves = c_chaves, .tipos = c_tipos, .funcoes = sem_funcoes, .constantes = c_consts,
};

uint32_t edrealce_cor(int classe)
{
    bool escuro = sagui_esq.escuro;
    switch (classe) {
    case RC_CHAVE:      return escuro ? 0xC792EA : 0x7A2E9B;
    case RC_TIPO:       return escuro ? 0x4EC9B0 : 0x0B7A8F;
    case RC_CADEIA:     return escuro ? 0xE6A06A : 0xB5472A;
    case RC_COMENTARIO: return escuro ? 0x7F9A7F : 0x5E8060;
    case RC_NUMERO:     return escuro ? 0xF2C26B : 0xA85F00;
    case RC_FUNCAO:     return escuro ? 0x6CB6FF : 0x1A5FB4;
    case RC_OPERADOR:   return escuro ? 0xC0C8D0 : 0x4A5560;
    case RC_PREPROC:    return escuro ? 0xD7BA7D : 0x8A6D00;
    case RC_CONSTANTE:  return escuro ? 0xEE82C0 : 0xA4358F;
    default:            return mod_tema.texto;
    }
}

static bool eh_letra(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0xC0; }
static bool eh_digito(char c) { return c >= '0' && c <= '9'; }
static bool eh_alnum(char c) { return eh_letra(c) || eh_digito(c); }

static bool na_lista(const char *const *l, const char *s, int n, bool sem_caixa)
{
    for (; *l; l++) {
        int k = 0;
        while (k < n && (*l)[k]) {
            char a = s[k], b = (*l)[k];
            if (sem_caixa && a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (a != b) break;
            k++;
        }
        if (k == n && !(*l)[k]) return true;
    }
    return false;
}

static bool comeca_em(const char *s, int n, int i, const char *p)
{
    if (!p) return false;
    int k = 0;
    while (p[k]) { if (i + k >= n || s[i + k] != p[k]) return false; k++; }
    return k > 0;
}

static void pintar(unsigned char *cl, int a, int b, int c)
{
    if (cl) for (int i = a; i < b; i++) cl[i] = (unsigned char)c;
}

/* Busca «fim» desde i; devuelve la posición después de él, o -1. */
static int achar_fim(const char *s, int n, int i, const char *fim)
{
    for (; i < n; i++) if (comeca_em(s, n, i, fim)) return i + (int)tam_texto(fim);
    return -1;
}

int edrealce_linha(const edrealce_regras_t *r, const char *s, int n, int estado, unsigned char *cl)
{
    if (cl) memset(cl, RC_NORMAL, (size_t)n);
    if (!r) return 0;
    int i = 0;
    if (estado == 1 || estado == 2) {
        int k = achar_fim(s, n, 0, estado == 1 ? r->bloco_fim : r->cadeia_fim);
        if (k < 0) { pintar(cl, 0, n, estado == 1 ? RC_COMENTARIO : RC_CADEIA); return estado; }
        pintar(cl, 0, k, estado == 1 ? RC_COMENTARIO : RC_CADEIA);
        i = k;
    }
    while (i < n) {
        char c = s[i];
        if (r->preproc && c == r->preproc) {
            bool ini = true;
            for (int k = 0; k < i; k++) if (s[k] != ' ' && s[k] != '\t') { ini = false; break; }
            if (ini) { pintar(cl, i, n, RC_PREPROC); return 0; }
        }
        if (comeca_em(s, n, i, r->bloco_ini)) {
            int k = achar_fim(s, n, i + (int)tam_texto(r->bloco_ini), r->bloco_fim);
            if (k < 0) { pintar(cl, i, n, RC_COMENTARIO); return 1; }
            pintar(cl, i, k, RC_COMENTARIO);
            i = k;
            continue;
        }
        if (comeca_em(s, n, i, r->linha_com)) { pintar(cl, i, n, RC_COMENTARIO); return 0; }
        if (comeca_em(s, n, i, r->cadeia_ini)) {
            int k = achar_fim(s, n, i + (int)tam_texto(r->cadeia_ini), r->cadeia_fim);
            if (k < 0) { pintar(cl, i, n, RC_CADEIA); return 2; }
            pintar(cl, i, k, RC_CADEIA);
            i = k;
            continue;
        }
        if (r->aspas && c && acha_char(r->aspas, c)) {
            int k = i + 1;
            for (; k < n; k++) {
                if (r->escapes && s[k] == '\\') { k++; continue; }
                if (s[k] == c) { k++; break; }
            }
            if (k > n) k = n;
            pintar(cl, i, k, RC_CADEIA);
            i = k;
            continue;
        }
        if (eh_digito(c) || (c == '.' && i + 1 < n && eh_digito(s[i + 1]) && (i == 0 || !eh_alnum(s[i - 1])))) {
            int k = i;
            if (k + 1 < n && s[k] == '0' && (s[k + 1] == 'x' || s[k + 1] == 'X' || s[k + 1] == 'b' || s[k + 1] == 'B')) k += 2;
            while (k < n && (eh_alnum(s[k]) || s[k] == '.')) {
                if ((s[k] == 'e' || s[k] == 'E') && k + 1 < n && (s[k + 1] == '+' || s[k + 1] == '-')) k++;
                k++;
            }
            pintar(cl, i, k, RC_NUMERO);
            i = k;
            continue;
        }
        if (eh_letra(c)) {
            int k = i;
            while (k < n && eh_alnum(s[k])) k++;
            int w = k - i;
            int cls = RC_NORMAL;
            if (r->chaves && na_lista(r->chaves, s + i, w, r->sem_caixa)) cls = RC_CHAVE;
            else if (r->constantes && na_lista(r->constantes, s + i, w, r->sem_caixa)) cls = RC_CONSTANTE;
            else if (r->tipos && na_lista(r->tipos, s + i, w, r->sem_caixa)) cls = RC_TIPO;
            else if (r->funcoes && na_lista(r->funcoes, s + i, w, r->sem_caixa)) cls = RC_FUNCAO;
            else {
                int q = k;
                while (q < n && (s[q] == ' ' || s[q] == '\t')) q++;
                if (q < n && s[q] == '(' && (i == 0 || s[i - 1] != '.')) cls = RC_FUNCAO;   /* llamada a función */
            }
            pintar(cl, i, k, cls);
            i = k;
            continue;
        }
        if (c && acha_char("+-*/%^<>=~&|!", c)) pintar(cl, i, i + 1, RC_OPERADOR);
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------ el componente */

#define CODIGO_MAX_BYTES (4 * 1024 * 1024)
#define CODIGO_FILA 16
#define BARRA_L 8

struct codigo {
    int x, y, l, a;
    char *buf; int tam, cap;
    int *inicio; unsigned char *estado; int nlin, cap_lin;
    int cursor, ancora;
    int topo, esq;
    int col_desejada;
    const edrealce_regras_t *regras;
    bool foco, modificado, arrastando;
    int sangria;
    int erro_linha; char erro_msg[80];
    bool buscando; char busca[40]; int bcursor; bool caso; bool nao_achou;
    bool quebrar;   /* el próximo tecleo no se funde con el anterior */
    pilha_desfazer_t pilha;
};

typedef struct {
    codigo_t *c;
    int pos, ndel, nins;
    char *del, *ins;
    int cur0, cur1;
    bool fundivel;
} edicao_t;

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

static void reindexar(codigo_t *c)
{
    int n = 1;
    for (int i = 0; i < c->tam; i++) if (c->buf[i] == '\n') n++;
    if (n > c->cap_lin) {
        int nc = n + 64;
        int *ni = realoca(c->inicio, (size_t)nc * sizeof *ni);
        unsigned char *ne = realoca(c->estado, (size_t)nc);
        if (ni) c->inicio = ni;
        if (ne) c->estado = ne;
        if (!ni || !ne) return;
        c->cap_lin = nc;
    }
    c->nlin = 0;
    c->inicio[c->nlin++] = 0;
    for (int i = 0; i < c->tam; i++) if (c->buf[i] == '\n') c->inicio[c->nlin++] = i + 1;
    int st = 0;
    for (int k = 0; k < c->nlin; k++) {
        c->estado[k] = (unsigned char)st;
        int ini = c->inicio[k], fim = k + 1 < c->nlin ? c->inicio[k + 1] - 1 : c->tam;
        st = c->regras ? edrealce_linha(c->regras, c->buf + ini, fim - ini, st, NULL) : 0;
    }
    if (c->cursor > c->tam) c->cursor = c->tam;
    if (c->ancora > c->tam) c->ancora = c->tam;
}

static int linha_de(const codigo_t *c, int pos)
{
    int a = 0, b = c->nlin - 1;
    while (a < b) {
        int m = (a + b + 1) / 2;
        if (c->inicio[m] <= pos) a = m; else b = m - 1;
    }
    return a;
}

static int fim_linha(const codigo_t *c, int k) { return k + 1 < c->nlin ? c->inicio[k + 1] - 1 : c->tam; }

codigo_t *codigo_criar(int x, int y, int l, int a)
{
    codigo_t *c = aloca_zerado(sizeof *c);
    if (!c) return NULL;
    c->x = x; c->y = y; c->l = l; c->a = a;
    c->cap = 1024;
    c->buf = aloca((size_t)c->cap);
    if (!c->buf) { libera(c); return NULL; }
    c->ancora = -1;
    c->sangria = 2;
    c->regras = &edrealce_lua;
    ed_desfazer_iniciar(&c->pilha, 500);
    reindexar(c);
    return c;
}

void codigo_liberar(codigo_t *c)
{
    if (!c) return;
    ed_desfazer_liberar(&c->pilha);
    libera(c->buf); libera(c->inicio); libera(c->estado);
    libera(c);
}

void codigo_mover(codigo_t *c, int x, int y, int l, int a) { c->x = x; c->y = y; c->l = l; c->a = a; }
void codigo_regras(codigo_t *c, const edrealce_regras_t *r) { c->regras = r; reindexar(c); }
void codigo_sangria(codigo_t *c, int espacos) { c->sangria = espacos < 1 ? 1 : espacos > 8 ? 8 : espacos; }
void codigo_foco(codigo_t *c, bool foco) { c->foco = foco; }
bool codigo_modificado(const codigo_t *c) { return c->modificado; }
int  codigo_linhas(const codigo_t *c) { return c->nlin; }
const char *codigo_texto(const codigo_t *c, int *n) { if (n) *n = c->tam; c->buf[c->tam < c->cap ? c->tam : c->cap - 1] = 0; return c->buf; }
int  codigo_linha(const codigo_t *c) { return linha_de(c, c->cursor) + 1; }
int  codigo_coluna(const codigo_t *c) { return c->cursor - c->inicio[linha_de(c, c->cursor)] + 1; }
const char *codigo_erro(const codigo_t *c, int *linha) { if (linha) *linha = c->erro_linha; return c->erro_linha ? c->erro_msg : NULL; }

static bool garantir(codigo_t *c, int extra)
{
    if (c->tam + extra + 1 <= c->cap) return true;
    int nc = c->cap;
    while (nc < c->tam + extra + 1) nc *= 2;
    if (nc > CODIGO_MAX_BYTES) return false;
    char *nb = realoca(c->buf, (size_t)nc);
    if (!nb) return false;
    c->buf = nb; c->cap = nc;
    return true;
}

/* Cambio en bruto (sin deshacer). */
static bool cod_substituir(codigo_t *c, int pos, int nrem, const char *txt, int nins)
{
    if (!garantir(c, nins > nrem ? nins - nrem : 0)) return false;
    memmove(c->buf + pos + nins, c->buf + pos + nrem, (size_t)(c->tam - pos - nrem));
    if (nins) memcpy(c->buf + pos, txt, (size_t)nins);
    c->tam += nins - nrem;
    c->modificado = true;
    reindexar(c);
    return true;
}

static void aplicar_edicao(void *d, bool refazer)
{
    edicao_t *e = d;
    codigo_t *c = e->c;
    if (refazer) { cod_substituir(c, e->pos, e->ndel, e->ins, e->nins); c->cursor = e->cur1; }
    else { cod_substituir(c, e->pos, e->nins, e->del, e->ndel); c->cursor = e->cur0; }
    c->ancora = -1;
    c->quebrar = true;
}

static void liberar_edicao(void *d)
{
    edicao_t *e = d;
    libera(e->del); libera(e->ins); libera(e);
}

/* Reemplaza [pos, pos + nrem) por txt, recuerda la edición y deja el cursor tras lo insertado. */
static bool editar(codigo_t *c, int pos, int nrem, const char *txt, int nins, bool fundivel)
{
    if (pos < 0 || pos + nrem > c->tam) return false;
    int cur0 = c->cursor;
    /* fusión de tecleos seguidos */
    edicao_t *t = ed_desfazer_topo(&c->pilha);
    if (fundivel && !c->quebrar && t && t->fundivel && t->ndel == 0 && nrem == 0 && pos == t->pos + t->nins && cur0 == pos) {
        char *ni = realoca(t->ins, (size_t)(t->nins + nins));
        if (ni) {
            memcpy(ni + t->nins, txt, (size_t)nins);
            t->ins = ni; t->nins += nins;
            if (!cod_substituir(c, pos, 0, txt, nins)) { t->nins -= nins; return false; }
            c->cursor = pos + nins;
            t->cur1 = c->cursor;
            return true;
        }
    }
    edicao_t *e = aloca_zerado(sizeof *e);
    if (!e) return false;
    e->c = c; e->pos = pos; e->ndel = nrem; e->nins = nins; e->cur0 = cur0; e->fundivel = fundivel;
    e->del = aloca((size_t)nrem + 1); e->ins = aloca((size_t)nins + 1);
    if (!e->del || !e->ins) { liberar_edicao(e); return false; }
    memcpy(e->del, c->buf + pos, (size_t)nrem);
    memcpy(e->ins, txt, (size_t)nins);
    if (!cod_substituir(c, pos, nrem, txt, nins)) { liberar_edicao(e); return false; }
    c->cursor = pos + nins;
    e->cur1 = c->cursor;
    ed_desfazer_registrar(&c->pilha, N_("Edición de texto"), aplicar_edicao, liberar_edicao, e);
    c->quebrar = false;
    return true;
}

void codigo_definir(codigo_t *c, const char *texto, int n)
{
    if (n < 0) n = (int)tam_texto(texto);
    ed_desfazer_limpar(&c->pilha);
    c->tam = 0;
    if (garantir(c, n)) { memcpy(c->buf, texto, (size_t)n); c->tam = n; }
    c->cursor = 0; c->ancora = -1; c->topo = c->esq = 0;
    c->erro_linha = 0;
    c->modificado = false;
    c->quebrar = true;
    reindexar(c);
}

void codigo_marcar_erro(codigo_t *c, int linha, const char *msg)
{
    c->erro_linha = linha > 0 && linha <= c->nlin ? linha : 0;
    copia_texto(c->erro_msg, msg ? msg : "", sizeof c->erro_msg);
}

/* ------------------------------------------------------------ selección y medidas */

static int sel_ini(const codigo_t *c) { return c->ancora < 0 ? c->cursor : (c->ancora < c->cursor ? c->ancora : c->cursor); }
static int sel_fim(const codigo_t *c) { return c->ancora < 0 ? c->cursor : (c->ancora < c->cursor ? c->cursor : c->ancora); }
static bool tem_selecao(const codigo_t *c) { return c->ancora >= 0 && c->ancora != c->cursor; }

int codigo_selecao(const codigo_t *c, char *dst, int max)
{
    int a = sel_ini(c), b = sel_fim(c);
    if (!tem_selecao(c)) { if (max > 0) dst[0] = 0; return 0; }
    int n = b - a < max - 1 ? b - a : max - 1;
    memcpy(dst, c->buf + a, (size_t)n);
    dst[n] = 0;
    return n;
}

static int calha(const codigo_t *c)
{
    int d = 3, n = c->nlin;
    while (n >= 10) { d++; n /= 10; }
    return d * LETRA_L + 16;
}

static int altura_busca(const codigo_t *c) { return c->buscando ? 26 : 0; }
static int linhas_caben(const codigo_t *c) { int v = (c->a - 2 - altura_busca(c)) / CODIGO_FILA; return v < 1 ? 1 : v; }
static int colunas_caben(const codigo_t *c) { int v = (c->l - calha(c) - BARRA_L - 4) / LETRA_L; return v < 1 ? 1 : v; }

static void tornar_visivel(codigo_t *c)
{
    int k = linha_de(c, c->cursor), col = c->cursor - c->inicio[k];
    if (k < c->topo) c->topo = k;
    if (k >= c->topo + linhas_caben(c)) c->topo = k - linhas_caben(c) + 1;
    if (col < c->esq) c->esq = col;
    if (col >= c->esq + colunas_caben(c)) c->esq = col - colunas_caben(c) + 1;
    int max = c->nlin - linhas_caben(c);
    if (max < 0) max = 0;
    if (c->topo > max) c->topo = max;
    if (c->topo < 0) c->topo = 0;
    if (c->esq < 0) c->esq = 0;
}

/* ------------------------------------------------------------ dibujo */

void codigo_desenhar(jan_t *j, codigo_t *c)
{
    int cal = calha(c), tx = c->x + cal, vis = linhas_caben(c), cols = colunas_caben(c);
    g_retangulo(j, c->x, c->y, c->l, c->a, mod_tema.campo);
    g_retangulo(j, c->x, c->y, cal - 4, c->a - altura_busca(c), mod_tema.barra);
    g_retangulo(j, c->x + cal - 4, c->y, 1, c->a - altura_busca(c), mod_tema.borde);
    int ck = linha_de(c, c->cursor), s0 = sel_ini(c), s1 = sel_fim(c);
    unsigned char cls[512];
    char tmp[513];
    for (int r = 0; r < vis; r++) {
        int k = c->topo + r;
        if (k >= c->nlin) break;
        int y = c->y + 1 + r * CODIGO_FILA;
        int ini = c->inicio[k], fim = fim_linha(c, k), n = fim - ini;
        bool erro = c->erro_linha == k + 1;
        if (erro) g_retangulo(j, tx - 3, y, c->l - cal - BARRA_L + 3, CODIGO_FILA, mod_tema.peligro_suave);
        else if (k == ck && c->foco && !tem_selecao(c)) g_retangulo(j, tx - 3, y, c->l - cal - BARRA_L + 3, CODIGO_FILA, mod_mezclar(mod_tema.campo, mod_tema.acento_suave, 160));
        /* selección */
        if (tem_selecao(c) && s1 > ini && s0 <= fim) {
            int a = (s0 > ini ? s0 : ini) - ini, b = (s1 < fim ? s1 : fim) - ini;
            if (s1 > fim) b++;   /* incluye el salto de línea */
            a -= c->esq; b -= c->esq;
            if (a < 0) a = 0;
            if (b > cols) b = cols;
            if (b > a) g_retangulo(j, tx + a * LETRA_L, y, (b - a) * LETRA_L, CODIGO_FILA, COR_SEL_SUAVE);
        }
        /* número de línea y marca de error */
        char num[12];
        formatar_texto(num, sizeof num, "%d", k + 1);
        g_texto(j, c->x + cal - 12 - (int)tam_texto(num) * LETRA_L, y, num, erro ? mod_tema.peligro : k == ck ? mod_tema.texto : mod_tema.texto_fraco);
        if (erro) g_retangulo_red(j, c->x + 4, y + 4, 8, 8, 4, mod_tema.peligro);
        /* texto por clases */
        int m = n < 512 ? n : 512;
        for (int i = 0; i < m; i++) tmp[i] = c->buf[ini + i] == '\t' ? ' ' : c->buf[ini + i];
        tmp[m] = 0;
        edrealce_linha(c->regras, c->buf + ini, m, c->estado[k], cls);
        if (!c->regras) memset(cls, 0, (size_t)m);
        int i = c->esq;
        while (i < m && i - c->esq < cols) {
            int e = i;
            while (e < m && cls[e] == cls[i] && e - c->esq < cols) e++;
            g_texto_n(j, tx + (i - c->esq) * LETRA_L, y, tmp + i, e - i, erro && cls[i] == RC_NORMAL ? mod_tema.peligro : edrealce_cor(cls[i]));
            i = e;
        }
        if (erro && c->erro_msg[0]) {   /* el mensaje, tras el texto */
            int mx = m - c->esq + 3;
            if (mx < 0) mx = 0;
            if (mx < cols) g_texto_n(j, tx + mx * LETRA_L, y, c->erro_msg, cols - mx, mod_tema.peligro);
        }
        if (c->foco && k == ck) {
            int cc = c->cursor - ini - c->esq;
            if (cc >= 0 && cc <= cols) g_retangulo(j, tx + cc * LETRA_L, y, 2, CODIGO_FILA, mod_tema.texto);
        }
    }
    /* barra de desplazamiento */
    if (c->nlin > vis) {
        int trilho = c->a - altura_busca(c) - 4, pol_a = trilho * vis / c->nlin;
        if (pol_a < 10) pol_a = 10;
        int pol_y = (trilho - pol_a) * c->topo / (c->nlin - vis);
        g_retangulo_red(j, c->x + c->l - BARRA_L + 1, c->y + 2 + pol_y, 5, pol_a, 2, sagui_esq.scroll_polegar);
    }
    /* barra de búsqueda */
    if (c->buscando) {
        int by = c->y + c->a - 26;
        g_retangulo(j, c->x, by, c->l, 26, mod_tema.barra);
        g_retangulo(j, c->x, by, c->l, 1, mod_tema.borde);
        const char *rot = idioma_traducir_sl(N_("Buscar"));
        g_texto(j, c->x + 6, by + 5, rot, mod_tema.texto);
        int fx = c->x + 12 + (int)tam_texto(rot) * LETRA_L;
        g_caixa_red(j, fx, by + 3, 200, 20, 3, mod_tema.campo, c->nao_achou ? mod_tema.peligro : mod_tema.foco);
        g_texto_n(j, fx + 4, by + 5, c->busca, 23, mod_tema.texto);
        g_retangulo(j, fx + 4 + c->bcursor * LETRA_L, by + 5, 1, 12, mod_tema.texto);
        const char *info = c->nao_achou ? idioma_traducir_sl(N_("sin resultados")) : idioma_traducir_sl(N_("Intro: siguiente · Esc: cerrar"));
        g_texto_n(j, fx + 208, by + 5, info, (c->l - (fx + 208 - c->x)) / LETRA_L, c->nao_achou ? mod_tema.peligro : mod_tema.texto_fraco);
    }
    g_moldura(j, c->x, c->y, c->l, c->a, c->foco ? mod_tema.foco : mod_tema.borde);
    jan_mostrar_area(j, c->x, c->y, c->l, c->a);
    GR_REG(j, "codigo", c, c->x, c->y, c->l, c->a, "", 0, 0);
}

/* ------------------------------------------------------------ buscar */

static bool igual_en(const codigo_t *c, int pos, const char *t, int n, bool caso)
{
    for (int k = 0; k < n; k++) {
        char a = c->buf[pos + k], b = t[k];
        if (!caso) { if (a >= 'A' && a <= 'Z') a = (char)(a + 32); if (b >= 'A' && b <= 'Z') b = (char)(b + 32); }
        if (a != b) return false;
    }
    return true;
}

bool codigo_buscar(codigo_t *c, const char *txt, bool adiante, bool caso)
{
    int n = (int)tam_texto(txt);
    if (n == 0 || n > c->tam) return false;
    int ini = adiante ? sel_fim(c) : sel_ini(c) - 1;
    for (int paso = 0; paso <= c->tam; paso++) {
        int p = adiante ? (ini + paso) % (c->tam + 1) : ((ini - paso) % (c->tam + 1) + c->tam + 1) % (c->tam + 1);
        if (p + n > c->tam) continue;
        if (igual_en(c, p, txt, n, caso)) {
            c->ancora = p; c->cursor = p + n;
            c->quebrar = true;
            tornar_visivel(c);
            return true;
        }
    }
    return false;
}

void codigo_ir_linha(codigo_t *c, int linha)
{
    if (linha < 1) linha = 1;
    if (linha > c->nlin) linha = c->nlin;
    c->cursor = c->inicio[linha - 1];
    c->ancora = -1;
    c->quebrar = true;
    int k = linha - 1;
    c->topo = k - linhas_caben(c) / 2;
    if (c->topo < 0) c->topo = 0;
    tornar_visivel(c);
}

bool codigo_desfazer(codigo_t *c)
{
    bool r = ed_desfazer(&c->pilha);
    if (r) tornar_visivel(c);
    return r;
}

bool codigo_refazer(codigo_t *c)
{
    bool r = ed_refazer(&c->pilha);
    if (r) tornar_visivel(c);
    return r;
}

/* ------------------------------------------------------------ sangría automática */

/* ¿La línea k termina abriendo un bloque? */
static bool abre_bloco(const codigo_t *c, int k, int ate)
{
    if (!c->regras) return false;
    int ini = c->inicio[k], n = ate - ini;
    if (n <= 0) return false;
    unsigned char cls[512];
    if (n > 512) return false;
    edrealce_linha(c->regras, c->buf + ini, n, c->estado[k], cls);
    int e = n - 1;
    while (e >= 0 && (c->buf[ini + e] == ' ' || c->buf[ini + e] == '\t')) e--;
    if (e < 0 || cls[e] == RC_COMENTARIO || cls[e] == RC_CADEIA) {
        if (e >= 0) while (e >= 0 && cls[e] == RC_COMENTARIO) e--;   /* un comentario final no cuenta */
        while (e >= 0 && (c->buf[ini + e] == ' ' || c->buf[ini + e] == '\t')) e--;
        if (e < 0 || cls[e] == RC_CADEIA) return false;
    }
    if (c->regras == &edrealce_c) return c->buf[ini + e] == '{';
    if (c->regras != &edrealce_lua) return false;
    if (c->buf[ini + e] == '{') return true;
    int b = e;
    while (b >= 0 && eh_alnum(c->buf[ini + b])) b--;
    b++;
    int w = e - b + 1;
    const char *p = c->buf + ini + b;
    if ((w == 4 && comeca_em(p, w, 0, "then")) || (w == 2 && comeca_em(p, w, 0, "do")) || (w == 4 && comeca_em(p, w, 0, "else")) ||
        (w == 6 && comeca_em(p, w, 0, "repeat"))) return true;
    if (c->buf[ini + e] == ')' || w > 0) {   /* «function nombre(...)» sin end en la misma línea */
        bool tem_fn = false, tem_end = false;
        for (int i = 0; i + 8 <= n; i++)
            if (cls[i] == RC_CHAVE && comeca_em(c->buf + ini, n, i, "function") && (i + 8 >= n || !eh_alnum(c->buf[ini + i + 8]))) tem_fn = true;
        for (int i = 0; i + 3 <= n; i++)
            if (cls[i] == RC_CHAVE && comeca_em(c->buf + ini, n, i, "end") && (i + 3 >= n || !eh_alnum(c->buf[ini + i + 3]))) tem_end = true;
        return tem_fn && !tem_end;
    }
    return false;
}

static void inserir_texto(codigo_t *c, const char *t, int n, bool fundivel)
{
    ed_desfazer_agrupar_inicio(&c->pilha, N_("Escribir"));
    if (tem_selecao(c)) {
        int a = sel_ini(c), b = sel_fim(c);
        c->cursor = a;
        editar(c, a, b - a, "", 0, false);
        c->ancora = -1;
        fundivel = false;
    }
    editar(c, c->cursor, 0, t, n, fundivel);
    ed_desfazer_agrupar_fim(&c->pilha);
    c->ancora = -1;
}

void codigo_inserir(codigo_t *c, const char *t, int n)
{
    inserir_texto(c, t, n < 0 ? (int)tam_texto(t) : n, false);
    tornar_visivel(c);
}

static void nova_linha(codigo_t *c)
{
    if (tem_selecao(c)) { inserir_texto(c, "", 0, false); }
    int k = linha_de(c, c->cursor), ini = c->inicio[k];
    int esp = 0;
    while (ini + esp < c->cursor && (c->buf[ini + esp] == ' ' || c->buf[ini + esp] == '\t')) esp++;
    char t[80];
    int n = 0;
    t[n++] = '\n';
    int extra = abre_bloco(c, k, c->cursor) ? c->sangria : 0;
    for (int i = 0; i < esp && n < 70; i++) t[n++] = c->buf[ini + i] == '\t' ? '\t' : ' ';
    for (int i = 0; i < extra && n < 78; i++) t[n++] = ' ';
    inserir_texto(c, t, n, false);
}

/* Tras teclear la última letra de end/else/elseif/until o una «}»: la línea retrocede un nivel. */
static void autodedent(codigo_t *c)
{
    int k = linha_de(c, c->cursor), ini = c->inicio[k];
    int esp = 0;
    while (ini + esp < c->cursor && c->buf[ini + esp] == ' ') esp++;
    if (esp < c->sangria) return;
    int w = c->cursor - ini - esp;
    const char *p = c->buf + ini + esp;
    bool dedent = false;
    if (c->regras == &edrealce_lua) {
        dedent = (w == 3 && comeca_em(p, w, 0, "end")) || (w == 4 && comeca_em(p, w, 0, "else")) ||
                 (w == 5 && comeca_em(p, w, 0, "until"));
    } else if (c->regras == &edrealce_c) dedent = w == 1 && p[0] == '}';
    if (!dedent) return;
    /* solo si no hay nada más en la línea tras el cursor que pueda confundirse (es la palabra entera) */
    int fim = fim_linha(c, k);
    if (c->cursor < fim && eh_alnum(c->buf[c->cursor])) return;
    int salva = c->cursor;
    editar(c, ini, c->sangria, "", 0, false);
    c->cursor = salva - c->sangria;
    edicao_t *t = ed_desfazer_topo(&c->pilha);
    if (t) t->cur1 = c->cursor;
}

/* ------------------------------------------------------------ teclado y ratón */

static void mover_a(codigo_t *c, int pos, bool estende)
{
    if (pos < 0) pos = 0;
    if (pos > c->tam) pos = c->tam;
    if (estende) { if (c->ancora < 0) c->ancora = c->cursor; }
    else c->ancora = -1;
    c->cursor = pos;
    c->quebrar = true;
}

static int pos_em(const codigo_t *c, int mx, int my)
{
    int r = (my - c->y - 1) / CODIGO_FILA;
    if (my < c->y + 1) r = -1;
    int k = c->topo + r;
    if (k < 0) k = 0;
    if (k >= c->nlin) k = c->nlin - 1;
    int col = (mx - c->x - calha(c) + LETRA_L / 2) / LETRA_L + c->esq;
    if (col < 0) col = 0;
    int len = fim_linha(c, k) - c->inicio[k];
    if (col > len) col = len;
    return c->inicio[k] + col;
}

static int palavra_ini(const codigo_t *c, int p) { while (p > 0 && eh_alnum(c->buf[p - 1])) p--; return p; }
static int palavra_fim(const codigo_t *c, int p) { while (p < c->tam && eh_alnum(c->buf[p])) p++; return p; }

static void busca_atualizar(codigo_t *c)
{
    c->nao_achou = false;
    if (!c->busca[0]) return;
    int ini = sel_ini(c);
    c->cursor = ini; c->ancora = -1;   /* la coincidencia actual se conserva si sigue valiendo */
    if (!codigo_buscar(c, c->busca, true, c->caso)) { c->nao_achou = true; }
}

static int tratar_busca(codigo_t *c, const evento_t *e)
{
    unsigned t = e->tecla;
    int n = (int)tam_texto(c->busca);
    if (t == TECLA_ESC) { c->buscando = false; c->nao_achou = false; return CODIGO_MOVEU; }
    if (t == '\n' || t == TECLA_BAIXO || t == TECLA_CIMA) {
        bool adiante = t == TECLA_BAIXO || (t == '\n' && !(e->modificadores & 1));
        c->nao_achou = !codigo_buscar(c, c->busca, adiante, c->caso);
        return CODIGO_MOVEU;
    }
    if (t == '\b') {
        if (c->bcursor > 0) { memmove(c->busca + c->bcursor - 1, c->busca + c->bcursor, (size_t)(n - c->bcursor + 1)); c->bcursor--; busca_atualizar(c); }
        return CODIGO_MOVEU;
    }
    if (t == TECLA_ESQUERDA) { if (c->bcursor > 0) c->bcursor--; return CODIGO_NADA; }
    if (t == TECLA_DIREITA) { if (c->bcursor < n) c->bcursor++; return CODIGO_NADA; }
    if (t >= 0x20 && t < 0x100 && t != 0x7F && n < (int)sizeof c->busca - 1) {
        memmove(c->busca + c->bcursor + 1, c->busca + c->bcursor, (size_t)(n - c->bcursor + 1));
        c->busca[c->bcursor++] = (char)t;
        busca_atualizar(c);
        return CODIGO_MOVEU;
    }
    return CODIGO_NADA;
}

static void cod_apagar(codigo_t *c, int a, int b)
{
    if (b <= a) return;
    c->cursor = c->cursor > b ? c->cursor : c->cursor;
    editar(c, a, b - a, "", 0, false);
    c->ancora = -1;
}

int codigo_tratar(jan_t *j, codigo_t *c, const evento_t *e)
{
    int r = CODIGO_NADA;
    if (e->tipo == EV_RODA || e->tipo == EV_RODA_H) {
        if (!dentro(e->x, e->y, c->x, c->y, c->l, c->a)) return r;
        if (e->tipo == EV_RODA && !(e->modificadores & 1)) {
            int max = c->nlin - linhas_caben(c);
            c->topo += (int)e->cliques * RODA_LINHAS;
            if (c->topo > max) c->topo = max;
            if (c->topo < 0) c->topo = 0;
        } else {
            c->esq += (int)e->cliques * 4;
            if (c->esq < 0) c->esq = 0;
        }
        codigo_desenhar(j, c);
        return CODIGO_MOVEU;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        bool dentro_c = dentro(e->x, e->y, c->x, c->y, c->l, c->a);
        bool antes = c->foco;
        c->foco = dentro_c;
        if (!dentro_c) { if (antes) codigo_desenhar(j, c); return r; }
        if (c->buscando && e->y >= c->y + c->a - 26) { codigo_desenhar(j, c); return r; }
        if (e->x >= c->x + c->l - BARRA_L - 2) {   /* barra: una página arriba o abajo */
            int vis = linhas_caben(c), max = c->nlin - vis;
            c->topo += e->y < c->y + c->a / 2 ? -vis : vis;
            if (c->topo > max) c->topo = max;
            if (c->topo < 0) c->topo = 0;
            codigo_desenhar(j, c);
            return CODIGO_MOVEU;
        }
        int p = pos_em(c, e->x, e->y);
        if (e->cliques >= 2) { c->ancora = palavra_ini(c, p); c->cursor = palavra_fim(c, p); }
        else if (e->modificadores & 1) { if (c->ancora < 0) c->ancora = c->cursor; c->cursor = p; }
        else { c->ancora = p; c->cursor = p; c->arrastando = true; }
        c->quebrar = true;
        tornar_visivel(c);
        codigo_desenhar(j, c);
        return CODIGO_MOVEU;
    }
    if (e->tipo == EV_MOVER && c->arrastando && (e->botoes & 1)) {
        int p = pos_em(c, e->x, e->y);
        if (p != c->cursor) { c->cursor = p; tornar_visivel(c); codigo_desenhar(j, c); return CODIGO_MOVEU; }
        return r;
    }
    if (e->tipo == EV_SOLTOU && c->arrastando) {
        c->arrastando = false;
        if (c->ancora == c->cursor) c->ancora = -1;
        return r;
    }
    if (e->tipo != EV_TECLA || !c->foco) return r;
    unsigned t = e->tecla;
    if (t == 6) {   /* Ctrl+F */
        c->buscando = true;
        if (tem_selecao(c) && sel_fim(c) - sel_ini(c) < (int)sizeof c->busca - 1) {
            bool una = true;
            for (int i = sel_ini(c); i < sel_fim(c); i++) if (c->buf[i] == '\n') una = false;
            if (una) codigo_selecao(c, c->busca, sizeof c->busca);
        }
        c->bcursor = (int)tam_texto(c->busca);
        tornar_visivel(c);
        codigo_desenhar(j, c);
        return CODIGO_MOVEU;
    }
    if (c->buscando) {
        r = tratar_busca(c, e);
        tornar_visivel(c);
        codigo_desenhar(j, c);
        return r;
    }
    bool mover = t == TECLA_ESQUERDA || t == TECLA_DIREITA || t == TECLA_CIMA || t == TECLA_BAIXO || t == TECLA_PGUP ||
                 t == TECLA_PGDN || t == TECLA_HOME || t == TECLA_END;
    bool shift = (e->modificadores & 1) != 0;
    int k = linha_de(c, c->cursor), col = c->cursor - c->inicio[k];
    if (mover) {
        int p = c->cursor;
        bool vertical = false;
        if (t == TECLA_ESQUERDA) { if (tem_selecao(c) && !shift) p = sel_ini(c); else p--; }
        else if (t == TECLA_DIREITA) { if (tem_selecao(c) && !shift) p = sel_fim(c); else p++; }
        else if (t == TECLA_HOME) {
            int ini = c->inicio[k], e0 = ini;
            while (e0 < fim_linha(c, k) && (c->buf[e0] == ' ' || c->buf[e0] == '\t')) e0++;
            p = c->cursor == e0 ? ini : e0;
        }
        else if (t == TECLA_END) p = fim_linha(c, k);
        else {
            vertical = true;
            int d = t == TECLA_CIMA ? -1 : t == TECLA_BAIXO ? 1 : t == TECLA_PGUP ? -linhas_caben(c) : linhas_caben(c);
            int nk = k + d;
            if (nk < 0) { nk = 0; if (t == TECLA_CIMA) p = 0; }
            if (nk >= c->nlin) { nk = c->nlin - 1; if (t == TECLA_BAIXO) p = c->tam; }
            if (!((t == TECLA_CIMA && k == 0) || (t == TECLA_BAIXO && k == c->nlin - 1))) {
                int len = fim_linha(c, nk) - c->inicio[nk];
                int cc = c->col_desejada > len ? len : c->col_desejada;
                p = c->inicio[nk] + cc;
            }
        }
        if (!vertical) c->col_desejada = p - c->inicio[linha_de(c, p < 0 ? 0 : p > c->tam ? c->tam : p)];
        mover_a(c, p, shift);
        if (t == TECLA_PGUP || t == TECLA_PGDN) { c->topo += t == TECLA_PGUP ? -linhas_caben(c) : linhas_caben(c); }
        tornar_visivel(c);
        codigo_desenhar(j, c);
        return CODIGO_MOVEU;
    }
    if (t == 1) { c->ancora = 0; c->cursor = c->tam; codigo_desenhar(j, c); return CODIGO_MOVEU; }
    if (t == 3 || t == 24) {   /* copiar y cortar */
        if (tem_selecao(c)) {
            copiar_texto(c->buf + sel_ini(c), (unsigned)(sel_fim(c) - sel_ini(c)));
            if (t == 24) { cod_apagar(c, sel_ini(c), sel_fim(c)); tornar_visivel(c); codigo_desenhar(j, c); return CODIGO_MUDOU; }
        }
        return r;
    }
    if (t == 22) {   /* pegar */
        char *tmp = aloca(PORTAPAPEIS_MAX + 1);
        if (!tmp) return r;
        int n = colar_texto(tmp, PORTAPAPEIS_MAX);
        n = (int)tam_texto(tmp);
        if (n > 0) { inserir_texto(c, tmp, n, false); r = CODIGO_MUDOU; }
        libera(tmp);
        tornar_visivel(c);
        codigo_desenhar(j, c);
        return r;
    }
    if (t == 26) { r = codigo_desfazer(c) ? CODIGO_MUDOU : CODIGO_NADA; codigo_desenhar(j, c); return r; }
    if (t == 25) { r = codigo_refazer(c) ? CODIGO_MUDOU : CODIGO_NADA; codigo_desenhar(j, c); return r; }
    if (t == '\b') {
        if (tem_selecao(c)) cod_apagar(c, sel_ini(c), sel_fim(c));
        else if (c->cursor > 0) {
            int p = c->cursor, ini = c->inicio[k];
            int n = 1;
            /* una sangría de solo espacios retrocede de a «sangria» */
            if (col >= c->sangria && col % c->sangria == 0) {
                bool solo = true;
                for (int i = 0; i < col; i++) if (c->buf[ini + i] != ' ') { solo = false; break; }
                if (solo) n = c->sangria;
            }
            cod_apagar(c, p - n, p);
            c->cursor = p - n;
        } else return r;
        r = CODIGO_MUDOU;
    } else if (t == TECLA_DELETE) {
        if (tem_selecao(c)) cod_apagar(c, sel_ini(c), sel_fim(c));
        else if (c->cursor < c->tam) { int p = c->cursor; editar(c, p, 1, "", 0, false); c->cursor = p; }
        else return r;
        r = CODIGO_MUDOU;
    } else if (t == '\n') {
        nova_linha(c);
        r = CODIGO_MUDOU;
    } else if (t == '\t') {
        int n = c->sangria - (col % c->sangria);
        inserir_texto(c, "        ", n, false);
        r = CODIGO_MUDOU;
    } else if (t >= 0x20 && t < 0x100 && t != 0x7F) {
        char ch = (char)t;
        bool fund = !tem_selecao(c);
        ed_desfazer_agrupar_inicio(&c->pilha, N_("Escribir"));
        inserir_texto(c, &ch, 1, fund);
        autodedent(c);
        ed_desfazer_agrupar_fim(&c->pilha);
        r = CODIGO_MUDOU;
    } else return r;
    c->col_desejada = c->cursor - c->inicio[linha_de(c, c->cursor)];
    tornar_visivel(c);
    codigo_desenhar(j, c);
    return r;
}

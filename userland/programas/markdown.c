// SPDX-License-Identifier: GPL-2.0-or-later
/* markdown [-t] [archivo.md] - visor y editor de Markdown de SaguiOS (D045).
 *
 * Visor: el texto se analiza en bloques (títulos, párrafos, listas, citas,
 * código, tablas...) y se "maqueta" en una lista de elementos con posición
 * absoluta en el documento: trozos de texto con estilo y rectángulos de fondo.
 * Desplazarse solo dibuja los elementos visibles; volver a maquetar solo ocurre
 * cuando cambian el texto o el ancho (maquetar(largura)).
 *
 * La ventana es redimensionable y maximizable (D048): en EV_REDIMENSIONAR los
 * paneles se recalculan a partir de j->larg y j->alt, el visor se
 * vuelve a maquetar con el ancho nuevo y el desplazamiento se mantiene en la
 * misma proporción del documento. La rueda del mouse desplaza el panel que está debajo
 * del cursor (visor o editor), RODA_LINHAS líneas por muesca.
 *
 * Editor: texto plano con resaltado leve de la sintaxis. Ctrl+E (o F5) alterna entre
 * visor y editor; el botón "Dividir" muestra los dos lado a lado. Selección con
 * Shift + flechas o arrastrando el mouse; Ctrl+X/C/V/A y el menú contextual
 * usan el portapapeles del sistema (D074).
 *
 * Modo texto (-t): la misma maquetación, en columnas y líneas en vez de píxeles,
 * impresa en la terminal (títulos subrayados, viñetas, tablas alineadas).
 *
 * El texto queda en Latin-1 en memoria (como en todo el sistema); al abrir,
 * UTF-8 se convierte a Latin-1 (D027) y, al guardar, el archivo se graba en UTF-8. */
#include "sagui.h"
#include "gui.h"

extern const unsigned char fonte_sagui[256][16];

/* ------------------------------------------------------------ ventana */

#define LARG_INICIO 800
#define ALT_INICIO  600
#define LARG_MIN    560   /* los seis botones caben; "Ctrl+E alterna" solo aparece si entra */
#define ALT_MIN     240
#define LARG     (j->larg)   /* la ventana es redimensionable (D048): el tamaño es el actual */
#define ALT      (j->alt)
#define BARRA    36
#define RODAPE   20
#define ROLAGEM  12
#define MARGEM   16
#define TOPO_DOC 10

/* Colores del documento. */
#define COR_H1          0x14532D
#define COR_H2          0x0B7A3E
#define COR_H3          0x1F4E8C
#define COR_H4          0x2B3A4A
#define COR_H5          0x55606B
#define COR_LINHA_TIT   0xC8CDD2
#define COR_CURSIVA     0x6B3FA0
#define COR_COD         0x9A2A3A
#define COR_FUNDO_COD   0xE8EAED
#define COR_BLOCO       0xF1F3F5
#define COR_BLOCO_BORDA 0x9AA3AC
#define COR_BLOCO_TEXTO 0x24292E
#define COR_ENLACE      0x1A5FB4
#define COR_IMAGEM      0x6A737C
#define COR_CITA_FUNDO  0xF2F6F1
#define COR_CITA_BARRA  0x9CC5A8
#define COR_CITA_TEXTO  0x3E4A44
#define COR_MARCADOR    0xB5651D
#define COR_REGRA       0xB8BEC4
#define COR_TAB_CAB     0xE3EEE6
#define COR_TAB_ZEBRA   0xF6F7F8
#define COR_TAB_GRADE   0xB8BEC4

/* Estilos de un trozo de texto. */
#define E_NEGRITA 1
#define E_CURSIVA 2
#define E_CODIGO  4
#define E_ENLACE  8
#define E_TACHADO 16
#define E_IMAGEM  32

#define MAX_PROF 16   /* anidamiento máximo de listas y citas */
#define TECLA_F5_MD 0x10E  /* F5 (TECLA_F1 + 4 en teclado.h del kernel) */

static void sem_memoria(void)
{
    registrar("markdown: sin memoria\n");
    escrever_texto(ERROS, "markdown: sin memoria\n");
    sair(1);
}

/* Aumenta un vector dinámico para que quepan "precisa" elementos. */
static void *crescer(void *p, int *cap, int precisa, int tam_item)
{
    if (precisa <= *cap) return p;
    int nova = *cap ? *cap : 256;
    while (nova < precisa) nova *= 2;
    void *q = realoca(p, (size_t)nova * (size_t)tam_item);
    if (!q) sem_memoria();
    *cap = nova;
    return q;
}

/* ------------------------------------------------------------ elementos maquetados */

enum { T_TEXTO, T_RET, T_LINHA };

typedef struct {
    int      y;          /* en el documento (píxeles, o líneas en el modo texto) */
    int16_t  x, l, a;    /* T_LINHA: l y a son el desplazamiento hasta el fin */
    uint8_t  tipo, escala;
    uint16_t estilo;
    int16_t  enlace;     /* índice en enlaces, o -1 */
    int      off, n;     /* texto en "pool" */
    uint32_t cor;
} elem_t;

typedef struct { int off, n; } enlace_t;
typedef struct { int off, n; uint16_t estilo; int16_t enlace; } seg_t;
typedef struct { const char *s; int n; } lin_t;

static elem_t   *els;   static int nels, cap_els;
static char     *pool;  static int npool, cap_pool;
static seg_t    *segs;  static int nsegs, cap_segs, seg_base;
static enlace_t *enls;  static int nenls, cap_enls;
static char     *tmp;   static int ntmp, cap_tmp;
static char     *fonte; static int cap_fonte;   /* copia del texto con tabulaciones expandidas */
static lin_t    *linhas; static int cap_linhas;

/* Estado de la maquetación. */
static bool     mt;            /* modo texto: las unidades son columnas y líneas */
static int      CW;            /* ancho de una letra */
static int      GAP;           /* espacio entre bloques */
static int      y_cur;
static int      n_blocos, n_linhas, n_titulos;
static int      doc_alt;
static uint32_t cor_base = COR_TEXTO;
static int      fluir_xfim;
static bool     reg_titulos;   /* registrar los títulos en la serial (pruebas) */

static int pool_add(const char *s, int n)
{
    pool = crescer(pool, &cap_pool, npool + n + 1, 1);
    memcpy(pool + npool, s, (size_t)n);
    int off = npool;
    npool += n;
    return off;
}

static int novo_el(int tipo)
{
    els = crescer(els, &cap_els, nels + 1, sizeof(elem_t));
    elem_t *e = &els[nels];
    memset(e, 0, sizeof *e);
    e->tipo = (uint8_t)tipo;
    e->escala = 1;
    e->enlace = -1;
    return nels++;
}

static int el_ret(int x, int y, int l, int a, uint32_t cor)
{
    int k = novo_el(T_RET);
    els[k].x = (int16_t)x; els[k].y = y; els[k].l = (int16_t)l; els[k].a = (int16_t)a; els[k].cor = cor;
    return k;
}

static void el_linha(int x, int y, int dx, int dy, uint32_t cor)
{
    int k = novo_el(T_LINHA);
    els[k].x = (int16_t)x; els[k].y = y; els[k].l = (int16_t)dx; els[k].a = (int16_t)dy; els[k].cor = cor;
}

static void el_texto(int x, int y, int off, int n, uint16_t est, int enl, int esc, uint32_t cor)
{
    if (n <= 0) return;
    int k = novo_el(T_TEXTO);
    elem_t *e = &els[k];
    e->x = (int16_t)x; e->y = y; e->off = off; e->n = n; e->estilo = est; e->enlace = (int16_t)enl;
    e->escala = (uint8_t)esc; e->cor = cor;
    e->l = (int16_t)(n * CW * esc);
    e->a = (int16_t)(mt ? 1 : 16 * esc + 2);
}

static void el_texto_s(int x, int y, const char *s, uint16_t est, int esc, uint32_t cor)
{
    int n = (int)tam_texto(s);
    el_texto(x, y, pool_add(s, n), n, est, -1, esc, cor);
}

/* ------------------------------------------------------------ texto en línea */

static bool pontuacao(char c)
{
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
}

static bool alfanum(char ch)
{
    unsigned char c = (unsigned char)ch;
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0xC0;
}

static void por(char c, uint16_t est, int enl)
{
    int off = npool;
    pool_add(&c, 1);
    if (nsegs > seg_base) {
        seg_t *u = &segs[nsegs - 1];
        if (u->estilo == est && u->enlace == enl && u->off + u->n == off) { u->n++; return; }
    }
    segs = crescer(segs, &cap_segs, nsegs + 1, sizeof(seg_t));
    segs[nsegs].off = off; segs[nsegs].n = 1; segs[nsegs].estilo = est; segs[nsegs].enlace = (int16_t)enl;
    nsegs++;
}

static void por_texto(const char *s, int n, uint16_t est, int enl)
{
    for (int i = 0; i < n; i++) por(s[i], est, enl);
}

static int sequencia(const char *s, int n, int i, char c)
{
    int k = 0;
    while (i + k < n && s[i + k] == c) k++;
    return k;
}

/* ¿Hay un delimitador de cierre (k veces c, después de algo que no es espacio)
 * desde "de"? */
static bool acha_fecho(const char *s, int n, int de, char c, int k)
{
    for (int j = de + 1; j < n; j++) {
        if (s[j] == '\\') { j++; continue; }
        if (s[j] != c) continue;
        int r = sequencia(s, n, j, c);
        if (r >= k && s[j - 1] != ' ' && (c != '_' || j + r >= n || !alfanum(s[j + r]))) return true;
        j += r - 1;
    }
    return false;
}

static int fecha_colchete(const char *s, int n, int i)
{
    int prof = 0;
    for (int k = i; k < n; k++) {
        if (s[k] == '\\') { k++; continue; }
        if (s[k] == '[') prof++;
        else if (s[k] == ']' && --prof == 0) return k;
    }
    return -1;
}

/* Destino de un enlace: "(url "título")". Devuelve la posición después del ")". */
static int destino(const char *s, int n, int k, int *u0, int *un)
{
    if (k >= n || s[k] != '(') return -1;
    int prof = 0, j;
    for (j = k; j < n; j++) {
        if (s[j] == '\\') { j++; continue; }
        if (s[j] == '(') prof++;
        else if (s[j] == ')' && --prof == 0) break;
    }
    if (j >= n) return -1;
    int a = k + 1, b = j;
    while (a < b && s[a] == ' ') a++;
    if (a < b && s[a] == '<') {
        int f = a + 1;
        while (f < b && s[f] != '>') f++;
        *u0 = a + 1; *un = f - a - 1;
    } else {
        int f = a;
        while (f < b && s[f] != ' ') f++;
        *u0 = a; *un = f - a;
    }
    return j + 1;
}

static int novo_enlace(const char *s, int n)
{
    enls = crescer(enls, &cap_enls, nenls + 1, sizeof(enlace_t));
    enls[nenls].off = pool_add(s, n);
    enls[nenls].n = n;
    return nenls++;
}

static void inline_rec(const char *s, int n, uint16_t est, int enl, int prof)
{
    bool neg = false, cur = false, tach = false;
    for (int i = 0; i < n;) {
        uint16_t e = est | (neg ? E_NEGRITA : 0) | (cur ? E_CURSIVA : 0) | (tach ? E_TACHADO : 0);
        char c = s[i];
        if (c == '\\' && i + 1 < n && pontuacao(s[i + 1])) { por(s[i + 1], e, enl); i += 2; continue; }
        if (c == '`') {
            int k = sequencia(s, n, i, '`'), j;
            for (j = i + k; j < n; j++)
                if (s[j] == '`') {
                    int k2 = sequencia(s, n, j, '`');
                    if (k2 == k) break;
                    j += k2 - 1;
                }
            if (j < n) {
                int a = i + k, b = j;
                if (b - a >= 2 && s[a] == ' ' && s[b - 1] == ' ') { a++; b--; }
                for (int q = a; q < b; q++) por(s[q] == '\n' ? ' ' : s[q], e | E_CODIGO, enl);
                i = j + k;
                continue;
            }
            por_texto(s + i, k, e, enl);
            i += k;
            continue;
        }
        if (prof < 4 && (c == '[' || (c == '!' && i + 1 < n && s[i + 1] == '['))) {
            int a = c == '!' ? i + 1 : i;
            int f = fecha_colchete(s, n, a), u0 = 0, un = 0;
            int fim = f >= 0 ? destino(s, n, f + 1, &u0, &un) : -1;
            if (fim > 0) {
                if (c == '!') {
                    por_texto("[imagen: ", 9, e | E_IMAGEM, -1);
                    inline_rec(s + a + 1, f - a - 1, e | E_IMAGEM, -1, prof + 1);
                    por(']', e | E_IMAGEM, -1);
                } else {
                    int idx = novo_enlace(s + u0, un);
                    inline_rec(s + a + 1, f - a - 1, e | E_ENLACE, idx, prof + 1);
                    if (mt && un > 0 && (f - a - 1 != un || memcmp(s + a + 1, s + u0, (size_t)un) != 0)) {
                        por_texto(" <", 2, e, -1);
                        por_texto(s + u0, un, e, -1);
                        por('>', e, -1);
                    }
                }
                i = fim;
                continue;
            }
        }
        if (c == '<') {   /* enlace automático: <https://...> */
            int j = i + 1;
            bool tem = false;
            while (j < n && s[j] != '>' && s[j] != ' ' && s[j] != '<') { if (s[j] == ':' || s[j] == '@') tem = true; j++; }
            if (j < n && s[j] == '>' && tem && j > i + 1) {
                int idx = novo_enlace(s + i + 1, j - i - 1);
                por_texto(s + i + 1, j - i - 1, e | E_ENLACE, idx);
                i = j + 1;
                continue;
            }
        }
        if (c == '*' || c == '_' || c == '~') {
            int k = sequencia(s, n, i, c);
            bool antes_esp = i == 0 || s[i - 1] == ' ' || s[i - 1] == '\n';
            bool depois_esp = i + k >= n || s[i + k] == ' ' || s[i + k] == '\n';
            bool abre = !depois_esp, fecha = !antes_esp;
            if (c == '_') {
                if (i > 0 && alfanum(s[i - 1])) abre = false;
                if (i + k < n && alfanum(s[i + k])) fecha = false;
            }
            if (c == '~') {
                if (k == 2) {
                    if (tach && fecha) { tach = false; i += 2; continue; }
                    if (!tach && abre && acha_fecho(s, n, i + 2, c, 2)) { tach = true; i += 2; continue; }
                }
                por_texto(s + i, k, e, enl);
                i += k;
                continue;
            }
            if (fecha && k >= 2 && neg) { neg = false; i += 2; continue; }
            if (fecha && cur) { cur = false; i += 1; continue; }
            if (abre && k >= 2 && !neg && acha_fecho(s, n, i + 2, c, 2)) { neg = true; i += 2; continue; }
            if (abre && !cur && acha_fecho(s, n, i + 1, c, 1)) { cur = true; i += 1; continue; }
            por(c, e, enl);
            i++;
            continue;
        }
        por(c, e, enl);
        i++;
    }
}

/* ------------------------------------------------------------ ajuste de líneas */

static void emitir(int x, int y, int off, int n, const seg_t *sg, int esc, uint32_t cor)
{
    uint16_t est = sg->estilo;
    if (!mt && (est & E_CODIGO)) el_ret(x, y, n * CW * esc, 16 * esc + 2, COR_FUNDO_COD);
    uint32_t c = cor;
    if (est & E_CODIGO) c = COR_COD;
    else if (est & E_ENLACE) c = COR_ENLACE;
    else if (est & E_IMAGEM) c = COR_IMAGEM;
    else if ((est & E_CURSIVA) && cor == COR_TEXTO) c = COR_CURSIVA;
    el_texto(x, y + (mt ? 0 : 1), off, n, est, sg->enlace, esc, c);
}

static void alinhar(int de, int x, int xmax, int alin)
{
    if (!alin || x >= xmax) return;
    int d = alin == 1 ? (xmax - x) / 2 : xmax - x;
    for (int k = de; k < nels; k++) els[k].x = (int16_t)(els[k].x + d);
}

/* Distribuye los segmentos [s0, s1) en líneas entre x0 y xmax, a partir de y.
 * alin: 0 izquierda, 1 centro, 2 derecha. Devuelve la y después de la última línea. */
static int fluir(int s0, int s1, int x0, int xmax, int y, int esc, uint32_t cor, int alin)
{
    int lh = mt ? 1 : 16 * esc + 2, cw = CW * esc;
    int x = x0, el0 = nels, si = s0, ci = 0, esp_si = 0, esp_off = 0;
    bool vazia = true, esp = false;
    fluir_xfim = x0;
    if (xmax - x0 < cw) xmax = x0 + cw;
#define NOVA_LINHA() do { alinhar(el0, x, xmax, alin); if (x > fluir_xfim) fluir_xfim = x; \
                          y += lh; n_linhas++; x = x0; vazia = true; el0 = nels; } while (0)
    for (;;) {
        while (si < s1 && ci >= segs[si].n) { si++; ci = 0; }
        if (si >= s1) break;
        char c = pool[segs[si].off + ci];
        if (c == ' ') {
            if (!esp) { esp = true; esp_si = si; esp_off = segs[si].off + ci; }
            ci++;
            continue;
        }
        if (c == '\n') { NOVA_LINHA(); esp = false; ci++; continue; }
        /* mide la palabra (puede atravesar segmentos de estilos diferentes) */
        int k_si = si, k_ci = ci, w = 0;
        for (;;) {
            while (k_si < s1 && k_ci >= segs[k_si].n) { k_si++; k_ci = 0; }
            if (k_si >= s1) break;
            char d = pool[segs[k_si].off + k_ci];
            if (d == ' ' || d == '\n') break;
            w++;
            k_ci++;
        }
        int sp = (esp && !vazia) ? cw : 0;
        if (!vazia && x + sp + w * cw > xmax) { NOVA_LINHA(); sp = 0; }
        if (sp) {
            if (segs[esp_si].estilo & (E_CODIGO | E_ENLACE | E_TACHADO)) emitir(x, y, esp_off, 1, &segs[esp_si], esc, cor);
            x += sp;
        }
        esp = false;
        while (w > 0) {
            while (ci >= segs[si].n) { si++; ci = 0; }
            int m = segs[si].n - ci;
            if (m > w) m = w;
            int cabe = (xmax - x) / cw;
            if (m > cabe) {   /* palabra más grande que la línea: corta */
                if (cabe < 1) {
                    if (x == x0) cabe = 1;
                    else { NOVA_LINHA(); continue; }
                }
                m = cabe;
            }
            emitir(x, y, segs[si].off + ci, m, &segs[si], esc, cor);
            x += m * cw;
            ci += m;
            w -= m;
            vazia = false;
            if (w > 0 && x + cw > xmax) NOVA_LINHA();
        }
    }
    if (!vazia) NOVA_LINHA();
#undef NOVA_LINHA
    return y;
}

/* ------------------------------------------------------------ bloques */

static int recuo(lin_t l) { int k = 0; while (k < l.n && l.s[k] == ' ') k++; return k; }
static bool vazia(lin_t l) { return recuo(l) == l.n; }
static lin_t cortar(lin_t l, int k) { if (k > l.n) k = l.n; l.s += k; l.n -= k; return l; }

static bool eh_regra(lin_t l)
{
    int i = recuo(l);
    if (i > 3 || i >= l.n) return false;
    char c = l.s[i];
    if (c != '-' && c != '*' && c != '_') return false;
    int qtd = 0;
    for (; i < l.n; i++) {
        if (l.s[i] == c) qtd++;
        else if (l.s[i] != ' ') return false;
    }
    return qtd >= 3;
}

/* Título ATX: devuelve el nivel (0 = no es) y el texto en [*a, *b). */
static int eh_titulo(lin_t l, int *a, int *b)
{
    int i = recuo(l);
    if (i > 3) return 0;
    int k = sequencia(l.s, l.n, i, '#');
    if (k < 1 || k > 6 || (i + k < l.n && l.s[i + k] != ' ')) return 0;
    int ini = i + k, fim = l.n;
    while (ini < fim && l.s[ini] == ' ') ini++;
    while (fim > ini && l.s[fim - 1] == ' ') fim--;
    int f = fim;
    while (f > ini && l.s[f - 1] == '#') f--;
    if (f == ini || l.s[f - 1] == ' ') { fim = f; while (fim > ini && l.s[fim - 1] == ' ') fim--; }
    if (a) { *a = ini; *b = fim; }
    return k;
}

static int eh_cerca(lin_t l, char *ch)
{
    int i = recuo(l);
    if (i > 3 || i >= l.n || (l.s[i] != '`' && l.s[i] != '~')) return 0;
    int k = sequencia(l.s, l.n, i, l.s[i]);
    if (k < 3) return 0;
    if (ch) *ch = l.s[i];
    return k;
}

static bool eh_citacao(lin_t l) { int i = recuo(l); return i <= 3 && i < l.n && l.s[i] == '>'; }

/* Marcador de lista: devuelve la posición justo después de él (-1 = no es). */
static int marcador(lin_t l, int *ordenada, int *num, char *ch)
{
    int i = recuo(l);
    if (i > 3 || i >= l.n) return -1;
    char c = l.s[i];
    if (c == '-' || c == '*' || c == '+') {
        if (i + 1 < l.n && l.s[i + 1] != ' ') return -1;
        if (ordenada) { *ordenada = 0; *num = 0; *ch = c; }
        return i + 1;
    }
    int k = 0, v = 0;
    while (i + k < l.n && k < 9 && l.s[i + k] >= '0' && l.s[i + k] <= '9') { v = v * 10 + l.s[i + k] - '0'; k++; }
    if (k == 0 || i + k >= l.n || (l.s[i + k] != '.' && l.s[i + k] != ')')) return -1;
    if (i + k + 1 < l.n && l.s[i + k + 1] != ' ') return -1;
    if (ordenada) { *ordenada = 1; *num = v; *ch = l.s[i + k]; }
    return i + k + 1;
}

static int eh_setext(lin_t l)
{
    int i = recuo(l);
    if (i > 3 || i >= l.n || (l.s[i] != '=' && l.s[i] != '-')) return 0;
    char c = l.s[i];
    int f = l.n;
    while (f > i && l.s[f - 1] == ' ') f--;
    for (int k = i; k < f; k++) if (l.s[k] != c) return 0;
    return c == '=' ? 1 : 2;
}

static bool tem_barra(lin_t l)
{
    for (int i = 0; i < l.n; i++) {
        if (l.s[i] == '\\') { i++; continue; }
        if (l.s[i] == '|') return true;
    }
    return false;
}

/* Divide una línea de tabla en celdas (sin las barras de las puntas). */
static int celulas(lin_t l, lin_t *cel, int max)
{
    int i = recuo(l), f = l.n;
    while (f > i && l.s[f - 1] == ' ') f--;
    if (i < f && l.s[i] == '|') i++;
    if (f > i && l.s[f - 1] == '|' && (f < 2 || l.s[f - 2] != '\\')) f--;
    int qtd = 0, ini = i;
    for (int k = i; k <= f; k++) {
        if (k < f && l.s[k] == '\\') { k++; continue; }
        if (k == f || l.s[k] == '|') {
            if (qtd < max) {
                int a = ini, b = k;
                while (a < b && l.s[a] == ' ') a++;
                while (b > a && l.s[b - 1] == ' ') b--;
                cel[qtd].s = l.s + a; cel[qtd].n = b - a;
                qtd++;
            }
            ini = k + 1;
        }
    }
    return qtd;
}

/* Línea separadora de tabla: | :--- | :---: | ---: | */
static bool eh_delim(lin_t l, int *alin, int max, int *qtd)
{
    if (!tem_barra(l)) return false;
    lin_t c[32];
    int n = celulas(l, c, 32);
    if (n < 1) return false;
    for (int k = 0; k < n; k++) {
        const char *s = c[k].s;
        int m = c[k].n, a = 0, b = m;
        if (m == 0) return false;
        bool esq = s[0] == ':', dir = s[m - 1] == ':';
        if (esq) a++;
        if (dir && b > a) b--;
        if (b <= a) return false;
        for (int q = a; q < b; q++) if (s[q] != '-') return false;
        if (alin && k < max) alin[k] = esq && dir ? 1 : dir ? 2 : 0;
    }
    if (qtd) *qtd = n;
    return true;
}

static bool inicia_bloco(lin_t l)
{
    return eh_cerca(l, NULL) || eh_titulo(l, NULL, NULL) || eh_regra(l) || eh_citacao(l) || marcador(l, NULL, NULL, NULL) >= 0;
}

static void tmp_add(const char *s, int n)
{
    tmp = crescer(tmp, &cap_tmp, ntmp + n + 1, 1);
    memcpy(tmp + ntmp, s, (size_t)n);
    ntmp += n;
}

/* Une las líneas de un párrafo (salto forzado: dos espacios o "\" al final). */
static void juntar(lin_t *L, int a, int b)
{
    ntmp = 0;
    for (int k = a; k < b; k++) {
        lin_t l = cortar(L[k], recuo(L[k]));
        int n = l.n;
        bool quebra = false;
        if (k < b - 1) {
            if (n >= 2 && l.s[n - 1] == ' ' && l.s[n - 2] == ' ') quebra = true;
            else if (n >= 1 && l.s[n - 1] == '\\') { quebra = true; n--; }
        }
        while (n > 0 && l.s[n - 1] == ' ') n--;
        tmp_add(l.s, n);
        if (k < b - 1) tmp_add(quebra ? "\n" : " ", 1);
    }
}

static const uint32_t cor_titulo[7] = { 0, COR_H1, COR_H2, COR_H3, COR_H4, COR_H5, COR_H5 };

static void titulo(int nivel, const char *s, int n, int x0, int xmax, bool primeiro)
{
    int s0 = nsegs;
    seg_base = nsegs;
    inline_rec(s, n, E_NEGRITA, -1, 0);
    int s1 = nsegs;
    if (reg_titulos) {
        char t[96];
        int k = 0;
        for (int i = s0; i < s1 && k < 90; i++)
            for (int q = 0; q < segs[i].n && k < 90; q++) t[k++] = pool[segs[i].off + q];
        t[k] = 0;
        registrar("markdown: h%d %s\n", nivel, t);
    }
    n_titulos++;
    if (mt) {
        y_cur = fluir(s0, s1, x0, xmax, y_cur, 1, cor_base, 0);
        if (y_cur > 0 && fluir_xfim > x0) {
            int w = fluir_xfim - x0;
            char ch = nivel == 1 ? '=' : nivel == 2 ? '-' : '~';
            int off = npool;
            for (int i = 0; i < w; i++) pool_add(&ch, 1);
            el_texto(x0, y_cur, off, w, 0, -1, 1, cor_base);
            y_cur++;
        }
        return;
    }
    if (!primeiro && nivel <= 2) y_cur += 6;
    y_cur = fluir(s0, s1, x0, xmax, y_cur, nivel == 1 ? 2 : 1, cor_titulo[nivel], 0);
    if (nivel <= 2) {
        y_cur += 2;
        el_ret(x0, y_cur, xmax - x0, nivel == 1 ? 2 : 1, COR_LINHA_TIT);
        y_cur += 4;
    }
}

static void bloco_codigo(lin_t *L, int a, int b, int corte, int x0, int xmax)
{
    while (b > a && vazia(L[b - 1])) b--;
    if (mt) {
        for (int k = a; k < b; k++) {
            lin_t l = L[k];
            int c = recuo(l);
            l = cortar(l, c < corte ? c : corte);
            el_texto(x0 + 4, y_cur, pool_add(l.s, l.n), l.n, E_CODIGO, -1, 1, cor_base);
            y_cur++;
            n_linhas++;
        }
        return;
    }
    int y0 = y_cur;
    int fundo = el_ret(x0, y0, xmax - x0, 0, COR_BLOCO);
    int borda = el_ret(x0, y0, 3, 0, COR_BLOCO_BORDA);
    y_cur += 6;
    int cabem = (xmax - x0 - 16) / CW;
    for (int k = a; k < b; k++) {
        lin_t l = L[k];
        int c = recuo(l);
        l = cortar(l, c < corte ? c : corte);
        int n = l.n;
        if (n > cabem) {   /* línea larga: corta y muestra » */
            n = cabem - 1;
            el_texto_s(x0 + 10 + n * CW, y_cur + 1, "\xBB", 0, 1, COR_TEXTO_FRACO);
        }
        el_texto(x0 + 10, y_cur + 1, pool_add(l.s, n), n, 0, -1, 1, COR_BLOCO_TEXTO);
        y_cur += 18;
        n_linhas++;
    }
    y_cur += 6;
    els[fundo].a = (int16_t)(y_cur - y0);
    els[borda].a = (int16_t)(y_cur - y0);
}

static void blocos(lin_t *L, int nl, int x0, int xmax, int prof, bool apertado);

static void bloco_tabela(lin_t *L, int i, int fim, int ncols, const int *alin, int x0, int xmax)
{
    int nr = fim - i - 1;   /* encabezado + cuerpo (sin el separador) */
    if (ncols > 32) ncols = 32;
    int *cs0 = aloca((size_t)(nr * ncols) * sizeof(int) * 3);
    if (!cs0) sem_memoria();
    int *cs1 = cs0 + nr * ncols, *clen = cs1 + nr * ncols;
    int wc[32];
    for (int c = 0; c < ncols; c++) wc[c] = 1;
    for (int r = 0; r < nr; r++) {
        lin_t cel[32];
        int q = celulas(L[r == 0 ? i : i + 1 + r], cel, 32);
        for (int c = 0; c < ncols; c++) {
            int k = r * ncols + c;
            seg_base = nsegs;
            cs0[k] = nsegs;
            if (c < q) inline_rec(cel[c].s, cel[c].n, r == 0 ? E_NEGRITA : 0, -1, 0);
            cs1[k] = nsegs;
            seg_base = nsegs;
            int len = 0;
            for (int s = cs0[k]; s < cs1[k]; s++) len += segs[s].n;
            clen[k] = len;
            if (len > wc[c]) wc[c] = len;
        }
    }
    int pad = mt ? 0 : 6, sep = mt ? 3 : 2 * pad + 1;
    int disp = xmax - x0;
    for (;;) {   /* achica la columna más ancha hasta que quepa */
        int total = sep * (ncols - 1) + (mt ? 0 : 2 * pad + 2), maior = 0;
        for (int c = 0; c < ncols; c++) { total += wc[c] * CW; if (wc[c] > wc[maior]) maior = c; }
        if (total <= disp || wc[maior] <= 4) break;
        wc[maior]--;
    }
    int cx[33];
    cx[0] = x0 + (mt ? 0 : 1 + pad);
    for (int c = 0; c < ncols; c++) cx[c + 1] = cx[c] + wc[c] * CW + sep;
    int largura = mt ? cx[ncols] - sep - x0 : cx[ncols] - sep + pad + 1 - x0;
    int y0 = y_cur;
    int *ylin = aloca((size_t)(nr + 1) * sizeof(int));
    if (!ylin) sem_memoria();
    int cab = mt ? -1 : el_ret(x0, y0, largura, 0, COR_TAB_CAB);
    for (int r = 0; r < nr; r++) {
        int yr = y_cur;
        ylin[r] = yr;
        int zebra = (!mt && r > 0 && r % 2 == 0) ? el_ret(x0, yr, largura, 0, COR_TAB_ZEBRA) : -1;
        int ymax = yr + (mt ? 1 : 22);
        for (int c = 0; c < ncols; c++) {
            int k = r * ncols + c;
            int ye = fluir(cs0[k], cs1[k], cx[c], cx[c] + wc[c] * CW, yr + (mt ? 0 : 2), 1,
                           r == 0 ? COR_TEXTO : cor_base, r == 0 && !mt ? 1 : alin[c]);
            ye += mt ? 0 : 2;
            if (ye > ymax) ymax = ye;
        }
        y_cur = ymax;
        if (zebra >= 0) els[zebra].a = (int16_t)(y_cur - yr);
        if (mt) {
            for (int yy = yr; yy < y_cur; yy++)
                for (int c = 1; c < ncols; c++) el_texto_s(cx[c] - 2, yy, "|", 0, 1, cor_base);
            if (r == 0) {   /* separador del encabezado: ----+----- */
                int off = npool;
                for (int c = 0; c < ncols; c++) {
                    for (int q = 0; q < wc[c] + (c > 0) + (c < ncols - 1); q++) pool_add("-", 1);
                    if (c < ncols - 1) pool_add("+", 1);
                }
                el_texto(x0, y_cur, off, npool - off, 0, -1, 1, cor_base);
                y_cur++;
            }
        } else if (r == 0) {
            els[cab].a = (int16_t)(y_cur - y0);
        }
    }
    ylin[nr] = y_cur;
    if (!mt) {
        for (int r = 0; r <= nr; r++) el_ret(x0, ylin[r], largura, 1, COR_TAB_GRADE);
        el_ret(x0, y0, 1, y_cur - y0, COR_TAB_GRADE);
        for (int c = 1; c <= ncols; c++) el_ret(cx[c] - pad - 1, y0, 1, y_cur - y0 + 1, COR_TAB_GRADE);
        y_cur++;
    }
    n_linhas += nr;
    libera(ylin);
    libera(cs0);
}

typedef struct { int a, b, tarefa; } item_t;

static void bloco_lista(lin_t *L, int *pi, int nl, int x0, int xmax, int prof, bool ordenada0, int num0, char ch0)
{
    int i = *pi;
    lin_t *sub = aloca((size_t)(nl - i + 1) * sizeof(lin_t));
    item_t *it = aloca((size_t)(nl - i + 1) * sizeof(item_t));
    if (!sub || !it) sem_memoria();
    int ns = 0, ni = 0, j = i;
    bool solta = false;
    while (j < nl) {
        int ord, num;
        char ch;
        int p = marcador(L[j], &ord, &num, &ch);
        if (p < 0 || ord != ordenada0 || ch != ch0) break;
        lin_t l = L[j];
        int esp = 0;
        while (p + esp < l.n && l.s[p + esp] == ' ') esp++;
        int w = (p + esp >= l.n || esp > 4) ? p + 1 : p + esp;
        it[ni].a = ns;
        it[ni].tarefa = 0;
        sub[ns++] = cortar(l, w);
        j++;
        bool vaz_ant = false;
        int ultima = ns;
        while (j < nl) {
            lin_t q = L[j];
            if (vazia(q)) { sub[ns++] = cortar(q, q.n); vaz_ant = true; j++; continue; }
            int r = recuo(q);
            if (r >= w) { sub[ns++] = cortar(q, w); vaz_ant = false; ultima = ns; j++; continue; }
            if (!vaz_ant && !inicia_bloco(q) && !vazia(sub[ultima - 1])) { sub[ns++] = cortar(q, r); ultima = ns; j++; continue; }
            break;
        }
        bool vazias_no_fim = ns > ultima;
        for (int k = it[ni].a; k < ultima; k++)
            if (vazia(sub[k]) && k > it[ni].a) {
                /* línea vacía dentro del elemento (fuera de bloque de código) deja la lista "suelta" */
                bool cerca = false;
                for (int q = it[ni].a; q < k; q++) if (eh_cerca(sub[q], NULL)) cerca = !cerca;
                if (!cerca) solta = true;
            }
        ns = ultima;
        it[ni].b = ultima;
        /* lista de tareas: [ ] o [x] */
        lin_t f = sub[it[ni].a];
        if (f.n >= 3 && f.s[0] == '[' && f.s[2] == ']' && (f.s[1] == ' ' || f.s[1] == 'x' || f.s[1] == 'X') &&
            (f.n == 3 || f.s[3] == ' ')) {
            it[ni].tarefa = f.s[1] == ' ' ? 1 : 2;
            sub[it[ni].a] = cortar(f, f.n > 3 ? 4 : 3);
        }
        ni++;
        if (vazias_no_fim) {
            if (j < nl && marcador(L[j], &ord, &num, &ch) >= 0 && ord == ordenada0 && ch == ch0) solta = true;
            else break;
        }
    }
    *pi = j;
    for (int k = 0; k < ni; k++) {
        if (k > 0 && solta) y_cur += GAP;
        int yi = y_cur;
        char mk[16];
        int mn, ind;
        if (it[k].tarefa) {
            copia_texto(mk, it[k].tarefa == 2 ? "[x]" : "[ ]", sizeof mk);
            mn = 3;
            ind = mt ? 4 : 24;
        } else if (ordenada0) {
            mn = formatar_texto(mk, sizeof mk, "%d%c", num0 + k, ch0);
            ind = mt ? mn + 1 : (mn * CW + 10 > 24 ? mn * CW + 10 : 24);
        } else {
            mk[0] = "*-+"[prof % 3]; mk[1] = 0;
            mn = 1;
            ind = mt ? 2 : 22;
        }
        if (prof < MAX_PROF) blocos(sub + it[k].a, it[k].b - it[k].a, x0 + ind, xmax, prof + 1, !solta);
        if (y_cur == yi) y_cur += mt ? 1 : 18;
        if (mt) { el_texto_s(x0, yi, mk, 0, 1, cor_base); continue; }
        if (it[k].tarefa) {
            el_ret(x0 + 4, yi + 3, 13, 13, COR_BORDA);
            el_ret(x0 + 5, yi + 4, 11, 11, it[k].tarefa == 2 ? COR_DESTAQUE : COR_BRANCO);
            if (it[k].tarefa == 2) {
                el_linha(x0 + 7, yi + 9, 2, 3, COR_BRANCO);
                el_linha(x0 + 7, yi + 10, 2, 3, COR_BRANCO);
                el_linha(x0 + 9, yi + 12, 4, -6, COR_BRANCO);
                el_linha(x0 + 9, yi + 13, 4, -6, COR_BRANCO);
            }
        } else if (ordenada0) {
            el_texto_s(x0 + ind - 6 - mn * CW, yi + 1, mk, 0, 1, COR_MARCADOR);
        } else if (prof % 3 == 0) {   /* círculo lleno */
            el_ret(x0 + 8, yi + 7, 6, 4, COR_MARCADOR);
            el_ret(x0 + 9, yi + 6, 4, 6, COR_MARCADOR);
        } else if (prof % 3 == 1) {   /* cuadrado vacío */
            el_ret(x0 + 8, yi + 6, 6, 1, COR_MARCADOR);
            el_ret(x0 + 8, yi + 11, 6, 1, COR_MARCADOR);
            el_ret(x0 + 8, yi + 6, 1, 6, COR_MARCADOR);
            el_ret(x0 + 13, yi + 6, 1, 6, COR_MARCADOR);
        } else {                      /* cuadrado lleno */
            el_ret(x0 + 9, yi + 7, 4, 4, COR_MARCADOR);
        }
    }
    libera(it);
    libera(sub);
}

static void blocos(lin_t *L, int nl, int x0, int xmax, int prof, bool apertado)
{
    bool primeiro = true;
    int i = 0;
#define ESPACO() do { if (!primeiro && !apertado) y_cur += GAP; n_blocos++; } while (0)
    while (i < nl) {
        lin_t l = L[i];
        if (vazia(l)) { i++; continue; }
        int ind = recuo(l), a, b, ord, num;
        char ch;
        /* código con sangría de 4 espacios */
        if (ind >= 4) {
            int j = i;
            while (j < nl && (vazia(L[j]) || recuo(L[j]) >= 4)) j++;
            ESPACO();
            bloco_codigo(L, i, j, 4, x0, xmax);
            i = j;
            primeiro = false;
            continue;
        }
        /* código entre cercas ``` o ~~~ */
        int k = eh_cerca(l, &ch);
        if (k) {
            int j = i + 1;
            while (j < nl) {
                char c2;
                int k2 = eh_cerca(L[j], &c2);
                if (k2 >= k && c2 == ch) {
                    lin_t r = cortar(L[j], recuo(L[j]) + k2);
                    if (vazia(r)) break;
                }
                j++;
            }
            ESPACO();
            bloco_codigo(L, i + 1, j, ind, x0, xmax);
            i = j < nl ? j + 1 : j;
            primeiro = false;
            continue;
        }
        int nivel = eh_titulo(l, &a, &b);
        if (nivel) {
            ESPACO();
            titulo(nivel, l.s + a, b - a, x0, xmax, primeiro);
            i++;
            primeiro = false;
            continue;
        }
        if (eh_regra(l)) {
            ESPACO();
            if (mt) {
                int off = npool;
                for (int q = x0; q < xmax; q++) pool_add("-", 1);
                el_texto(x0, y_cur, off, xmax - x0, 0, -1, 1, cor_base);
                y_cur++;
            } else {
                el_ret(x0, y_cur + 6, xmax - x0, 2, COR_REGRA);
                y_cur += 14;
            }
            i++;
            primeiro = false;
            continue;
        }
        if (eh_citacao(l)) {
            lin_t *sub = aloca((size_t)(nl - i) * sizeof(lin_t));
            if (!sub) sem_memoria();
            int ns = 0, j = i;
            bool ult_vazia = false;
            while (j < nl) {
                lin_t q = L[j];
                if (eh_citacao(q)) {
                    int c = recuo(q) + 1;
                    if (c < q.n && q.s[c] == ' ') c++;
                    sub[ns++] = cortar(q, c);
                    ult_vazia = vazia(sub[ns - 1]);
                    j++;
                } else if (ns > 0 && !vazia(q) && !ult_vazia && !inicia_bloco(q)) {
                    sub[ns++] = q;
                    j++;
                } else break;
            }
            ESPACO();
            int y0 = y_cur;
            if (mt) {
                if (prof < MAX_PROF) blocos(sub, ns, x0 + 2, xmax, prof + 1, false);
                for (int yy = y0; yy < y_cur; yy++) el_texto_s(x0, yy, ">", 0, 1, cor_base);
            } else {
                int fundo = el_ret(x0, y0, xmax - x0, 0, COR_CITA_FUNDO);
                int barra = el_ret(x0, y0, 4, 0, COR_CITA_BARRA);
                y_cur += 5;
                uint32_t antes = cor_base;
                cor_base = COR_CITA_TEXTO;
                if (prof < MAX_PROF) blocos(sub, ns, x0 + 14, xmax - 8, prof + 1, false);
                cor_base = antes;
                y_cur += 5;
                els[fundo].a = (int16_t)(y_cur - y0);
                els[barra].a = (int16_t)(y_cur - y0);
            }
            libera(sub);
            i = j;
            primeiro = false;
            continue;
        }
        if (marcador(l, &ord, &num, &ch) >= 0) {
            ESPACO();
            bloco_lista(L, &i, nl, x0, xmax, prof, ord, num, ch);
            primeiro = false;
            continue;
        }
        int alin[32], ncols = 0;
        if (tem_barra(l) && i + 1 < nl && eh_delim(L[i + 1], alin, 32, &ncols)) {
            lin_t cab[32];
            int qc = celulas(l, cab, 32);
            if (qc == ncols) {
                for (int c = ncols; c < 32; c++) alin[c] = 0;
                int j = i + 2;
                while (j < nl && !vazia(L[j]) && tem_barra(L[j]) && !inicia_bloco(L[j])) j++;
                ESPACO();
                bloco_tabela(L, i, j, ncols, alin, x0, xmax);
                i = j;
                primeiro = false;
                continue;
            }
        }
        /* párrafo (o título setext, si la línea siguiente es === o ---) */
        int j = i + 1, setext = 0;
        while (j < nl && !vazia(L[j])) {
            if ((setext = eh_setext(L[j])) != 0) break;
            if (inicia_bloco(L[j])) break;
            if (tem_barra(L[j]) && j + 1 < nl && eh_delim(L[j + 1], NULL, 0, NULL)) break;
            j++;
        }
        ESPACO();
        juntar(L, i, j);
        if (setext) {
            titulo(setext, tmp, ntmp, x0, xmax, primeiro);
            i = j + 1;
        } else {
            int s0 = nsegs;
            seg_base = nsegs;
            inline_rec(tmp, ntmp, 0, -1, 0);
            y_cur = fluir(s0, nsegs, x0, xmax, y_cur, 1, cor_base, 0);
            i = j;
        }
        primeiro = false;
    }
#undef ESPACO
}

/* Maqueta "texto" (Latin-1, tam bytes) entre x0 y xmax. */
static void maquetar_nucleo(const char *texto, int tam, int x0, int xmax)
{
    nels = npool = nsegs = seg_base = nenls = 0;
    n_blocos = n_linhas = n_titulos = 0;
    cor_base = COR_TEXTO;
    /* copia con tabulaciones expandidas (de 4 en 4) y sin \r */
    int n = 0, col = 0;
    for (int i = 0; i < tam; i++) {
        fonte = crescer(fonte, &cap_fonte, n + 5, 1);
        char c = texto[i];
        if (c == '\r') continue;
        if (c == '\t') { do { fonte[n++] = ' '; col++; } while (col % 4); continue; }
        fonte[n++] = c;
        col = c == '\n' ? 0 : col + 1;
    }
    int nl = 0;
    for (int i = 0, ini = 0; i <= n; i++)
        if (i == n || fonte[i] == '\n') {
            linhas = crescer(linhas, &cap_linhas, nl + 1, sizeof(lin_t));
            linhas[nl].s = fonte + ini;
            linhas[nl].n = i - ini;
            nl++;
            ini = i + 1;
        }
    y_cur = mt ? 0 : TOPO_DOC;
    blocos(linhas, nl, x0, xmax, 0, false);
    doc_alt = y_cur + (mt ? 0 : TOPO_DOC + 10);
}

/* ------------------------------------------------------------ modo texto (-t) */

/* ------------------------------------------------------------ documento y archivo */

static jan_t *j;
static char  *txt;  static int tam, cap_txt;   /* el texto del documento (Latin-1) */
static char   caminho[160];
static bool   modificado;
static int    versao = 1;

static char *ler_arquivo(const char *c, int *n_saida, int *erro)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) { *erro = fd; return NULL; }
    int cap = 8192, n = 0, r;
    char *buf = aloca((size_t)cap);
    if (!buf) sem_memoria();
    for (;;) {
        if (n + 4096 > cap) {
            char *b = realoca(buf, (size_t)cap * 2);
            if (!b) sem_memoria();
            buf = b;
            cap *= 2;
        }
        r = ler(fd, buf + n, (unsigned)(cap - n - 1));
        if (r <= 0) break;
        n += r;
    }
    fechar(fd);
    n = (int)utf8_para_latin1(buf, (size_t)n, NULL);   /* D027 */
    int k = 0;
    for (int i = 0; i < n; i++) if (buf[i] != '\r') buf[k++] = buf[i];
    buf[k] = 0;
    *n_saida = k;
    return buf;
}

/* ------------------------------------------------------------ modo texto */

static int modo_texto(const char *c, int colunas)
{
    int n, erro = 0;
    char *buf = ler_arquivo(c, &n, &erro);
    if (!buf) { imprimir_em(ERROS, "markdown: no se ha podido abrir %s (%s)\n", c, nome_erro(erro)); return 1; }
    mt = true; CW = 1; GAP = 1;
    maquetar_nucleo(buf, n, 0, colunas);
    int linhas_t = doc_alt, larg = 200;
    char *grade = aloca((size_t)(linhas_t + 1) * (size_t)larg);
    if (!grade) sem_memoria();
    memset(grade, ' ', (size_t)(linhas_t + 1) * (size_t)larg);
    for (int k = 0; k < nels; k++) {
        elem_t *e = &els[k];
        if (e->tipo != T_TEXTO || e->y < 0 || e->y >= linhas_t) continue;
        for (int q = 0; q < e->n && e->x + q < larg; q++)
            if (e->x + q >= 0) grade[e->y * larg + e->x + q] = pool[e->off + q];
    }
    while (linhas_t > 0) {   /* quita las líneas vacías del final */
        int q = 0;
        while (q < larg && grade[(linhas_t - 1) * larg + q] == ' ') q++;
        if (q < larg) break;
        linhas_t--;
    }
    /* Junta todo y escribe de una vez: sagsh ejecuta el markdown en segundo
     * plano (es un programa gráfico) y redibuja el prompt en cada escritura. */
    int k = 0;
    for (int y = 0; y < linhas_t; y++) {
        char *l = grade + y * larg;
        int f = larg;
        while (f > 0 && l[f - 1] == ' ') f--;
        memmove(grade + k, l, (size_t)f);
        k += f;
        grade[k++] = '\n';
    }
    escrever_tudo(SAIDA, grade, (unsigned)k);
    libera(grade);
    libera(buf);
    return 0;
}

/* ------------------------------------------------------------ dibujo */

typedef struct { int x, y, l, a; } area_tela_t;
static area_tela_t clip;

static inline void ponto(int x, int y, uint32_t c)
{
    if (x >= clip.x && y >= clip.y && x < clip.x + clip.l && y < clip.y + clip.a)
        j->px[y * j->larg + x] = c;
}

static void ret_clip(int x, int y, int l, int a, uint32_t cor)
{
    int x1 = x + l, y1 = y + a;
    if (x < clip.x) x = clip.x;
    if (y < clip.y) y = clip.y;
    if (x1 > clip.x + clip.l) x1 = clip.x + clip.l;
    if (y1 > clip.y + clip.a) y1 = clip.y + clip.a;
    if (x1 > x && y1 > y) g_retangulo(j, x, y, x1 - x, y1 - y, cor);
}

static void linha_clip(int x0, int y0, int dx, int dy, uint32_t cor)
{
    int passos = dx < 0 ? -dx : dx;
    if ((dy < 0 ? -dy : dy) > passos) passos = dy < 0 ? -dy : dy;
    if (passos == 0) { ponto(x0, y0, cor); return; }
    for (int k = 0; k <= passos; k++) ponto(x0 + dx * k / passos, y0 + dy * k / passos, cor);
}

/* Una letra de la fuente 8x16, con escala, negrita (dibuja de nuevo 1 px a la
 * derecha) y cursiva (cada fila desplazada según la altura). */
static void letra(int x, int y, unsigned char ch, uint32_t cor, int esc, uint16_t est)
{
    if (x + 10 * esc < clip.x || x > clip.x + clip.l || y + 16 * esc < clip.y || y > clip.y + clip.a) return;
    const unsigned char *g = fonte_sagui[ch];
    for (int lin = 0; lin < 16; lin++) {
        unsigned m = (unsigned)g[lin] << 1;
        if (!m) continue;
        if (est & E_NEGRITA) m |= g[lin];
        int dx = (est & E_CURSIVA) ? ((15 - lin) / 4 - 1) * esc : 0;
        for (int col = 0; col < 9; col++)
            if (m & (0x100u >> col)) {
                int bx = x + col * esc + dx, by = y + lin * esc;
                if (esc == 1) ponto(bx, by, cor);
                else for (int a = 0; a < esc; a++) for (int b = 0; b < esc; b++) ponto(bx + b, by + a, cor);
            }
    }
}

static void texto_cor(int x, int y, const char *s, int n, uint32_t cor)
{
    for (int k = 0; k < n; k++) letra(x + k * LETRA_L, y, (unsigned char)s[k], cor, 1, 0);
}

static void barra_rolagem(int x, int y, int a, int total, int vis, int pos, int *pol_y, int *pol_a)
{
    g_retangulo(j, x, y, ROLAGEM, a, 0xE4E7EA);
    g_retangulo(j, x, y, 1, a, 0xD0D4D8);
    int trilho = a - 4, pa, py;
    if (total <= vis) { pa = trilho; py = 0; }
    else {
        pa = trilho * vis / total;
        if (pa < 16) pa = 16;
        py = (trilho - pa) * pos / (total - vis);
    }
    g_retangulo(j, x + 2, y + 2 + py, ROLAGEM - 4, pa, 0x9AA3AC);
    if (pol_y) { *pol_y = y + 2 + py; *pol_a = pa; }
}

/* ------------------------------------------------------------ editor */

static int *li;     static int nli, cap_li;    /* inicio de cada línea */
static uint8_t *est_li; static int cap_est;    /* 1 = dentro de bloque de código */
static int ed_cur, ed_topo, ed_esq, ed_col_desejada = -1;
static int ed_anc = -1;          /* otra punta de la selección (D074); -1 = ninguna */
static bool ed_arrastando;       /* botón apretado en el editor: mover selecciona */

static void ed_reindexar(void)
{
    nli = 0;
    bool cerca = false;
    char ch_cerca = 0;
    int k_cerca = 0;
    for (int i = 0, ini = 0; i <= tam; i++)
        if (i == tam || txt[i] == '\n') {
            li = crescer(li, &cap_li, nli + 2, sizeof(int));
            est_li = crescer(est_li, &cap_est, nli + 2, 1);
            li[nli] = ini;
            lin_t l = { txt + ini, i - ini };
            char ch;
            int k = eh_cerca(l, &ch);
            if (cerca) {
                est_li[nli] = 1;
                if (k >= k_cerca && ch == ch_cerca) cerca = false;
            } else if (k) {
                est_li[nli] = 1;
                cerca = true; ch_cerca = ch; k_cerca = k;
            } else est_li[nli] = 0;
            nli++;
            ini = i + 1;
        }
    li[nli] = tam + 1;
}

static int ed_linha_de(int pos)
{
    int a = 0, b = nli - 1;
    while (a < b) {
        int m = (a + b + 1) / 2;
        if (li[m] <= pos) a = m; else b = m - 1;
    }
    return a;
}

static int ed_tam_linha(int lin) { return li[lin + 1] - 1 - li[lin]; }

static void doc_mudou(void) { ed_reindexar(); versao++; modificado = true; }

static void ed_inserir(const char *s, int n)
{
    txt = crescer(txt, &cap_txt, tam + n + 1, 1);
    memmove(txt + ed_cur + n, txt + ed_cur, (size_t)(tam - ed_cur + 1));
    memcpy(txt + ed_cur, s, (size_t)n);
    tam += n;
    ed_cur += n;
    doc_mudou();
}

static void ed_remover(int pos)
{
    if (pos < 0 || pos >= tam) return;
    memmove(txt + pos, txt + pos + 1, (size_t)(tam - pos));
    tam--;
    doc_mudou();
}

/* ---- selección y portapapeles (D074) */

static bool ed_selecao(int *s0, int *s1)
{
    if (ed_anc < 0 || ed_anc == ed_cur) return false;
    *s0 = ed_anc < ed_cur ? ed_anc : ed_cur;
    *s1 = ed_anc < ed_cur ? ed_cur : ed_anc;
    if (*s1 > tam) *s1 = tam;
    return *s0 < *s1;
}

static bool ed_apagar_selecao(void)
{
    int s0, s1;
    if (!ed_selecao(&s0, &s1)) { ed_anc = -1; return false; }
    memmove(txt + s0, txt + s1, (size_t)(tam - s1 + 1));
    tam -= s1 - s0;
    ed_cur = s0;
    ed_anc = -1;
    doc_mudou();
    return true;
}

static bool ed_copiar(void)
{
    int s0, s1;
    if (!ed_selecao(&s0, &s1)) return false;
    copiar_texto(txt + s0, (unsigned)(s1 - s0));
    registrar("markdown: copiado %d bytes\n", s1 - s0);
    return true;
}

static bool ed_colar(void)
{
    int n = colar_texto(NULL, 0);
    if (n <= 0) return false;
    char *b = aloca((size_t)n + 1);
    if (!b) sem_memoria();
    n = colar_texto(b, (unsigned)n + 1);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r') b[k++] = b[i];
    ed_apagar_selecao();
    ed_inserir(b, k);
    libera(b);
    registrar("markdown: pegado %d bytes\n", k);
    return true;
}

/* Colores de cada letra de una línea del editor (resaltado de Markdown). */
static void colorir(const char *s, int n, int estado, uint32_t *cor, uint8_t *neg)
{
    for (int i = 0; i < n; i++) { cor[i] = COR_TEXTO; neg[i] = 0; }
    lin_t l = { s, n };
    if (estado) { for (int i = 0; i < n; i++) cor[i] = COR_COD; return; }
    if (eh_titulo(l, NULL, NULL)) { for (int i = 0; i < n; i++) { cor[i] = COR_H3; neg[i] = 1; } return; }
    if (eh_regra(l)) { for (int i = 0; i < n; i++) cor[i] = COR_TEXTO_FRACO; return; }
    int i = 0;
    while (i < n) {   /* prefijos de cita y marcadores de lista */
        int r = recuo((lin_t){ s + i, n - i });
        lin_t q = { s + i, n - i };
        if (eh_citacao(q)) { for (int k = i; k <= i + r; k++) cor[k] = COR_H2; i += r + 1; continue; }
        int p = marcador(q, NULL, NULL, NULL);
        if (p >= 0) {
            for (int k = i + r; k < i + p; k++) { cor[k] = COR_MARCADOR; neg[k] = 1; }
            i += p;
            int e = i;
            while (e < n && s[e] == ' ') e++;
            if (e + 2 < n && s[e] == '[' && s[e + 2] == ']') for (int k = e; k < e + 3; k++) cor[k] = COR_MARCADOR;
        }
        break;
    }
    for (; i < n; i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) { cor[i] = cor[i + 1] = COR_TEXTO_FRACO; i++; continue; }
        if (c == '|') { cor[i] = COR_TEXTO_FRACO; continue; }
        if (c == '`') {
            int k = sequencia(s, n, i, '`'), f = i + k;
            while (f < n && !(s[f] == '`' && sequencia(s, n, f, '`') == k)) f++;
            int fim = f < n ? f + k : n;
            for (int q = i; q < fim; q++) cor[q] = COR_COD;
            i = fim - 1;
            continue;
        }
        if ((c == '*' || c == '_' || c == '~') && i + 1 < n && s[i + 1] != ' ') {
            int k = sequencia(s, n, i, c), f = i + k + 1;
            while (f < n && !(s[f] == c && s[f - 1] != ' ' && sequencia(s, n, f, c) >= k)) f++;
            if (f < n) {
                for (int q = i; q < f + k; q++) { cor[q] = COR_CURSIVA; if (k >= 2) neg[q] = 1; }
                i = f + k - 1;
            }
            continue;
        }
        if (c == '[' || (c == '!' && i + 1 < n && s[i + 1] == '[')) {
            int a = c == '!' ? i + 1 : i, f = fecha_colchete(s, n, a), u0, un;
            int fim = f >= 0 ? destino(s, n, f + 1, &u0, &un) : -1;
            if (fim > 0) {
                for (int q = i; q < fim; q++) cor[q] = COR_ENLACE;
                i = fim - 1;
            }
        }
    }
}

/* ------------------------------------------------------------ estado de la ventana */

enum { VISOR, EDITOR, DIVIDIDO };
static int modo = VISOR;
static int desl;                 /* desplazamiento del visor, en píxeles */
static int versao_maq = -1, larg_maq = -1;
static bool arrastando;          /* arrastrando el pulgar de la barra del visor */
static int arraste_dy;
static char aviso[160];          /* mensaje del pie */

typedef struct { int x, y, l, a; } painel_t;

static void paineis(painel_t *ed, painel_t *vi)
{
    int a = ALT - BARRA - RODAPE;
    if (modo == DIVIDIDO) {
        *ed = (painel_t){ 0, BARRA, LARG / 2 - 2, a };
        *vi = (painel_t){ LARG / 2 + 2, BARRA, LARG - LARG / 2 - 2, a };
    } else {
        *ed = (painel_t){ 0, BARRA, LARG, a };
        *vi = *ed;
    }
}

/* maquetar(largura): solo rehace cuando cambiaron el texto o el ancho. */
static void maquetar(int largura)
{
    if (versao_maq == versao && larg_maq == largura) return;
    mt = false; CW = LETRA_L; GAP = 8;
    maquetar_nucleo(txt, tam, MARGEM, largura - MARGEM);
    if (reg_titulos) {
        registrar("markdown: maquetado %d bloques, %d lineas, %d elementos, alto %d px, ancho %d\n",
                  n_blocos, n_linhas, nels, doc_alt, largura);
        for (int k = 0, ult = -1; k < nels; k++)   /* dónde empieza cada enlace (para las pruebas) */
            if (els[k].tipo == T_TEXTO && els[k].enlace > ult) {
                ult = els[k].enlace;
                char u[80];
                int n = enls[ult].n < 79 ? enls[ult].n : 79;
                memcpy(u, pool + enls[ult].off, (size_t)n);
                u[n] = 0;
                registrar("markdown: enlace %d %s en %d,%d\n", ult, u, els[k].x, els[k].y);
            }
    }
    versao_maq = versao;
    larg_maq = largura;
}

static void limitar_desl(int vis)
{
    if (desl > doc_alt - vis) desl = doc_alt - vis;
    if (desl < 0) desl = 0;
}

static int pol_y, pol_a;   /* pulgar de la barra del visor (para arrastrar) */

static void desenhar_visor(void)
{
    painel_t pe, p;
    paineis(&pe, &p);
    int vl = p.l - ROLAGEM;
    maquetar(vl);
    limitar_desl(p.a);
    clip = (area_tela_t){ p.x, p.y, vl, p.a };
    g_retangulo(j, p.x, p.y, vl, p.a, COR_BRANCO);
    int oy = p.y - desl;
    for (int passo = 0; passo < 2; passo++)
        for (int k = 0; k < nels; k++) {
            elem_t *e = &els[k];
            if ((e->tipo == T_TEXTO) != (passo == 1)) continue;
            int ya = e->tipo == T_LINHA ? (e->a < 0 ? e->a : 0) : 0, yb = e->tipo == T_LINHA ? (e->a > 0 ? e->a : 0) + 1 : e->a;
            if (e->y + yb < desl || e->y + ya > desl + p.a) continue;
            int x = p.x + e->x, y = oy + e->y;
            if (e->tipo == T_RET) ret_clip(x, y, e->l, e->a, e->cor);
            else if (e->tipo == T_LINHA) linha_clip(x, y, e->l, e->a, e->cor);
            else {
                int cw = LETRA_L * e->escala;
                for (int q = 0; q < e->n; q++)
                    letra(x + q * cw, y, (unsigned char)pool[e->off + q], e->cor, e->escala, e->estilo);
                if (e->estilo & E_ENLACE) ret_clip(x, y + 15 * e->escala, e->n * cw, e->escala, e->cor);
                if (e->estilo & E_TACHADO) ret_clip(x, y + 8 * e->escala, e->n * cw, 1, e->cor);
            }
        }
    barra_rolagem(p.x + vl, p.y, p.a, doc_alt, p.a, desl, &pol_y, &pol_a);
    jan_mostrar_area(j, p.x, p.y, p.l, p.a);
}

static int ed_visiveis(painel_t p) { return (p.a - 8) / LETRA_A; }
static int ed_colunas(painel_t p) { return (p.l - ROLAGEM - 10) / LETRA_L; }

static void ed_cursor_visivel(void)
{
    painel_t p, pv;
    paineis(&p, &pv);
    int lin = ed_linha_de(ed_cur), col = ed_cur - li[lin], vis = ed_visiveis(p), cols = ed_colunas(p);
    if (lin < ed_topo) ed_topo = lin;
    if (lin >= ed_topo + vis) ed_topo = lin - vis + 1;
    if (ed_topo > nli - 1) ed_topo = nli - 1;
    if (ed_topo < 0) ed_topo = 0;
    if (col < ed_esq) ed_esq = col;
    if (col >= ed_esq + cols) ed_esq = col - cols + 1;
}

static void desenhar_editor(void)
{
    painel_t p, pv;
    paineis(&p, &pv);
    int tl = p.l - ROLAGEM;
    clip = (area_tela_t){ p.x, p.y, tl, p.a };
    g_retangulo(j, p.x, p.y, tl, p.a, COR_BRANCO);
    g_moldura(j, p.x, p.y, tl, p.a, COR_DESTAQUE);
    clip = (area_tela_t){ p.x + 1, p.y + 1, tl - 2, p.a - 2 };
    int vis = ed_visiveis(p), cols = ed_colunas(p);
    int lc = ed_linha_de(ed_cur), cc = ed_cur - li[lc];
    static uint32_t cor[1024];
    static uint8_t neg[1024];
    int s0 = 0, s1 = 0;
    bool tem_sel = ed_selecao(&s0, &s1);
    for (int k = 0; k < vis && ed_topo + k < nli; k++) {
        int lin = ed_topo + k, n = ed_tam_linha(lin), y = p.y + 4 + k * LETRA_A;
        if (n > 1024) n = 1024;
        const char *s = txt + li[lin];
        if (est_li[lin]) ret_clip(p.x + 1, y, tl - 2, LETRA_A, COR_BLOCO);
        if (tem_sel && s0 <= li[lin] + n && s1 > li[lin]) {   /* fondo de la selección */
            int c0 = s0 > li[lin] ? s0 - li[lin] : 0, c1 = s1 - li[lin] > n ? n + 1 : s1 - li[lin];
            if (c0 < ed_esq) c0 = ed_esq;
            if (c1 > c0) ret_clip(p.x + 5 + (c0 - ed_esq) * LETRA_L, y, (c1 - c0) * LETRA_L, LETRA_A, 0xB9D3F2);
        }
        colorir(s, n, est_li[lin], cor, neg);
        for (int c = ed_esq; c < n && c < ed_esq + cols + 1; c++)
            letra(p.x + 5 + (c - ed_esq) * LETRA_L, y, (unsigned char)s[c], cor[c], 1, neg[c] ? E_NEGRITA : 0);
        if (lin == lc && cc >= ed_esq && cc <= ed_esq + cols)
            ret_clip(p.x + 5 + (cc - ed_esq) * LETRA_L, y, 2, LETRA_A, COR_TEXTO);
    }
    barra_rolagem(p.x + tl, p.y, p.a, nli, vis, ed_topo, NULL, NULL);
    jan_mostrar_area(j, p.x, p.y, p.l, p.a);
}

static botao_t bt[6] = {
    { 8, 6, 64, 24, "Nuevo", false }, { 78, 6, 64, 24, "Abrir", false },
    { 148, 6, 72, 24, "Guardar", false }, { 226, 6, 112, 24, "Guardar como", false },
    { 356, 6, 72, 24, "Editar", false }, { 434, 6, 72, 24, "Dividir", false },
};

static const char *nome_curto(void)
{
    if (!caminho[0]) return "sin nombre";
    const char *b = acha_ultimo_char(caminho, '/');
    return b ? b + 1 : caminho;
}

static void atualizar_titulo(void)
{
    static char ultimo[120];
    char t[120];
    formatar_texto(t, sizeof t, "Markdown - %s%s", nome_curto(), modificado ? " *" : "");
    if (compara_texto(t, ultimo) == 0) return;
    copia_texto(ultimo, t, sizeof ultimo);
    titulo_janela(j->id, t);
    registrar("markdown: titulo %s\n", t);
}

static void desenhar_rodape(void)
{
    int y = ALT - RODAPE;
    clip = (area_tela_t){ 0, y, LARG, RODAPE };
    g_retangulo(j, 0, y, LARG, RODAPE, 0xE6E9EC);
    g_retangulo(j, 0, y, LARG, 1, COR_BORDA);
    char dir[64];
    if (modo == VISOR) {
        painel_t pe, p;
        paineis(&pe, &p);
        int max = doc_alt - p.a;
        formatar_texto(dir, sizeof dir, "Visor  %d%%", max > 0 ? desl * 100 / max : 100);
    } else {
        int lin = ed_linha_de(ed_cur);
        formatar_texto(dir, sizeof dir, "Lín %d, col %d", lin + 1, ed_cur - li[lin] + 1);
    }
    const char *esq = aviso[0] ? aviso : caminho[0] ? caminho : "(sin nombre)";
    int n = (int)tam_texto(esq), cabe = (LARG - 16 - g_largura_texto(dir) - 24) / LETRA_L;
    texto_cor(8, y + 2, esq, n < cabe ? n : cabe, aviso[0] ? COR_ENLACE : COR_TEXTO_FRACO);
    texto_cor(LARG - 8 - g_largura_texto(dir), y + 2, dir, (int)tam_texto(dir), COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, y, LARG, RODAPE);
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, LARG, BARRA, 0xE6E9EC);
    g_retangulo(j, 0, BARRA - 1, LARG, 1, COR_BORDA);
    bt[4].texto = modo == VISOR ? "Editar" : "Ver";
    bt[5].texto = modo == DIVIDIDO ? "Una vista" : "Dividir";
    for (int i = 0; i < 6; i++) botao_desenhar(j, &bt[i]);
    if (LARG >= 524 + 14 * LETRA_L + 8) rotulo(j, 524, 10, "Ctrl+E alterna", COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, 0, LARG, BARRA);
}

static void desenhar_tudo(void)
{
    desenhar_barra();
    if (modo == DIVIDIDO) g_retangulo(j, LARG / 2 - 2, BARRA, 4, ALT - BARRA - RODAPE, 0xC4CBD2);
    if (modo != VISOR) desenhar_editor();
    if (modo != EDITOR) desenhar_visor();
    desenhar_rodape();
    jan_mostrar(j);
}

static void definir_texto(const char *s, int n)
{
    txt = crescer(txt, &cap_txt, n + 1, 1);
    memcpy(txt, s, (size_t)n);
    txt[n] = 0;
    tam = n;
    ed_cur = ed_topo = ed_esq = 0;
    ed_anc = -1;
    desl = 0;
    ed_reindexar();
    versao++;
    modificado = false;
}

static bool abrir_arquivo(const char *c)
{
    int n, erro = 0;
    char *buf = ler_arquivo(c, &n, &erro);
    if (!buf) {
        char m[220];
        formatar_texto(m, sizeof m, "No se ha podido abrir\n%s\n(%s)", c, nome_erro(erro));
        mensagem("Markdown", m);
        return false;
    }
    definir_texto(buf, n);
    libera(buf);
    copia_texto(caminho, c, sizeof caminho);
    aviso[0] = 0;
    registrar("markdown: aberto %s (%d bytes)\n", caminho, n);
    return true;
}

static bool salvar_como(void);

/* Graba en UTF-8 (el texto en memoria es Latin-1). */
static bool salvar(void)
{
    if (!caminho[0]) return salvar_como();
    int n = 0;
    for (int i = 0; i < tam; i++) n += (unsigned char)txt[i] >= 0x80 ? 2 : 1;
    char *out = aloca((size_t)n + 1);
    if (!out) sem_memoria();
    int k = 0;
    for (int i = 0; i < tam; i++) {
        unsigned char c = (unsigned char)txt[i];
        if (c >= 0x80) { out[k++] = (char)(0xC0 | (c >> 6)); out[k++] = (char)(0x80 | (c & 0x3F)); }
        else out[k++] = (char)c;
    }
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    bool ok = fd >= 0 && escrever_tudo(fd, out, (unsigned)n) == n;
    if (fd >= 0) fechar(fd);
    libera(out);
    if (!ok) {
        char m[220];
        formatar_texto(m, sizeof m, "No se ha podido guardar\n%s", caminho);
        mensagem("Markdown", m);
        return false;
    }
    modificado = false;
    formatar_texto(aviso, sizeof aviso, "Guardado: %s", caminho);
    registrar("markdown: guardado %s (%d bytes)\n", caminho, n);
    return true;
}

/* Diálogos de archivo (D272): guardar pone la extensión .md; abrir acepta md y txt. */
#define FILTROS_MD_GUARDAR "Markdown (*.md)|*.md"
#define FILTROS_MD_ABRIR "Markdown y texto (*.md;*.txt)|*.md;*.txt|Todos los archivos (*.*)|*"

static bool salvar_como(void)
{
    char c[160];
    copia_texto(c, caminho[0] ? caminho : "/tmp/", sizeof c);
    if (!dialogo_guardar("Guardar como", FILTROS_MD_GUARDAR, c, c, sizeof c) || !c[0]) return false;
    copia_texto(caminho, c, sizeof caminho);
    return salvar();
}

static bool pode_descartar(void)
{
    if (!modificado) return true;
    if (confirmar("Markdown", "El documento tiene cambios sin guardar.\n¿Guardar antes?")) return salvar();
    return true;
}

static void mudar_modo(int novo)
{
    if (novo == modo) return;
    modo = novo;
    registrar("markdown: modo %s\n", modo == VISOR ? "visor" : modo == EDITOR ? "editor" : "dividido");
    if (modo != VISOR) ed_cursor_visivel();
    desenhar_tudo();
}

/* Enlace relativo a otro .md: se resuelve respecto de la carpeta del archivo actual. */
static void seguir_enlace(int idx)
{
    char url[160];
    int n = enls[idx].n < 159 ? enls[idx].n : 159;
    memcpy(url, pool + enls[idx].off, (size_t)n);
    url[n] = 0;
    registrar("markdown: enlace %s\n", url);
    char *h = acha_char(url, '#');
    if (h) *h = 0;
    int u = (int)tam_texto(url);
    bool externo = acha_char(url, ':') != NULL || u == 0;
    if (externo || u < 3 || compara_texto(url + u - 3, ".md") != 0) {
        memcpy(url, pool + enls[idx].off, (size_t)n);   /* de nuevo, con el "#" */
        formatar_texto(aviso, sizeof aviso, externo ? "Enlace externo (no se abre): %s" : "Solo se abren enlaces a archivos .md: %s", url);
        desenhar_rodape();
        return;
    }
    char dest[256];
    if (url[0] == '/') copia_texto(dest, url, sizeof dest);
    else {
        copia_texto(dest, caminho[0] ? caminho : "/x", sizeof dest);
        char *b = acha_ultimo_char(dest, '/');
        if (b) b[1] = 0; else dest[0] = 0;
        junta_texto(dest, url, sizeof dest);
    }
    /* normaliza "." y ".." */
    char norm[256] = "";
    int nn = 0;
    const char *p = dest;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *f = p;
        while (*f && *f != '/') f++;
        int k = (int)(f - p);
        if (k == 1 && p[0] == '.') {}
        else if (k == 2 && p[0] == '.' && p[1] == '.') { while (nn > 0 && norm[nn - 1] != '/') nn--; if (nn > 0) nn--; }
        else if (nn + k + 2 < (int)sizeof norm) { norm[nn++] = '/'; memcpy(norm + nn, p, (size_t)k); nn += k; }
        norm[nn] = 0;
        p = f;
    }
    if (!nn) copia_texto(norm, "/", sizeof norm);
    if (!pode_descartar()) return;
    if (abrir_arquivo(norm)) {
        atualizar_titulo();
        mudar_modo(VISOR);
        desenhar_tudo();
    }
}

static int enlace_em(int x, int y)
{
    painel_t pe, p;
    paineis(&pe, &p);
    int dx = x - p.x, dy = y - p.y + desl;
    for (int k = 0; k < nels; k++) {
        elem_t *e = &els[k];
        if (e->tipo == T_TEXTO && e->enlace >= 0 && dx >= e->x && dx < e->x + e->l && dy >= e->y && dy < e->y + e->a)
            return e->enlace;
    }
    return -1;
}

static bool visor_tecla(unsigned t)
{
    painel_t pe, p;
    paineis(&pe, &p);
    int antes = desl;
    if (t == TECLA_CIMA) desl -= 36;
    else if (t == TECLA_BAIXO) desl += 36;
    else if (t == TECLA_PGUP) desl -= p.a - 36;
    else if (t == TECLA_PGDN || t == ' ') desl += p.a - 36;
    else if (t == TECLA_HOME) desl = 0;
    else if (t == TECLA_END) desl = doc_alt;
    else return false;
    limitar_desl(p.a);
    if (desl != antes) { desenhar_visor(); desenhar_rodape(); }
    return true;
}

static void ed_ir_coluna(int lin, int col)
{
    int n = ed_tam_linha(lin);
    ed_cur = li[lin] + (col < n ? col : n);
}

static bool editor_tecla(unsigned t, unsigned mods)
{
    painel_t p, pv;
    paineis(&p, &pv);
    int lin = ed_linha_de(ed_cur), col = ed_cur - li[lin], vis = ed_visiveis(p);
    bool vertical = false;
    if (ed_col_desejada < 0) ed_col_desejada = col;
    bool mover = t == TECLA_ESQUERDA || t == TECLA_DIREITA || t == TECLA_CIMA || t == TECLA_BAIXO ||
                 t == TECLA_PGUP || t == TECLA_PGDN || t == TECLA_HOME || t == TECLA_END;
    if (mover) {   /* Shift + movimiento extiende la selección; sin Shift, desaparece */
        if (mods & 1) { if (ed_anc < 0) ed_anc = ed_cur; }
        else ed_anc = -1;
    } else if (t == '\b' || t == TECLA_DELETE || t == '\n' || t == '\t' || (t >= 0x20 && t < 0x100 && t != 0x7F)) {
        if (ed_apagar_selecao() && (t == '\b' || t == TECLA_DELETE)) { ed_col_desejada = -1; ed_cursor_visivel(); return true; }
        lin = ed_linha_de(ed_cur); col = ed_cur - li[lin];
    }
    if (t == TECLA_ESQUERDA) { if (ed_cur > 0) ed_cur--; }
    else if (t == TECLA_DIREITA) { if (ed_cur < tam) ed_cur++; }
    else if (t == TECLA_CIMA) { if (lin > 0) ed_ir_coluna(lin - 1, ed_col_desejada); vertical = true; }
    else if (t == TECLA_BAIXO) { if (lin < nli - 1) ed_ir_coluna(lin + 1, ed_col_desejada); vertical = true; }
    else if (t == TECLA_PGUP) {
        int nova = lin - vis < 0 ? 0 : lin - vis;
        ed_topo -= vis; if (ed_topo < 0) ed_topo = 0;
        ed_ir_coluna(nova, ed_col_desejada); vertical = true;
    } else if (t == TECLA_PGDN) {
        int nova = lin + vis > nli - 1 ? nli - 1 : lin + vis;
        ed_topo += vis;
        ed_ir_coluna(nova, ed_col_desejada); vertical = true;
    }
    else if (t == TECLA_HOME) ed_cur = (mods & 2) ? 0 : li[lin];
    else if (t == TECLA_END) ed_cur = (mods & 2) ? tam : li[lin] + ed_tam_linha(lin);
    else if (t == '\b') { if (ed_cur > 0) { ed_cur--; ed_remover(ed_cur); } }
    else if (t == TECLA_DELETE) ed_remover(ed_cur);
    else if (t == '\n') ed_inserir("\n", 1);
    else if (t == '\t') ed_inserir("    ", 4);
    else if (t >= 0x20 && t < 0x100 && t != 0x7F) { char c = (char)t; ed_inserir(&c, 1); }
    else return false;
    if (!vertical) ed_col_desejada = -1;
    ed_cursor_visivel();
    return true;
}

static void editor_clique(int x, int y)
{
    painel_t p, pv;
    paineis(&p, &pv);
    int vis = ed_visiveis(p);
    if (x >= p.x + p.l - ROLAGEM) {
        ed_topo += y < p.y + p.a / 2 ? -vis : vis;
        if (ed_topo > nli - vis) ed_topo = nli - vis;
        if (ed_topo < 0) ed_topo = 0;
        return;
    }
    int lin = ed_topo + (y - p.y - 4) / LETRA_A, col = ed_esq + (x - p.x - 5 + LETRA_L / 2) / LETRA_L;
    if (y < p.y + 4) lin = ed_topo - 1;
    if (lin >= nli) lin = nli - 1;
    if (lin < 0) lin = 0;
    ed_ir_coluna(lin, col < 0 ? 0 : col);
    ed_col_desejada = -1;
}

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

/* La ventana cambió de tamaño (arrastrar el borde, maximizar, restaurar):
 * maqueta el visor con el ancho nuevo y redibuja todo. El desplazamiento del visor
 * queda en la misma fracción del documento (el alto cambia con el ancho). */
static void novo_tamanho(void)
{
    int alto_antes = doc_alt;
    painel_t pe, pv;
    paineis(&pe, &pv);
    maquetar(pv.l - ROLAGEM);
    if (alto_antes > 0) desl = desl * 1000 / alto_antes * doc_alt / 1000;
    limitar_desl(pv.a);
    if (modo != VISOR) ed_cursor_visivel();
    registrar("markdown: ventana %dx%d, visor de %d px de ancho\n", LARG, ALT, pv.l - ROLAGEM);
    desenhar_tudo();
}

/* Rueda del mouse: desplaza el panel debajo del cursor, RODA_LINHAS líneas por
 * muesca (en el visor, una "línea" = la altura de la letra). */
static void rodar(evento_t *e, bool *redesenhar_ed, bool *redesenhar_vi)
{
    int n = (int)e->cliques;
    painel_t pe, pv;
    paineis(&pe, &pv);
    if (modo != EDITOR && dentro(e->x, e->y, pv.x, pv.y, pv.l, pv.a)) {
        int antes = desl;
        desl += n * RODA_LINHAS * LETRA_A;
        limitar_desl(pv.a);
        if (desl != antes) {
            *redesenhar_vi = true;
            registrar("markdown: rueda %d, visor en %d px de %d\n", n, desl, doc_alt);
        }
    } else if (modo != VISOR && dentro(e->x, e->y, pe.x, pe.y, pe.l, pe.a)) {
        int antes = ed_topo, vis = ed_visiveis(pe);
        ed_topo += n * RODA_LINHAS;
        if (ed_topo > nli - vis) ed_topo = nli - vis;
        if (ed_topo < 0) ed_topo = 0;
        if (ed_topo != antes) {
            *redesenhar_ed = true;
            registrar("markdown: rueda %d, editor en la linea %d de %d\n", n, ed_topo, nli);
        }
    }
}

static bool tratar(evento_t *e, bool *redesenhar_ed, bool *redesenhar_vi);

/* Lo que hace cada botón de la barra (también usado por el menú contextual). */
static bool acao_botao(int i, bool *redesenhar_ed, bool *redesenhar_vi)
{
    if (i == 0 && pode_descartar()) {
        definir_texto("", 0);
        caminho[0] = 0;
        aviso[0] = 0;
        if (modo == VISOR) modo = EDITOR;
        desenhar_tudo();
    } else if (i == 1) {
        evento_t f = { .tipo = EV_TECLA, .tecla = 15 };
        return tratar(&f, redesenhar_ed, redesenhar_vi);
    } else if (i == 2) { salvar(); desenhar_rodape(); }
    else if (i == 3) { salvar_como(); desenhar_rodape(); }
    else if (i == 4) mudar_modo(modo == VISOR ? EDITOR : VISOR);
    else if (i == 5) mudar_modo(modo == DIVIDIDO ? EDITOR : DIVIDIDO);
    return true;
}

/* Atiende un evento. Devuelve false para cerrar el programa. */
static bool tratar(evento_t *e, bool *redesenhar_ed, bool *redesenhar_vi)
{
    if (e->tipo == EV_FECHAR) return !pode_descartar();
    if (jan_redimensionar(j, e)) { novo_tamanho(); return true; }
    if (e->tipo == EV_RODA) { rodar(e, redesenhar_ed, redesenhar_vi); return true; }
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla;
        if (t == 5 || t == TECLA_F5_MD) { mudar_modo(modo == VISOR ? EDITOR : VISOR); return true; }   /* Ctrl+E, F5 */
        if (t == 19) { salvar(); desenhar_rodape(); return true; }                                        /* Ctrl+S */
        if (modo != VISOR && (t == 1 || t == 3 || t == 24 || t == 22)) {                                   /* Ctrl+A/C/X/V */
            if (t == 1) { ed_anc = 0; ed_cur = tam; }
            else if (t == 3) { ed_copiar(); return true; }
            else if (t == 24) { if (ed_copiar()) ed_apagar_selecao(); }
            else ed_colar();
            ed_cursor_visivel();
            *redesenhar_ed = true; *redesenhar_vi = modo == DIVIDIDO;
            return true;
        }
        if (t == 15) {                                                                                    /* Ctrl+O */
            if (pode_descartar()) {
                char c[160];
                copia_texto(c, caminho[0] ? caminho : "/", sizeof c);
                if (dialogo_abrir("Abrir", FILTROS_MD_ABRIR, c, c, sizeof c) && abrir_arquivo(c)) { mudar_modo(VISOR); desenhar_tudo(); }
            }
            return true;
        }
        if (modo == VISOR) { visor_tecla(t); return true; }
        if (editor_tecla(t, e->modificadores)) { *redesenhar_ed = true; *redesenhar_vi = modo == DIVIDIDO; aviso[0] = 0; }
        return true;
    }
    /* Menú contextual (D051): las acciones de los botones, sin tener que ir a la barra.
     * Las líneas 0-1 y 3-6 son los botones 4-5 y 0-3 (la 2 es el separador); después
     * de otro separador (7), las de edición (8-11, D074), que en el visor y sin
     * selección aparecen desactivadas. */
    if (e->tipo == EV_CLIQUE && (e->botoes & 2) && e->y >= BARRA) {
        int s0, s1;
        bool ed = modo != VISOR, sel = ed && ed_selecao(&s0, &s1);
        char itens[300];
        formatar_texto(itens, sizeof itens, "%s\tCtrl+E\n%s\n-\nNuevo\nAbrir...\tCtrl+O\nGuardar\tCtrl+S\nGuardar como...\n-\n"
                       "%sCortar\tCtrl+X\n%sCopiar\tCtrl+C\n%sPegar\tCtrl+V\n%sSeleccionar todo\tCtrl+A",
                       modo == VISOR ? "Editar texto" : "Ver documento", modo == DIVIDIDO ? "Solo el editor" : "Vista dividida",
                       sel ? "" : "~", sel ? "" : "~", ed ? "" : "~", ed ? "" : "~");
        menu_contextual(j->id, e->x, e->y, itens, 0);
        return true;
    }
    if (e->tipo == EV_MENU) {
        static const int botao_da_linha[] = { 4, 5, -1, 0, 1, 2, 3 };
        static const unsigned tecla_da_linha[] = { 24, 3, 22, 1 };
        registrar("markdown: menu -> %d\n", e->x);
        if (e->x >= 8 && e->x <= 11) {
            evento_t f = { .tipo = EV_TECLA, .tecla = tecla_da_linha[e->x - 8] };
            return tratar(&f, redesenhar_ed, redesenhar_vi);
        }
        if (e->x < 0 || e->x > 6 || botao_da_linha[e->x] < 0) return true;
        return acao_botao(botao_da_linha[e->x], redesenhar_ed, redesenhar_vi);
    }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU) {
        for (int i = 0; i < 6; i++)
            if (botao_tratar(j, &bt[i], e)) return acao_botao(i, redesenhar_ed, redesenhar_vi);
    }
    painel_t pe, pv;
    paineis(&pe, &pv);
    if (e->tipo == EV_SOLTOU) {
        arrastando = false;
        if (ed_arrastando && ed_anc == ed_cur) ed_anc = -1;
        ed_arrastando = false;
        return true;
    }
    if (e->tipo == EV_MOVER && ed_arrastando && (e->botoes & 1) && modo != VISOR) {
        int antes = ed_cur;
        editor_clique(e->x, e->y);
        if (ed_cur != antes) { ed_cursor_visivel(); *redesenhar_ed = true; }
        return true;
    }
    if (e->tipo == EV_MOVER && arrastando && (e->botoes & 1)) {
        int trilho = pv.a - 4, total = doc_alt - pv.a;
        if (total > 0 && trilho > pol_a) {
            desl = (e->y - arraste_dy - pv.y - 2) * total / (trilho - pol_a);
            limitar_desl(pv.a);
            *redesenhar_vi = true;
        }
        return true;
    }
    if (e->tipo != EV_CLIQUE || !(e->botoes & 1)) return true;
    if (modo != EDITOR && dentro(e->x, e->y, pv.x, pv.y, pv.l, pv.a)) {
        if (e->x >= pv.x + pv.l - ROLAGEM) {
            if (e->y >= pol_y && e->y < pol_y + pol_a) { arrastando = true; arraste_dy = e->y - pol_y; return true; }
            desl += e->y < pol_y ? -(pv.a - 36) : pv.a - 36;
            limitar_desl(pv.a);
            *redesenhar_vi = true;
            return true;
        }
        int idx = enlace_em(e->x, e->y);
        if (idx >= 0) seguir_enlace(idx);
        return true;
    }
    if (modo != VISOR && dentro(e->x, e->y, pe.x, pe.y, pe.l, pe.a)) {
        int antes = ed_cur;
        editor_clique(e->x, e->y);
        if (e->x < pe.x + pe.l - ROLAGEM) {   /* en el texto: Shift extiende, arrastrar selecciona */
            if (e->modificadores & 1) { if (ed_anc < 0) ed_anc = antes; }
            else ed_anc = ed_cur;
            ed_arrastando = true;
        }
        *redesenhar_ed = true;
    }
    return true;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && compara_texto(argv[1], "-t") == 0) {
        if (argc < 3) { escrever_texto(ERROS, "uso: markdown -t archivo.md [columnas]\n"); return 1; }
        int cols = argc > 3 ? texto_para_int(argv[3]) : 78;
        if (cols < 20) cols = 20;
        if (cols > 190) cols = 190;
        return modo_texto(argv[2], cols);
    }
    j = jan_abrir("Markdown - sin nombre", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    reg_titulos = true;
    definir_texto("", 0);
    modo = EDITOR;
    if (argc > 1) {
        info_arquivo_t inf;
        if (info_arquivo(argv[1], &inf) >= 0) {
            if (abrir_arquivo(argv[1])) modo = VISOR;
        } else {
            copia_texto(caminho, argv[1], sizeof caminho);   /* archivo nuevo */
            formatar_texto(aviso, sizeof aviso, "Archivo nuevo: %s", caminho);
        }
    }
    atualizar_titulo();
    desenhar_tudo();
    registrar("markdown: pronto (modo %s)\n", modo == VISOR ? "visor" : "editor");

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        bool red_ed = false, red_vi = false;
        continuar = tratar(&e, &red_ed, &red_vi);
        /* une los eventos que ya llegaron antes de redibujar */
        while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e, &red_ed, &red_vi);
        if (!continuar) break;
        if (red_ed && modo != VISOR) desenhar_editor();
        if (red_vi && modo != EDITOR) {
            reg_titulos = modo != DIVIDIDO;   /* no llena la serial en cada tecla en la vista dividida */
            desenhar_visor();
            reg_titulos = true;
        }
        if (red_ed || red_vi) desenhar_rodape();
        atualizar_titulo();
    }
    registrar("markdown: fechado\n");
    jan_fechar(j);
    return 0;
}

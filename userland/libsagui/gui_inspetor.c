// SPDX-License-Identifier: GPL-3.0-only
/* gui_inspetor.c - inspector de propiedades (D454): rejilla nombre/valor con secciones plegables y un editor por tipo.
 *
 *   entero y punto fijo   campo con flechas (arriba/abajo) y arrastre horizontal (4 px = un paso); clic suelto: se teclea
 *   casilla               clic o espacio
 *   desplegable           menú contextual del sistema (EV_MENU con el código del inspector)
 *   deslizador            arrastrar por la pista
 *   texto                 se teclea (Enter acepta, Esc cancela)
 *   color                 muestra + selector (seletor_cor_dialogo)
 *   archivo               campo + botón «...» (dialogo_abrir)
 * Cada cambio actualiza la propiedad, avisa por el callback (mudou) y devuelve INSPETOR_MUDOU con s->ultimo. Sin coma
 * flotante: el punto fijo guarda el valor multiplicado por 10^decimais. */
#include "gui.h"

#define BARRA_L 6

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

static int pot10(int d) { int r = 1; while (d-- > 0) r *= 10; return r; }

/* ------------------------------------------------------------ constructores */

static ins_prop_t *nova(ins_prop_t *p, int tipo, const char *nome)
{
    memset(p, 0, sizeof *p);
    p->tipo = tipo;
    copia_texto(p->nome, nome, sizeof p->nome);
    p->passo = 1;
    return p;
}

int inspetor_secao(ins_prop_t *p, const char *nome, bool aberta) { nova(p, INS_SECAO, nome)->aberta = aberta; return 0; }
void inspetor_int(ins_prop_t *p, const char *nome, int valor, int min, int max, int passo)
{
    nova(p, INS_INT, nome); p->valor = valor; p->min = min; p->max = max; p->passo = passo ? passo : 1;
}
void inspetor_fixo(ins_prop_t *p, const char *nome, int valor, int decimais, int min, int max, int passo)
{
    nova(p, INS_FIXO, nome); p->valor = valor; p->decimais = decimais; p->min = min; p->max = max; p->passo = passo ? passo : 1;
}
void inspetor_bool(ins_prop_t *p, const char *nome, bool valor) { nova(p, INS_BOOL, nome)->valor = valor ? 1 : 0; }
void inspetor_enum(ins_prop_t *p, const char *nome, int sel, const char *const *opcoes, int n)
{
    nova(p, INS_ENUM, nome); p->valor = sel; p->opcoes = opcoes; p->nopcoes = n; p->min = 0; p->max = n - 1;
}
void inspetor_deslizador(ins_prop_t *p, const char *nome, int valor, int min, int max)
{
    nova(p, INS_DESLIZ, nome); p->valor = valor; p->min = min; p->max = max;
}
void inspetor_texto(ins_prop_t *p, const char *nome, const char *texto)
{
    nova(p, INS_TEXTO, nome); copia_texto(p->texto, texto, sizeof p->texto);
}
void inspetor_cor(ins_prop_t *p, const char *nome, uint32_t argb, bool com_alfa)
{
    nova(p, INS_COR, nome); p->valor = (int)argb; p->com_alfa = com_alfa;
}
void inspetor_arquivo(ins_prop_t *p, const char *nome, const char *caminho, const char *filtro)
{
    nova(p, INS_ARQUIVO, nome); copia_texto(p->texto, caminho, sizeof p->texto); p->filtro = filtro;
}

void inspetor_iniciar(inspetor_t *s, int x, int y, int l, int a, ins_prop_t *p, int n)
{
    memset(s, 0, sizeof *s);
    s->x = x; s->y = y; s->l = l; s->a = a; s->p = p; s->n = n;
    s->col_nome = l * 45 / 100 < 80 ? 80 : l * 45 / 100;
    s->sel = -1; s->ultimo = -1; s->editando = -1; s->arrasto = -1; s->menu_prop = -1;
    s->codigo = 7450;
}

/* ------------------------------------------------------------ filas visibles */

static bool visivel(const inspetor_t *s, int i)
{
    if (s->p[i].tipo == INS_SECAO) return true;
    for (int k = i - 1; k >= 0; k--)
        if (s->p[k].tipo == INS_SECAO) return s->p[k].aberta;
    return true;
}

int inspetor_linhas(const inspetor_t *s)
{
    int v = 0;
    for (int i = 0; i < s->n; i++) if (visivel(s, i)) v++;
    return v;
}

static int fila_prop(const inspetor_t *s, int f)   /* propiedad de la fila visible f, o -1 */
{
    int v = 0;
    for (int i = 0; i < s->n; i++) {
        if (!visivel(s, i)) continue;
        if (v == f) return i;
        v++;
    }
    return -1;
}

static int fila_de(const inspetor_t *s, int prop)
{
    int v = 0;
    for (int i = 0; i < s->n; i++) {
        if (!visivel(s, i)) continue;
        if (i == prop) return v;
        v++;
    }
    return -1;
}

static int filas_caben(const inspetor_t *s) { int v = (s->a - 2) / INS_FILA; return v < 1 ? 1 : v; }

static void limitar_topo(inspetor_t *s)
{
    int max = inspetor_linhas(s) - filas_caben(s);
    if (max < 0) max = 0;
    if (s->topo > max) s->topo = max;
    if (s->topo < 0) s->topo = 0;
}

static void mostrar_prop(inspetor_t *s, int prop)
{
    int f = fila_de(s, prop);
    if (f < 0) return;
    if (f < s->topo) s->topo = f;
    if (f >= s->topo + filas_caben(s)) s->topo = f - filas_caben(s) + 1;
    limitar_topo(s);
}

/* ------------------------------------------------------------ valores */

static void limitar(ins_prop_t *p)
{
    if (p->tipo == INS_ENUM) {
        if (p->valor >= p->nopcoes) p->valor = p->nopcoes - 1;
        if (p->valor < 0) p->valor = 0;
    } else if (p->max > p->min) {
        if (p->valor < p->min) p->valor = p->min;
        if (p->valor > p->max) p->valor = p->max;
    }
}

void inspetor_valor_texto(const ins_prop_t *p, char *dst, int max)
{
    switch (p->tipo) {
    case INS_INT: case INS_DESLIZ: formatar_texto(dst, (size_t)max, "%d", p->valor); break;
    case INS_FIXO: {
        int d = p->decimais, m = pot10(d);
        int v = p->valor < 0 ? -p->valor : p->valor;
        if (d <= 0) formatar_texto(dst, (size_t)max, "%d", p->valor);
        else formatar_texto(dst, (size_t)max, "%s%d,%0*d", p->valor < 0 ? "-" : "", v / m, d, v % m);
        break;
    }
    case INS_BOOL: copia_texto(dst, idioma_traducir_sl(p->valor ? "Sí" : "No"), (size_t)max); break;
    case INS_ENUM: copia_texto(dst, p->valor >= 0 && p->valor < p->nopcoes ? idioma_traducir_sl(p->opcoes[p->valor]) : "", (size_t)max); break;
    case INS_COR:
        if (p->com_alfa) formatar_texto(dst, (size_t)max, "#%06x/%d", (unsigned)p->valor & 0xFFFFFF, ((unsigned)p->valor >> 24) & 255);
        else formatar_texto(dst, (size_t)max, "#%06x", (unsigned)p->valor & 0xFFFFFF);
        break;
    case INS_TEXTO: case INS_ARQUIVO: copia_texto(dst, p->texto, (size_t)max); break;
    default: if (max > 0) dst[0] = 0;
    }
}

/* Texto tecleado -> valor (entero o punto fijo). false = no es un número. */
static bool interpretar(const ins_prop_t *p, const char *t, int *v)
{
    bool neg = false;
    while (*t == ' ') t++;
    if (*t == '-') { neg = true; t++; }
    if (*t < '0' || *t > '9') {
        if (!((*t == ',' || *t == '.') && p->tipo == INS_FIXO)) return false;
    }
    long ent = 0;
    while (*t >= '0' && *t <= '9') { ent = ent * 10 + (*t++ - '0'); if (ent > 100000000) ent = 100000000; }
    long frac = 0;
    int d = p->tipo == INS_FIXO ? p->decimais : 0;
    if (p->tipo == INS_FIXO && (*t == ',' || *t == '.')) {
        t++;
        int k = 0;
        while (*t >= '0' && *t <= '9') { if (k < d) { frac = frac * 10 + (*t - '0'); k++; } t++; }
        while (k < d) { frac *= 10; k++; }
    }
    long r = ent * pot10(d) + frac;
    *v = (int)(neg ? -r : r);
    return true;
}

static void avisar(inspetor_t *s, int i)
{
    s->ultimo = i;
    if (s->mudou) s->mudou(s->ctx, s, i);
}

static void fijar(inspetor_t *s, int i, int valor)
{
    ins_prop_t *p = &s->p[i];
    int v = p->valor;
    p->valor = valor;
    limitar(p);
    if (p->valor != v) avisar(s, i);
}

void inspetor_definir_int(inspetor_t *s, int i, int valor)
{
    if (i >= 0 && i < s->n) fijar(s, i, valor);
}

/* ------------------------------------------------------------ geometría de la fila */

typedef struct { int vx, vl, fy; } cel_t;

static cel_t celda(const inspetor_t *s, int k)   /* k = fila visible desde arriba de la vista */
{
    cel_t c;
    int barra = inspetor_linhas(s) > filas_caben(s) ? BARRA_L : 0;
    c.vx = s->x + s->col_nome;
    c.vl = s->l - s->col_nome - 4 - barra;
    c.fy = s->y + 1 + k * INS_FILA;
    return c;
}

static void texto_corto(jan_t *j, int x, int y, int l, const char *t, uint32_t cor)
{
    int cols = l / LETRA_L;
    if (cols > 0) g_texto_n(j, x, y, t, cols, cor);
}

/* Campo redondeado con el valor dentro. */
static void campo(jan_t *j, int x, int y, int l, int a, bool foco)
{
    g_caixa_red(j, x, y, l, a, 3, mod_tema.campo, foco ? mod_tema.foco : mod_tema.borde);
}

/* Cuadrito de ajedrez para el color con alfa. */
static void muestra_cor(jan_t *j, int x, int y, int l, int a, uint32_t argb, bool alfa)
{
    uint32_t rgb = argb & 0xFFFFFF;
    int al = alfa ? (int)(argb >> 24) & 255 : 255;
    for (int yy = 0; yy < a; yy += 5)
        for (int xx = 0; xx < l; xx += 5) {
            uint32_t f = (((xx / 5) + (yy / 5)) & 1) ? 0xB8B8B8 : 0xF0F0F0;
            g_retangulo(j, x + xx, y + yy, l - xx < 5 ? l - xx : 5, a - yy < 5 ? a - yy : 5, al >= 255 ? rgb : mod_mezclar(f, rgb, (unsigned)al));
        }
    g_moldura(j, x, y, l, a, mod_tema.borde_forte);
}

static void desenhar_valor(jan_t *j, const inspetor_t *s, int i, cel_t c)
{
    const ins_prop_t *p = &s->p[i];
    int vx = c.vx, vl = c.vl, y = c.fy + 2, a = INS_FILA - 4;
    char t[96];
    uint32_t cor = p->desab ? mod_tema.desab_texto : mod_tema.texto;
    bool edit = s->editando == i;
    switch (p->tipo) {
    case INS_INT: case INS_FIXO: {
        campo(j, vx, y, vl, a, edit);
        int ar = vl - 14;
        if (edit) {
            int cols = (ar - 8) / LETRA_L;
            int ini = s->cursor > cols ? s->cursor - cols : 0;
            int n = (int)tam_texto(s->buf) - ini;
            if (s->sel_tudo) g_retangulo(j, vx + 4, y + 1, (int)tam_texto(s->buf) * LETRA_L < ar - 8 ? (int)tam_texto(s->buf) * LETRA_L : ar - 8, a - 2, COR_SEL_SUAVE);
            g_texto_n(j, vx + 4, y + 2, s->buf + ini, n < cols ? n : cols, cor);
            g_retangulo(j, vx + 4 + (s->cursor - ini) * LETRA_L, y + 2, 1, 12, mod_tema.texto);
        } else {
            inspetor_valor_texto(p, t, sizeof t);
            texto_corto(j, vx + 4, y + 2, ar - 8, t, cor);
        }
        if (!p->desab) {
            g_retangulo(j, vx + vl - 14, y + 1, 1, a - 2, mod_tema.borde);
            g_flecha(j, vx + vl - 11, y + 4, 0, mod_tema.texto_fraco);
            g_flecha(j, vx + vl - 11, y + a - 8, 1, mod_tema.texto_fraco);
        }
        break;
    }
    case INS_BOOL: {
        int bx = vx + 2, by = c.fy + 4;
        g_caixa_red(j, bx, by, 14, 14, 3, p->valor ? mod_tema.acento : mod_tema.campo, p->valor ? mod_tema.acento : mod_tema.borde_forte);
        if (p->valor) {
            g_linha(j, bx + 3, by + 7, bx + 5, by + 9, mod_tema.sobre_acento);
            g_linha(j, bx + 5, by + 9, bx + 10, by + 4, mod_tema.sobre_acento);
            g_linha(j, bx + 3, by + 8, bx + 5, by + 10, mod_tema.sobre_acento);
            g_linha(j, bx + 5, by + 10, bx + 10, by + 5, mod_tema.sobre_acento);
        }
        inspetor_valor_texto(p, t, sizeof t);
        texto_corto(j, bx + 22, y + 2, vl - 26, t, mod_tema.texto_fraco);
        break;
    }
    case INS_ENUM:
        campo(j, vx, y, vl, a, s->menu_prop == i);
        inspetor_valor_texto(p, t, sizeof t);
        texto_corto(j, vx + 4, y + 2, vl - 24, t, cor);
        g_flecha(j, vx + vl - 12, y + 7, 1, mod_tema.texto_fraco);
        break;
    case INS_DESLIZ: {
        int tl = vl - 34, ty = c.fy + INS_FILA / 2 - 2;
        int rango = p->max - p->min ? p->max - p->min : 1;
        int pos = (p->valor - p->min) * tl / rango;
        g_retangulo_red(j, vx, ty, tl, 4, 2, mod_tema.pista);
        g_retangulo_red(j, vx, ty, pos < 4 ? 4 : pos, 4, 2, mod_tema.acento);
        g_caixa_red(j, vx + pos - 4, c.fy + 5, 9, 12, 3, 0xFFFFFF, mod_tema.borde_forte);
        inspetor_valor_texto(p, t, sizeof t);
        g_texto(j, vx + tl + 6, y + 2, t, cor);
        break;
    }
    case INS_TEXTO: case INS_ARQUIVO: {
        int bl = p->tipo == INS_ARQUIVO ? 22 : 0;
        campo(j, vx, y, vl - bl - (bl ? 2 : 0), a, edit);
        int fl = vl - bl - (bl ? 2 : 0);
        if (edit) {
            int cols = (fl - 8) / LETRA_L;
            int ini = s->cursor > cols ? s->cursor - cols : 0;
            int n = (int)tam_texto(s->buf) - ini;
            g_texto_n(j, vx + 4, y + 2, s->buf + ini, n < cols ? n : cols, cor);
            g_retangulo(j, vx + 4 + (s->cursor - ini) * LETRA_L, y + 2, 1, 12, mod_tema.texto);
        } else {
            texto_corto(j, vx + 4, y + 2, fl - 8, p->texto, cor);
        }
        if (bl) {
            g_caixa_red(j, vx + vl - bl, y, bl, a, 3, mod_tema.barra, mod_tema.borde);
            g_texto(j, vx + vl - bl + 3, y + 2, "..", mod_tema.texto);
        }
        break;
    }
    case INS_COR:
        muestra_cor(j, vx + 2, c.fy + 4, 30, 14, (uint32_t)p->valor, p->com_alfa);
        inspetor_valor_texto(p, t, sizeof t);
        texto_corto(j, vx + 38, y + 2, vl - 40, t, mod_tema.texto_fraco);
        break;
    }
}

void inspetor_desenhar(jan_t *j, inspetor_t *s)
{
    limitar_topo(s);
    g_retangulo(j, s->x, s->y, s->l, s->a, mod_tema.superficie);
    int cab = filas_caben(s);
    for (int k = 0; k < cab; k++) {
        int i = fila_prop(s, s->topo + k);
        if (i < 0) break;
        cel_t c = celda(s, k);
        const ins_prop_t *p = &s->p[i];
        bool sel = i == s->sel;
        if (p->tipo == INS_SECAO) {
            g_retangulo(j, s->x + 1, c.fy, s->l - 2, INS_FILA, mod_tema.barra);
            g_flecha(j, s->x + 5, c.fy + 9, p->aberta ? 1 : 3, mod_tema.texto);
            const char *n = idioma_traducir_sl(p->nome);
            g_texto(j, s->x + 18, c.fy + 3, n, mod_tema.texto);
            g_texto(j, s->x + 19, c.fy + 3, n, mod_tema.texto);
            continue;
        }
        if (sel) g_retangulo(j, s->x + 1, c.fy, s->l - 2, INS_FILA, s->foco ? mod_tema.acento_suave : mod_tema.hover);
        g_retangulo(j, s->x + 1, c.fy + INS_FILA - 1, s->l - 2, 1, mod_tema.pista);
        texto_corto(j, s->x + 14, c.fy + 3, s->col_nome - 18, idioma_traducir_sl(p->nome), p->desab ? mod_tema.desab_texto : mod_tema.texto);
        g_retangulo(j, s->x + s->col_nome - 3, c.fy, 1, INS_FILA, mod_tema.pista);
        desenhar_valor(j, s, i, c);
    }
    int total = inspetor_linhas(s);
    if (total > cab) {
        int trilho = s->a - 4, pol_a = trilho * cab / total;
        if (pol_a < 8) pol_a = 8;
        int pol_y = (trilho - pol_a) * s->topo / (total - cab);
        g_retangulo_red(j, s->x + s->l - 6, s->y + 2 + pol_y, 4, pol_a, 2, sagui_esq.scroll_polegar);
    }
    g_moldura(j, s->x, s->y, s->l, s->a, s->foco ? mod_tema.foco : mod_tema.borde);
    jan_mostrar_area(j, s->x, s->y, s->l, s->a);
    GR_REG(j, "inspetor", s, s->x, s->y, s->l, s->a, "", 0, 0);
}

/* ------------------------------------------------------------ edición */

static void comecar_edicao(inspetor_t *s, int i)
{
    ins_prop_t *p = &s->p[i];
    if (p->desab) return;
    s->editando = i;
    if (p->tipo == INS_TEXTO || p->tipo == INS_ARQUIVO) { copia_texto(s->buf, p->texto, sizeof s->buf); s->sel_tudo = false; }
    else { inspetor_valor_texto(p, s->buf, sizeof s->buf); s->sel_tudo = true; }
    s->cursor = (int)tam_texto(s->buf);
}

/* Acepta lo escrito. */
static bool terminar_edicao(inspetor_t *s, bool aceitar)
{
    int i = s->editando;
    if (i < 0) return false;
    s->editando = -1;
    s->sel_tudo = false;
    if (!aceitar) return false;
    ins_prop_t *p = &s->p[i];
    if (p->tipo == INS_TEXTO || p->tipo == INS_ARQUIVO) {
        if (compara_texto(p->texto, s->buf) == 0) return false;
        copia_texto(p->texto, s->buf, sizeof p->texto);
        avisar(s, i);
        return true;
    }
    int v;
    if (!interpretar(p, s->buf, &v)) return false;
    int antes = p->valor;
    fijar(s, i, v);
    return p->valor != antes;
}

static bool teclear(inspetor_t *s, const evento_t *e)
{
    ins_prop_t *p = &s->p[s->editando];
    bool numerico = p->tipo == INS_INT || p->tipo == INS_FIXO;
    unsigned t = e->tecla;
    int n = (int)tam_texto(s->buf);
    if (t == TECLA_ESC) { terminar_edicao(s, false); return false; }
    if (t == '\n') return terminar_edicao(s, true);
    if (t == 1) { s->sel_tudo = true; return false; }
    if (t == TECLA_ESQUERDA) { if (s->cursor > 0) s->cursor--; s->sel_tudo = false; return false; }
    if (t == TECLA_DIREITA) { if (s->cursor < n) s->cursor++; s->sel_tudo = false; return false; }
    if (t == TECLA_HOME) { s->cursor = 0; s->sel_tudo = false; return false; }
    if (t == TECLA_END) { s->cursor = n; s->sel_tudo = false; return false; }
    if (s->sel_tudo && (t == '\b' || t == TECLA_DELETE || (t >= 0x20 && t < 0x100))) { s->buf[0] = 0; s->cursor = 0; n = 0; s->sel_tudo = false; if (t == '\b' || t == TECLA_DELETE) return false; }
    if (t == '\b') {
        if (s->cursor > 0) { memmove(s->buf + s->cursor - 1, s->buf + s->cursor, (size_t)(n - s->cursor + 1)); s->cursor--; }
    } else if (t == TECLA_DELETE) {
        if (s->cursor < n) memmove(s->buf + s->cursor, s->buf + s->cursor + 1, (size_t)(n - s->cursor));
    } else if (t >= 0x20 && t < 0x100 && t != 0x7F && n < (int)sizeof s->buf - 1) {
        if (numerico && !((t >= '0' && t <= '9') || t == '-' || ((t == ',' || t == '.') && p->tipo == INS_FIXO))) return false;
        memmove(s->buf + s->cursor + 1, s->buf + s->cursor, (size_t)(n - s->cursor + 1));
        s->buf[s->cursor++] = (char)t;
    }
    return false;
}

static void menu_enum(jan_t *j, inspetor_t *s, int i)
{
    ins_prop_t *p = &s->p[i];
    char menu[512];
    menu[0] = 0;
    for (int k = 0; k < p->nopcoes; k++) {
        if (k) junta_texto(menu, "\n", sizeof menu);
        junta_texto(menu, idioma_traducir_sl(p->opcoes[k]), sizeof menu);
    }
    int f = fila_de(s, i) - s->topo;
    cel_t c = celda(s, f < 0 ? 0 : f);
    s->menu_prop = i;
    menu_contextual(j->id, c.vx, c.fy + INS_FILA, menu, s->codigo);
}

static void escolher_cor(inspetor_t *s, int i)
{
    ins_prop_t *p = &s->p[i];
    uint32_t c = (uint32_t)p->valor;
    if (seletor_cor_dialogo(N_("Elegir color"), &c, p->com_alfa)) fijar(s, i, (int)c);
}

static void escolher_arquivo(inspetor_t *s, int i)
{
    ins_prop_t *p = &s->p[i];
    char saida[96];
    copia_texto(saida, p->texto, sizeof saida);
    if (dialogo_abrir(N_("Elegir archivo"), p->filtro, p->texto, saida, sizeof saida)) {
        copia_texto(p->texto, saida, sizeof p->texto);
        avisar(s, i);
    }
}

int inspetor_tratar(jan_t *j, inspetor_t *s, const evento_t *e)
{
    int r = INSPETOR_NADA;
    if (s->n == 0) return r;
    if (e->tipo == EV_RODA) {
        if (!dentro(e->x, e->y, s->x, s->y, s->l, s->a)) return r;
        int antes = s->topo;
        s->topo += (int)e->cliques * RODA_LINHAS;
        limitar_topo(s);
        if (s->topo != antes) inspetor_desenhar(j, s);
        return r;
    }
    if (e->tipo == EV_MENU && s->menu_prop >= 0 && e->tecla == (unsigned)s->codigo) {
        int i = s->menu_prop;
        s->menu_prop = -1;
        if ((int)e->x >= 0 && (int)e->x < s->p[i].nopcoes) {
            fijar(s, i, (int)e->x);
            r = INSPETOR_MUDOU;
        }
        inspetor_desenhar(j, s);
        return r;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        bool en = dentro(e->x, e->y, s->x, s->y, s->l, s->a);
        s->foco = en;
        if (!en) {
            bool m = terminar_edicao(s, true);
            inspetor_desenhar(j, s);
            return m ? INSPETOR_MUDOU : INSPETOR_NADA;
        }
        int k = (e->y - s->y - 1) / INS_FILA;
        int i = fila_prop(s, s->topo + k);
        if (s->editando >= 0 && s->editando != i) r = terminar_edicao(s, true) ? INSPETOR_MUDOU : INSPETOR_NADA;
        if (i < 0) { inspetor_desenhar(j, s); return r; }
        ins_prop_t *p = &s->p[i];
        s->sel = i;
        if (p->tipo == INS_SECAO) {
            p->aberta = !p->aberta;
            limitar_topo(s);
            inspetor_desenhar(j, s);
            return INSPETOR_SECAO;
        }
        cel_t c = celda(s, k);
        bool en_valor = e->x >= c.vx && e->x < c.vx + c.vl;
        if (en_valor && !p->desab) {
            int rel = e->x - c.vx;
            switch (p->tipo) {
            case INS_INT: case INS_FIXO:
                if (s->editando == i) break;
                if (rel >= c.vl - 14) {
                    bool arriba = e->y - c.fy < INS_FILA / 2;
                    fijar(s, i, p->valor + (arriba ? p->passo : -p->passo));
                    r = INSPETOR_MUDOU;
                } else {
                    s->arrasto = i; s->arr_x0 = e->x; s->arr_v0 = p->valor; s->moveu = false;
                }
                break;
            case INS_BOOL:
                fijar(s, i, !p->valor);
                r = INSPETOR_MUDOU;
                break;
            case INS_ENUM:
                inspetor_desenhar(j, s);
                menu_enum(j, s, i);
                return r;
            case INS_DESLIZ:
                s->arrasto = i;
                {
                    int tl = c.vl - 34;
                    fijar(s, i, p->min + (rel < 0 ? 0 : rel > tl ? tl : rel) * (p->max - p->min) / (tl > 0 ? tl : 1));
                }
                r = INSPETOR_MUDOU;
                break;
            case INS_TEXTO:
                if (s->editando != i) comecar_edicao(s, i);
                break;
            case INS_ARQUIVO:
                if (rel >= c.vl - 22) { inspetor_desenhar(j, s); escolher_arquivo(s, i); r = INSPETOR_MUDOU; }
                else if (s->editando != i) comecar_edicao(s, i);
                break;
            case INS_COR:
                inspetor_desenhar(j, s);
                escolher_cor(s, i);
                r = INSPETOR_MUDOU;
                break;
            }
        }
        inspetor_desenhar(j, s);
        return r;
    }
    if (e->tipo == EV_MOVER && s->arrasto >= 0 && (e->botoes & 1)) {
        ins_prop_t *p = &s->p[s->arrasto];
        int dx = e->x - s->arr_x0;
        if (p->tipo == INS_DESLIZ) {
            int f = fila_de(s, s->arrasto) - s->topo;
            cel_t c = celda(s, f < 0 ? 0 : f);
            int tl = c.vl - 34, rel = e->x - c.vx;
            int v = p->valor;
            fijar(s, s->arrasto, p->min + (rel < 0 ? 0 : rel > tl ? tl : rel) * (p->max - p->min) / (tl > 0 ? tl : 1));
            if (p->valor != v) { inspetor_desenhar(j, s); return INSPETOR_MUDOU; }
            return r;
        }
        if (dx > 3 || dx < -3) s->moveu = true;
        if (!s->moveu) return r;
        int v = p->valor;
        fijar(s, s->arrasto, s->arr_v0 + (dx / 4) * p->passo);
        if (p->valor != v) { inspetor_desenhar(j, s); return INSPETOR_MUDOU; }
        return r;
    }
    if (e->tipo == EV_SOLTOU && s->arrasto >= 0) {
        int i = s->arrasto;
        s->arrasto = -1;
        if (!s->moveu && (s->p[i].tipo == INS_INT || s->p[i].tipo == INS_FIXO)) { comecar_edicao(s, i); inspetor_desenhar(j, s); }
        s->moveu = false;
        return r;
    }
    if (e->tipo != EV_TECLA || !s->foco) return r;
    if (s->editando >= 0) {
        bool m = teclear(s, e);
        mostrar_prop(s, s->sel);
        inspetor_desenhar(j, s);
        return m ? INSPETOR_MUDOU : INSPETOR_NADA;
    }
    unsigned t = e->tecla;
    int f = s->sel >= 0 ? fila_de(s, s->sel) : -1;
    if (t == TECLA_BAIXO || t == TECLA_CIMA) {
        int nf = f + (t == TECLA_BAIXO ? 1 : -1);
        int i = nf >= 0 ? fila_prop(s, nf) : -1;
        if (i >= 0) { s->sel = i; mostrar_prop(s, i); }
        else if (f < 0) { s->sel = fila_prop(s, 0); }
    } else if (s->sel >= 0) {
        ins_prop_t *p = &s->p[s->sel];
        if (p->tipo == INS_SECAO) {
            if (t == '\n' || t == ' ' || t == TECLA_DIREITA || t == TECLA_ESQUERDA) {
                p->aberta = t == TECLA_DIREITA ? true : t == TECLA_ESQUERDA ? false : !p->aberta;
                limitar_topo(s);
                inspetor_desenhar(j, s);
                return INSPETOR_SECAO;
            }
        } else if (!p->desab) {
            if ((p->tipo == INS_INT || p->tipo == INS_FIXO || p->tipo == INS_DESLIZ) && (t == TECLA_DIREITA || t == TECLA_ESQUERDA)) {
                int v = p->valor;
                fijar(s, s->sel, p->valor + (t == TECLA_DIREITA ? p->passo : -p->passo));
                if (p->valor != v) r = INSPETOR_MUDOU;
            } else if (t == '\n' || t == ' ') {
                if (p->tipo == INS_BOOL) { fijar(s, s->sel, !p->valor); r = INSPETOR_MUDOU; }
                else if (p->tipo == INS_ENUM) { inspetor_desenhar(j, s); menu_enum(j, s, s->sel); return r; }
                else if (p->tipo == INS_COR) { inspetor_desenhar(j, s); escolher_cor(s, s->sel); r = INSPETOR_MUDOU; }
                else if (p->tipo == INS_ARQUIVO && t == ' ') { inspetor_desenhar(j, s); escolher_arquivo(s, s->sel); r = INSPETOR_MUDOU; }
                else if (t == '\n' && p->tipo != INS_DESLIZ) comecar_edicao(s, s->sel);
            }
        }
    } else return r;
    inspetor_desenhar(j, s);
    return r;
}

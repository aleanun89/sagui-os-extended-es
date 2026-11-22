// SPDX-License-Identifier: GPL-3.0-only
/* gui_consola.c - consola de salida con filtros (D454): errores, avisos y mensajes en colores, con contadores y
 * botones para filtrar por nivel, copiar lo visible y limpiar. Guarda las últimas CONSOLA_MAX líneas (la más vieja se
 * pierde). Una línea puede remitir a una línea de código (consola_escrever_linha): el doble clic devuelve CONSOLA_IR
 * con la línea en c->ir_linha, para que el editor de código vaya allí. */
#include "gui.h"

#define CON_CAB 28
#define CON_FILA 16

static bool dentro(int px, int py, int x, int y, int l, int a)
{
    return px >= x && py >= y && px < x + l && py < y + a;
}

void consola_iniciar(consola_t *c, int x, int y, int l, int a)
{
    memset(c, 0, sizeof *c);
    c->x = x; c->y = y; c->l = l; c->a = a;
    c->mascara = 7;
    c->sel = -1;
    c->seguir = true;
}

void consola_limpar(consola_t *c)
{
    c->n = 0;
    memset(c->cont, 0, sizeof c->cont);
    c->topo = 0; c->sel = -1; c->seguir = true;
}

void consola_escrever_linha(consola_t *c, int nivel, int linha, const char *texto)
{
    if (nivel < 0 || nivel > 2) nivel = CON_MSG;
    if (c->n >= CONSOLA_MAX) {
        c->cont[c->lin[0].nivel]--;
        memmove(c->lin, c->lin + 1, (size_t)(CONSOLA_MAX - 1) * sizeof c->lin[0]);
        c->n--;
        if (c->sel > 0) c->sel--;
    }
    con_linha_t *l = &c->lin[c->n++];
    copia_texto(l->texto, texto, sizeof l->texto);
    l->nivel = (unsigned char)nivel;
    l->linha = linha;
    c->cont[nivel]++;
}

void consola_escrever(consola_t *c, int nivel, const char *texto) { consola_escrever_linha(c, nivel, 0, texto); }

int consola_visiveis(const consola_t *c)
{
    int v = 0;
    for (int i = 0; i < c->n; i++) if (c->mascara & (1u << c->lin[i].nivel)) v++;
    return v;
}

static const con_linha_t *fila(const consola_t *c, int f)
{
    int v = 0;
    for (int i = 0; i < c->n; i++) {
        if (!(c->mascara & (1u << c->lin[i].nivel))) continue;
        if (v == f) return &c->lin[i];
        v++;
    }
    return NULL;
}

static int filas_caben(const consola_t *c) { int v = (c->a - CON_CAB - 2) / CON_FILA; return v < 1 ? 1 : v; }

static void limitar(consola_t *c)
{
    int max = consola_visiveis(c) - filas_caben(c);
    if (max < 0) max = 0;
    if (c->seguir) c->topo = max;
    if (c->topo > max) c->topo = max;
    if (c->topo < 0) c->topo = 0;
}

int consola_copiar(consola_t *c)
{
    int total = 0, v = consola_visiveis(c);
    for (int f = 0; f < v; f++) total += (int)tam_texto(fila(c, f)->texto) + 1;
    char *buf = aloca((size_t)total + 1);
    if (!buf) return 0;
    int k = 0;
    for (int f = 0; f < v; f++) {
        const con_linha_t *l = fila(c, f);
        int n = (int)tam_texto(l->texto);
        memcpy(buf + k, l->texto, (size_t)n);
        k += n;
        buf[k++] = '\n';
    }
    buf[k] = 0;
    int g = copiar_texto(buf, (unsigned)k);
    libera(buf);
    c->copiado = g;
    return g;
}

/* Los cinco botones de la cabecera: tres filtros, Copiar y Limpiar. */
typedef struct { int x, l; char txt[32]; } con_boton_t;

static void botones(const consola_t *c, con_boton_t b[5])
{
    static const char *const nom[3] = { N_("Mensajes"), N_("Avisos"), N_("Errores") };
    static const int nivel[3] = { CON_MSG, CON_AVISO, CON_ERRO };
    int x = c->x + 4;
    for (int k = 0; k < 5; k++) {
        if (k < 3) formatar_texto(b[k].txt, sizeof b[k].txt, "%s %d", idioma_traducir_sl(nom[k]), c->cont[nivel[k]]);
        else copia_texto(b[k].txt, idioma_traducir_sl(k == 3 ? N_("Copiar") : N_("Limpiar")), sizeof b[k].txt);
        b[k].l = (int)tam_texto(b[k].txt) * LETRA_L + 20;
        if (k == 3) x = c->x + c->l - 4 - b[3].l - 4 - (int)(tam_texto(idioma_traducir_sl(N_("Limpiar"))) * LETRA_L + 20);
        b[k].x = x;
        x += b[k].l + 4;
    }
}

bool consola_boton(const consola_t *c, int k, int *x, int *y, int *l, int *a)
{
    con_boton_t b[5];
    if (k < 0 || k > 4) return false;
    botones(c, b);
    if (x) *x = b[k].x;
    if (y) *y = c->y + 3;
    if (l) *l = b[k].l;
    if (a) *a = 22;
    return true;
}

static int boton_em(const consola_t *c, int x, int y)
{
    con_boton_t b[5];
    botones(c, b);
    if (y < c->y + 3 || y >= c->y + 3 + 22) return -1;
    for (int k = 0; k < 5; k++) if (x >= b[k].x && x < b[k].x + b[k].l) return k;
    return -1;
}

void consola_desenhar(jan_t *j, consola_t *c)
{
    limitar(c);
    g_retangulo(j, c->x, c->y, c->l, c->a, mod_tema.superficie);
    g_retangulo(j, c->x, c->y, c->l, CON_CAB, mod_tema.barra);
    g_retangulo(j, c->x, c->y + CON_CAB - 1, c->l, 1, mod_tema.borde);
    con_boton_t b[5];
    botones(c, b);
    for (int k = 0; k < 5; k++) {
        mbotao_t m;
        memset(&m, 0, sizeof m);
        m.x = b[k].x; m.y = c->y + 3; m.l = b[k].l; m.a = 22;
        m.variante = k < 3 ? MB_SECUNDARIO : MB_PLANO;
        m.texto = b[k].txt;
        m.fundo = mod_tema.barra;
        m.ativo = k < 3 && (c->mascara & (1u << (k == 0 ? CON_MSG : k == 1 ? CON_AVISO : CON_ERRO)));
        mbotao_desenhar(j, &m);
    }
    int cab = filas_caben(c);
    for (int k = 0; k < cab; k++) {
        const con_linha_t *l = fila(c, c->topo + k);
        if (!l) break;
        int y = c->y + CON_CAB + 1 + k * CON_FILA;
        uint32_t cor = l->nivel == CON_ERRO ? mod_tema.peligro : l->nivel == CON_AVISO ? mod_tema.aviso : mod_tema.texto;
        bool sel = c->topo + k == c->sel;
        if (sel) g_retangulo(j, c->x + 1, y, c->l - 2, CON_FILA, mod_tema.acento_suave);
        else if (l->nivel == CON_ERRO) g_retangulo(j, c->x + 1, y, c->l - 2, CON_FILA, mod_tema.peligro_suave);
        if (l->nivel != CON_MSG) g_retangulo(j, c->x + 1, y, 3, CON_FILA, cor);
        int cols = (c->l - 16) / LETRA_L;
        g_texto_n(j, c->x + 8, y, l->texto, cols, l->nivel == CON_MSG ? mod_tema.texto : cor);
    }
    int total = consola_visiveis(c);
    if (total > cab) {
        int trilho = c->a - CON_CAB - 4, pol_a = trilho * cab / total;
        if (pol_a < 8) pol_a = 8;
        int pol_y = (trilho - pol_a) * c->topo / (total - cab);
        g_retangulo_red(j, c->x + c->l - 6, c->y + CON_CAB + 2 + pol_y, 4, pol_a, 2, sagui_esq.scroll_polegar);
    }
    g_moldura(j, c->x, c->y, c->l, c->a, mod_tema.borde);
    jan_mostrar_area(j, c->x, c->y, c->l, c->a);
    GR_REG(j, "consola", c, c->x, c->y, c->l, c->a, "", 0, 0);
}

int consola_tratar(jan_t *j, consola_t *c, const evento_t *e)
{
    if (e->tipo == EV_RODA) {
        if (!dentro(e->x, e->y, c->x, c->y, c->l, c->a)) return CONSOLA_NADA;
        int max = consola_visiveis(c) - filas_caben(c);
        c->seguir = false;
        c->topo += (int)e->cliques * RODA_LINHAS;
        if (c->topo < 0) c->topo = 0;
        if (c->topo >= max) { c->topo = max < 0 ? 0 : max; c->seguir = true; }
        consola_desenhar(j, c);
        return CONSOLA_NADA;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && dentro(e->x, e->y, c->x, c->y, c->l, c->a)) {
        int k = boton_em(c, e->x, e->y);
        if (k >= 0) {
            if (k < 3) c->mascara ^= 1u << (k == 0 ? CON_MSG : k == 1 ? CON_AVISO : CON_ERRO);
            else if (k == 3) consola_copiar(c);
            else consola_limpar(c);
            c->sel = -1;
            consola_desenhar(j, c);
            return CONSOLA_MUDOU;
        }
        if (e->y >= c->y + CON_CAB) {
            int f = c->topo + (e->y - c->y - CON_CAB - 1) / CON_FILA;
            const con_linha_t *l = fila(c, f);
            if (!l) { c->sel = -1; consola_desenhar(j, c); return CONSOLA_NADA; }
            c->sel = f;
            consola_desenhar(j, c);
            if (e->cliques >= 2 && l->linha > 0) { c->ir_linha = l->linha; return CONSOLA_IR; }
            return CONSOLA_MUDOU;
        }
    }
    return CONSOLA_NADA;
}

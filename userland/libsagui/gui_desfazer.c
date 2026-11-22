// SPDX-License-Identifier: GPL-3.0-only
/* gui_desfazer.c - deshacer/rehacer genérico y arrastrar y soltar entre componentes (D454).
 *
 * Deshacer: una pila de órdenes. Cada una trae una función aplicar(dados, refazer) que la deshace (refazer = false) o la
 * vuelve a hacer (true); se registra DESPUÉS de ejecutarla. Las que se registran entre desfazer_agrupar_inicio y
 * _fim forman un grupo que se deshace y rehace de una vez. Registrar una orden nueva descarta lo que había por rehacer.
 *
 * Arrastrar y soltar: quien empieza un arrastre llama a arrsol_preparar en el clic; los destinos se registran con
 * arrsol_alvo (cada repintado: arrsol_alvos_limpar y volver a registrar) y reciben ENTRA / MOVE / SAI / SOLTA con el
 * elemento (tipo, datos, rótulo). Mientras dura, arrsol_tratar se queda con los eventos del ratón y dibuja el rótulo
 * junto al puntero (restaurando lo de debajo); Esc cancela. */
#include "gui.h"

/* ------------------------------------------------------------ deshacer */

void ed_desfazer_iniciar(pilha_desfazer_t *p, int max)
{
    memset(p, 0, sizeof *p);
    p->max = max > 0 ? max : 100;
    p->prox = 1;
}

static void soltar(ordem_t *o)
{
    if (o->liberar) o->liberar(o->dados);
    o->dados = NULL;
}

void ed_desfazer_liberar(pilha_desfazer_t *p)
{
    for (int i = 0; i < p->n; i++) soltar(&p->o[i]);
    if (p->o) libera(p->o);
    p->o = NULL; p->n = p->cap = p->pos = 0;
}

void ed_desfazer_limpar(pilha_desfazer_t *p)
{
    for (int i = 0; i < p->n; i++) soltar(&p->o[i]);
    p->n = p->pos = 0;
    p->grupo = 0; p->prof = 0;
}

bool ed_desfazer_registrar(pilha_desfazer_t *p, const char *nome, void (*aplicar)(void *, bool), void (*liberar)(void *), void *dados)
{
    for (int i = p->pos; i < p->n; i++) soltar(&p->o[i]);   /* lo rehacible se pierde */
    p->n = p->pos;
    if (p->n >= p->max) {   /* se olvida lo más viejo (un grupo entero si lo parte) */
        int g = p->o[0].grupo, k = 0;
        while (k < p->n && (k == 0 || p->o[k].grupo == g)) { soltar(&p->o[k]); k++; }
        memmove(p->o, p->o + k, (size_t)(p->n - k) * sizeof *p->o);
        p->n -= k; p->pos = p->n;
    }
    if (p->n >= p->cap) {
        int nc = p->cap ? p->cap * 2 : 16;
        ordem_t *no = realoca(p->o, (size_t)nc * sizeof *no);
        if (!no) { if (liberar) liberar(dados); return false; }
        p->o = no; p->cap = nc;
    }
    ordem_t *o = &p->o[p->n++];
    o->aplicar = aplicar; o->liberar = liberar; o->dados = dados;
    copia_texto(o->nome, p->prof > 0 ? p->nome_grupo : nome, sizeof o->nome);
    o->grupo = p->prof > 0 ? p->grupo : p->prox++;
    p->pos = p->n;
    return true;
}

void ed_desfazer_agrupar_inicio(pilha_desfazer_t *p, const char *nome)
{
    if (p->prof++ == 0) {
        p->grupo = p->prox++;
        copia_texto(p->nome_grupo, nome ? nome : "", sizeof p->nome_grupo);
    }
}

void ed_desfazer_agrupar_fim(pilha_desfazer_t *p)
{
    if (p->prof > 0 && --p->prof == 0) p->grupo = 0;
}

bool ed_desfazer_pode(const pilha_desfazer_t *p) { return p->pos > 0; }
bool ed_refazer_pode(const pilha_desfazer_t *p) { return p->pos < p->n; }
const char *ed_desfazer_nome(const pilha_desfazer_t *p) { return p->pos > 0 ? p->o[p->pos - 1].nome : NULL; }
const char *ed_refazer_nome(const pilha_desfazer_t *p) { return p->pos < p->n ? p->o[p->pos].nome : NULL; }
void *ed_desfazer_topo(const pilha_desfazer_t *p) { return p->pos > 0 ? p->o[p->pos - 1].dados : NULL; }

bool ed_desfazer(pilha_desfazer_t *p)
{
    if (p->pos <= 0) return false;
    int g = p->o[p->pos - 1].grupo;
    while (p->pos > 0 && p->o[p->pos - 1].grupo == g) {
        p->pos--;
        if (p->o[p->pos].aplicar) p->o[p->pos].aplicar(p->o[p->pos].dados, false);
    }
    return true;
}

bool ed_refazer(pilha_desfazer_t *p)
{
    if (p->pos >= p->n) return false;
    int g = p->o[p->pos].grupo;
    while (p->pos < p->n && p->o[p->pos].grupo == g) {
        if (p->o[p->pos].aplicar) p->o[p->pos].aplicar(p->o[p->pos].dados, true);
        p->pos++;
    }
    return true;
}

/* ------------------------------------------------------------ arrastrar y soltar */

#define FANT_L 140
#define FANT_A 20

static struct {
    int x, y, l, a;
    arrsol_aviso_t cb;
    void *ctx;
} alvos[ARRSOL_ALVOS_MAX];
static int n_alvos;
static arrsol_item_t item;
static bool pendente, ativo;
static int px0, py0, atual = -1;
static bool atual_aceita;
static uint32_t fant_salvo[FANT_L * FANT_A];
static int fant_x, fant_y, fant_l, fant_a;
static bool fant_visivel;
static int ult_x, ult_y;   /* último punto visto: EV_SOLTOU no trae posición fiable */

void arrsol_alvos_limpar(void) { n_alvos = 0; }   /* atual se conserva: los destinos se registran siempre en el mismo orden */

int arrsol_alvo(int x, int y, int l, int a, arrsol_aviso_t cb, void *ctx)
{
    if (n_alvos >= ARRSOL_ALVOS_MAX) return -1;
    alvos[n_alvos].x = x; alvos[n_alvos].y = y; alvos[n_alvos].l = l; alvos[n_alvos].a = a;
    alvos[n_alvos].cb = cb; alvos[n_alvos].ctx = ctx;
    return n_alvos++;
}

void arrsol_preparar(const arrsol_item_t *it, int x, int y)
{
    item = *it;
    pendente = true; ativo = false;
    px0 = x; py0 = y;
    atual = -1; atual_aceita = false;
    fant_visivel = false;
}

bool arrsol_ativo(void) { return ativo; }
const arrsol_item_t *arrsol_item(void) { return &item; }
int  arrsol_alvo_atual(void) { return atual_aceita ? atual : -1; }

static void fantasma_restaurar(jan_t *j)
{
    if (!fant_visivel) return;
    fant_visivel = false;
    for (int y = 0; y < fant_a; y++)
        memcpy(j->px + (fant_y + y) * j->larg + fant_x, fant_salvo + y * FANT_L, (size_t)fant_l * 4);
    jan_mostrar_area(j, fant_x, fant_y, fant_l, fant_a);
}

static void fantasma_desenhar(jan_t *j, int x, int y)
{
    int l = (int)tam_texto(item.rotulo) * LETRA_L + 12;
    if (l > FANT_L) l = FANT_L;
    int a = FANT_A;
    int fx = x + 12, fy = y + 12;
    if (fx + l > j->larg) fx = j->larg - l;
    if (fy + a > j->alt) fy = y - a - 4;
    if (fx < 0 || fy < 0) return;
    for (int yy = 0; yy < a; yy++) memcpy(fant_salvo + yy * FANT_L, j->px + (fy + yy) * j->larg + fx, (size_t)l * 4);
    fant_x = fx; fant_y = fy; fant_l = l; fant_a = a; fant_visivel = true;
    g_caixa_red(j, fx, fy, l, a, 4, atual_aceita ? mod_tema.acento : mod_tema.dica_fundo, atual_aceita ? mod_tema.acento_press : mod_tema.borde_forte);
    g_texto_n(j, fx + 6, fy + 2, item.rotulo, (l - 12) / LETRA_L, atual_aceita ? mod_tema.sobre_acento : mod_tema.dica_texto);
    jan_mostrar_area(j, fx, fy, l, a);
}

static int alvo_em(int x, int y)
{
    for (int i = n_alvos - 1; i >= 0; i--)
        if (x >= alvos[i].x && y >= alvos[i].y && x < alvos[i].x + alvos[i].l && y < alvos[i].y + alvos[i].a) return i;
    return -1;
}

static void sair_do_alvo(int x, int y)
{
    if (atual >= 0 && atual < n_alvos && alvos[atual].cb) alvos[atual].cb(alvos[atual].ctx, ARRSOL_SAI, &item, x, y);
    atual = -1; atual_aceita = false;
}

int arrsol_tratar(jan_t *j, const evento_t *e)
{
    if (!pendente && !ativo) return ARRSOL_NADA;
    if (e->tipo == EV_SOLTOU) {
        if (!ativo) { pendente = false; return ARRSOL_NADA; }
        fantasma_restaurar(j);
        int ex = ult_x, ey = ult_y;
        int i = alvo_em(ex, ey);
        bool consumido = false;
        if (i == atual && atual_aceita && alvos[i].cb) consumido = alvos[i].cb(alvos[i].ctx, ARRSOL_SOLTA, &item, ex, ey);
        else if (i >= 0 && alvos[i].cb) {   /* soltó sin haber pasado por encima (sin ratón intermedio) */
            if (alvos[i].cb(alvos[i].ctx, ARRSOL_ENTRA, &item, ex, ey)) consumido = alvos[i].cb(alvos[i].ctx, ARRSOL_SOLTA, &item, ex, ey);
        }
        if (!consumido && atual >= 0) sair_do_alvo(ex, ey);
        ativo = false; pendente = false; atual = -1; atual_aceita = false;
        return ARRSOL_FIM;
    }
    if (e->tipo == EV_TECLA && e->tecla == TECLA_ESC && ativo) {
        fantasma_restaurar(j);
        sair_do_alvo(e->x, e->y);
        ativo = false; pendente = false;
        return ARRSOL_CANCELOU;
    }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) {
        ult_x = e->x; ult_y = e->y;
        if (!ativo) {
            int dx = e->x - px0, dy = e->y - py0;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx <= 4 && dy <= 4) return ARRSOL_NADA;
            ativo = true;
        }
        fantasma_restaurar(j);
        int i = alvo_em(e->x, e->y);
        if (i != atual) {
            sair_do_alvo(e->x, e->y);
            if (i >= 0 && alvos[i].cb) { atual = i; atual_aceita = alvos[i].cb(alvos[i].ctx, ARRSOL_ENTRA, &item, e->x, e->y); }
        } else if (atual >= 0 && alvos[atual].cb) {
            atual_aceita = alvos[atual].cb(alvos[atual].ctx, ARRSOL_MOVE, &item, e->x, e->y);
        }
        fantasma_desenhar(j, e->x, e->y);
        return ARRSOL_ATIVO;
    }
    if (ativo && (e->tipo == EV_CLIQUE)) return ARRSOL_ATIVO;
    return ARRSOL_NADA;
}

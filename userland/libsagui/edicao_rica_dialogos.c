// SPDX-License-Identifier: GPL-2.0-or-later
/* edicao_rica_dialogos.c - diálogos del control de texto con formato (mejora 116, D291):
 * Fuente, Párrafo y Buscar/Reemplazar. Ventanas modales con listas, casillas y cuadros de
 * texto del kit gráfico. */
#include "gui.h"

#define CTL_MAX 12

/* ------------------------------------------------------------ cm <-> twips */

int twips_de_cm_texto(const char *t)
{
    int neg = 0, ent = 0, frac = 0, dig = 0;
    while (*t == ' ') t++;
    if (*t == '-') { neg = 1; t++; }
    while (*t >= '0' && *t <= '9') ent = ent * 10 + (*t++ - '0');
    if (*t == ',' || *t == '.') {
        t++;
        while (*t >= '0' && *t <= '9' && dig < 2) { frac = frac * 10 + (*t++ - '0'); dig++; }
        if (dig == 1) frac *= 10;
    }
    int v = ent * 567 + frac * 567 / 100;
    return neg ? -v : v;
}

char *cm_texto_de_twips(char *d, int max, int tw)
{
    int neg = tw < 0;
    if (neg) tw = -tw;
    int cent = (tw * 100 + 283) / 567;
    formatar_texto(d, (size_t)max, "%s%d,%02d", neg ? "-" : "", cent / 100, cent % 100);
    return d;
}

/* ------------------------------------------------------------ utilidades de diálogo */

static bool en(int px, int py, int x, int y, int l, int a) { return px >= x && py >= y && px < x + l && py < y + a; }

static void etiqueta(jan_t *j, int x, int y, const char *t) { rotulo(j, x, y, t, W98_NEGRO); }

static void fondo(jan_t *j) { g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE); }

void boton98(jan_t *j, botao_t *b)
{
    g_retangulo(j, b->x, b->y, b->l, b->a, W98_FACE);
    g_bisel(j, b->x, b->y, b->l, b->a, b->apertado);
    int tx = b->x + (b->l - g_largura_texto(b->texto)) / 2 + (b->apertado ? 1 : 0);
    g_texto(j, tx, b->y + (b->a - LETRA_A) / 2 + (b->apertado ? 1 : 0), b->texto, W98_NEGRO);
    jan_mostrar_area(j, b->x, b->y, b->l, b->a);
    GR_REG(j, "botao", b, b->x, b->y, b->l, b->a, b->texto, b->l - 2, GRF_CONTROL);
}

bool boton98_tratar(jan_t *j, botao_t *b, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && en(e->x, e->y, b->x, b->y, b->l, b->a)) { b->apertado = true; boton98(j, b); return false; }
    if (e->tipo == EV_SOLTOU && b->apertado) { b->apertado = false; boton98(j, b); return en(e->x, e->y, b->x, b->y, b->l, b->a); }
    return false;
}

void casilla98(jan_t *j, casilla_t *c, const char *texto)
{
    g_retangulo(j, c->x, c->y, 13, 13, COR_BRANCO);
    g_bisel(j, c->x, c->y, 13, 13, true);
    if (c->marcada) {
        for (int i = 0; i < 3; i++) { g_retangulo(j, c->x + 3 + i, c->y + 5 + i, 1, 3, W98_NEGRO); }
        for (int i = 0; i < 4; i++) { g_retangulo(j, c->x + 6 + i, c->y + 7 - i, 1, 3, W98_NEGRO); }
    }
    rotulo(j, c->x + 20, c->y - 2, texto, W98_NEGRO);
    jan_mostrar_area(j, c->x, c->y - 2, 20 + g_largura_texto(texto), 18);
    GR_REG(j, "casilla", c, c->x, c->y, 13, 13, "", 0, 0);
}

bool casilla98_tratar(jan_t *j, casilla_t *c, const char *texto, const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && en(e->x, e->y, c->x, c->y - 2, 24 + g_largura_texto(texto), 17)) {
        c->marcada = !c->marcada;
        casilla98(j, c, texto);
        return true;
    }
    return false;
}

void lista_en_marco(jan_t *j, lista_t *l)
{
    g_bisel(j, l->x - 2, l->y - 2, l->l + 4, l->a + 4, true);
    lista_desenhar(j, l);
}

/* ------------------------------------------------------------ Fuente */

static const int tams[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
#define N_TAMS ((int)(sizeof tams / sizeof tams[0]))
static const struct { const char *nome; uint32_t cor; } cores_fonte[] = {
    { "Negro", 0x000000 }, { "Rojo", 0xFF0000 }, { "Verde oscuro", 0x008000 }, { "Azul", 0x0000FF },
    { "Azul oscuro", 0x000080 }, { "Rojo oscuro", 0x800000 }, { "Gris", 0x808080 }, { "Naranja", 0xFF8000 },
};
#define N_CORES ((int)(sizeof cores_fonte / sizeof cores_fonte[0]))

bool dialogo_fonte(rte_fmt_t *f)
{
    static char it_fam[3][64] = { "Helvetica (sans)", "Times (serif)", "Courier (mono)" };
    static char it_est[4][64] = { "Normal", "Cursiva", "Negrita", "Negrita cursiva" };
    static char it_tam[16][64], it_cor[8][64];
    for (int i = 0; i < N_TAMS; i++) formatar_texto(it_tam[i], 64, "%d", tams[i]);
    for (int i = 0; i < N_CORES; i++) copia_texto(it_cor[i], cores_fonte[i].nome, 64);
    jan_t *j = jan_abrir("Fuente", 500, 350);
    if (!j) return false;
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    int est = (f->est & RTE_NEGRITA ? 2 : 0) | (f->est & RTE_CURSIVA ? 1 : 0);
    int sel_tam = -1, sel_cor = -1;
    for (int i = 0; i < N_TAMS; i++) if (tams[i] == f->tam) sel_tam = i;
    for (int i = 0; i < N_CORES; i++) if (cores_fonte[i].cor == f->cor) sel_cor = i;
    lista_t l[4] = {
        { 14, 30, 150, 104, it_fam, 3, f->fam < 3 ? f->fam : 0, 0, NULL, NULL },
        { 176, 30, 124, 104, it_est, 4, est, 0, NULL, NULL },
        { 306, 30, 56, 104, it_tam, N_TAMS, sel_tam, 0, NULL, NULL },
        { 368, 30, 118, 104, it_cor, N_CORES, sel_cor, 0, NULL, NULL },
    };
    for (int i = 0; i < 4; i++) {
        int vis = (l[i].a - 4) / LISTA_ALTURA_ITEM;
        if (l[i].sel >= vis) l[i].topo = l[i].sel - vis + 1;
    }
    casilla_t c_sub = { 14, 156, (f->est & RTE_SUBLINHADO) != 0 }, c_ris = { 170, 156, (f->est & RTE_RISCADO) != 0 };
    botao_t ok = { 300, 312, 90, 26, "Aceptar", false }, cancel = { 400, 312, 90, 26, "Cancelar", false };
    int foco = 0;
    bool aceptar = false;
    evento_t e;
    bool redib = true;
    for (;;) {
        if (redib) {
            fondo(j);
            etiqueta(j, 14, 10, "Fuente:"); etiqueta(j, 176, 10, "Estilo:"); etiqueta(j, 306, 10, "Tamaño:"); etiqueta(j, 368, 10, "Color:");
            for (int i = 0; i < 4; i++) lista_en_marco(j, &l[i]);
            casilla98(j, &c_sub, "Subrayado"); casilla98(j, &c_ris, "Tachado");
            etiqueta(j, 14, 186, "Vista previa");
            g_retangulo(j, 14, 206, 472, 90, COR_BRANCO);
            g_bisel(j, 12, 204, 476, 94, true);
            rte_fmt_t g = *f;
            g.fam = (uint8_t)l[0].sel;
            g.est = (uint8_t)((l[1].sel & 1 ? RTE_CURSIVA : 0) | (l[1].sel & 2 ? RTE_NEGRITA : 0) |
                              (c_sub.marcada ? RTE_SUBLINHADO : 0) | (c_ris.marcada ? RTE_RISCADO : 0));
            if (l[2].sel >= 0) g.tam = (uint8_t)tams[l[2].sel];
            if (g.tam > 40) g.tam = 40;   /* la muestra cabe en el cuadro */
            if (l[3].sel >= 0) g.cor = cores_fonte[l[3].sel].cor;
            rte_amostra(j, 14, 206, 472, 90, &g, "AaBbYyZz Texto de ejemplo");
            boton98(j, &ok); boton98(j, &cancel);
            jan_mostrar(j);
            redib = false;
        }
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (e.tipo == EV_TECLA && e.tecla == '\n') { aceptar = true; break; }
        if (boton98_tratar(j, &ok, &e)) { aceptar = true; break; }
        if (boton98_tratar(j, &cancel, &e)) break;
        if (casilla98_tratar(j, &c_sub, "Subrayado", &e) || casilla98_tratar(j, &c_ris, "Tachado", &e)) redib = true;
        for (int i = 0; i < 4; i++) {
            if (e.tipo == EV_CLIQUE && en(e.x, e.y, l[i].x, l[i].y, l[i].l, l[i].a)) foco = i;
            if (e.tipo == EV_TECLA && i != foco) continue;
            if (lista_tratar(j, &l[i], &e) != LISTA_NADA) redib = true;
        }
    }
    if (aceptar) {
        f->fam = (uint8_t)l[0].sel;
        f->est = (uint8_t)((l[1].sel & 1 ? RTE_CURSIVA : 0) | (l[1].sel & 2 ? RTE_NEGRITA : 0) |
                           (c_sub.marcada ? RTE_SUBLINHADO : 0) | (c_ris.marcada ? RTE_RISCADO : 0));
        if (l[2].sel >= 0) f->tam = (uint8_t)tams[l[2].sel];
        if (l[3].sel >= 0) f->cor = cores_fonte[l[3].sel].cor;
    }
    jan_fechar(j);
    return aceptar;
}

/* ------------------------------------------------------------ Párrafo */

bool dialogo_paragrafo(rte_par_t *p)
{
    static char it_alin[4][64] = { "Izquierda", "Centrada", "Derecha", "Justificada" };
    static char it_inter[3][64] = { "Sencillo", "1,5 líneas", "Doble" };
    jan_t *j = jan_abrir("Párrafo", 440, 330);
    if (!j) return false;
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    lista_t la = { 14, 30, 130, 84, it_alin, 4, p->alin < 4 ? p->alin : 0, 0, NULL, NULL };
    lista_t li = { 14, 150, 130, 64, it_inter, 3, p->interlin >= 190 ? 2 : p->interlin >= 140 ? 1 : 0, 0, NULL, NULL };
    caixa_t cx[5];
    memset(cx, 0, sizeof cx);
    char t[32];
    int xs[5] = { 290, 290, 290, 290, 290 }, ys[5] = { 30, 62, 94, 150, 182 };
    for (int i = 0; i < 5; i++) { cx[i].x = xs[i]; cx[i].y = ys[i]; cx[i].l = 80; }
    caixa_definir(&cx[0], cm_texto_de_twips(t, sizeof t, p->esq));
    caixa_definir(&cx[1], cm_texto_de_twips(t, sizeof t, p->dir));
    caixa_definir(&cx[2], cm_texto_de_twips(t, sizeof t, p->prim));
    formatar_texto(t, sizeof t, "%d", p->antes / 20); caixa_definir(&cx[3], t);
    formatar_texto(t, sizeof t, "%d", p->depois / 20); caixa_definir(&cx[4], t);
    cx[0].foco = true;
    casilla_t c_vin = { 160, 224, p->marcador != 0 };
    botao_t ok = { 240, 290, 90, 26, "Aceptar", false }, cancel = { 340, 290, 90, 26, "Cancelar", false };
    static const char *const rot[5] = { "Sangría izquierda (cm):", "Sangría derecha (cm):", "Primera línea (cm):", "Antes (pt):", "Después (pt):" };
    int foco_lista = -1;   /* -1: los cuadros de texto; 0: alineación; 1: interlineado */
    bool aceptar = false, redib = true;
    evento_t e;
    for (;;) {
        if (redib) {
            fondo(j);
            etiqueta(j, 14, 10, "Alineación:"); etiqueta(j, 14, 130, "Interlineado:");
            g_bisel(j, 12, 28, 134, 88, true); lista_desenhar(j, &la);
            g_bisel(j, 12, 148, 134, 68, true); lista_desenhar(j, &li);
            for (int i = 0; i < 5; i++) { etiqueta(j, 160, ys[i] + 2 - (i < 3 ? 0 : 0), rot[i]); }
            for (int i = 0; i < 5; i++) { cx[i].x = xs[i] + 80; cx[i].l = 70; caixa_desenhar(j, &cx[i]); }
            casilla98(j, &c_vin, "Viñetas");
            boton98(j, &ok); boton98(j, &cancel);
            jan_mostrar(j);
            redib = false;
        }
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (boton98_tratar(j, &ok, &e)) { aceptar = true; break; }
        if (boton98_tratar(j, &cancel, &e)) break;
        if (e.tipo == EV_TECLA && e.tecla == '\n') { aceptar = true; break; }
        if (casilla98_tratar(j, &c_vin, "Viñetas", &e)) continue;
        if (e.tipo == EV_CLIQUE) {
            if (en(e.x, e.y, la.x, la.y, la.l, la.a)) foco_lista = 0;
            else if (en(e.x, e.y, li.x, li.y, li.l, li.a)) foco_lista = 1;
            else for (int i = 0; i < 5; i++) if (en(e.x, e.y, cx[i].x, cx[i].y, cx[i].l, 24)) foco_lista = -1;
        }
        if (e.tipo == EV_TECLA && e.tecla == '\t') {
            int k = 0;
            for (int i = 0; i < 5; i++) if (cx[i].foco) k = i;
            cx[k].foco = false; cx[(k + 1) % 5].foco = true; foco_lista = -1; redib = true; continue;
        }
        if (e.tipo == EV_TECLA) {
            if (foco_lista == 0) lista_tratar(j, &la, &e);
            else if (foco_lista == 1) lista_tratar(j, &li, &e);
            else for (int i = 0; i < 5; i++) caixa_tratar(j, &cx[i], &e);
        } else {
            lista_tratar(j, &la, &e); lista_tratar(j, &li, &e);
            for (int i = 0; i < 5; i++) caixa_tratar(j, &cx[i], &e);
        }
    }
    if (aceptar) {
        p->alin = (uint8_t)la.sel;
        p->esq = (int16_t)twips_de_cm_texto(cx[0].texto);
        p->dir = (int16_t)twips_de_cm_texto(cx[1].texto);
        p->prim = (int16_t)twips_de_cm_texto(cx[2].texto);
        int a = 0, d = 0;
        for (const char *s = cx[3].texto; *s >= '0' && *s <= '9'; s++) a = a * 10 + (*s - '0');
        for (const char *s = cx[4].texto; *s >= '0' && *s <= '9'; s++) d = d * 10 + (*s - '0');
        p->antes = (int16_t)(a * 20); p->depois = (int16_t)(d * 20);
        p->interlin = (uint16_t)(li.sel == 2 ? 200 : li.sel == 1 ? 150 : 100);
        p->marcador = c_vin.marcada ? 1 : 0;
    }
    jan_fechar(j);
    return aceptar;
}

/* ------------------------------------------------------------ Buscar y reemplazar */

int dialogo_buscar(char *buscar, int max_b, char *por, int max_p, bool *maius, bool com_subst)
{
    int alt = com_subst ? 190 : 140;
    jan_t *j = jan_abrir(com_subst ? "Reemplazar" : "Buscar", 480, alt);
    if (!j) return 0;
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    caixa_t cb, cp;
    memset(&cb, 0, sizeof cb); memset(&cp, 0, sizeof cp);
    cb.x = 150; cb.y = 12; cb.l = 200; cb.foco = true;
    cp.x = 150; cp.y = 44; cp.l = 200;
    caixa_definir(&cb, buscar);
    caixa_definir(&cp, por);
    casilla_t cm = { 14, com_subst ? 84 : 52, *maius };
    botao_t b1 = { 366, 10, 104, 26, "Buscar", false };
    botao_t b2 = { 366, 42, 104, 26, "Reemplazar", false };
    botao_t b3 = { 366, 74, 104, 26, "Reemplazar todo", false };
    botao_t bc = { 366, alt - 40, 104, 26, "Cerrar", false };
    int res = 0;
    bool redib = true;
    evento_t e;
    while (!res) {
        if (redib) {
            fondo(j);
            etiqueta(j, 14, 16, "Buscar:");
            caixa_desenhar(j, &cb);
            if (com_subst) { etiqueta(j, 14, 48, "Reemplazar con:"); caixa_desenhar(j, &cp); }
            casilla98(j, &cm, "Distinguir mayúsculas");
            boton98(j, &b1);
            if (com_subst) { boton98(j, &b2); boton98(j, &b3); }
            boton98(j, &bc);
            jan_mostrar(j);
            redib = false;
        }
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (e.tipo == EV_TECLA && e.tecla == '\t' && com_subst) { cb.foco = !cb.foco; cp.foco = !cb.foco; redib = true; continue; }
        if (casilla98_tratar(j, &cm, "Distinguir mayúsculas", &e)) continue;
        if (boton98_tratar(j, &b1, &e)) res = DLG_BUSCAR_SEGUINTE;
        else if (com_subst && boton98_tratar(j, &b2, &e)) res = DLG_BUSCAR_SUBSTITUIR;
        else if (com_subst && boton98_tratar(j, &b3, &e)) res = DLG_BUSCAR_TUDO;
        else if (boton98_tratar(j, &bc, &e)) break;
        else if (caixa_tratar(j, &cb, &e)) res = DLG_BUSCAR_SEGUINTE;
        else if (com_subst && caixa_tratar(j, &cp, &e)) res = DLG_BUSCAR_SUBSTITUIR;
    }
    copia_texto(buscar, cb.texto, (size_t)max_b);
    copia_texto(por, cp.texto, (size_t)max_p);
    *maius = cm.marcada;
    jan_fechar(j);
    return res;
}

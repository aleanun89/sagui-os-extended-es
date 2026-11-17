// SPDX-License-Identifier: GPL-2.0-or-later
/* inicio.c - los menús Inicio de columnas: «Dos columnas» y «Moderno» (D361).
 *
 * Diseño propio, inspirado en el estilo de los escritorios de principios de los 2000 y de finales de esa década
 * (sin copiar iconos, logotipos ni gráficos de nadie: solo se usa el arte del SaguiOS).
 *
 *   Dos columnas: cabecera con la cuenta y su imagen (el sagui), a la izquierda los programas más usados y
 *                 «Todos los programas» (un árbol por grupos que se despliega en el mismo sitio), a la derecha los
 *                 lugares del sistema y, abajo, los botones de apagar y reiniciar.
 *   Moderno:      buscador integrado (arriba o abajo, a elección), una lista de programas que se desplaza dentro
 *                 del menú y se filtra al teclear, una columna derecha oscura con los lugares y los botones de
 *                 apagar y reiniciar abajo. Translúcido (mezcla con lo que hay debajo) o con un degradado oscuro.
 *
 * Como el menú contextual (menu.c), no es una ventana: se dibuja por encima de todo en el mismo gui_mostrar,
 * se queda con el ratón y el teclado mientras está abierto, y avisa a barra.c al cerrarse (acción o -1). El
 * panel se vuelve a componer entero en cada cambio (la mezcla con el fondo solo es correcta si lo de debajo
 * se recompone también, como la sombra de menu.c). */
#include "gui.h"
#include "console.h"
#include "idioma.h"
#include "mouse.h"
#include "teclado.h"
#include "texto.h"
#include "video.h"

#define LIN_ITEM   24
#define LIN_CAB    22
#define LIN_SEP    9
#define LIN_TODOS  28
#define MAX_LIN    256
#define BUSCA_MAX  31
#define SOMBRA     4
#define BARRA_INF  34   /* franja de abajo del estilo Moderno: el buscador a la izquierda y los botones a la derecha */
#define BOTON_L    26   /* los botones de apagar y reiniciar: cuadrados, solo con icono */

enum { L_ITEM, L_CAB, L_SEP, L_TODOS, L_ATRAS, L_VACIO };
typedef struct { uint8_t tipo; int16_t item; int16_t grupo; } linha_t;

static struct {
    bool aberto;
    inicio_pedido_t p;
    int x, y, l, a;                 /* el panel entero */
    int esq_x, esq_y, esq_l, esq_a; /* la lista de la izquierda */
    int dir_x, dir_y, dir_l, dir_a; /* la columna de la derecha */
    int busca_x, busca_y, busca_l, busca_a;   /* el buscador (Moderno) */
    int cab_a, rod_a;               /* alturas de la cabecera y del pie (Dos columnas) */
    linha_t lin[MAX_LIN];
    int nlin;
    int alt_total;                  /* altura de todas las líneas, para el desplazamiento */
    int rolo;                       /* desplazamiento de la lista, en píxeles */
    int col, sel;                   /* columna con el resalte (0 lista, 1 lugares, 2 botones) y línea/índice en ella */
    bool todos;                     /* Dos columnas: enseñando «Todos los programas» */
    bool expandido[10];
    char busca[BUSCA_MAX + 1];
    int nbusca;
    int lugar[INICIO_MAX], nlugar;  /* índices a itens[] de la columna derecha */
    int botao[4], nbotao;           /* y los de los botones */
} S;

bool inicio_painel_aberto(void) { return S.aberto; }
void inicio_painel_retangulo(int *x, int *y, int *l, int *a)
{
    *x = S.aberto ? S.x : 0; *y = S.aberto ? S.y : 0; *l = S.aberto ? S.l : 0; *a = S.aberto ? S.a : 0;
}

static bool moderno(void) { return S.p.estilo == BC_MENU_MODERNO; }
static const inicio_item_t *it(int i) { return &S.p.itens[i]; }

static void sujar(void) { gui_redesenhar(S.x, S.y, S.l + SOMBRA, S.a + SOMBRA); }

/* ------------------------------------------------------------ las líneas de la lista */

/* ¿El texto contiene la búsqueda (sin distinguir mayúsculas de minúsculas, solo ASCII)? */
static char minuscula(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
static bool contem(const char *t, const char *q)
{
    for (; *t; t++) {
        int k = 0;
        while (q[k] && t[k] && minuscula(t[k]) == minuscula(q[k])) k++;
        if (!q[k]) return true;
    }
    return false;
}

static void add(int tipo, int item, int grupo)
{
    if (S.nlin >= MAX_LIN) return;
    S.lin[S.nlin++] = (linha_t){ (uint8_t)tipo, (int16_t)item, (int16_t)grupo };
}

static int altura_lin(const linha_t *l)
{
    return l->tipo == L_ITEM ? LIN_ITEM : l->tipo == L_CAB ? LIN_CAB : l->tipo == L_SEP ? LIN_SEP : l->tipo == L_VACIO ? LIN_CAB : LIN_TODOS;
}
static bool escolhivel(const linha_t *l) { return l->tipo == L_ITEM || l->tipo == L_TODOS || l->tipo == L_ATRAS || (l->tipo == L_CAB && !moderno()); }

static void rolo_ajustar(void);

static void reconstruir(void)
{
    S.nlin = 0;
    if (!moderno()) {
        if (!S.todos) {
            int n = 0;
            for (int i = 0; i < S.p.n && n < 6; i++) if (it(i)->grupo == IG_FREQ) { add(L_ITEM, i, 0); n++; }
            add(L_SEP, 0, 0);
            add(L_TODOS, 0, 0);
        } else {
            add(L_ATRAS, 0, 0);
            for (int g = 0; g < S.p.ngrupos; g++) {
                bool tem = false;
                for (int i = 0; i < S.p.n && !tem; i++) tem = it(i)->grupo == g;
                if (!tem) continue;
                add(L_CAB, 0, g);
                if (S.expandido[g])
                    for (int i = 0; i < S.p.n; i++) if (it(i)->grupo == g) add(L_ITEM, i, g);
            }
        }
    } else if (S.nbusca) {
        int n = 0;
        for (int i = 0; i < S.p.n; i++)
            if (it(i)->grupo >= 0 && contem(it(i)->texto, S.busca)) { add(L_ITEM, i, 0); n++; }
        if (!n) add(L_VACIO, 0, 0);
    } else {
        for (int i = 0; i < S.p.n; i++) if (it(i)->grupo == IG_FREQ) add(L_ITEM, i, 0);
        add(L_SEP, 0, 0);
        for (int g = 0; g < S.p.ngrupos; g++) {
            bool tem = false;
            for (int i = 0; i < S.p.n && !tem; i++) tem = it(i)->grupo == g;
            if (!tem) continue;
            add(L_CAB, 0, g);
            for (int i = 0; i < S.p.n; i++) if (it(i)->grupo == g) add(L_ITEM, i, g);
        }
    }
    S.alt_total = 0;
    for (int k = 0; k < S.nlin; k++) S.alt_total += altura_lin(&S.lin[k]);
    rolo_ajustar();
}

static int topo_lin(int k)
{
    int y = 0;
    for (int q = 0; q < k; q++) y += altura_lin(&S.lin[q]);
    return y;
}

/* El desplazamiento siempre cae en el borde de arriba de una línea (y la última línea de abajo se ve entera):
 * así nunca queda una línea cortada. rolo_max es el menor borde que deja la lista hasta el final. */
static int rolo_max(void)
{
    if (S.alt_total <= S.esq_a) return 0;
    int y = 0;
    for (int k = 0; k < S.nlin; k++) {
        if (y >= S.alt_total - S.esq_a) return y;
        y += altura_lin(&S.lin[k]);
    }
    return y;
}

static void rolo_ajustar(void)
{
    int y = 0, m = rolo_max();
    for (int k = 0; k < S.nlin && y < S.rolo; k++) y += altura_lin(&S.lin[k]);   /* el borde de línea a partir de rolo */
    S.rolo = y > m ? m : y;
}

/* ¿La línea que empieza en y (relativo a la lista) y mide h se ve entera? */
static bool lin_inteira(int y, int h) { return y >= S.esq_y && y + h <= S.esq_y + S.esq_a; }

/* La línea de la lista bajo el punto, o -1. */
static int lin_em(int px, int py)
{
    if (px < S.esq_x || px >= S.esq_x + S.esq_l || py < S.esq_y || py >= S.esq_y + S.esq_a) return -1;
    int y = S.esq_y - S.rolo;
    for (int k = 0; k < S.nlin; k++) {
        int h = altura_lin(&S.lin[k]);
        if (py >= y && py < y + h) return lin_inteira(y, h) ? k : -1;
        y += h;
    }
    return -1;
}

/* Que la línea k se vea entera. */
static void ver_lin(int k)
{
    if (k < 0 || k >= S.nlin) return;
    int t = topo_lin(k), h = altura_lin(&S.lin[k]);
    if (t < S.rolo) S.rolo = t;
    for (int q = 0; t + h > S.rolo + S.esq_a && q < S.nlin; q++) {   /* sube de línea en línea hasta que quepa entera */
        int y = 0, n = 0;
        while (n < S.nlin && y <= S.rolo) y += altura_lin(&S.lin[n++]);
        S.rolo = y;
    }
    rolo_ajustar();
}

static int primeira_escolhivel(int de, int passo)
{
    for (int k = de; k >= 0 && k < S.nlin; k += passo) if (escolhivel(&S.lin[k])) return k;
    return -1;
}

/* ------------------------------------------------------------ geometría */

static int rodape_botao_x(int i, int *l);

static void geometria(void)
{
    int L = (int)video_largura(), A = (int)video_altura();
    S.nlugar = S.nbotao = 0;
    for (int i = 0; i < S.p.n; i++) {
        if (it(i)->grupo == IG_LUGAR && S.nlugar < INICIO_MAX) S.lugar[S.nlugar++] = i;
        if (it(i)->grupo == IG_ENERGIA && S.nbotao < 4) S.botao[S.nbotao++] = i;
    }
    if (!moderno()) {
        S.l = 470; S.cab_a = 54; S.rod_a = 36;
        int corpo = 280;
        S.a = S.cab_a + corpo + S.rod_a;
    } else {
        S.l = 510; S.cab_a = 0; S.rod_a = 0;
        S.a = 400;
    }
    if (S.a > A - S.p.alt_barra - 4) S.a = A - S.p.alt_barra - 4;
    if (S.l > L) S.l = L;
    S.x = S.p.x;
    if (S.x + S.l > L) S.x = L - S.l;
    if (S.x < 0) S.x = 0;
    S.y = S.p.acima ? S.p.y_barra - S.a : S.p.y_barra + S.p.alt_barra;
    if (S.y < 0) S.y = 0;
    if (!moderno()) {
        int corpo = S.a - S.cab_a - S.rod_a;
        S.esq_x = S.x + 1; S.esq_y = S.y + S.cab_a; S.esq_l = 209; S.esq_a = corpo;
        S.dir_x = S.x + 210; S.dir_y = S.y + S.cab_a; S.dir_l = S.l - 211; S.dir_a = corpo;
    } else {
        int bh = BARRA_INF;
        int esq_l = 268;
        S.busca_l = esq_l - 12; S.busca_a = 24; S.busca_x = S.x + 6;   /* solo en la columna de la izquierda, con 6 px de margen */
        S.busca_y = S.p.buscador_abajo ? S.y + S.a - bh + (bh - 24) / 2 : S.y + 5;
        S.esq_x = S.x + 1; S.esq_l = esq_l;
        S.esq_y = S.p.buscador_abajo ? S.y + 1 : S.y + bh;
        S.esq_a = S.a - bh - 1;
        S.dir_x = S.x + esq_l + 1; S.dir_y = S.y + 1; S.dir_l = S.l - esq_l - 2; S.dir_a = S.a - 2;
    }
}

/* El botón i de apagar/reiniciar: su x y su ancho. Dos columnas: pegados a la derecha del pie; Moderno: en la
 * columna de la derecha, abajo. */
static int rodape_botao_x(int i, int *l)
{
    int x = (moderno() ? S.dir_x + S.dir_l : S.x + S.l) - 8;
    for (int k = S.nbotao - 1; k >= 0; k--) {
        x -= BOTON_L;
        if (k == i) { *l = BOTON_L; return x; }
        x -= 6;
    }
    *l = 0;
    return 0;
}

static int rodape_y(void) { return moderno() ? S.y + S.a - BARRA_INF + (BARRA_INF - BOTON_L) / 2 : S.y + S.a - S.rod_a + (S.rod_a - BOTON_L) / 2; }

static int botao_em(int px, int py)
{
    int by = rodape_y();
    if (py < by || py >= by + BOTON_L) return -1;
    for (int k = 0; k < S.nbotao; k++) {
        int bl, bx = rodape_botao_x(k, &bl);
        if (px >= bx && px < bx + bl) return k;
    }
    return -1;
}

static int lugar_y0(void) { return moderno() ? S.dir_y + 12 : S.dir_y + 8; }
#define LIN_LUGAR 26
static int lugar_em(int px, int py)
{
    if (px < S.dir_x || px >= S.dir_x + S.dir_l) return -1;
    int y = lugar_y0();
    for (int k = 0; k < S.nlugar; k++, y += LIN_LUGAR)
        if (py >= y && py < y + LIN_LUGAR) return k;
    return -1;
}

/* ------------------------------------------------------------ abrir y cerrar */

bool inicio_painel_abrir(const inicio_pedido_t *p)
{
    if (!gui_ativa() || p->n <= 0) return false;
    S.p = *p;
    S.aberto = true;
    S.rolo = 0;
    S.todos = false;
    S.nbusca = 0;
    S.busca[0] = 0;
    for (int g = 0; g < 10; g++) S.expandido[g] = false;
    geometria();
    reconstruir();
    S.col = 0;
    S.sel = S.p.teclado ? primeira_escolhivel(0, 1) : -1;
    sujar();
    {   /* el diseño, para las pruebas: el buscador y los botones no se solapan ni se salen de su columna */
        char b[200];
        int u = snformatar(b, sizeof b, "busca=(%d,%d,%d,%d) lista=(%d,%d,%d,%d) dir=(%d,%d,%d,%d) botoes=", S.busca_x, S.busca_y, moderno() ? S.busca_l : 0,
                           moderno() ? S.busca_a : 0, S.esq_x, S.esq_y, S.esq_l, S.esq_a, S.dir_x, S.dir_y, S.dir_l, S.dir_a);
        for (int k = 0; k < S.nbotao && u < (int)sizeof b - 30; k++) {
            int bl, bx = rodape_botao_x(k, &bl);
            u += snformatar(b + u, sizeof b - (uint32_t)u, "(%d,%d,%d,%d)", bx, rodape_y(), bl, bl);
        }
        klog("[GUI] inicio diseno %s\n", b);
    }
    klog("[GUI] inicio painel aberto estilo %d em (%d,%d) %dx%d, %d linhas, %d lugares\n", S.p.estilo, S.x, S.y, S.l, S.a, S.nlin, S.nlugar);
    return true;
}

static void fechar(int acao)
{
    if (!S.aberto) return;
    sujar();
    S.aberto = false;
    void (*r)(int) = S.p.acao;
    klog("[GUI] inicio painel fechado (acao %d)\n", acao);
    if (r) r(acao);
}

void inicio_painel_fechar(void) { fechar(-1); }

/* ------------------------------------------------------------ ratón */

static void destacar(int col, int sel)
{
    if (col == S.col && sel == S.sel) return;
    S.col = col;
    S.sel = sel;
    sujar();
}

static void ativar_lin(int k)
{
    if (k < 0 || k >= S.nlin) return;
    linha_t *l = &S.lin[k];
    if (l->tipo == L_ITEM) { fechar(it(l->item)->acao); return; }
    if (l->tipo == L_TODOS) { klog("[GUI] inicio: todos os programas\n"); S.todos = true; S.rolo = 0; reconstruir(); S.sel = primeira_escolhivel(0, 1); sujar(); return; }
    if (l->tipo == L_ATRAS) { klog("[GUI] inicio: de volta aos mais usados\n"); S.todos = false; S.rolo = 0; reconstruir(); S.sel = primeira_escolhivel(0, 1); sujar(); return; }
    if (l->tipo == L_CAB && !moderno()) {
        S.expandido[l->grupo] = !S.expandido[l->grupo];
        klog("[GUI] inicio: grupo %d %s\n", l->grupo, S.expandido[l->grupo] ? "aberto" : "fechado");
        reconstruir();
        sujar();
    }
}

bool inicio_painel_mouse(int x, int y, unsigned botoes, unsigned mudaram)
{
    if (!S.aberto) return false;
    unsigned apertou = mudaram & botoes;
    bool dentro = x >= S.x && x < S.x + S.l && y >= S.y && y < S.y + S.a;
    int k = lin_em(x, y), q = lugar_em(x, y), b = botao_em(x, y);
    if (!apertou) {
        if (!dentro) return true;
        if (k >= 0 && escolhivel(&S.lin[k])) destacar(0, k);
        else if (q >= 0) destacar(1, q);
        else if (b >= 0) destacar(2, b);
        else if (S.sel >= 0 && S.col == 0 && lin_em(x, y) < 0 && !(x >= S.esq_x && x < S.esq_x + S.esq_l)) destacar(0, -1);
        return true;
    }
    if (!dentro) { fechar(-1); return true; }   /* clic fuera: cierra y el clic muere aquí, como en menu.c */
    if (!(mudaram & BOTAO_ESQUERDO)) return true;
    if (k >= 0) ativar_lin(k);
    else if (q >= 0) fechar(it(S.lugar[q])->acao);
    else if (b >= 0) fechar(it(S.botao[b])->acao);
    return true;
}

bool inicio_painel_roda(int dz)
{
    if (!S.aberto) return false;
    /* dz > 0: rueda hacia abajo; de tres en tres líneas, siempre sobre el borde de una línea */
    int k = 0, y = 0;
    while (k < S.nlin && y < S.rolo) y += altura_lin(&S.lin[k++]);
    k += dz * 3;
    if (k < 0) k = 0;
    if (k > S.nlin - 1) k = S.nlin - 1;
    S.rolo = topo_lin(k);
    rolo_ajustar();
    sujar();
    return true;
}

/* ------------------------------------------------------------ teclado */

static void mover(int passo)
{
    if (S.col == 0) {
        int k = S.sel < 0 ? (passo > 0 ? primeira_escolhivel(0, 1) : primeira_escolhivel(S.nlin - 1, -1)) : primeira_escolhivel(S.sel + passo, passo);
        if (k >= 0) { S.sel = k; ver_lin(k); sujar(); }
    } else if (S.col == 1) {
        int n = S.sel + passo;
        if (S.sel < 0) n = passo > 0 ? 0 : S.nlugar - 1;
        if (n >= 0 && n < S.nlugar) { S.sel = n; sujar(); }
    } else {
        int n = S.sel + passo;
        if (n >= 0 && n < S.nbotao) { S.sel = n; sujar(); }
    }
}

static void busca_mudou(void)
{
    S.rolo = 0;
    reconstruir();
    S.col = 0;
    S.sel = primeira_escolhivel(0, 1);
    int n = 0;
    for (int k = 0; k < S.nlin; k++) if (S.lin[k].tipo == L_ITEM) n++;
    klog("[GUI] inicio busca '%s': %d resultados\n", S.busca, n);
    sujar();
}

bool inicio_painel_tecla(int t)
{
    if (!S.aberto) return false;
    if (t == TECLA_SUPER) { fechar(-1); return true; }
    if (t == TECLA_ESC) {
        if (moderno() && S.nbusca) { S.nbusca = 0; S.busca[0] = 0; busca_mudou(); }
        else if (!moderno() && S.todos) { S.todos = false; reconstruir(); S.col = 0; S.sel = primeira_escolhivel(0, 1); sujar(); }
        else fechar(-1);
        return true;
    }
    if (t == TECLA_BAIXO) mover(1);
    else if (t == TECLA_CIMA) mover(-1);
    else if (t == TECLA_HOME) { if (S.col == 0) { S.sel = primeira_escolhivel(0, 1); ver_lin(S.sel); sujar(); } }
    else if (t == TECLA_END) { if (S.col == 0) { S.sel = primeira_escolhivel(S.nlin - 1, -1); ver_lin(S.sel); sujar(); } }
    else if (t == TECLA_PGDN) { for (int i = 0; i < 8; i++) mover(1); }
    else if (t == TECLA_PGUP) { for (int i = 0; i < 8; i++) mover(-1); }
    else if (t == TECLA_DIREITA) {
        if (S.col == 0 && !(S.sel >= 0 && S.lin[S.sel].tipo == L_TODOS)) { if (S.nlugar) destacar(1, 0); }
        else if (S.col == 0) ativar_lin(S.sel);
        else if (S.col == 1 && S.nbotao) destacar(2, 0);
    }
    else if (t == TECLA_ESQUERDA) {
        if (S.col == 2) destacar(1, S.nlugar ? 0 : -1);
        else if (S.col == 1) destacar(0, primeira_escolhivel(0, 1));
        else if (S.sel >= 0 && S.lin[S.sel].tipo == L_CAB && !moderno() && S.expandido[S.lin[S.sel].grupo]) ativar_lin(S.sel);
    }
    else if (t == '\t') {   /* Tab: lista, lugares, botones */
        int nc = (S.col + 1) % 3;
        if (nc == 1 && !S.nlugar) nc = 2;
        if (nc == 2 && !S.nbotao) nc = 0;
        destacar(nc, nc == 0 ? primeira_escolhivel(0, 1) : 0);
    }
    else if (t == '\n' || t == '\r' || (t == ' ' && !moderno())) {
        if (S.col == 0 && S.sel >= 0) ativar_lin(S.sel);
        else if (S.col == 1 && S.sel >= 0 && S.sel < S.nlugar) fechar(it(S.lugar[S.sel])->acao);
        else if (S.col == 2 && S.sel >= 0 && S.sel < S.nbotao) fechar(it(S.botao[S.sel])->acao);
    }
    else if (moderno() && (t == '\b' || t == 127)) {
        if (S.nbusca) { S.busca[--S.nbusca] = 0; busca_mudou(); }
    }
    else if (moderno() && t >= 32 && t < 256 && t != 127) {
        if (S.nbusca < BUSCA_MAX) { S.busca[S.nbusca++] = (char)t; S.busca[S.nbusca] = 0; busca_mudou(); }
    }
    return true;   /* el panel es modal: las otras teclas mueren */
}

/* ------------------------------------------------------------ dibujo */

/* Mezcla el color con lo que hay debajo: alfa de 0 a 255 (255 = solo el color). */
static void retangulo_alfa(superficie_t *t, int x, int y, int l, int a, uint32_t cor, int alfa)
{
    int x0 = x < t->cx0 ? t->cx0 : x, y0 = y < t->cy0 ? t->cy0 : y;
    int x1 = x + l > t->cx1 ? t->cx1 : x + l, y1 = y + a > t->cy1 ? t->cy1 : y + a;
    if (x0 >= x1) return;
    uint32_t cr = (cor >> 16) & 255, cg = (cor >> 8) & 255, cb = cor & 255;
    for (int j = y0; j < y1; j++) {
        uint32_t *p = t->px + j * t->larg;
        for (int i = x0; i < x1; i++) {
            uint32_t v = p[i], r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
            r = (cr * (uint32_t)alfa + r * (uint32_t)(255 - alfa)) / 255;
            g = (cg * (uint32_t)alfa + g * (uint32_t)(255 - alfa)) / 255;
            b = (cb * (uint32_t)alfa + b * (uint32_t)(255 - alfa)) / 255;
            p[i] = r << 16 | g << 8 | b;
        }
    }
}

static uint32_t entre(uint32_t a, uint32_t b, int k, int n)   /* de a a b, k de n */
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int ca = (int)((a >> s) & 255), cb = (int)((b >> s) & 255);
        r |= (uint32_t)(ca + (cb - ca) * k / (n > 0 ? n : 1)) << s;
    }
    return r;
}

static void degradado(superficie_t *t, int x, int y, int l, int a, uint32_t cima, uint32_t baixo)
{
    for (int j = 0; j < a; j++) sup_retangulo(t, x, y + j, l, 1, entre(cima, baixo, j, a - 1));
}

static void texto_cabe(char *dst, int max, const char *src, int px)
{
    int cabe = px / 8;
    if (cabe >= max) cabe = max - 1;
    int n = 0;
    while (src[n] && n < cabe) { dst[n] = src[n]; n++; }
    dst[n] = 0;
    if (src[n] && n >= 3) dst[n - 1] = dst[n - 2] = '.';
}

static superficie_t recortada(superficie_t *t, int x, int y, int l, int a)
{
    superficie_t c = *t;
    if (x > c.cx0) c.cx0 = x;
    if (y > c.cy0) c.cy0 = y;
    if (x + l < c.cx1) c.cx1 = x + l;
    if (y + a < c.cy1) c.cy1 = y + a;
    return c;
}

static void ic16(superficie_t *t, int x, int y, const desenho_t *d)
{
    if (d) sup_desenho_metade(t, x, y, d);
}

static void seta_dir(superficie_t *t, int x, int y, uint32_t cor)
{
    for (int k = 0; k < 4; k++) sup_retangulo(t, x + k, y + k, 1, 7 - 2 * k, cor);
}
static void seta_cima(superficie_t *t, int x, int y, uint32_t cor)
{
    for (int k = 0; k < 4; k++) sup_retangulo(t, x + k, y + 3 - k, 7 - 2 * k, 1, cor);
}
static void seta_baixo(superficie_t *t, int x, int y, uint32_t cor)
{
    for (int k = 0; k < 4; k++) sup_retangulo(t, x + k, y + k, 7 - 2 * k, 1, cor);
}

typedef struct { uint32_t fundo_esq, texto_esq, cab_texto, sel_fundo, sel_texto, fundo_dir, texto_dir, dir_sel, dir_sel_texto; } cores_t;

static void cores(cores_t *c)
{
    if (!moderno()) {
        *c = (cores_t){ 0xFFFFFF, 0x1E2328, 0x1B3F94, 0x316AC5, 0xFFFFFF, 0xD3E5FA, 0x0F2A5C, 0x316AC5, 0xFFFFFF };
    } else {
        *c = (cores_t){ 0xF2F6FA, 0x1E2328, 0x4A5A6A, 0x9CC0EC, 0x0A1A30, 0x0C1624, 0xE8EEF4, 0x3A6EB0, 0xFFFFFF };
    }
}

static void desenhar_lista(superficie_t *t, const cores_t *c)
{
    superficie_t cl = recortada(t, S.esq_x, S.esq_y, S.esq_l, S.esq_a);
    int y = S.esq_y - S.rolo;
    for (int k = 0; k < S.nlin; k++) {
        linha_t *l = &S.lin[k];
        int h = altura_lin(l);
        if (lin_inteira(y, h)) {
            bool sel = S.col == 0 && S.sel == k;
            char tx[64];
            if (l->tipo == L_SEP) {
                sup_retangulo(&cl, S.esq_x + 10, y + LIN_SEP / 2, S.esq_l - 20, 1, moderno() ? 0xC0C8D0 : 0xD0D8E4);
            } else if (l->tipo == L_ITEM) {
                if (sel) {
                    if (moderno()) { retangulo_alfa(&cl, S.esq_x + 3, y + 1, S.esq_l - 6, h - 2, c->sel_fundo, 200); sup_moldura(&cl, S.esq_x + 3, y + 1, S.esq_l - 6, h - 2, 0x6E9AD0); }
                    else sup_retangulo(&cl, S.esq_x + 2, y, S.esq_l - 4, h, c->sel_fundo);
                }
                bool sub = l->grupo >= 0 && !(moderno() && S.nbusca);
                int ix = S.esq_x + (sub ? 22 : 8);
                if (!moderno() && S.todos) ix = S.esq_x + 22;
                ic16(&cl, ix, y + (h - 16) / 2, it(l->item)->icone);
                texto_cabe(tx, sizeof tx, it(l->item)->texto, S.esq_x + S.esq_l - 6 - (ix + 22));
                sup_texto(&cl, ix + 22, y + (h - 16) / 2, tx, sel ? c->sel_texto : c->texto_esq);
            } else if (l->tipo == L_CAB) {
                if (moderno()) {
                    texto_cabe(tx, sizeof tx, S.p.grupos[l->grupo], S.esq_l - 24);
                    sup_texto(&cl, S.esq_x + 10, y + (h - 16) / 2, tx, c->cab_texto);
                    sup_retangulo(&cl, S.esq_x + 14 + texto_largura(tx), y + h / 2, S.esq_l - 30 - texto_largura(tx), 1, 0xD0D8E0);
                } else {
                    if (sel) sup_retangulo(&cl, S.esq_x + 2, y, S.esq_l - 4, h, c->sel_fundo);
                    uint32_t cor = sel ? c->sel_texto : c->texto_esq;
                    if (S.expandido[l->grupo]) seta_baixo(&cl, S.esq_x + 8, y + (h - 4) / 2, cor);
                    else seta_dir(&cl, S.esq_x + 10, y + (h - 7) / 2, cor);
                    texto_cabe(tx, sizeof tx, S.p.grupos[l->grupo], S.esq_l - 40);
                    sup_texto(&cl, S.esq_x + 22, y + (h - 16) / 2, tx, cor);
                    sup_texto(&cl, S.esq_x + 23, y + (h - 16) / 2, tx, cor);
                }
            } else if (l->tipo == L_TODOS || l->tipo == L_ATRAS) {
                uint32_t cor = sel ? c->sel_texto : c->texto_esq;
                if (sel) sup_retangulo(&cl, S.esq_x + 2, y, S.esq_l - 4, h, c->sel_fundo);
                sup_retangulo(&cl, S.esq_x + 10, y, S.esq_l - 20, 1, 0xD0D8E4);
                const char *r = l->tipo == L_TODOS ? _("Todos los programas") : _("Atrás");
                if (l->tipo == L_TODOS) {
                    sup_texto(&cl, S.esq_x + 12, y + (h - 16) / 2, r, cor);
                    sup_texto(&cl, S.esq_x + 13, y + (h - 16) / 2, r, cor);
                    seta_dir(&cl, S.esq_x + S.esq_l - 22, y + (h - 7) / 2, sel ? c->sel_texto : 0x1B8A3A);
                } else {
                    seta_dir(&cl, S.esq_x + 12, y + (h - 7) / 2, sel ? c->sel_texto : 0x1B8A3A);
                    sup_texto(&cl, S.esq_x + 26, y + (h - 16) / 2, r, cor);
                    sup_texto(&cl, S.esq_x + 27, y + (h - 16) / 2, r, cor);
                }
            } else if (l->tipo == L_VACIO) {
                sup_texto(&cl, S.esq_x + 10, y + (h - 16) / 2, _("Sin resultados"), 0x6A737C);
            }
        }
        y += h;
    }
    /* si la lista no cabe: una barra de desplazamiento y flechitas arriba y abajo que dicen que hay más */
    if (S.alt_total > S.esq_a) {
        int th = S.esq_a * S.esq_a / S.alt_total;
        if (th < 16) th = 16;
        int ty = S.esq_y + (S.esq_a - th) * S.rolo / (S.alt_total - S.esq_a);
        if (ty + th > S.esq_y + S.esq_a) ty = S.esq_y + S.esq_a - th;
        sup_retangulo(t, S.esq_x + S.esq_l - 5, S.esq_y, 3, S.esq_a, moderno() ? 0xDCE4EC : 0xE4ECF8);
        sup_retangulo(t, S.esq_x + S.esq_l - 5, ty, 3, th, moderno() ? 0x6A84A0 : 0x7C9CCC);
        if (S.rolo > 0) seta_cima(t, S.esq_x + S.esq_l - 20, S.esq_y + 4, 0x4A6080);
        if (S.rolo < rolo_max()) seta_baixo(t, S.esq_x + S.esq_l - 20, S.esq_y + S.esq_a - 8, 0x4A6080);
    }
}

static void desenhar_lugares(superficie_t *t, const cores_t *c)
{
    int y = lugar_y0();
    for (int k = 0; k < S.nlugar; k++, y += LIN_LUGAR) {
        const inicio_item_t *q = it(S.lugar[k]);
        bool sel = S.col == 1 && S.sel == k;
        if (sel) {
            if (moderno()) { retangulo_alfa(t, S.dir_x + 4, y, S.dir_l - 8, LIN_LUGAR - 2, c->dir_sel, 210); sup_moldura(t, S.dir_x + 4, y, S.dir_l - 8, LIN_LUGAR - 2, 0x6E9AD0); }
            else sup_retangulo(t, S.dir_x + 2, y, S.dir_l - 4, LIN_LUGAR - 2, c->dir_sel);
        }
        char tx[64];
        int tx0 = S.dir_x + 12 + (q->icone ? 22 : 0);
        ic16(t, S.dir_x + 12, y + (LIN_LUGAR - 2 - 16) / 2, q->icone);
        texto_cabe(tx, sizeof tx, q->texto, S.dir_x + S.dir_l - 6 - tx0);
        uint32_t cor = sel ? c->dir_sel_texto : c->texto_dir;
        sup_texto(t, tx0, y + (LIN_LUGAR - 2 - 16) / 2, tx, cor);
        if (!moderno()) sup_texto(t, tx0 + 1, y + (LIN_LUGAR - 2 - 16) / 2, tx, cor);   /* negrita, como en los menús de dos columnas */
    }
}

/* Los iconos de los botones de energía, de 16x16, dibujados con rectángulos (arte propio): 1 apagar (círculo abierto con
 * una raya), 2 reiniciar (flecha en círculo), 3 cerrar sesión (candado). */
static void ico_energia(superficie_t *t, int x, int y, int tipo, uint32_t c)
{
    if (tipo == 1) {
        sup_retangulo(t, x + 7, y, 2, 8, c);
        sup_retangulo(t, x + 3, y + 2, 2, 1, c); sup_retangulo(t, x + 2, y + 3, 1, 2, c); sup_retangulo(t, x + 1, y + 5, 2, 6, c);
        sup_retangulo(t, x + 2, y + 11, 1, 2, c); sup_retangulo(t, x + 3, y + 13, 2, 1, c); sup_retangulo(t, x + 5, y + 14, 6, 2, c);
        sup_retangulo(t, x + 11, y + 13, 2, 1, c); sup_retangulo(t, x + 13, y + 11, 1, 2, c); sup_retangulo(t, x + 13, y + 5, 2, 6, c);
        sup_retangulo(t, x + 13, y + 3, 1, 2, c); sup_retangulo(t, x + 11, y + 2, 2, 1, c);
    } else if (tipo == 2) {
        sup_retangulo(t, x + 5, y + 1, 6, 2, c); sup_retangulo(t, x + 3, y + 3, 2, 2, c); sup_retangulo(t, x + 1, y + 5, 2, 6, c);
        sup_retangulo(t, x + 3, y + 11, 2, 2, c); sup_retangulo(t, x + 5, y + 13, 6, 2, c); sup_retangulo(t, x + 11, y + 11, 2, 2, c);
        sup_retangulo(t, x + 13, y + 7, 2, 4, c);
        sup_retangulo(t, x + 10, y + 3, 5, 1, c); sup_retangulo(t, x + 11, y + 4, 4, 1, c); sup_retangulo(t, x + 12, y + 5, 3, 1, c);   /* la punta de la flecha */
    } else {
        sup_retangulo(t, x + 5, y + 1, 6, 2, c); sup_retangulo(t, x + 4, y + 3, 2, 3, c); sup_retangulo(t, x + 10, y + 3, 2, 3, c);
        sup_retangulo(t, x + 2, y + 7, 12, 8, c);
        sup_retangulo(t, x + 7, y + 9, 2, 3, moderno() ? 0x0C1624 : 0x1B3F94);
    }
}

/* Un botón cuadrado con el icono; con el ratón o el teclado encima se aclara. */
static void desenhar_botao(superficie_t *t, int bx, int by, int tipo, bool sel)
{
    uint32_t fundo = moderno() ? (sel ? 0x4A7EC0 : 0x2A3E58) : (sel ? 0x4A7EE0 : 0x1B3F94);
    sup_retangulo(t, bx, by, BOTON_L, BOTON_L, fundo);
    sup_moldura(t, bx, by, BOTON_L, BOTON_L, sel ? 0xE8F0FA : moderno() ? 0x7A92B0 : 0x9CB8EC);
    ico_energia(t, bx + (BOTON_L - 16) / 2, by + (BOTON_L - 16) / 2, tipo, 0xFFFFFF);
}

/* La etiqueta del botón con el resalte (Apagar, Reiniciar, Cerrar sesión), encima de él y dentro del panel. */
static void dica_botao(superficie_t *t)
{
    if (S.col != 2 || S.sel < 0 || S.sel >= S.nbotao) return;
    char tx[40];
    int n = 0;
    const char *o = it(S.botao[S.sel])->texto;
    while (o[n] && n < (int)sizeof tx - 1) { tx[n] = o[n]; n++; }
    while (n > 0 && o[n - 1] == '.') n--;
    tx[n] = 0;
    int bl, bx = rodape_botao_x(S.sel, &bl);
    int w = texto_largura(tx) + 12, x = bx + bl - w;
    if (x < S.x + 4) x = S.x + 4;
    int y = rodape_y() - 22;
    sup_retangulo(t, x, y, w, 20, 0xFFFFE0);
    sup_moldura(t, x, y, w, 20, 0x404040);
    sup_texto(t, x + 6, y + 2, tx, 0x101010);
}

/* Una lupa de 14x14: el buscador. */
static void lupa(superficie_t *t, int x, int y, uint32_t cor)
{
    sup_moldura(t, x + 1, y, 8, 8, cor);
    sup_retangulo(t, x, y + 1, 1, 6, cor); sup_retangulo(t, x + 9, y + 1, 1, 6, cor);
    sup_retangulo(t, x + 2, y + 8, 6, 1, cor);
    for (int k = 0; k < 4; k++) sup_retangulo(t, x + 8 + k, y + 8 + k, 2, 2, cor);
}

void inicio_painel_desenhar(superficie_t *t)
{
    if (!S.aberto) return;
    cores_t c;
    cores(&c);
    /* sombra (mezcla con lo de debajo) */
    retangulo_alfa(t, S.x + S.l, S.y + SOMBRA, SOMBRA, S.a, 0x000000, 110);
    retangulo_alfa(t, S.x + SOMBRA, S.y + S.a, S.l - SOMBRA, SOMBRA, 0x000000, 110);
    if (!moderno()) {
        sup_retangulo(t, S.x, S.y, S.l, S.a, c.fundo_esq);
        /* cabecera con degradado azul, la cuenta y su imagen (el sagui) */
        degradado(t, S.x, S.y, S.l, S.cab_a, 0x2A5FC8, 0x1B3F94);
        sup_retangulo(t, S.x + 8, S.y + 8, 38, 38, 0xF2F2F2);
        sup_moldura(t, S.x + 8, S.y + 8, 38, 38, 0xFFFFFF);
        sup_desenho(t, S.x + 11, S.y + 11, &arte_sagui, 1);
        char nm[40];
        texto_cabe(nm, sizeof nm, S.p.usuario ? S.p.usuario : "", S.l - 70);
        sup_texto(t, S.x + 58, S.y + 19, nm, 0xFFFFFF);
        sup_texto(t, S.x + 59, S.y + 19, nm, 0xFFFFFF);
        sup_retangulo(t, S.x, S.y + S.cab_a - 2, S.l, 2, 0xE8A030);   /* el filete de color bajo la cabecera */
        sup_retangulo(t, S.dir_x, S.dir_y, S.dir_l + 1, S.dir_a, c.fundo_dir);
        sup_retangulo(t, S.dir_x - 1, S.dir_y, 1, S.dir_a, 0x9CB8E0);
        desenhar_lista(t, &c);
        desenhar_lugares(t, &c);
        degradado(t, S.x, S.y + S.a - S.rod_a, S.l, S.rod_a, 0x1B3F94, 0x2A5FC8);
        for (int k = 0; k < S.nbotao; k++) {
            int bl, bx = rodape_botao_x(k, &bl);
            (void)bl;
            desenhar_botao(t, bx, rodape_y(), it(S.botao[k])->icono, S.col == 2 && S.sel == k);
        }
        sup_moldura(t, S.x, S.y, S.l, S.a, 0x0A3C9C);
        dica_botao(t);
    } else {
        if (S.p.translucido) {
            retangulo_alfa(t, S.x, S.y, S.l, S.a, 0x0A1C34, 205);   /* cristal: lo de debajo se transparenta */
            retangulo_alfa(t, S.x, S.y, S.l, S.a / 3, 0x6A9AD0, 40);
        } else {
            degradado(t, S.x, S.y, S.l, S.a, 0x1E3250, 0x060C16);
        }
        /* la lista, sobre un fondo claro */
        retangulo_alfa(t, S.esq_x, S.esq_y, S.esq_l, S.esq_a, c.fundo_esq, S.p.translucido ? 236 : 255);
        desenhar_lista(t, &c);
        /* el buscador */
        sup_retangulo(t, S.busca_x, S.busca_y, S.busca_l, S.busca_a, 0xFFFFFF);
        sup_moldura(t, S.busca_x, S.busca_y, S.busca_l, S.busca_a, 0x7A8EA8);
        lupa(t, S.busca_x + S.busca_l - 20, S.busca_y + 5, 0x5A6A7A);
        char b[40];
        if (S.nbusca) {
            texto_cabe(b, sizeof b, S.busca, S.busca_l - 34);
            sup_texto(t, S.busca_x + 6, S.busca_y + 4, b, 0x1E2328);
            sup_retangulo(t, S.busca_x + 6 + texto_largura(b), S.busca_y + 4, 1, 16, 0x1E2328);
        } else {
            sup_texto(t, S.busca_x + 6, S.busca_y + 4, _("Buscar programas"), 0x7A8696);
        }
        desenhar_lugares(t, &c);
        sup_retangulo(t, S.dir_x + 8, S.y + S.a - BARRA_INF, S.dir_l - 16, 1, 0x4A6080);   /* separa los botones de los lugares */
        for (int k = 0; k < S.nbotao; k++) {
            int bl, bx = rodape_botao_x(k, &bl);
            (void)bl;
            desenhar_botao(t, bx, rodape_y(), it(S.botao[k])->icono, S.col == 2 && S.sel == k);
        }
        sup_moldura(t, S.x, S.y, S.l, S.a, 0x8AA8D0);
        dica_botao(t);
    }
}

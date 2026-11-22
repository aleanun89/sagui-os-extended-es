// SPDX-License-Identifier: GPL-2.0-or-later
/* menu.c - menú contextual del botón derecho (D051) y submenús (D064).
 *
 * Un solo menú abierto a la vez, dibujado por el gestor de ventanas por encima
 * de todo (ventanas, avisos y barra superior), en el mismo buffer de atrás y en el
 * mismo gui_mostrar: no hay una "ventana" para el menú, porque una ventana
 * tendría dueño, foco y cola de eventos, y el menú no necesita nada de eso.
 *
 * Mientras está abierto, el menú se queda con todo el ratón y todo el teclado (como
 * en Windows y GNOME): pasar el ratón resalta, hacer clic en una línea elige,
 * clic fuera o Esc cancela, flechas + Enter eligen con el teclado. Al
 * cerrarse, llama a quien lo abrió con el índice elegido (o -1).
 *
 * Las líneas vienen en un texto solo, separadas por '\n' (el mismo formato de la
 * llamada de sistema SIS_MENU_CONTEXTUAL): "-" es un separador y "~Texto" es
 * una línea desactivada (aparece en gris y no se puede elegir); un '\t'
 * separa el texto del atajo, que va alineado a la derecha. "*Texto" sale
 * marcada (la opción actual de una lista, como la resolución de la pantalla).
 *
 * Submenús (D064): ">Texto" abre un submenú, y las líneas siguientes son de él
 * hasta una línea "<" sola. Puede haber submenús dentro de submenús (hasta
 * MENU_PROF paneles). El submenú se abre al lado al pasar el ratón, al hacer clic
 * o con la flecha a la derecha; la flecha a la izquierda y Esc cierran sólo él.
 *
 * El índice devuelto cuenta todas las líneas del texto, separadores, ">" y "<"
 * incluidos: así no cambia cuando una línea se activa o desactiva, y
 * un texto sin submenús sigue con los mismos índices de antes. */
#include "gui.h"
#include "aparencia.h"
#include "abi.h"
#include "console.h"
#include "teclado.h"
#include "texto.h"
#include "video.h"
#include "versao.h"

#define LINHA_A    22    /* altura de una línea con texto */
#define SEP_A      9     /* altura de un separador */
#define MARGEM_X   14    /* espacio a la izquierda del texto */
#define MARGEM_ICONE 34  /* con iconos de 16x16: el icono en 12, el texto en 34 */
#define SETA_L     16    /* espacio a la derecha para la flecha del submenú */
#define MENU_L_MIN 150
#define SOMBRA     4
#define MENU_PROF  4     /* paneles abiertos al mismo tiempo: el menú y 3 niveles de submenú */

/* D428: los colores del menú salen del esquema de apariencia (con «SaguiOS», los de siempre). */
#define COR_FUNDO_MENU  (esquema()->menu_fundo)
#define COR_BORDA_MENU  (esquema()->menu_borde)
#define COR_TEXTO_MENU  (esquema()->menu_texto)
#define COR_DESATIVADO  (esquema()->texto_dis)
#define COR_DESTAQUE    (esquema()->menu_sel)
#define COR_SEPARADOR   (esquema()->menu_sep)
#define COR_ATALHO      (esquema()->menu_atalho)

enum { ITEM_NORMAL, ITEM_DESATIVADO, ITEM_SEPARADOR, ITEM_SUBMENU, ITEM_FIM };

typedef struct {
    int pai;                       /* línea ">" que abrió este panel; -1 = el menú de arriba */
    int x, y, l, a;
    int sel;                       /* línea resaltada, -1 = ninguna */
    bool icones;                   /* alguna línea tiene icono: el texto empieza más a la derecha */
} painel_t;

static struct {
    bool aberto;
    int  qtd;
    char texto[MENU_MAX_ITENS][MENU_MAX_TEXTO + 1];
    uint8_t tipo[MENU_MAX_ITENS];
    bool marcada[MENU_MAX_ITENS];
    int8_t pai[MENU_MAX_ITENS];    /* panel al que pertenece la línea (la línea ">" de él, o -1) */
    const desenho_t *icone[MENU_MAX_ITENS];
    painel_t p[MENU_PROF];
    int  np;                       /* paneles abiertos; el último es el que tiene el teclado */
    menu_resposta_t resposta;
    int  janela, dado;
    char faixa[24];                /* D361: texto de la franja lateral del primer panel ("" = sin franja) */
    int  tema;                     /* 1 = clásico (gris y azul marino) */
} m;

static int ancora_acima = -1;      /* D361: el menú que se abre ahora termina en esta y (la barra abajo) */
static char faixa_pendente[24];
static int  tema_pendente;
#define FAIXA_L 26                 /* ancho de la franja lateral del estilo clásico */

extern const unsigned char fonte_sagui[256][16];

void menu_ancorar_acima(int y) { ancora_acima = y; }
void menu_decorar(const char *faixa, int tema)
{
    faixa_pendente[0] = 0;
    if (faixa) { int i = 0; while (faixa[i] && i < (int)sizeof faixa_pendente - 1) { faixa_pendente[i] = faixa[i]; i++; } faixa_pendente[i] = 0; }
    tema_pendente = tema;
}

bool menu_aberto(void) { return m.aberto || inicio_painel_aberto(); }
int  menu_janela(void) { return m.aberto ? m.janela : 0; }
static bool tem_faixa(const painel_t *p) { return m.faixa[0] && p->pai < 0; }

static int altura_item(int i) { return m.tipo[i] == ITEM_SEPARADOR ? SEP_A : LINHA_A; }
static bool visivel(int i, int pai) { return m.pai[i] == pai && m.tipo[i] != ITEM_FIM; }

/* Un submenú sin ninguna línea visible no se abre: se trata como desactivado. */
static bool submenu_vazio(int i)
{
    for (int k = i + 1; k < m.qtd; k++)
        if (visivel(k, i)) return false;
    return true;
}

static bool escolhivel(int i)
{
    return m.tipo[i] == ITEM_NORMAL || (m.tipo[i] == ITEM_SUBMENU && !submenu_vazio(i));
}

/* Divide "Texto\tAtajo" (en el lugar: el '\t' se vuelve 0). */
static void separar_atalho(char *texto)
{
    for (char *c = texto; *c; c++)
        if (*c == '\t') { *c = 0; return; }
}

/* El atajo que separar_atajo() dejó después del primer 0, o "". */
static const char *atalho_de(int i)
{
    const char *at = m.texto[i] + txt_tamanho(m.texto[i]) + 1;
    return at < m.texto[i] + sizeof m.texto[i] ? at : "";
}

static void sujar_painel(painel_t *p)
{
    gui_redesenhar(p->x, p->y, p->l + SOMBRA, p->a + SOMBRA);
}

static void sujar(void)
{
    for (int k = 0; k < m.np; k++) sujar_painel(&m.p[k]);
}

/* Lee las líneas del texto. Devuelve cuántas. */
static int ler_itens(const char *itens)
{
    int n = 0, pilha[MENU_PROF], prof = 0, pai = -1;
    const char *p = itens;
    while (*p && n < MENU_MAX_ITENS) {
        const char *fim = p;
        while (*fim && *fim != '\n') fim++;
        uint8_t tipo = ITEM_NORMAL;
        bool marcada = false;
        const char *ini = p;
        m.pai[n] = (int8_t)pai;
        if (fim - p == 1 && *p == '-') {
            tipo = ITEM_SEPARADOR;
        } else if (fim - p == 1 && *p == '<') {
            tipo = ITEM_FIM;
            if (prof > 0) pai = pilha[--prof];
        } else {
            /* Prefijos, en cualquier orden y cada uno una vez: ~ desactivada,
             * * marcada, > submenú. Un submenú desactivado guarda las líneas
             * de él igual (sólo que nunca aparecen). */
            bool des = false, sub = false;
            for (;; ini++) {
                if (*ini == '~' && !des) des = true;
                else if (*ini == '*' && !marcada) marcada = true;
                else if (*ini == '>' && !sub) sub = true;
                else break;
            }
            if (sub && prof < MENU_PROF - 1) {
                tipo = des ? ITEM_DESATIVADO : ITEM_SUBMENU;
                pilha[prof++] = pai;
                pai = n;
            } else if (des) {
                tipo = ITEM_DESATIVADO;
            }
        }
        int k = 0;
        memset(m.texto[n], 0, sizeof m.texto[n]);   /* el atajo se busca después del primer 0 */
        if (tipo != ITEM_SEPARADOR && tipo != ITEM_FIM)
            for (const char *c = ini; c < fim && k < MENU_MAX_TEXTO; c++) m.texto[n][k++] = *c;
        m.texto[n][k] = 0;
        m.tipo[n] = tipo;
        m.marcada[n] = marcada;
        m.icone[n] = NULL;
        n++;
        p = *fim ? fim + 1 : fim;
    }
    return n;
}

/* Tamaño del panel de las líneas de "pai". Devuelve cuántas líneas muestra. */
static int medir(painel_t *p)
{
    int n = 0, larg = MENU_L_MIN, alt = 4;
    bool ic = false, setas = false;
    for (int i = 0; i < m.qtd; i++) {
        if (!visivel(i, p->pai)) continue;
        ic |= m.icone[i] != NULL;
        setas |= m.tipo[i] == ITEM_SUBMENU;
    }
    for (int i = 0; i < m.qtd; i++) {
        if (!visivel(i, p->pai)) continue;
        const char *at = atalho_de(i);
        int tl = texto_largura(m.texto[i]) + (at[0] ? texto_largura(at) + 32 : 0) +
                 (ic ? MARGEM_ICONE : MARGEM_X) + MARGEM_X + 4 + (setas ? SETA_L : 0);
        if (tl > larg) larg = tl;
        alt += altura_item(i);
        n++;
    }
    if (tem_faixa(p)) larg += FAIXA_L;
    p->l = larg; p->a = alt; p->icones = ic;
    return n;
}

/* Una línea en el log con lo que muestra el panel: las pruebas encuentran las líneas
 * por aquí ("-" separador, ">" submenú, "~" desactivada, "*" marcada). */
static void registrar_painel(painel_t *p, int n)
{
    char s[400];
    int k = 0;
    for (int i = 0; i < m.qtd && k < (int)sizeof s - 2; i++) {
        if (!visivel(i, p->pai)) continue;
        const char *pre = m.tipo[i] == ITEM_SUBMENU ? ">" : m.tipo[i] == ITEM_DESATIVADO ? "~" : "";
        k += snformatar(s + k, sizeof s - (uint32_t)k, "%s%s%s%s", k ? "|" : "", m.marcada[i] ? "*" : "", pre,
                        m.tipo[i] == ITEM_SEPARADOR ? "-" : m.texto[i]);
    }
    s[k < (int)sizeof s ? k : (int)sizeof s - 1] = 0;
    if (p->pai < 0)
        klog("[GUI] menu aberto em (%d,%d) %dx%d com %d linhas\n", p->x, p->y, p->l, p->a, n);
    else
        klog("[GUI] submenu %d (%s) aberto em (%d,%d) %dx%d com %d linhas\n", p->pai, m.texto[p->pai], p->x, p->y,
             p->l, p->a, n);
    klog("[GUI] menu linhas: %s\n", s);
}

/* Cierra los paneles a partir de k (k >= 1). */
static void fechar_paineis(int k)
{
    while (m.np > k) {
        sujar_painel(&m.p[--m.np]);
        klog("[GUI] submenu %d fechado\n", m.p[m.np].pai);
    }
}

/* Abre el submenú de la línea i al lado del último panel (que es el suyo). */
static void abrir_submenu(int i)
{
    if (m.np >= MENU_PROF || m.tipo[i] != ITEM_SUBMENU || !escolhivel(i)) return;
    if (m.p[m.np - 1].pai == i) return;   /* ya abierto */
    painel_t *dono = &m.p[m.np - 1];
    int y = dono->y + 2;
    for (int k = 0; k < i; k++)
        if (visivel(k, dono->pai)) y += altura_item(k);
    painel_t *p = &m.p[m.np];
    p->pai = i;
    p->sel = -1;
    int n = medir(p);
    /* A la derecha del panel de arriba, un poco por encima de su borde; si no
     * cabe, a la izquierda (como en los otros sistemas). En altura, la primera
     * línea queda al lado de la línea que abrió, subiendo si no cabe. */
    int L = (int)video_largura(), A = (int)video_altura();
    int x = dono->x + dono->l - 3;
    if (x + p->l > L) x = dono->x - p->l + 3;
    if (x < 0) x = 0;
    y -= 2;
    if (y + p->a > A) y = A - p->a;
    if (y < 0) y = 0;
    p->x = x; p->y = y;
    m.np++;
    sujar_painel(p);
    registrar_painel(p, n);
}

/* Cierra el menú y avisa a quien lo abrió. La respuesta se llama después de que el menú
 * ya esté cerrado: puede abrir otro menú (o una ventana) sin confusión. */
static void fechar_com(int indice)
{
    if (!m.aberto) return;
    sujar();
    m.aberto = false;
    m.np = 0;
    menu_resposta_t r = m.resposta;
    int janela = m.janela, dado = m.dado;
    if (indice >= 0) klog("[GUI] menu: escolhido %d (%s)\n", indice, m.texto[indice]);
    else klog("[GUI] menu: cancelado\n");
    if (r) r(indice, janela, dado);
}

void menu_cancelar(void)
{
    if (inicio_painel_aberto()) inicio_painel_fechar();
    fechar_com(-1);
}

bool menu_abrir_icones(int x, int y, const char *itens, const desenho_t *const *icones, menu_resposta_t resposta,
                       int janela, int dado)
{
    if (!gui_ativa()) return false;
    menu_cancelar();   /* uno a la vez: el anterior responde -1 */
    int ancora = ancora_acima;
    ancora_acima = -1;
    for (int i = 0; i < (int)sizeof m.faixa; i++) m.faixa[i] = faixa_pendente[i];
    m.tema = tema_pendente;
    m.qtd = ler_itens(itens);
    for (int i = 0; i < m.qtd; i++) {
        separar_atalho(m.texto[i]);
        if (icones) m.icone[i] = icones[i];
    }
    painel_t *p = &m.p[0];
    p->pai = -1;
    p->sel = -1;
    int n = medir(p);
    if (n == 0) return false;
    /* Recoloca para que quepa en la pantalla: como en los otros sistemas, si no cabe a la
     * derecha del cursor abre a la izquierda, y si no cabe abajo abre hacia
     * arriba; si ni así cabe, se pega al borde. */
    int L = (int)video_largura(), A = (int)video_altura();
    if (x + p->l > L) x -= p->l;
    if (ancora >= 0) y = ancora - p->a;   /* D361: la barra está abajo: el menú sube desde su borde */
    else if (y + p->a > A) y -= p->a;
    if (x + p->l > L) x = L - p->l;
    if (y + p->a > A) y = A - p->a;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    p->x = x; p->y = y;
    m.np = 1;
    m.resposta = resposta;
    m.janela = janela;
    m.dado = dado;
    m.aberto = true;
    sujar_painel(p);
    registrar_painel(p, n);
    return true;
}

bool menu_abrir(int x, int y, const char *itens, menu_resposta_t resposta, int janela, int dado)
{
    return menu_abrir_icones(x, y, itens, NULL, resposta, janela, dado);
}

/* Línea bajo el punto en el panel p (sólo las que se pueden elegir), o -1. */
static int item_em(painel_t *p, int px, int py)
{
    if (px < p->x + 2 || px >= p->x + p->l - 2) return -1;
    int y = p->y + 2;
    for (int i = 0; i < m.qtd; i++) {
        if (!visivel(i, p->pai)) continue;
        int h = altura_item(i);
        if (py >= y && py < y + h) return escolhivel(i) ? i : -1;
        y += h;
    }
    return -1;
}

/* El panel más de arriba (el último abierto) que contiene el punto, o -1. */
static int painel_em(int x, int y)
{
    for (int k = m.np - 1; k >= 0; k--) {
        painel_t *p = &m.p[k];
        if (x >= p->x && x < p->x + p->l && y >= p->y && y < p->y + p->a) return k;
    }
    return -1;
}

static void destacar(int k, int i)
{
    if (i == m.p[k].sel) return;
    m.p[k].sel = i;
    sujar_painel(&m.p[k]);
}

/* El ratón está sobre la línea i del panel k: resalta, cierra los submenús que
 * no son suyos y abre el suyo; con clic en una línea común, elige. */
static void apontar(int k, int i, bool clique)
{
    if (i < 0) {   /* separador o desactivada: sólo quita el resalte si no se abrió nada desde aquí */
        if (m.np == k + 1) destacar(k, -1);
        return;
    }
    destacar(k, i);
    if (m.np > k + 1 && m.p[k + 1].pai != i) fechar_paineis(k + 1);
    if (m.tipo[i] == ITEM_SUBMENU) abrir_submenu(i);
    else if (clique) fechar_com(i);
}

bool menu_tratar_mouse(int x, int y, unsigned botoes, unsigned mudaram)
{
    if (inicio_painel_aberto()) return inicio_painel_mouse(x, y, botoes, mudaram);
    if (!m.aberto) return false;
    unsigned apertou = mudaram & botoes;
    int k = painel_em(x, y);
    if (!apertou) {   /* movimiento, rueda o botón suelto: sólo acompaña el resalte */
        if (k >= 0) apontar(k, item_em(&m.p[k], x, y), false);
        return true;
    }
    if (k < 0) { fechar_com(-1); return true; }   /* clic fuera: cancela y el clic muere aquí */
    apontar(k, item_em(&m.p[k], x, y), true);     /* separador o desactivada: no pasa nada */
    return true;
}

/* Próxima línea del panel p que se puede elegir, avanzando "passo" (+1 o
 * -1) desde la línea "de" (-1 = desde el principio o el fin), dando la vuelta. */
static int proxima(painel_t *p, int de, int passo)
{
    int lin[MENU_MAX_ITENS], n = 0, pos = -1;
    for (int i = 0; i < m.qtd; i++)
        if (visivel(i, p->pai)) { if (i == de) pos = n; lin[n++] = i; }
    if (!n) return -1;
    for (int k = 1; k <= n; k++) {
        int j = ((pos < 0 ? (passo > 0 ? -1 : 0) : pos) + passo * k + n * 2) % n;
        if (escolhivel(lin[j])) return lin[j];
    }
    return -1;
}

static void destacar_teclado(int k, int i)
{
    destacar(k, i);
    if (i >= 0) klog("[GUI] menu: destaque %d (%s)\n", i, m.texto[i]);
}

/* Abre el submenú resaltado en el último panel y resalta su primera línea. */
static void entrar_submenu(void)
{
    painel_t *p = &m.p[m.np - 1];
    if (p->sel < 0 || m.tipo[p->sel] != ITEM_SUBMENU) return;
    int sel = p->sel;
    abrir_submenu(sel);
    if (m.p[m.np - 1].pai == sel) destacar_teclado(m.np - 1, proxima(&m.p[m.np - 1], -1, 1));
}

void menu_destacar_primeira(void)
{
    if (m.aberto) destacar_teclado(m.np - 1, proxima(&m.p[m.np - 1], -1, 1));
}

bool menu_tratar_tecla(int t)
{
    if (inicio_painel_aberto()) return inicio_painel_tecla(t);
    if (!m.aberto) return false;
    int k = m.np - 1;
    painel_t *p = &m.p[k];
    if (t == TECLA_SUPER) fechar_com(-1);
    else if (t == TECLA_ESC || t == TECLA_ESQUERDA) {
        if (k > 0) fechar_paineis(k);   /* vuelve al panel de arriba, con la línea del submenú resaltada */
        else if (t == TECLA_ESC) fechar_com(-1);
    }
    else if (t == TECLA_BAIXO) destacar_teclado(k, proxima(p, p->sel, 1));
    else if (t == TECLA_CIMA) destacar_teclado(k, proxima(p, p->sel, -1));
    else if (t == TECLA_HOME) destacar_teclado(k, proxima(p, -1, 1));
    else if (t == TECLA_END) destacar_teclado(k, proxima(p, -1, -1));
    else if (t == TECLA_DIREITA) entrar_submenu();
    else if ((t == '\n' || t == '\r' || t == ' ') && p->sel >= 0) {
        if (m.tipo[p->sel] == ITEM_SUBMENU) entrar_submenu();
        else fechar_com(p->sel);
    }
    return true;   /* las otras teclas mueren: el menú es modal */
}

/* Oscurece lo que ya está dibujado debajo (sombra translúcida, sin alfa
 * en el vídeo: cada canal a la mitad). Sólo funciona porque todo el rectángulo
 * se recompone en cada gui_mostrar, así que nunca oscurece dos veces. */
static void sombra(superficie_t *t, int x, int y, int l, int a)
{
    int x0 = x < t->cx0 ? t->cx0 : x, y0 = y < t->cy0 ? t->cy0 : y;
    int x1 = x + l > t->cx1 ? t->cx1 : x + l, y1 = y + a > t->cy1 ? t->cy1 : y + a;
    for (int j = y0; j < y1; j++) {
        uint32_t *p = t->px + j * t->larg;
        for (int i = x0; i < x1; i++) p[i] = (p[i] >> 1) & 0x7F7F7F;
    }
}

/* Flecha del submenú: un triángulo de 4x7 apuntando a la derecha. */
static void seta(superficie_t *t, int x, int y, uint32_t cor)
{
    for (int k = 0; k < 4; k++) sup_retangulo(t, x + k, y + k, 1, 7 - 2 * k, cor);
}

/* Marca de la línea "*": una tilde de 7x7. */
static void visto(superficie_t *t, int x, int y, uint32_t cor)
{
    static const int8_t dy[7] = { 3, 4, 5, 4, 3, 2, 1 };
    for (int k = 0; k < 7; k++) sup_retangulo(t, x + k, y + dy[k], 1, 2, cor);
}

/* D361: texto en vertical, de abajo arriba (como las franjas laterales de los menús clásicos). */
static void texto_vertical(superficie_t *t, int x, int ybase, const char *txt, uint32_t cor)
{
    for (int k = 0; txt[k]; k++) {
        const unsigned char *g = fonte_sagui[(uint8_t)txt[k]];
        for (int j = 0; j < 16; j++)
            for (int i = 0; i < 8; i++)
                if (g[j] & (0x80 >> i)) {
                    int px = x + j, py = ybase - k * 8 - i;
                    if (px >= t->cx0 && px < t->cx1 && py >= t->cy0 && py < t->cy1) t->px[py * t->larg + px] = cor;
                }
    }
}

/* La franja lateral del estilo clásico: un degradado vertical de azul marino a azul claro y el nombre del sistema. */
static void desenhar_faixa(superficie_t *t, painel_t *p)
{
    for (int j = 0; j < p->a - 2; j++) {
        int k = j * 255 / (p->a > 3 ? p->a - 3 : 1);   /* 0 arriba, 255 abajo */
        /* abajo (k = 255) el azul marino; arriba el azul claro */
        uint32_t rr = (uint32_t)(0x6C + (0x08 - 0x6C) * k / 255), gv = (uint32_t)(0x9C + (0x24 - 0x9C) * k / 255),
                 bb = (uint32_t)(0xD8 + (0x6A - 0xD8) * k / 255);
        sup_retangulo(t, p->x + 1, p->y + 1 + j, FAIXA_L - 2, 1, rr << 16 | gv << 8 | bb);
    }
    texto_vertical(t, p->x + 5, p->y + p->a - 8, m.faixa, 0xFFFFFF);
    texto_vertical(t, p->x + 6, p->y + p->a - 8, m.faixa, 0xFFFFFF);   /* negrita */
}

static void desenhar_painel(superficie_t *t, painel_t *p)
{
    bool classico = m.tema == 1 && !esquema()->barra_propia;   /* el estilo clásico es de SaguiOS; los demás esquemas traen sus colores */
    uint32_t fundo_c = classico ? 0xD4D0C8 : COR_FUNDO_MENU, destaque_c = classico ? 0x0A246A : COR_DESTAQUE;
    int dx = tem_faixa(p) ? FAIXA_L : 0;
    sombra(t, p->x + p->l, p->y + SOMBRA, SOMBRA, p->a);
    sombra(t, p->x + SOMBRA, p->y + p->a, p->l - SOMBRA, SOMBRA);
    sup_retangulo(t, p->x, p->y, p->l, p->a, fundo_c);
    sup_moldura(t, p->x, p->y, p->l, p->a, classico ? 0x404040 : COR_BORDA_MENU);
    if (classico) { sup_retangulo(t, p->x + 1, p->y + 1, p->l - 2, 1, 0xFFFFFF); sup_retangulo(t, p->x + 1, p->y + 1, 1, p->a - 2, 0xFFFFFF); }
    if (dx) desenhar_faixa(t, p);
    else if (!classico) sup_retangulo(t, p->x + 1, p->y + 1, 3, p->a - 2, COR_DESTAQUE);   /* la franja verde de la marca */
    int y = p->y + 2, tx = p->x + dx + (p->icones ? MARGEM_ICONE : MARGEM_X);
    for (int i = 0; i < m.qtd; i++) {
        if (!visivel(i, p->pai)) continue;
        int h = altura_item(i);
        if (m.tipo[i] == ITEM_SEPARADOR) {
            sup_retangulo(t, p->x + 8 + dx, y + SEP_A / 2, p->l - 16 - dx, 1, classico ? 0x808080 : COR_SEPARADOR);
        } else {
            bool ativo = escolhivel(i);
            uint32_t cor = ativo ? COR_TEXTO_MENU : COR_DESATIVADO;
            if (i == p->sel) {
                sup_retangulo(t, p->x + 4 + dx, y, p->l - 6 - dx, h, destaque_c);
                cor = classico ? 0xFFFFFF : esquema()->menu_sel_texto;
            }
            if (m.icone[i]) sup_desenho_metade(t, p->x + 12 + dx, y + (h - 16) / 2, m.icone[i]);
            if (m.marcada[i]) visto(t, p->x + 5 + dx + (p->icones ? 20 : 0), y + (h - 8) / 2, cor);
            sup_texto(t, tx, y + (h - 16) / 2, m.texto[i], cor);
            const char *at = atalho_de(i);
            if (*at)
                sup_texto(t, p->x + p->l - MARGEM_X - texto_largura(at) - (m.tipo[i] == ITEM_SUBMENU ? SETA_L : 0),
                          y + (h - 16) / 2, at, i == p->sel ? (classico ? 0xFFFFFF : esquema()->menu_sel_texto) : ativo ? COR_ATALHO : COR_DESATIVADO);
            if (m.tipo[i] == ITEM_SUBMENU) seta(t, p->x + p->l - 14, y + (h - 7) / 2, cor);
        }
        y += h;
    }
}

void menu_desenhar(superficie_t *t)
{
    inicio_painel_desenhar(t);
    if (!m.aberto) return;
    for (int k = 0; k < m.np; k++) desenhar_painel(t, &m.p[k]);
}

/* ------------------------------------------------------------ menús armados por el kernel */

void menu_lista_iniciar(menu_lista_t *ml)
{
    ml->texto[0] = 0;
    ml->tam = 0;
    ml->qtd = 0;
}

/* Añade una línea (sin el '\n'), la acción que dispara en la respuesta y
 * el icono (o NULL). Las líneas de más que no caben se ignoran. */
void menu_lista_linha(menu_lista_t *ml, const char *linha, int acao, const desenho_t *icone)
{
    if (ml->qtd >= MENU_MAX_ITENS) return;
    int n = (int)txt_tamanho(linha);
    if (ml->tam + n + 2 > (int)sizeof ml->texto) return;
    if (ml->qtd) ml->texto[ml->tam++] = '\n';
    memcpy(ml->texto + ml->tam, linha, (size_t)n);
    ml->tam += n;
    ml->texto[ml->tam] = 0;
    ml->acao[ml->qtd] = acao;
    ml->icone[ml->qtd] = icone;
    ml->qtd++;
}

int menu_lista_acao(const menu_lista_t *ml, int indice)
{
    return indice >= 0 && indice < ml->qtd ? ml->acao[indice] : MENU_NADA;
}

bool menu_lista_abrir(int x, int y, const menu_lista_t *ml, menu_resposta_t resposta, int janela, int dado)
{
    return menu_abrir_icones(x, y, ml->texto, ml->icone, resposta, janela, dado);
}

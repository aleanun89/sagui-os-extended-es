// SPDX-License-Identifier: GPL-3.0-only
/* janelas.c - el gestor de ventanas (H21). Ver D021.
 *
 * El kernel guarda la lista de ventanas en orden de apilamiento (la última es la
 * de arriba y tiene el foco), dibuja los marcos y entrega los eventos. El
 * contenido de cada ventana lo dibuja el propio programa, en el anillo 3,
 * en páginas que el kernel comparte con él.
 *
 * Redibujar = recomponer sólo el rectángulo que cambió: fondo listo del
 * escritorio, ventanas de abajo hacia arriba (marco + contenido), barra superior
 * al final. Todo recortado en el rectángulo, en un buffer de atrás (video).
 *
 * Una tarea del kernel despierta en cada tic, consume las colas del ratón y del
 * teclado, arrastra, enfoca y cierra ventanas, y entrega los eventos. Ella y las
 * llamadas al sistema tocan las mismas listas; por eso la tarea trabaja
 * con las interrupciones desactivadas (las llamadas ya corren así). */
#include "acoes.h"
#include "gui.h"
#include "idioma.h"
#include "avisos.h"
#include "montagem.h"
#include "vfs.h"
#include "permisos_dialogo.h"
#include "sessao.h"
#include "aparencia.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "mouse.h"
#include "paginacao.h"
#include "processo.h"
#include "rapido.h"
#include "relogio.h"
#include "sistema.h"
#include "versao.h"
#include "teclado.h"
#include "texto.h"
#include "video.h"
#include "telas.h"
#include "disco.h"
#include "acel2d.h"
#include "sensores_conv.h"

#define MAX_JANELAS     32
#define FILA_EVENTOS    64
#define BARRA_TITULO    24
#define BORDA           2
#define JANELAS_USUARIO USUARIO_JANELAS   /* donde aparecen los píxeles en el espacio del programa (D049) */
#define ESPACO_JANELA   0x00800000u   /* 8 MB por ventana: cabe la pantalla completa en 1920x1080 (D059) */
#define DUPLO_CLIQUE    45            /* tics (450 ms) */

typedef struct {
    bool       usada;
    int        id;
    int        pid;
    int        x, y;          /* esquina del marco */
    int        larg, alt;     /* área de contenido */
    char       titulo[48];
    uint32_t  *px;            /* píxeles (físico = virtual en el kernel) */
    uint32_t   paginas;
    uint32_t   virt;          /* dirección de los píxeles en el espacio del programa */
    evento_t   fila[FILA_EVENTOS];
    uint32_t   ini, fim;
    /* Tamaño variable, maximizar, minimizar y pantalla completa (D044). */
    uint32_t   estilo;                   /* JAN_REDIMENSIONAVEL, JAN_TELA_CHEIA_OK */
    int        min_l, min_a;             /* menor área de contenido aceptada */
    bool       maximizada, minimizada, tela_cheia;
    int        rest_x, rest_y, rest_l, rest_a;   /* geometría antes de maximizar */
    int        tc_x, tc_y, tc_l, tc_a;           /* geometría antes de pantalla completa */
    /* D365: ventana de escritorio (widget): sin marco (el contenido empieza en x, y), siempre debajo de las
     * normales, sin foco ni botón en la barra. opac: 1-100 % (100 = opaca). */
    bool       widget, clave;
    int        opac;
} janela_t;

static janela_t janelas[MAX_JANELAS];
static int ordem[MAX_JANELAS], qtd;   /* índices en janelas[], de abajo hacia arriba */
static int proximo_id = 1;
static bool ativa;
static superficie_t tela;
static int suj_x0, suj_y0, suj_x1, suj_y1;   /* rectángulo a recomponer */

/* Estado del ratón en el gestor. */
static int arrastando = -1, arr_dx, arr_dy;   /* ventana que se arrastra y dónde se agarró */
/* D195: medida del arrastre (cuadros y ciclos del TSC de cada vuelta del gestor que movió la ventana). */
static bool arr_moveu, arr_gpu;   /* arr_gpu: en este arrastre movió la GPU */
static uint32_t arr_quadros;
static uint64_t arr_ciclos;
static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static int captura = -1;                       /* ventana que recibió el último clic */
static int cap_id = -1;                        /* D242: id de la ventana con el ratón capturado, o -1 */
static uint32_t ult_clique_tique;
static int ult_clique_x, ult_clique_y, ult_clique_alvo = -999;

/* Tamaño variable y pantalla completa (D044): funciones al final del archivo. */
static int redimensionando = -1;   /* ventana cuya esquina se está arrastrando */
static void desenhar_botoes_extras(janela_t *j, bool foco);
static void desenhar_alca(janela_t *j);
static void desenhar_contorno_redimensionar(void);
static bool desenhar_tela_cheia(void);
static bool tratar_mouse_extras(evento_mouse_t *m);
static void tratar_roda(evento_mouse_t *m);
static void tratar_roda_h(evento_mouse_t *m);
static bool comecar_redimensionar(int idx, int rx, int ry);
static bool clique_titulo(janela_t *j, int rx, int ry);
static bool tecla_do_gerenciador(janela_t *j, int t);
static void sair_tela_cheia(janela_t *j);
static void menu_titulo(janela_t *j, int x, int y);
static void geometria_tela_cheia(janela_t *j, int *ox, int *oy, int *dl, int *da);

bool gui_ativa(void) { return ativa; }

/* Las notificaciones (H34, D125, D233) viven en avisos.c (issue #16); el diálogo de permisos (D269) en permisos_dialogo.c. */

/* D365: el desplazamiento del contenido respecto a la esquina de la ventana (un widget no tiene marco). */
static int bd(const janela_t *j) { return j->widget ? 0 : BORDA; }
static int td(const janela_t *j) { return j->widget ? 0 : BARRA_TITULO; }
static int moldura_l(janela_t *j) { return j->widget ? j->larg : j->larg + 2 * BORDA; }
static int moldura_a(janela_t *j) { return j->widget ? j->alt : j->alt + BARRA_TITULO + BORDA; }
static bool widgets_editar;   /* D365: modo «editar widgets»: se arrastran sin Alt y se ve su contorno */

void gui_redesenhar(int x, int y, int l, int a)
{
    if (l <= 0 || a <= 0) return;
    if (suj_x0 >= suj_x1) { suj_x0 = x; suj_y0 = y; suj_x1 = x + l; suj_y1 = y + a; return; }
    if (x < suj_x0) suj_x0 = x;
    if (y < suj_y0) suj_y0 = y;
    if (x + l > suj_x1) suj_x1 = x + l;
    if (y + a > suj_y1) suj_y1 = y + a;
}

/* Incluye el contorno oscuro de 1 píxel, dibujado por fuera del marco (sin
 * eso, arrastrar dejaba rastros). */
static void redesenhar_janela(janela_t *j)
{
    gui_redesenhar(j->x - 1, j->y - 1, moldura_l(j) + 2, moldura_a(j) + 2);
}

/* La ventana de arriba que no está minimizada. */
static janela_t *focada(void)
{
    for (int k = qtd - 1; k >= 0; k--)
        if (!janelas[ordem[k]].minimizada && !janelas[ordem[k]].widget) return &janelas[ordem[k]];
    return NULL;
}

static void desenhar_moldura(janela_t *j, bool foco)
{
    int x = j->x, y = j->y, l = moldura_l(j), a = moldura_a(j);
    uint32_t barra = foco ? aparencia_acento() : 0x5A6470;
    sup_retangulo(&tela, x, y, l, BARRA_TITULO, barra);
    sup_retangulo(&tela, x, y + BARRA_TITULO, BORDA, a - BARRA_TITULO, barra);
    sup_retangulo(&tela, x + l - BORDA, y + BARRA_TITULO, BORDA, a - BARRA_TITULO, barra);
    sup_retangulo(&tela, x, y + a - BORDA, l, BORDA, barra);
    sup_moldura(&tela, x - 1, y - 1, l + 2, a + 2, 0x101010);
    sup_texto(&tela, x + 10, y + 4, j->titulo, foco ? 0xFFFFFF : 0xD0D4D8);
    /* Botón de cerrar: un cuadrado rojo con una X. */
    int bx = x + l - BORDA - 20, by = y + 4;
    sup_retangulo(&tela, bx, by, 16, 16, foco ? 0xD03C32 : 0x8A5A56);
    for (int k = 3; k < 13; k++) {
        sup_retangulo(&tela, bx + k, by + k, 2, 1, 0xFFFFFF);
        sup_retangulo(&tela, bx + 15 - k - 1, by + k, 2, 1, 0xFFFFFF);
    }
    desenhar_botoes_extras(j, foco);
}

/* La ventana más alta cuyo contenido cubre todo el recorte: lo que está
 * debajo de ella (fondo y otras ventanas) no aparece y no necesita ser
 * dibujado. Es el caso común: un programa actualizando su propia ventana
 * (D042). Devuelve -1 si ninguna cubre (dibuja todo desde el fondo). */
static int primeira_visivel(const superficie_t *t)
{
    for (int k = qtd - 1; k >= 0; k--) {
        janela_t *j = &janelas[ordem[k]];
        if (j->minimizada || (j->widget && (j->opac < 100 || j->clave))) continue;
        int cx = j->x + bd(j), cy = j->y + td(j);
        if (cx <= t->cx0 && cy <= t->cy0 && cx + j->larg >= t->cx1 && cy + j->alt >= t->cy1)
            return k;
    }
    return -1;
}

/* Recompone el rectángulo sucio y muestra. Las copias y los rectángulos grandes
 * usan SSE2 cuando la medición del arranque lo eligió (rapido.c); el inicio/fin
 * envuelve todo para guardar los XMM del proceso sólo una vez. */
void gui_mostrar(void)
{
    if (!ativa || suj_x0 >= suj_x1) return;
    int x = suj_x0, y = suj_y0, l = suj_x1 - suj_x0, a = suj_y1 - suj_y0;
    suj_x0 = suj_x1 = 0;
    sup_recorte(&tela, x, y, l, a);
    if (tela.cx0 >= tela.cx1 || tela.cy0 >= tela.cy1) return;
    bool sse = rapido_inicio();
    if (desenhar_tela_cheia()) {
        atalhos_desenhar(&tela);
        menu_desenhar(&tela);
        pd_desenhar(&tela);   /* D269 */
        sessao_desenhar(&tela);   /* D340 */
        video_marcar(tela.cx0, tela.cy0, tela.cx1 - tela.cx0, tela.cy1 - tela.cy0);
        video_mostrar();
        rapido_fim(sse);
        return;
    }
    janela_t *foco = focada();
    int k0 = primeira_visivel(&tela);
    bool coberto = k0 >= 0;
    if (!coberto) { k0 = 0; area_fundo(&tela); }
    for (int k = k0; k < qtd; k++) {
        janela_t *j = &janelas[ordem[k]];
        if (j->minimizada) continue;
        if (j->widget) {   /* D365: sin marco; con opacidad se mezcla con lo que hay debajo */
            if (j->opac < 100 || j->clave) sup_misturar(&tela, j->x, j->y, j->px, j->larg, j->alt, j->larg, j->opac, j->clave);
            else sup_copiar(&tela, j->x, j->y, j->px, j->larg, j->alt, j->larg);
            if (widgets_editar) sup_moldura(&tela, j->x, j->y, j->larg, j->alt, aparencia_acento());
            continue;
        }
        if (!(coberto && k == k0)) desenhar_moldura(j, j == foco);   /* la de abajo sólo muestra el contenido */
        sup_copiar(&tela, j->x + BORDA, j->y + BARRA_TITULO, j->px, j->larg, j->alt, j->larg);
        desenhar_alca(j);
    }
    desenhar_contorno_redimensionar();
    avisos_desenhar(&tela);
    if (barra_intersecta(tela.cy0, tela.cy1)) barra_desenhar(&tela);   /* Inicio, tareas y reloj (D065); arriba o abajo (D361) */
    barra_popup_desenhar(&tela);   /* panel o ayuda de la bandeja (D114) */
    atalhos_desenhar(&tela);       /* panel de Alt+Tab (D119) */
    menu_desenhar(&tela);   /* el menú contextual queda por encima de todo (D051) */
    pd_desenhar(&tela);     /* D269: el diálogo de permisos, sobre todo lo demás, atenúa también la barra */
    sessao_desenhar(&tela); /* D340: la pantalla de inicio de sesión lo cubre todo */
    video_marcar(tela.cx0, tela.cy0, tela.cx1 - tela.cx0, tela.cy1 - tela.cy0);
    video_mostrar();
    rapido_fim(sse);
}

/* ------------------------------------------------------------ eventos */

static void entregar(janela_t *j, evento_t *e)
{
    e->janela = j->id;
    if (e->tipo == EV_MOVER_CRU && j->ini != j->fim) {   /* D242: los deltas crudos se suman, no se pierden */
        evento_t *u = &j->fila[(j->fim + FILA_EVENTOS - 1) % FILA_EVENTOS];
        if (u->tipo == EV_MOVER_CRU && u->botoes == e->botoes) { u->x += e->x; u->y += e->y; return; }
    }
    uint32_t prox = (j->fim + 1) % FILA_EVENTOS;
    if (prox == j->ini) {
        /* Cola llena: los movimientos pueden descartarse; lo demás no. */
        if (e->tipo == EV_MOVER) return;
        j->ini = (j->ini + 1) % FILA_EVENTOS;
    }
    j->fila[j->fim] = *e;
    j->fim = prox;
    acordar(processo_por_pid(j->pid));
}

/* D230: el puntero del sistema no se dibuja sobre el contenido de la ventana
 * con foco si tiene JAN_SEM_CURSOR (el programa dibuja el suyo: C&C, juegos de
 * la SDL). Vuelve a verse fuera de ella, con un menú, el panel de la bandeja o
 * el de Alt+Tab abiertos, mientras se arrastra o redimensiona una ventana,
 * sobre el aviso, si la ventana pierde el foco y cuando se cierra (también si el
 * programa muere: su ventana se destruye). Se recalcula en cada vuelta del
 * gestor; el ratón dibuja (en su IRQ) con la zona que hay. */
static void atualizar_zona_cursor(void)
{
    janela_t *j = ativa ? focada() : NULL;
    int x = 0, y = 0, l = 0, a = 0;
    if (j && (j->estilo & JAN_SEM_CURSOR) && !pd_ativo() && !sessao_ativa() && !menu_aberto() && !barra_popup_visivel() && !atalhos_visivel() &&
        arrastando < 0 && redimensionando < 0) {
        if (j->tela_cheia) {
            geometria_tela_cheia(j, &x, &y, &l, &a);
        } else {
            x = j->x + BORDA; y = j->y + BARRA_TITULO; l = j->larg; a = j->alt;
            if (y < BARRA_SUPERIOR) { a -= BARRA_SUPERIOR - y; y = BARRA_SUPERIOR; }   /* la barra de arriba va encima */
        }
        if (avisos_visivel()) {   /* el aviso (que se puede pulsar) va encima: con él, el cursor se ve */
            int ax, ay, al, aa;
            avisos_retangulo(&ax, &ay, &al, &aa);
            if (ax < x + l && ax + al > x && ay < y + a && ay + aa > y) l = a = 0;
        }
    }
    if (video_zona_sem_cursor(x, y, l, a)) {
        video_mostrar();
        klog("[GUI] cursor %s\n", l > 0 && a > 0 ? "oculto sobre a janela" : "visivel");
    }
}

static int janela_em(int x, int y)   /* índice en ordem[], de arriba hacia abajo */
{
    for (int k = qtd - 1; k >= 0; k--) {
        janela_t *j = &janelas[ordem[k]];
        if (j->minimizada) continue;
        if (x >= j->x && x < j->x + moldura_l(j) && y >= j->y && y < j->y + moldura_a(j))
            return k;
    }
    return -1;
}

static void trazer_para_cima(int k)
{
    if (janelas[ordem[k]].widget) return;   /* D365: un widget no sube ni toma el foco */
    area_foco_perder();   /* D268: el teclado deja de ser del escritorio */
    if (k == qtd - 1) return;
    janela_t *antes = focada();
    int i = ordem[k];
    for (; k < qtd - 1; k++) ordem[k] = ordem[k + 1];
    ordem[qtd - 1] = i;
    if (antes) redesenhar_janela(antes);
    redesenhar_janela(&janelas[i]);
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

/* D195: arrastrar con el acelerador 2D. Lo que ya está en la pantalla (la
 * ventana con su marco y su contorno) lo mueve la GPU con video_mover, y solo se
 * recompone lo que queda al descubierto: las franjas de la unión de las dos
 * posiciones que no cubre el destino. Solo si la ventana es la de arriba y no
 * hay nada dibujado encima (menú, panel, aviso, Alt+Tab): si no, el camino de
 * siempre (recomponer todo el rectángulo). */
static bool mover_com_gpu(janela_t *j, int nx, int ny)
{
    if (!acel2d_ativo() || j->tela_cheia || j->minimizada || qtd == 0 || &janelas[ordem[qtd - 1]] != j) return false;
    if (barra_cfg(BC_AUTOOCULTAR) || barra_fundo()) return false;   /* D361: la barra puede estar encima de lo que se copia */
    if (menu_aberto() || barra_popup_visivel() || atalhos_visivel() || avisos_visivel() || pd_ativo() || sessao_ativa() || redimensionando >= 0) return false;
    gui_mostrar();   /* lo pendiente primero: la pantalla tiene que ser lo que se compuso */
    int L = (int)video_largura(), A = (int)video_altura();
    int ox = j->x - 1, oy = j->y - 1, l = moldura_l(j) + 2, a = moldura_a(j) + 2;
    int ddx = nx - j->x, ddy = ny - j->y;
    /* Origen: lo visible de la posición vieja (la barra superior queda encima de
     * todo y no se mueve); destino: eso desplazado, recortado igual. */
    int sx0 = imax(ox, 0), sy0 = imax(oy, BARRA_SUPERIOR), sx1 = imin(ox + l, L), sy1 = imin(oy + a, A);
    int dx0 = imax(sx0 + ddx, 0), dy0 = imax(sy0 + ddy, BARRA_SUPERIOR);
    int dx1 = imin(sx1 + ddx, L), dy1 = imin(sy1 + ddy, A);
    j->x = nx; j->y = ny;
    int ux0 = imin(ox, ox + ddx), uy0 = imin(oy, oy + ddy), ux1 = imax(ox + l, ox + l + ddx), uy1 = imax(oy + a, oy + a + ddy);
    if (dx0 >= dx1 || dy0 >= dy1) {   /* nada visible que mover */
        gui_redesenhar(ux0, uy0, ux1 - ux0, uy1 - uy0);
        return true;
    }
    video_mover(dx0 - ddx, dy0 - ddy, dx1 - dx0, dy1 - dy0, ddx, ddy);
    /* Lo que queda de la unión fuera del destino: arriba, abajo, izquierda y derecha. */
    int f[4][4] = {
        { ux0, uy0, ux1 - ux0, dy0 - uy0 }, { ux0, dy1, ux1 - ux0, uy1 - dy1 },
        { ux0, dy0, dx0 - ux0, dy1 - dy0 }, { dx1, dy0, ux1 - dx1, dy1 - dy0 },
    };
    for (int k = 0; k < 4; k++)
        if (f[k][2] > 0 && f[k][3] > 0) {
            gui_redesenhar(f[k][0], f[k][1], f[k][2], f[k][3]);
            gui_mostrar();
        }
    video_mostrar();   /* el cursor, si la GPU lo movió con la ventana */
    arr_gpu = true;
    return true;
}

/* ---- D242: captura del ratón ---- */
static janela_t *janela_cap_id(int id)
{
    for (int i = 0; i < MAX_JANELAS; i++)
        if (janelas[i].usada && janelas[i].id == id) return &janelas[i];
    return NULL;
}

/* Suelta la captura (si la hay) y avisa a la ventana. El cursor reaparece en el centro de ella. */
static void soltar_captura(const char *motivo)
{
    if (cap_id < 0) return;
    janela_t *j = janela_cap_id(cap_id);
    cap_id = -1;
    mouse_captura(false, 0, 0);
    klog("[GUI] captura do mouse solta (%s)\n", motivo);
    if (j) { evento_t e = { .tipo = EV_CAPTURA_SOLTA }; entregar(j, &e); }
}

/* Cada vuelta del gestor: la captura solo vale mientras su ventana tenga el foco y nada se
 * interponga (menús, paneles, Alt+Tab), y Ctrl+Alt siempre la suelta. */
static void verificar_captura(void)
{
    if (cap_id < 0) return;
    janela_t *j = janela_cap_id(cap_id);
    if (!j) { soltar_captura("ventana cerrada"); return; }
    if ((teclado_modificadores() & (MOD_CTRL | MOD_ALT)) == (MOD_CTRL | MOD_ALT)) soltar_captura("Ctrl+Alt");
    else if (j != focada() || j->minimizada) soltar_captura("perdio el foco");
    else if (menu_aberto() || barra_popup_visivel() || atalhos_visivel() || pd_ativo() || sessao_ativa() || arrastando >= 0) soltar_captura("menu o panel");
}

/* Con la captura, el ratón es solo de la ventana: deltas crudos, botones en el centro, rueda. */
static bool tratar_captura(evento_mouse_t *m)
{
    janela_t *j = janela_cap_id(cap_id);
    if (!j) return false;
    int cx = j->larg / 2, cy = j->alt / 2;
    if (m->dz) tratar_roda(m);
    if (m->dw) tratar_roda_h(m);
    if (m->dx || m->dy) {
        evento_t e = { .tipo = EV_MOVER_CRU, .x = m->dx, .y = m->dy, .botoes = m->botoes };
        entregar(j, &e);
    }
    if (m->mudaram & m->botoes) {
        evento_t e = { .tipo = EV_CLIQUE, .x = cx, .y = cy, .botoes = m->mudaram & m->botoes, .cliques = 1,
                       .modificadores = teclado_modificadores() & 3 };
        entregar(j, &e);
    }
    if (m->mudaram & ~m->botoes) {
        evento_t e = { .tipo = EV_SOLTOU, .x = cx, .y = cy, .botoes = m->mudaram & ~m->botoes };
        entregar(j, &e);
    }
    return true;
}

/* ---- D365: widgets de escritorio ---- */
static void resposta_widget(int indice, int id, int dado)
{
    (void)dado;
    janela_t *j = NULL;
    for (int i = 0; i < MAX_JANELAS; i++) if (janelas[i].usada && janelas[i].id == id) j = &janelas[i];
    if (!j || indice < 0) return;
    if (indice == 0) {   /* Mover: el modo de edición, para arrastrar sin Alt */
        gui_widgets_editar(true);
    } else if (indice == 1) {
        evento_t e = { .tipo = EV_WIDGET, .tecla = EVW_QUITAR };
        entregar(j, &e);
        klog("[GUI] widget %d (%s): quitar\n", j->id, j->titulo);
    } else if (indice == 3) {
        gui_widgets_editar(!widgets_editar);
    }
}

/* Botón apretado sobre un widget (k = su lugar en ordem[]). Alt+izquierdo, o el izquierdo en el modo de edición,
 * lo arrastra; el derecho abre su menú; el resto va al programa sin darle el foco. */
static void tratar_clique_widget(int k, evento_mouse_t *m)
{
    int idx = ordem[k];
    janela_t *j = &janelas[idx];
    captura = -1;
    if ((m->mudaram & BOTAO_ESQUERDO) && (widgets_editar || (teclado_modificadores() & MOD_ALT))) {
        arrastando = idx;
        arr_dx = m->x - j->x; arr_dy = m->y - j->y;
        return;
    }
    if (m->mudaram & BOTAO_DIREITO) {
        char t[MENU_MAX_TEXTO * 4];
        snformatar(t, sizeof t, "%s\n%s\n-\n%s%s", _("Mover widget"), _("Quitar widget"), widgets_editar ? "*" : "", _("Editar widgets"));
        menu_abrir(m->x, m->y, t, resposta_widget, j->id, 0);
        return;
    }
    evento_t e = { .tipo = EV_CLIQUE, .x = m->x - j->x, .y = m->y - j->y, .botoes = m->mudaram & m->botoes,
                   .cliques = 1, .modificadores = teclado_modificadores() & 3 };
    entregar(j, &e);
    captura = idx;
}

void gui_widgets_editar(bool ligar)
{
    if (widgets_editar == ligar) return;
    widgets_editar = ligar;
    for (int i = 0; i < MAX_JANELAS; i++) if (janelas[i].usada && janelas[i].widget) redesenhar_janela(&janelas[i]);
    gui_mostrar();
    klog("[GUI] editar widgets: %s\n", ligar ? "si" : "no");
}
bool gui_widgets_editando(void) { return widgets_editar; }
int gui_widgets_contar(void)
{
    int n = 0;
    for (int i = 0; i < MAX_JANELAS; i++) if (janelas[i].usada && janelas[i].widget) n++;
    return n;
}

/* Otra sesión (D340): los widgets de la anterior terminan; los de la nueva los lanza el arranque de la sesión. */
void gui_widgets_sessao_nova(void)
{
    widgets_editar = false;
    for (int i = 0; i < MAX_JANELAS; i++) {
        janela_t *j = &janelas[i];
        if (!j->usada || !j->widget) continue;
        evento_t e = { .tipo = EV_WIDGET, .tecla = EVW_FIN_SESION };
        entregar(j, &e);
    }
}

static void tratar_mouse(evento_mouse_t *m)
{
    if (sessao_ativa()) {   /* D340: la pantalla de inicio de sesión: todo el ratón es suyo */
        arrastando = -1; captura = -1; redimensionando = -1;
        sessao_mouse(m->x, m->y, m->botoes, m->mudaram);
        return;
    }
    if (pd_ativo()) {   /* D269: el diálogo de permisos es modal: todo el ratón es suyo */
        arrastando = -1; captura = -1; redimensionando = -1;
        pd_mouse(m->x, m->y, m->botoes, m->mudaram);
        return;
    }
    if (cap_id >= 0 && tratar_captura(m)) return;
    bool apertou = (m->mudaram & m->botoes) != 0;
    bool soltou = (m->mudaram & ~m->botoes) != 0;

    if (m->dw) {   /* D407: rueda horizontal: a la ventana de debajo, aparte */
        if (!menu_aberto() && !barra_popup_visivel()) tratar_roda_h(m);
        m->dw = 0;
        if (!m->mudaram && !m->dx && !m->dy && !m->dz) return;
    }
    if (m->dz && inicio_painel_roda(m->dz)) {   /* D361: la rueda desplaza la lista del menú Inicio de columnas */
        m->dz = 0;
        if (!m->mudaram && !m->dx && !m->dy) return;
    }
    /* Con un menú contextual abierto, todo el ratón es suyo (D051). */
    if (menu_tratar_mouse(m->x, m->y, m->botoes, m->mudaram)) return;
    if (barra_popup_mouse(m->x, m->y, m->botoes, m->mudaram)) return;   /* panel de la bandeja abierto (D114) */
    if (apertou && avisos_clique(m->x, m->y)) return;   /* aviso pulsable: abre el Visor o reproduce el CD (D125, D233) */
    if (m->dz && barra_roda(m->x, m->y, m->dz)) {   /* rueda sobre el icono del volumen */
        m->dz = 0;
        if (!m->mudaram && !m->dx && !m->dy) return;
    }
    if (m->dz) {   /* rueda (D048): aparte; si el paquete sólo tenía la rueda, terminó */
        tratar_roda(m);
        if (!m->mudaram && !m->dx && !m->dy) return;
    }
    if (tratar_mouse_extras(m)) return;   /* pantalla completa, redimensionar */
    if (area_icone_arrasto(m->x, m->y, m->botoes)) return;   /* D268: arrastre de iconos o rectángulo de selección */
    if (arrastando >= 0) {
        janela_t *j = &janelas[arrastando];
        if (!j->usada || !(m->botoes & BOTAO_ESQUERDO)) {
            if (j->usada) klog("[GUI] janela %d movida para (%d,%d)\n", j->id, j->x, j->y);
            if (j->usada && j->widget) {   /* D365: el widget guarda su posición */
                evento_t ew = { .tipo = EV_WIDGET, .tecla = EVW_MOVIDO, .x = j->x, .y = j->y };
                entregar(j, &ew);
            }
            if (arr_quadros)   /* D195: la medida, para la prueba h161 */
                klog("[GUI] arrastre: %u cuadros, %u kciclos por cuadro (2D: %s)\n", arr_quadros,
                     (uint32_t)sconv_div64(arr_ciclos, arr_quadros * 1000u), acel2d_nome());
            acel2d_contar_arrasto(arr_quadros, arr_ciclos);
            arr_quadros = 0; arr_ciclos = 0;
            /* D195: al soltar, la ventana se recompone una vez desde su búfer: la
             * GPU movió lo que había en la pantalla, y un programa puede haber
             * cambiado su búfer sin avisar de todo; así queda igual que con la CPU. */
            if (arr_gpu && j->usada) redesenhar_janela(j);
            arr_gpu = false;
            arrastando = -1;
            return;
        }
        int nx = m->x - arr_dx, ny = m->y - arr_dy;
        if (ny < BARRA_SUPERIOR) ny = BARRA_SUPERIOR;
        if (ny > (int)video_altura() - barra_fundo() - BARRA_TITULO) ny = (int)video_altura() - barra_fundo() - BARRA_TITULO;   /* D361: la barra de abajo no tapa el título */
        if (nx > (int)video_largura() - 40) nx = (int)video_largura() - 40;
        if (nx < 40 - moldura_l(j)) nx = 40 - moldura_l(j);
        if (nx != j->x || ny != j->y) {
            arr_moveu = true;
            if (j->widget || !mover_com_gpu(j, nx, ny)) {
                redesenhar_janela(j);
                j->x = nx; j->y = ny;
                redesenhar_janela(j);
            }
        }
        return;
    }

    int bl, ba;
    telas_principal_rect(&bl, &ba);
    if (apertou && barra_contem(m->x, m->y) && m->x < bl) {   /* D290: la barra solo ocupa la pantalla principal */   /* ninguna ventana sube hasta la barra (D065) */
        barra_tratar_clique(m->x, m->y, m->mudaram & m->botoes);
        captura = -1;
        return;
    }
    if (apertou) {
        int k = janela_em(m->x, m->y);
        if (k < 0) {
            /* En el escritorio: doble clic en un ícono abre el programa. */
            int ic = area_icone_em(m->x, m->y);
            uint32_t agora = relogio_tiques();
            bool duplo = ic >= 0 && ult_clique_alvo == 1000 + ic && agora - ult_clique_tique < DUPLO_CLIQUE;
            ult_clique_alvo = ic >= 0 ? 1000 + ic : -999;
            ult_clique_tique = agora;
            if (m->mudaram & BOTAO_ESQUERDO) area_icone_clique(ic, m->x, m->y, (teclado_modificadores() & MOD_CTRL) != 0);   /* D268 */
            if (duplo && (m->mudaram & BOTAO_ESQUERDO)) { area_icone_abrir(ic); ult_clique_alvo = -999; }
            if (m->mudaram & BOTAO_DIREITO) {   /* menú del ícono o del escritorio (D051) */
                if (ic >= 0) area_icone_direito(ic, m->x, m->y);
                else if (m->y >= BARRA_SUPERIOR) area_menu_fundo(m->x, m->y);
            }
            captura = -1;
            return;
        }
        if (janelas[ordem[k]].widget) {   /* D365: un widget no sube ni toma el foco */
            tratar_clique_widget(k, m);
            return;
        }
        trazer_para_cima(k);
        janela_t *j = focada();
        int idx = ordem[qtd - 1];
        int rx = m->x - j->x, ry = m->y - j->y;
        if ((m->mudaram & BOTAO_ESQUERDO) && comecar_redimensionar(idx, rx, ry)) { captura = -1; return; }
        if (ry < BARRA_TITULO) {
            int bx = moldura_l(j) - BORDA - 20;
            if ((m->mudaram & BOTAO_ESQUERDO) && clique_titulo(j, rx, ry)) { captura = -1; return; }
            if (m->mudaram & BOTAO_DIREITO) { menu_titulo(j, m->x, m->y); captura = -1; return; }
            if ((m->mudaram & BOTAO_ESQUERDO) && rx >= bx && rx < bx + 16 && ry >= 4 && ry < 20) {
                evento_t e = { .tipo = EV_FECHAR };
                entregar(j, &e);
                klog("[GUI] fechar janela %d (%s)\n", j->id, j->titulo);
            } else if (m->mudaram & BOTAO_ESQUERDO) {
                arrastando = idx;
                arr_dx = rx; arr_dy = ry;
            }
            captura = -1;
            return;
        }
        int cx = m->x - j->x - BORDA, cy = m->y - j->y - BARRA_TITULO;
        if (cx < 0 || cy < 0 || cx >= j->larg || cy >= j->alt) return;   /* en el borde */
        uint32_t agora = relogio_tiques();
        int dx = cx - ult_clique_x, dy = cy - ult_clique_y;
        bool duplo = ult_clique_alvo == j->id && agora - ult_clique_tique < DUPLO_CLIQUE &&
                     dx > -5 && dx < 5 && dy > -5 && dy < 5;
        evento_t e = { .tipo = EV_CLIQUE, .x = cx, .y = cy, .botoes = m->mudaram & m->botoes,
                       .cliques = duplo ? 2 : 1, .modificadores = teclado_modificadores() & 3 };
        ult_clique_alvo = duplo ? -999 : j->id;
        ult_clique_tique = agora;
        ult_clique_x = cx; ult_clique_y = cy;
        entregar(j, &e);
        captura = idx;
        return;
    }

    if (captura >= 0 && janelas[captura].usada) {
        janela_t *j = &janelas[captura];
        evento_t e = { .tipo = soltou ? EV_SOLTOU : EV_MOVER, .x = m->x - j->x - bd(j),
                       .y = m->y - j->y - td(j), .botoes = soltou ? m->mudaram : m->botoes };
        entregar(j, &e);
        if (soltou) captura = -1;
        return;
    }
    /* Movimiento sin botón: sólo para la ventana con foco, si el ratón está en ella. */
    janela_t *j = focada();
    if (j) {
        int cx = m->x - j->x - BORDA, cy = m->y - j->y - BARRA_TITULO;
        if (cx >= 0 && cy >= 0 && cx < j->larg && cy < j->alt) {
            evento_t e = { .tipo = EV_MOVER, .x = cx, .y = cy, .botoes = 0 };
            entregar(j, &e);
        }
    }
}

/* Abre un programa que sólo tiene sentido una vez (el Administrador de tareas,
 * la ventana "Pantalla") o, si ya tiene una ventana, sólo la trae hacia arriba
 * (restaurando si estaba minimizada). Devuelve true si ya estaba abierto. */
bool gui_abrir_unico(const char *processo, const char *caminho, int argc, char *argv[])
{
    for (int k = qtd - 1; k >= 0; k--) {
        if (janelas[ordem[k]].widget) continue;   /* D365: el widget de un programa no es «su ventana» (no sube) */
        processo_t *p = processo_por_pid(janelas[ordem[k]].pid);
        if (p && p->estado != P_ZUMBI && txt_compara(p->nome, processo) == 0) {
            if (janelas[ordem[k]].minimizada) janelas[ordem[k]].minimizada = false;
            trazer_para_cima(k);
            redesenhar_janela(&janelas[ordem[qtd - 1]]);
            klog("[GUI] %s ja aberto (pid %d): janela trazida para cima\n", processo, p->pid);
            return true;
        }
    }
    int pid = processo_executar(caminho, argc, argv, NULL);
    klog("[GUI] abrir %s (pid %d)\n", caminho, pid);
    return false;
}

/* Ctrl+Shift+Esc (D043) y los menús (D051, D065). */
void gui_abrir_tarefas(void)
{
    char *argv[] = { "tareas", NULL };
    if (gui_abrir_unico("tareas", "/bin/tareas", 1, argv))
        klog("[GUI] Ctrl+Shift+Esc: Administrador de tareas ja aberto\n");
    else
        klog("[GUI] Ctrl+Shift+Esc -> /bin/tareas\n");
}

static void tarefa_gerenciador(void *arg)
{
    (void)arg;
    static const char canal = 0;
    for (;;) {
        int_desliga();
        verificar_captura();   /* D242 */
        injecao_vigiar();   /* D251: el escritorio remoto se fue con teclas pulsadas */
        uint64_t t_volta = ler_tsc();   /* D195: medida del arrastre */
        arr_moveu = false;
        evento_mouse_t m;
        while (mouse_tentar(&m))
            tratar_mouse(&m);
        int t;
        uint32_t mods;   /* los modificadores de cuando se pulsó la tecla, no los de ahora (D119) */
        while ((t = teclado_tentar_mods(&mods)) >= 0) {
            if (sessao_tecla(t)) continue;   /* D340: la pantalla de inicio de sesión, con todo el teclado */
            if (pd_tecla(t)) continue;   /* D269: el diálogo de permisos se queda con todo el teclado */
            if (atalhos_tecla(t, mods)) continue;   /* Alt+Tab, Alt+F4, Windows, Ctrl+Alt+Supr (D119) */
            if (menu_tratar_tecla(t)) continue;   /* menú contextual abierto: el teclado es suyo (D051) */
            if (barra_popup_tecla(t)) continue;   /* panel de la bandeja (D114) */
            if (t == TECLA_ESC && (mods & 3) == 3) { gui_abrir_tarefas(); continue; }
            /* Ctrl+Esc: menú Inicio (D065; la tecla Windows va por atalhos.c). En
             * pantalla completa la barra no aparece: sale de ella antes, como en Windows. */
            if (t == TECLA_ESC && (mods & 3) == MOD_CTRL) {
                gui_sair_tela_cheia();
                barra_abrir_inicio(true);
                continue;
            }
            if (area_icone_tecla(t, mods)) continue;   /* D268: flechas e Intro con el foco en el escritorio */
            janela_t *j = focada();
            if (!j) continue;
            if (tecla_do_gerenciador(j, t)) continue;   /* F11 y Esc de la pantalla completa */
            /* & 7: Shift, Ctrl y Alt (el Mico usa Alt+flechas, D075); el AltGr queda fuera, ya convirtió el carácter. */
            evento_t e = { .tipo = EV_TECLA, .tecla = (unsigned)t, .modificadores = mods & 7 };
            entregar(j, &e);
        }
        atalhos_tique();   /* soltar Alt (Alt+Tab) o la tecla Windows sola (D119) */
        barra_tique();   /* reloj y botones de las tareas: sólo ensucia lo que cambió */
        /* Pendrive: los avisos del kernel se convierten en notificaciones; el ícono acompaña. */
        int tipo;
        char texto[256], caminho[64];
        while (disco_proximo_aviso(&tipo, texto, sizeof texto, caminho, sizeof caminho))
            gui_aviso_ex(texto, tipo == AVISO_AUDIO && caminho[0] ? caminho : NULL, tipo == AVISO_AUDIO,
                         tipo == AVISO_AUDIO && caminho[0] ? 10 : 0);   /* D233: pulsable, reproduce el CD */
        /* D403: las teclas de función (volumen, brillo, radio) y su aviso en pantalla. */
        acoes_atender();
        int osd_t, osd_v;
        if (acoes_osd_proximo(&osd_t, &osd_v)) gui_aviso_osd(osd_t, osd_v);
        char aviso_k[ACOES_AVISO_BYTES];   /* D405: avisos de texto del kernel (radios del HP...) */
        while (acoes_aviso_proximo(aviso_k, sizeof aviso_k)) gui_aviso(aviso_k);
        sessao_tique();  /* D340: inicio y fin de sesión */
        pd_tique();      /* D269: ¿hay un programa pidiendo permisos de administrador? */
        avisos_tique();  /* caduca el aviso y muestra el siguiente de la cola */
        disco_t *pd = NULL;
        for (int k = 0; k < MAX_DISCOS && !pd; k++) { disco_t *d = disco_indice(k); if (d && d->presente && !d->pai && d->removivel && !d->de_boot) pd = d; }
        if (area_pendrive(pd != NULL, pd ? (pd->rotulo[0] ? pd->rotulo : "Pendrive") : NULL))
            gui_redesenhar(0, BARRA_SUPERIOR, (int)video_largura(), (int)video_altura() - BARRA_SUPERIOR);   /* D268: el icono puede estar en cualquier casilla */
        gui_mostrar();
        atualizar_zona_cursor();   /* D230 */
        if (arr_moveu && arrastando >= 0) { arr_quadros++; arr_ciclos += ler_tsc() - t_volta; }
        /* Duerme hasta el próximo tic (el planificador despierta por el despertador). */
        processo_atual()->acordar_em = relogio_tiques() + 1;
        bloquear(&canal);
        ceder();
    }
}

/* D324 (8.4.3r2): la imagen «base» lleva /etc/apps-fuera en el ramdisk (el Makefile la pone solo
 * cuando las aplicaciones van en la partición SAGUI-APPS). Si al llegar al escritorio /apps no
 * está montada, el PC no ve bien el disco de arranque (#36): se avisa una vez por arranque. */
static void aviso_apps_ausentes(void)
{
    if (!vfs_existe_arquivo("/etc/apps-fuera")) return;
    const char *rel;
    montagem_t *m = montagem_de("/apps", &rel);
    if (m && m->tam_ponto == 5) return;   /* montada: todo bien */
    /* D379 (M1): con la raíz en disco, /apps va dentro de ella (no es una partición aparte). */
    meta_t mt;
    if (m && m == montagem_da_raiz() && vfs_meta("/apps/bin", &mt) == 0 && mt.tipo == TIPO_PASTA) return;
    klog("[GUI] aviso: falta /apps (SAGUI-APPS no montada); usa saguios-completo.img (docs/imagenes.md)\n");
    gui_aviso(_("No se encontró la partición de aplicaciones (/apps): este PC no ve bien el disco de arranque. "
                "Algunas aplicaciones no estarán. Usa saguios-completo.img (más información: docs/imagenes.md)."));
}

bool gui_iniciar(void)
{
    if (!video_ativo() || !area_iniciar()) return false;
    sup_iniciar(&tela, video_buffer(), (int)video_largura(), (int)video_altura());
    ativa = true;
    gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
    gui_mostrar();
    tarefa_kernel("janelas", tarefa_gerenciador, NULL);
    klog("[GUI] area de trabalho pronta\n");
    aviso_apps_ausentes();
    return true;
}

/* ------------------------------------------------------------ llamadas al sistema */

static janela_t *janela_do_processo(int id)
{
    for (int i = 0; i < MAX_JANELAS; i++)
        if (janelas[i].usada && janelas[i].id == id)
            return janelas[i].pid == processo_recursos()->pid ? &janelas[i] : NULL;   /* D167: del proceso, no del hilo */
    return NULL;
}

static void destruir(int i)
{
    janela_t *j = &janelas[i];
    redesenhar_janela(j);
    if (j->tela_cheia || j->minimizada) gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
    if (redimensionando == i) redimensionando = -1;
    processo_t *dono = processo_por_pid(j->pid);
    if (dono && dono->usuario && dono->espaco != espaco_kernel()) {
        for (uint32_t k = 0; k < j->paginas; k++)
            espaco_desmapear(dono->espaco, j->virt + k * PAGINA);
        /* Su programa puede estar corriendo en otra CPU con esas páginas en
         * la TLB: antes de liberarlas, que las olvide (D165). Con hilos
         * (D167), también cuando cierra la ventana el propio programa: otro
         * hilo suyo puede correr en otra CPU. Esta CPU ya las olvidó
         * (espaco_desmapear invalida si el espacio es el actual). */
        espaco_sincronizar(dono->espaco);
    }
    memfisica_liberar_seguidas((uint32_t)j->px, j->paginas);
    j->usada = false;
    if (menu_janela() == j->id) menu_cancelar();   /* el menú era de esta ventana: desaparece con ella */
    int k = 0;
    while (k < qtd && ordem[k] != i) k++;
    for (; k < qtd - 1; k++) ordem[k] = ordem[k + 1];
    if (k < qtd) qtd--;
    if (arrastando == i) arrastando = -1;
    if (captura == i) captura = -1;
    if (cap_id == j->id) { cap_id = -1; mouse_captura(false, 0, 0); klog("[GUI] captura do mouse solta (janela fechada)\n"); }
    if (focada()) redesenhar_janela(focada());   /* la nueva ventana de arriba gana el foco */
}

void janelas_do_processo_fechar(processo_t *p)
{
    for (int i = 0; i < MAX_JANELAS; i++)
        if (janelas[i].usada && janelas[i].pid == p->pid)
            destruir(i);
    gui_mostrar();
}

static int32_t criar_janela_comum(uint32_t ptitulo, uint32_t dims, uint32_t ppixels, bool widget, int opac, bool clave)
{
    if (!ativa) return ERRO_ACESSO;
    char titulo[48];
    int r = usuario_texto((const char *)ptitulo, titulo, sizeof titulo);
    if (r < 0) return r;
    int l = (int)(dims & 0xFFFF), a = (int)(dims >> 16);
    if (l < 40 || a < 20 || l > JANELA_LARG_MAX || a > JANELA_ALT_MAX) return ERRO_INVALIDO;
    int pl, pa;
    telas_principal_rect(&pl, &pa);   /* D290: las ventanas nuevas aparecen en la pantalla principal */
    if (widget && (l > pl || a > pa - BARRA_SUPERIOR)) return ERRO_INVALIDO;   /* D365: un widget cabe en la pantalla */
    if (!usuario_ok((void *)ppixels, 4, true)) return ERRO_ENDERECO;
    int i = 0;
    while (i < MAX_JANELAS && janelas[i].usada) i++;
    if (i == MAX_JANELAS) return ERRO_LIMITE;
    uint32_t paginas = ((uint32_t)(l * a) * 4 + PAGINA - 1) / PAGINA;
    uint32_t fis = memfisica_alocar_seguidas(paginas);
    if (!fis) return ERRO_SEM_MEMORIA;
    janela_t *j = &janelas[i];
    memset(j, 0, sizeof *j);
    j->usada = true;
    j->id = proximo_id++;
    j->widget = widget;
    j->opac = opac;
    j->clave = clave;
    if (!widget) area_foco_perder();   /* D268: una ventana nueva toma el teclado (un widget no) */
    j->pid = processo_recursos()->pid;   /* D167: la ventana es del proceso */
    j->larg = l; j->alt = a;
    j->px = (uint32_t *)fis;
    j->paginas = paginas;
    j->virt = JANELAS_USUARIO + (uint32_t)i * ESPACO_JANELA;
    txt_copia_n(j->titulo, titulo, sizeof j->titulo);
    for (uint32_t k = 0; k < (uint32_t)(l * a); k++) j->px[k] = 0xF4F4F0;
    for (uint32_t k = 0; k < paginas; k++)
        espaco_mapear(processo_atual()->espaco, j->virt + k * PAGINA, fis + k * PAGINA, PG_USUARIO | PG_ESCRITA | PG_EMPRESTADA | PG_NX);
    /* Posición en cascada, sin pasar de la pantalla. */
    int n = j->id % 8;
    j->x = 150 + n * 34;
    j->y = BARRA_SUPERIOR + 30 + n * 28;
    if (j->x + moldura_l(j) > pl) j->x = pl - moldura_l(j);
    if (j->y + moldura_a(j) > pa) j->y = pa - moldura_a(j);
    if (j->x < 0) j->x = 0;
    if (j->y < BARRA_SUPERIOR) j->y = BARRA_SUPERIOR;
    if (widget) {
        /* D365: los widgets ocupan siempre la parte de abajo de la pila (por encima del fondo). */
        int nw = 0;
        while (nw < qtd && janelas[ordem[nw]].widget) nw++;
        for (int k = qtd; k > nw; k--) ordem[k] = ordem[k - 1];
        ordem[nw] = i;
        qtd++;
        j->x = pl - j->larg - 20; j->y = BARRA_SUPERIOR + 20;
        if (j->x < 0) j->x = 0;
        redesenhar_janela(j);
        gui_mostrar();
        *(uint32_t *)ppixels = j->virt;
        klog("[GUI] widget %d criado por %d: \"%s\" %dx%d opacidade %d%%\n", j->id, j->pid, j->titulo, l, a, opac);
        return j->id;
    }
    janela_t *antes = focada();
    if (antes && antes->tela_cheia) sair_tela_cheia(antes);   /* la ventana nueva necesita aparecer */
    if (antes) redesenhar_janela(antes);
    ordem[qtd++] = i;
    redesenhar_janela(j);
    if (janelas_contar(j->pid) == 1) barra_janela_criada(processo_atual()->nome);   /* "más usados" del menú Inicio */
    gui_mostrar();
    *(uint32_t *)ppixels = j->virt;
    klog("[GUI] janela %d criada por %d: \"%s\" %dx%d em (%d,%d)\n", j->id, j->pid, j->titulo, l, a, j->x, j->y);
    return j->id;
}

int32_t sis_criar_janela(uint32_t ptitulo, uint32_t dims, uint32_t ppixels, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    return criar_janela_comum(ptitulo, dims, ppixels, false, 100, false);
}

/* D365: SIS_CRIAR_WIDGET (titulo, dims, &pixels, opacidad). */
int32_t sis_criar_widget(uint32_t ptitulo, uint32_t dims, uint32_t ppixels, uint32_t opac, uint32_t e)
{
    (void)e;
    int o = (opac & 0xFF) == 0 ? 100 : (int)(opac & 0xFF);
    if (o < 1 || o > 100 || (opac & ~(uint32_t)(0xFF | WIDGET_CLAVE))) return ERRO_INVALIDO;
    return criar_janela_comum(ptitulo, dims, ppixels, true, o, (opac & WIDGET_CLAVE) != 0);
}

int32_t sis_fechar_janela(uint32_t id, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    janela_t *j = janela_do_processo((int)id);
    if (!j) return ERRO_INVALIDO;
    if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351: otro hilo puede dormir escribiendo en sus píxeles */
    destruir((int)(j - janelas));
    gui_mostrar();
    return 0;
}

int32_t sis_atualizar_janela(uint32_t id, uint32_t pret, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    janela_t *j = janela_do_processo((int)id);
    if (!j) return ERRO_INVALIDO;
    int x = 0, y = 0, l = j->larg, a = j->alt;
    if (pret) {
        if (!usuario_ok((void *)pret, sizeof(retangulo_t), false)) return ERRO_ENDERECO;
        retangulo_t *r = (retangulo_t *)pret;
        x = r->x; y = r->y; l = r->largura; a = r->altura;
        if (x < 0) { l += x; x = 0; }
        if (y < 0) { a += y; y = 0; }
        if (x + l > j->larg) l = j->larg - x;
        if (y + a > j->alt) a = j->alt - y;
    }
    if (j->tela_cheia) {
        /* D245: solo la zona que cambió, no la pantalla entera (las bandas negras ya están). */
        int ox, oy, dl, da;
        geometria_tela_cheia(j, &ox, &oy, &dl, &da);
        int px0 = ox + x * dl / j->larg - 1, py0 = oy + y * da / j->alt - 1;
        int px1 = ox + (x + l) * dl / j->larg + 2, py1 = oy + (y + a) * da / j->alt + 2;
        if (l <= 0 || a <= 0) return 0;
        gui_redesenhar(px0, py0, px1 - px0, py1 - py0);
    } else gui_redesenhar(j->x + bd(j) + x, j->y + td(j) + y, l, a);
    gui_mostrar();
    return 0;
}

int32_t sis_proximo_evento(uint32_t id, uint32_t pev, uint32_t esperar, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (!usuario_ok((void *)pev, sizeof(evento_t), true)) return ERRO_ENDERECO;
    bool tem_janela = false;
    for (int i = 0; i < MAX_JANELAS; i++) {
        janela_t *j = &janelas[i];
        if (!j->usada || j->pid != processo_recursos()->pid || (id && j->id != (int)id)) continue;
        tem_janela = true;
        if (j->ini != j->fim) {
            *(evento_t *)pev = j->fila[j->ini];
            j->ini = (j->ini + 1) % FILA_EVENTOS;
            if (esperar & EVENTO_PRAZO) processo_atual()->acordar_em = 0;   /* el plazo ya no vale */
            return 1;
        }
    }
    if (!tem_janela) return ERRO_INVALIDO;
    if (!esperar) return 0;
    if (esperar & EVENTO_PRAZO) {
        /* Esperar con plazo (D083): los 31 bits de abajo son el instante, en ms
         * del reloj de SIS_TEMPO, en que se rinde. La llamada se repite al
         * despertar (por un evento o por el reloj) y devuelve 0 si el plazo
         * pasó sin nada en la cola. */
        uint32_t ms = relogio_tiques() * (1000 / TIQUES_POR_SEGUNDO);
        uint32_t falta = ((esperar & ~EVENTO_PRAZO) - ms) & ~EVENTO_PRAZO;
        if (falta == 0 || falta >= 0x40000000u) { processo_atual()->acordar_em = 0; return 0; }
        processo_atual()->acordar_em = relogio_tiques() + (falta * TIQUES_POR_SEGUNDO + 999) / 1000;
    }
    bloquear(processo_recursos());   /* "entregar" despierta al dueño de la ventana (D167: el proceso, todos sus hilos) */
    return SIS_REPETIR;
}

/* SIS_TECLAS_SEGURADAS (&mapa[32]): qué teclas están pulsadas ahora.
 * Sólo quien tiene la ventana en foco recibe el mapa; los demás reciben todo en cero,
 * para que un programa en segundo plano no espíe lo que se escribe. */
int32_t sis_teclas_seguradas(uint32_t pmapa, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    if (!usuario_ok((void *)pmapa, 32, true)) return ERRO_INVALIDO;
    uint8_t mapa[32];
    memset(mapa, 0, sizeof mapa);
    janela_t *j = focada();
    /* D371: con la pantalla de inicio de sesión o de bloqueo, o el diálogo de permisos, el teclado es suyo: las
     * teclas pulsadas (una contraseña) no se le dicen a la ventana que tenía el foco. */
    bool foco = j && j->pid == processo_recursos()->pid && !sessao_ativa() && !pd_ativo();
    if (foco) teclado_seguradas(mapa);
    memcpy((void *)pmapa, mapa, 32);
    return foco ? 1 : 0;
}

int32_t sis_titulo_janela(uint32_t id, uint32_t ptitulo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    janela_t *j = janela_do_processo((int)id);
    if (!j) return ERRO_INVALIDO;
    int r = usuario_texto((const char *)ptitulo, j->titulo, sizeof j->titulo);
    if (r < 0) return r;
    if (!j->widget) gui_redesenhar(j->x, j->y, moldura_l(j), BARRA_TITULO);
    gui_mostrar();
    return 0;
}

/* ------------------------------------------------------------ tamaño variable y pantalla completa (D044)
 *
 * Quien decide el tamaño es la persona (arrastrando la esquina, maximizando, F11),
 * pero quien cambia los píxeles es el programa: el kernel manda EV_REDIMENSIONAR
 * con el tamaño pedido y el programa responde con SIS_REDIMENSIONAR_JANELA,
 * que cambia las páginas en la misma dirección virtual y ya vuelve con el dibujo
 * anterior copiado. Un programa que no responde sigue con el tamaño que tenía. */

#define ALCA       14   /* esquina de abajo a la derecha que redimensiona */
#define BORDA_PEGA 4    /* franja a la derecha y abajo que redimensiona */

static int red_x0, red_y0, red_l0, red_a0, red_l, red_a, red_modo;   /* modo: 1 ancho, 2 alto */

static int tela_l(void) { return (int)video_largura(); }
static int tela_a(void) { return (int)video_altura(); }

/* D290: varias pantallas. tela_l y tela_a son el escritorio entero; la pantalla donde está una ventana
 * (la que contiene el centro de su marco) es la que se usa para maximizarla y para la pantalla completa.
 * Con una sola pantalla es todo el escritorio, como antes. */
typedef struct { int x, y, l, a; } mon_t;
static mon_t mon_da(janela_t *j)
{
    mon_t m = { 0, 0, (int)video_largura(), (int)video_altura() };
    if (telas_qtd() > 1) {
        int c = telas_da_posicao(j->x + moldura_l(j) / 2, j->y + moldura_a(j) / 2);
        telas_rect(c, &m.x, &m.y, &m.l, &m.a);
    }
    return m;
}
/* Donde empieza una ventana maximizada en esa pantalla: ninguna sube hasta la barra de arriba (D065). */
static int mon_topo(mon_t m) { return m.y > BARRA_SUPERIOR ? m.y : BARRA_SUPERIOR; }
static int mc_l(mon_t m) { return m.l - 2 * BORDA; }
/* D361: la barra de abajo (de la pantalla principal) también quita área de trabajo. */
static int mon_fundo(mon_t m) { return (m.x == 0 && m.y == 0) ? barra_fundo() : 0; }
static int mc_a(mon_t m) { return m.y + m.a - mon_topo(m) - mon_fundo(m) - BARRA_TITULO - BORDA; }
static int minimo_l(janela_t *j) { return j->min_l > 40 ? j->min_l : 40; }
static int minimo_a(janela_t *j) { return j->min_a > 20 ? j->min_a : 20; }
static bool redimensionavel(janela_t *j) { return (j->estilo & JAN_REDIMENSIONAVEL) != 0; }
static bool dialogo(janela_t *j) { return (j->estilo & JAN_DIALOGO) != 0; }
static void redesenhar_tudo(void) { gui_redesenhar(0, 0, tela_l(), tela_a()); }

static int indice_na_ordem(janela_t *j)
{
    for (int k = 0; k < qtd; k++)
        if (&janelas[ordem[k]] == j) return k;
    return -1;
}

static void pedir_tamanho(janela_t *j, int l, int a, unsigned estado)
{
    evento_t e = { .tipo = EV_REDIMENSIONAR, .x = l, .y = a, .tecla = estado };
    entregar(j, &e);
    klog("[GUI] janela %d: pedido de tamanho %dx%d\n", j->id, l, a);
}

/* ---- botones de maximizar y minimizar, asa de la esquina */

/* Posición (relativa al marco) de los botones a la izquierda de la X. */
static int botao_max_x(janela_t *j) { return moldura_l(j) - BORDA - 40; }
static int botao_min_x(janela_t *j) { return moldura_l(j) - BORDA - (redimensionavel(j) ? 60 : 40); }

static void desenhar_botoes_extras(janela_t *j, bool foco)
{
    uint32_t fundo = foco ? aparencia_clarear(aparencia_escurecer(aparencia_acento(), 25), 25) : 0x6E7680, risco = 0xFFFFFF;
    int by = j->y + 4;
    if (redimensionavel(j)) {
        int bx = j->x + botao_max_x(j);
        sup_retangulo(&tela, bx, by, 16, 16, fundo);
        if (j->maximizada) {   /* restaurar: dos cuadrados superpuestos */
            sup_moldura(&tela, bx + 6, by + 3, 7, 7, risco);
            sup_retangulo(&tela, bx + 3, by + 6, 7, 7, fundo);
            sup_moldura(&tela, bx + 3, by + 6, 7, 7, risco);
            sup_retangulo(&tela, bx + 3, by + 6, 7, 2, risco);
        } else {
            sup_moldura(&tela, bx + 3, by + 3, 10, 10, risco);
            sup_retangulo(&tela, bx + 3, by + 3, 10, 2, risco);
        }
    }
    if (dialogo(j)) return;   /* diálogo: sin botón en la barra de tareas, no hay cómo volver si se minimiza */
    int bx = j->x + botao_min_x(j);
    sup_retangulo(&tela, bx, by, 16, 16, fundo);
    sup_retangulo(&tela, bx + 4, by + 11, 8, 2, risco);
}

/* Tres rayas diagonales en la esquina de abajo a la derecha: "se puede tirar desde aquí". */
static void desenhar_alca(janela_t *j)
{
    if (!redimensionavel(j) || j->maximizada) return;
    int x1 = j->x + moldura_l(j) - BORDA - 1, y1 = j->y + moldura_a(j) - BORDA - 1;
    for (int r = 4; r <= 12; r += 4)
        for (int k = 0; k < r; k++) {
            sup_retangulo(&tela, x1 - r + k + 1, y1 - k, 1, 1, 0x5A6470);
            sup_retangulo(&tela, x1 - r + k + 2, y1 - k, 1, 1, 0xFFFFFF);
        }
}

/* ---- arrastrar la esquina o el borde */

static void contorno_retangulo(janela_t *j, int *x, int *y, int *l, int *a)
{
    *x = j->x - 1; *y = j->y - 1;
    *l = red_l + 2 * BORDA + 2; *a = red_a + BARRA_TITULO + BORDA + 2;
}

static void redesenhar_contorno(void)
{
    if (redimensionando < 0) return;
    int x, y, l, a;
    contorno_retangulo(&janelas[redimensionando], &x, &y, &l, &a);
    gui_redesenhar(x, y, l, a);
}

static void desenhar_contorno_redimensionar(void)
{
    if (redimensionando < 0) return;
    int x, y, l, a;
    contorno_retangulo(&janelas[redimensionando], &x, &y, &l, &a);
    sup_moldura(&tela, x, y, l, a, 0x101010);
    sup_moldura(&tela, x + 1, y + 1, l - 2, a - 2, 0xFFDF4A);
    sup_moldura(&tela, x + 2, y + 2, l - 4, a - 4, 0xFFDF4A);
}

static bool comecar_redimensionar(int idx, int rx, int ry)
{
    janela_t *j = &janelas[idx];
    if (!redimensionavel(j) || j->tela_cheia) return false;
    int ml = moldura_l(j), ma = moldura_a(j), modo = 0;
    if (rx >= ml - ALCA && ry >= ma - ALCA) modo = 3;
    else if (rx >= ml - BORDA_PEGA && ry >= BARRA_TITULO) modo = 1;
    else if (ry >= ma - BORDA_PEGA) modo = 2;
    if (!modo) return false;
    redimensionando = idx;
    red_modo = modo;
    red_x0 = j->x + rx; red_y0 = j->y + ry;
    red_l0 = red_l = j->larg; red_a0 = red_a = j->alt;
    redesenhar_contorno();
    return true;
}

static void continuar_redimensionar(evento_mouse_t *m)
{
    janela_t *j = &janelas[redimensionando];
    if (!j->usada) { redimensionando = -1; return; }
    if (!(m->botoes & BOTAO_ESQUERDO)) {
        redesenhar_contorno();
        redimensionando = -1;
        klog("[GUI] janela %d: canto solto em %dx%d\n", j->id, red_l, red_a);
        if (red_l != j->larg || red_a != j->alt) {
            if (j->maximizada) { j->maximizada = false; redesenhar_janela(j); }
            pedir_tamanho(j, red_l, red_a, JAN_ESTADO_NORMAL);
        }
        return;
    }
    int l = red_l0 + ((red_modo & 1) ? m->x - red_x0 : 0);
    int a = red_a0 + ((red_modo & 2) ? m->y - red_y0 : 0);
    int lmax = tela_l() - j->x - 2 * BORDA, amax = tela_a() - j->y - BARRA_TITULO - BORDA;
    if (l > lmax) l = lmax;
    if (a > amax) a = amax;
    if (l < minimo_l(j)) l = minimo_l(j);
    if (a < minimo_a(j)) a = minimo_a(j);
    if (l == red_l && a == red_a) return;
    redesenhar_contorno();
    red_l = l; red_a = a;
    redesenhar_contorno();
}

/* ---- maximizar, minimizar */

static void alternar_maximizar(janela_t *j)
{
    if (!redimensionavel(j) || j->tela_cheia) return;
    redesenhar_janela(j);
    if (!j->maximizada) {
        j->rest_x = j->x; j->rest_y = j->y; j->rest_l = j->larg; j->rest_a = j->alt;
        j->maximizada = true;
        mon_t m = mon_da(j);   /* D290: la pantalla donde está */
        j->x = m.x; j->y = mon_topo(m);
        pedir_tamanho(j, mc_l(m), mc_a(m), JAN_ESTADO_MAXIMIZADA);
    } else {
        j->maximizada = false;
        j->x = j->rest_x; j->y = j->rest_y;
        pedir_tamanho(j, j->rest_l, j->rest_a, JAN_ESTADO_NORMAL);
    }
    redesenhar_janela(j);
    klog("[GUI] janela %d %s\n", j->id, j->maximizada ? "maximizada" : "restaurada");
}

static void minimizar(janela_t *j)
{
    if (dialogo(j) || j->minimizada) return;
    redesenhar_janela(j);
    j->minimizada = true;
    if (menu_janela() == j->id) menu_cancelar();
    if (arrastando >= 0 && &janelas[arrastando] == j) arrastando = -1;
    if (captura >= 0 && &janelas[captura] == j) captura = -1;
    if (focada()) redesenhar_janela(focada());
    barra_sujar();
    klog("[GUI] janela %d minimizada\n", j->id);
}

static void desminimizar(janela_t *j)
{
    j->minimizada = false;
    int k = indice_na_ordem(j);
    if (k >= 0) trazer_para_cima(k);
    redesenhar_janela(j);
    barra_sujar();
    klog("[GUI] janela %d restaurada da barra\n", j->id);
}

/* Clic en la barra de título: maximizar, minimizar y doble clic.
 * Devuelve true si el clic se usó (si no, empieza a arrastrar). */
static bool clique_titulo(janela_t *j, int rx, int ry)
{
    bool no_botao = ry >= 4 && ry < 20;
    if (no_botao && redimensionavel(j) && rx >= botao_max_x(j) && rx < botao_max_x(j) + 16) {
        alternar_maximizar(j);
        return true;
    }
    if (no_botao && !dialogo(j) && rx >= botao_min_x(j) && rx < botao_min_x(j) + 16) {
        minimizar(j);
        return true;
    }
    if (rx >= moldura_l(j) - BORDA - 20) return false;   /* la X */
    uint32_t agora = relogio_tiques();
    bool duplo = ult_clique_alvo == 100000 + j->id && agora - ult_clique_tique < DUPLO_CLIQUE;
    ult_clique_alvo = duplo ? -999 : 100000 + j->id;
    ult_clique_tique = agora;
    if (duplo && redimensionavel(j)) {
        alternar_maximizar(j);
        return true;
    }
    return false;
}

/* ---- pantalla completa */

/* Dónde aparece el contenido en pantalla completa: 1:1 si el programa ya está del
 * tamaño de la pantalla; si no, ampliado (vecino más cercano), manteniendo la
 * proporción, centrado, con franjas negras. */
static void geometria_tela_cheia(janela_t *j, int *ox, int *oy, int *dl, int *da)
{
    mon_t m = mon_da(j);   /* D290: la pantalla donde está la ventana */
    int L = m.l, A = m.a;
    if (j->larg == L && j->alt == A) { *dl = L; *da = A; }
    else if (L * j->alt <= A * j->larg) { *dl = L; *da = j->alt * L / j->larg; }
    else { *da = A; *dl = j->larg * A / j->alt; }
    *ox = m.x + (L - *dl) / 2; *oy = m.y + (A - *da) / 2;
}

static void entrar_tela_cheia(janela_t *j)
{
    if (j->tela_cheia) return;
    j->tc_x = j->x; j->tc_y = j->y; j->tc_l = j->larg; j->tc_a = j->alt;
    j->tela_cheia = true;
    arrastando = redimensionando = captura = -1;
    if (redimensionavel(j)) { mon_t m = mon_da(j); pedir_tamanho(j, m.l, m.a, JAN_ESTADO_TELA_CHEIA); }
    redesenhar_tudo();
    klog("[GUI] janela %d em tela cheia (conteudo %dx%d)\n", j->id, j->larg, j->alt);
}

static void sair_tela_cheia(janela_t *j)
{
    if (!j->tela_cheia) return;
    j->tela_cheia = false;
    j->x = j->tc_x; j->y = j->tc_y;
    if (redimensionavel(j) && (j->larg != j->tc_l || j->alt != j->tc_a))
        pedir_tamanho(j, j->tc_l, j->tc_a, j->maximizada ? JAN_ESTADO_MAXIMIZADA : JAN_ESTADO_NORMAL);
    redesenhar_tudo();
    klog("[GUI] janela %d saiu da tela cheia\n", j->id);
}

/* D245: cuadros de pantalla completa dibujados (para medir los fps y para la prueba: un
 * programa que no cambia no suma). Cada ~5 s se anota la media en el log. */
static uint32_t tc_quadros, tc_quadros_ini, tc_fps, tc_ini_tique;
uint32_t gui_tc_quadros(void) { return tc_quadros; }
uint32_t gui_tc_fps(void) { return tc_fps; }
static void tc_contar(void)
{
    uint32_t agora = relogio_tiques();
    tc_quadros++;
    if (!tc_ini_tique || agora - tc_ini_tique > 10 * TIQUES_POR_SEGUNDO) {   /* empieza (o tras una pausa larga) */
        tc_ini_tique = agora; tc_quadros_ini = tc_quadros;
        return;
    }
    if (agora - tc_ini_tique >= 5 * TIQUES_POR_SEGUNDO) {
        tc_fps = (tc_quadros - tc_quadros_ini) * TIQUES_POR_SEGUNDO / (agora - tc_ini_tique);
        klog("[GUI] pantalla completa: %u fps\n", tc_fps);
        tc_ini_tique = agora; tc_quadros_ini = tc_quadros;
    }
}

static bool desenhar_tela_cheia(void)
{
    janela_t *j = focada();
    if (!j || !j->tela_cheia) return false;
    tc_contar();
    int ox, oy, dl, da;
    geometria_tela_cheia(j, &ox, &oy, &dl, &da);
    int L = tela_l(), A = tela_a();
    sup_retangulo(&tela, 0, 0, L, oy, 0);
    sup_retangulo(&tela, 0, oy + da, L, A - oy - da, 0);
    sup_retangulo(&tela, 0, oy, ox, da, 0);
    sup_retangulo(&tela, ox + dl, oy, L - ox - dl, da, 0);
    if (dl == j->larg && da == j->alt) sup_copiar(&tela, ox, oy, j->px, j->larg, j->alt, j->larg);
    else sup_escalar(&tela, ox, oy, dl, da, j->px, j->larg, j->alt);
    avisos_desenhar(&tela);
    return true;
}

/* F11 enciende y apaga la pantalla completa de la ventana con foco (si ella lo acepta);
 * Esc sólo sale de ella (y no llega al programa). */
static bool tecla_do_gerenciador(janela_t *j, int t)
{
    if (t == TECLA_F11 && (j->estilo & (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK))) {
        if (j->tela_cheia) sair_tela_cheia(j); else entrar_tela_cheia(j);
        return true;
    }
    if (t == TECLA_ESC && j->tela_cheia && !(j->estilo & JAN_ESC_LIVRE)) {
        sair_tela_cheia(j);
        return true;
    }
    return false;
}

/* En pantalla completa no hay marco: todo evento del ratón va a la ventana,
 * en coordenadas del contenido. */
static void mouse_tela_cheia(janela_t *j, evento_mouse_t *m)
{
    int ox, oy, dl, da;
    geometria_tela_cheia(j, &ox, &oy, &dl, &da);
    int cx = (m->x - ox) * j->larg / dl, cy = (m->y - oy) * j->alt / da;
    bool dentro = m->x >= ox && m->y >= oy && m->x < ox + dl && m->y < oy + da;
    bool apertou = (m->mudaram & m->botoes) != 0, soltou = (m->mudaram & ~m->botoes) != 0;
    evento_t e = { .x = cx, .y = cy, .modificadores = teclado_modificadores() & 3 };
    if (apertou) {
        if (!dentro) return;
        uint32_t agora = relogio_tiques();
        int dx = cx - ult_clique_x, dy = cy - ult_clique_y;
        bool duplo = ult_clique_alvo == j->id && agora - ult_clique_tique < DUPLO_CLIQUE &&
                     dx > -5 && dx < 5 && dy > -5 && dy < 5;
        ult_clique_alvo = duplo ? -999 : j->id;
        ult_clique_tique = agora;
        ult_clique_x = cx; ult_clique_y = cy;
        e.tipo = EV_CLIQUE; e.botoes = m->mudaram & m->botoes; e.cliques = duplo ? 2 : 1;
    } else if (soltou) {
        e.tipo = EV_SOLTOU; e.botoes = m->mudaram;
    } else {
        if (!dentro && !m->botoes) return;
        e.tipo = EV_MOVER; e.botoes = m->botoes;
    }
    entregar(j, &e);
}

/* Rueda del ratón (D048): EV_RODA para la ventana que está debajo del cursor,
 * incluso sin foco y sin traerla hacia arriba (como en Windows 10 en adelante y
 * en la mayoría de los entornos Linux): se puede desplazar un documento de fondo
 * mientras se escribe en otro. En pantalla completa va a la ventana de la pantalla
 * completa. Sobre el marco o la barra de título también vale (x, y pueden ser
 * negativos). Mientras se arrastra o redimensiona una ventana, se ignora. */
static void roda_para(evento_mouse_t *m, int tipo, int n);
static void tratar_roda(evento_mouse_t *m) { roda_para(m, EV_RODA, m->dz); }

/* D407: la rueda horizontal (panel táctil) va igual, como EV_RODA_H. */
static void tratar_roda_h(evento_mouse_t *m) { roda_para(m, EV_RODA_H, m->dw); }

static void roda_para(evento_mouse_t *m, int tipo, int n)
{
    janela_t *f = focada();
    evento_t e = { .tipo = (unsigned)tipo, .cliques = (unsigned)n, .botoes = m->botoes,
                   .modificadores = teclado_modificadores() & 3 };
    janela_t *j;
    if (f && f->tela_cheia) {
        int ox, oy, dl, da;
        geometria_tela_cheia(f, &ox, &oy, &dl, &da);
        e.x = (m->x - ox) * f->larg / dl;
        e.y = (m->y - oy) * f->alt / da;
        j = f;
    } else {
        if (arrastando >= 0 || redimensionando >= 0) return;
        int k = janela_em(m->x, m->y);
        if (k < 0) return;
        j = &janelas[ordem[k]];
        e.x = m->x - j->x - bd(j);
        e.y = m->y - j->y - td(j);
    }
    entregar(j, &e);
    klog("[GUI] roda%s %d para janela %d (%s) em (%d,%d)\n", tipo == EV_RODA_H ? " horizontal" : "", n, j->id, j->titulo, e.x, e.y);
}

/* Llamado al comienzo de tratar_mouse: true = el evento ya fue tratado aquí. */
static bool tratar_mouse_extras(evento_mouse_t *m)
{
    janela_t *f = focada();
    if (f && f->tela_cheia) { mouse_tela_cheia(f, m); return true; }
    if (redimensionando >= 0) { continuar_redimensionar(m); return true; }
    return false;
}

/* ---- llamadas al sistema */

/* SIS_MOVER_JANELA (id, x, y), D102: pone el marco de la ventana en una posición de la
 * pantalla (el SaguiMark la lleva a una esquina para no tapar el infopc). x o y
 * negativos cuentan desde el borde derecho/de abajo (-1 = pegada). */
int32_t sis_mover_janela(uint32_t id, uint32_t px, uint32_t py, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (!ativa) return ERRO_ACESSO;
    janela_t *j = janela_do_processo((int)id);
    if (!j) return ERRO_INVALIDO;
    if (j->tela_cheia || j->maximizada) return ERRO_INVALIDO;
    int x = (int)px, y = (int)py;
    int pl, pa;
    telas_principal_rect(&pl, &pa);   /* D290: las posiciones negativas cuentan desde el borde de la pantalla principal */
    if (x < 0) x = pl - moldura_l(j) + x + 1;
    if (y < 0) y = pa - moldura_a(j) + y + 1;
    if (x + moldura_l(j) > (int)video_largura()) x = (int)video_largura() - moldura_l(j);
    if (y + moldura_a(j) > (int)video_altura()) y = (int)video_altura() - moldura_a(j);
    if (x < 0) x = 0;
    if (y < BARRA_SUPERIOR) y = BARRA_SUPERIOR;
    redesenhar_janela(j);   /* el lugar viejo */
    j->x = x; j->y = y;
    redesenhar_janela(j);
    gui_mostrar();
    if (j->widget) klog("[GUI] widget %d colocado em (%d,%d) %dx%d\n", j->id, j->x, j->y, j->larg, j->alt);
    return 0;
}

/* SIS_ESTILO_JANELA (id, estilo, min_l | min_a << 16). JAN_TELA_CHEIA en el
 * estilo pone la ventana en pantalla completa ahora; sin él, la saca. */
int32_t sis_estilo_janela(uint32_t id, uint32_t estilo, uint32_t minimo, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    janela_t *j = janela_do_processo((int)id);
    if (!j || j->widget) return ERRO_INVALIDO;   /* D365: el tamaño y el estilo de un widget son fijos */
    if (estilo & JAN_SO_CAPTURA) {   /* D242: solo la captura del ratón */
        if (estilo & ~(uint32_t)(JAN_SO_CAPTURA | JAN_CAPTURA)) return ERRO_INVALIDO;
        if (!(estilo & JAN_CAPTURA)) {
            if (cap_id == j->id) soltar_captura("pedido do programa");
            return 0;
        }
        if (cap_id == j->id) return 0;
        if (j != focada() || j->minimizada || menu_aberto() || barra_popup_visivel() || atalhos_visivel()) return ERRO_ACESSO;
        if (cap_id >= 0) soltar_captura("outra janela");
        int x, y, l, a;
        if (j->tela_cheia) geometria_tela_cheia(j, &x, &y, &l, &a);
        else { x = j->x + BORDA; y = j->y + BARRA_TITULO; l = j->larg; a = j->alt; }
        cap_id = j->id;
        mouse_captura(true, x + l / 2, y + a / 2);
        klog("[GUI] janela %d: mouse capturado (Ctrl+Alt solta)\n", j->id);
        return 0;
    }
    if (estilo & JAN_SO_CURSOR) {   /* D230: solo el puntero; lo demás no se toca */
        if (estilo & ~(uint32_t)(JAN_SO_CURSOR | JAN_SEM_CURSOR)) return ERRO_INVALIDO;
        j->estilo = (j->estilo & ~(uint32_t)JAN_SEM_CURSOR) | (estilo & JAN_SEM_CURSOR);
        klog("[GUI] janela %d: cursor do sistema %s\n", j->id, (estilo & JAN_SEM_CURSOR) ? "oculto" : "visivel");
        atualizar_zona_cursor();
        return 0;
    }
    if (estilo & ~(uint32_t)(JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_TELA_CHEIA | JAN_DIALOGO | JAN_ESC_LIVRE |
                             JAN_SEM_CURSOR)) return ERRO_INVALIDO;
    int ml = (int)(minimo & 0xFFFF), ma = (int)(minimo >> 16);
    mon_t mj = mon_da(j);
    j->min_l = ml > mc_l(mj) ? mc_l(mj) : ml;
    j->min_a = ma > mc_a(mj) ? mc_a(mj) : ma;
    j->estilo = estilo & (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_DIALOGO | JAN_ESC_LIVRE | JAN_SEM_CURSOR);
    if (dialogo(j) && j->minimizada) desminimizar(j);
    if (estilo & JAN_TELA_CHEIA) {
        j->estilo |= JAN_TELA_CHEIA_OK;
        if (j->minimizada) j->minimizada = false;
        int k = indice_na_ordem(j);
        if (k >= 0) trazer_para_cima(k);
        janela_t *f = focada();
        if (f != j && f) sair_tela_cheia(f);
        entrar_tela_cheia(j);
    } else if (j->tela_cheia) {
        sair_tela_cheia(j);
    }
    if (!redimensionavel(j)) j->maximizada = false;
    redesenhar_janela(j);
    gui_mostrar();
    klog("[GUI] janela %d: estilo %u, minimo %dx%d\n", j->id, estilo, j->min_l, j->min_a);
    return 0;
}

/* SIS_REDIMENSIONAR_JANELA (id, ancho | alto << 16, &píxeles). Cambia las
 * páginas de los píxeles por otras del tamaño nuevo, en la misma dirección del
 * programa, copiando la esquina de arriba a la izquierda del dibujo antiguo. */
int32_t sis_redimensionar_janela(uint32_t id, uint32_t dims, uint32_t ppixels, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    janela_t *j = janela_do_processo((int)id);
    if (!j || j->widget) return ERRO_INVALIDO;
    int l = (int)(dims & 0xFFFF), a = (int)(dims >> 16);
    if (l < 40 || a < 20 || l > tela_l() || a > tela_a() || (uint32_t)(l * a) * 4 > ESPACO_JANELA)
        return ERRO_INVALIDO;
    if (ppixels && !usuario_ok((void *)ppixels, 4, true)) return ERRO_ENDERECO;
    if (l != j->larg || a != j->alt) {
        if (!espaco_pode_tirar()) return SIS_REPETIR;   /* D351: se quitan las páginas de los píxeles */
        uint32_t paginas = ((uint32_t)(l * a) * 4 + PAGINA - 1) / PAGINA;
        uint32_t fis = memfisica_alocar_seguidas(paginas);
        if (!fis) return ERRO_SEM_MEMORIA;
        uint32_t *novo = (uint32_t *)fis;
        uint32_t resto = j->px[j->larg * j->alt - 1];   /* color de la esquina: suele ser el fondo */
        for (int y = 0; y < a; y++)
            for (int x = 0; x < l; x++)
                novo[y * l + x] = (x < j->larg && y < j->alt) ? j->px[y * j->larg + x] : resto;
        redesenhar_janela(j);
        for (uint32_t k = 0; k < j->paginas; k++)
            espaco_desmapear(processo_atual()->espaco, j->virt + k * PAGINA);
        espaco_sincronizar(processo_atual()->espaco);   /* D167: otro hilo en otra CPU las tiene en la TLB */
        memfisica_liberar_seguidas((uint32_t)j->px, j->paginas);
        for (uint32_t k = 0; k < paginas; k++)
            espaco_mapear(processo_atual()->espaco, j->virt + k * PAGINA, fis + k * PAGINA, PG_USUARIO | PG_ESCRITA | PG_EMPRESTADA | PG_NX);
        j->px = novo;
        j->paginas = paginas;
        j->larg = l; j->alt = a;
        if (j->tela_cheia) redesenhar_tudo();
        else redesenhar_janela(j);
        klog("[GUI] janela %d redimensionada para %dx%d\n", j->id, l, a);
    }
    if (ppixels) *(uint32_t *)ppixels = j->virt;
    return 0;
}

/* ------------------------------------------------------------ administrador de tareas (D043) */

int janelas_contar(int pid)
{
    int n = 0;
    for (int i = 0; i < MAX_JANELAS; i++)
        if (janelas[i].usada && janelas[i].pid == pid) n++;
    return n;
}

/* Lista las ventanas de abajo hacia arriba (la última es la que tiene el foco). */
int janelas_listar(void *destino, int n)
{
    info_janela_t *v = destino;
    int k = 0;
    for (; k < qtd && k < n; k++) {
        janela_t *j = &janelas[ordem[k]];
        v[k].id = j->id;
        v[k].pid = j->pid;
        v[k].x = j->x;
        v[k].y = j->y;
        v[k].largura = j->larg;
        v[k].altura = j->alt;
        v[k].foco = k == qtd - 1 && !j->widget;
        txt_copia_n(v[k].titulo, j->titulo, sizeof v[k].titulo);
    }
    return k;
}

/* SIS_PEDIR_FECHAR (id): lo mismo que hacer clic en la X de la ventana, venido de otro
 * programa. El dueño recibe EV_FECHAR y decide (puede preguntar si guarda). */
int32_t sis_pedir_fechar(uint32_t id, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    for (int i = 0; i < MAX_JANELAS; i++) {
        janela_t *j = &janelas[i];
        if (!j->usada || j->id != (int)id) continue;
        evento_t ev = { .tipo = EV_FECHAR };
        entregar(j, &ev);
        klog("[GUI] fechar janela %d (%s) a pedido do processo %d\n", j->id, j->titulo, processo_recursos()->pid);
        return 0;
    }
    return ERRO_INVALIDO;
}

/* ------------------------------------------------------------ menú contextual (D051) */

static janela_t *janela_por_id(int id)
{
    for (int i = 0; i < MAX_JANELAS; i++)
        if (janelas[i].usada && janelas[i].id == id) return &janelas[i];
    return NULL;
}

/* Líneas del menú de la barra de título (el orden es el de los índices de abajo). */
enum { MT_RESTAURAR, MT_MINIMIZAR, MT_MAXIMIZAR, MT_TELA_CHEIA, MT_SEPARADOR, MT_FECHAR };

/* La respuesta busca la ventana por el id, no guarda un puntero: entre abrir
 * y elegir, la ventana puede haberse cerrado (y el lugar, reaprovechado). */
static void resposta_titulo(int indice, int id, int dado)
{
    (void)dado;
    janela_t *j = janela_por_id(id);
    if (!j || indice < 0) return;
    /* Desde la barra de tareas el menú también vale para una minimizada (D065):
     * cualquier cosa menos minimizar la trae de vuelta primero. */
    if (j->minimizada && indice != MT_MINIMIZAR && indice != MT_FECHAR) {
        desminimizar(j);
        if (indice == MT_RESTAURAR) return;
    }
    switch (indice) {
    case MT_RESTAURAR: if (j->maximizada) alternar_maximizar(j); break;
    case MT_MINIMIZAR: minimizar(j); break;
    case MT_MAXIMIZAR: if (!j->maximizada) alternar_maximizar(j); break;
    case MT_TELA_CHEIA: {   /* sólo la ventana de arriba puede quedar en pantalla completa */
        int k = indice_na_ordem(j);
        if (k >= 0) trazer_para_cima(k);
        entrar_tela_cheia(j);
        break;
    }
    case MT_FECHAR: {   /* lo mismo que la X: quien decide cerrar es el programa */
        evento_t e = { .tipo = EV_FECHAR };
        entregar(j, &e);
        klog("[GUI] fechar janela %d (%s) pelo menu\n", j->id, j->titulo);
        break;
    }
    }
}

static void menu_titulo(janela_t *j, int x, int y)
{
    char itens[160];
    bool red = redimensionavel(j);
    snformatar(itens, sizeof itens, "%s%s\n%s%s\n%s%s\n%s%s\tF11\n-\n%s",
               j->maximizada || j->minimizada ? "" : "~", _("Restaurar"), j->minimizada || dialogo(j) ? "~" : "", _("Minimizar"),
               red && !j->maximizada ? "" : "~", _("Maximizar"),
               (j->estilo & (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK)) ? "" : "~", _("Pantalla completa"), _("Cerrar"));
    arrastando = redimensionando = captura = -1;
    menu_abrir(x, y, itens, resposta_titulo, j->id, 0);
}

/* Menú pedido por un programa: la respuesta se convierte en EV_MENU en la cola de la ventana. */
static void resposta_programa(int indice, int id, int codigo)
{
    janela_t *j = janela_por_id(id);
    if (!j) return;
    evento_t e = { .tipo = EV_MENU, .x = indice, .tecla = (unsigned)codigo };
    entregar(j, &e);
    klog("[GUI] EV_MENU %d (codigo %d) para janela %d\n", indice, codigo, id);
}

/* SIS_MENU_CONTEXTUAL (id, x, y, itens, codigo). Sólo la ventana con el foco
 * puede abrir un menú: un programa en segundo plano no puede tomar el ratón
 * y el teclado de quien está usando otra ventana. Vuelve enseguida; la elección
 * llega después como EV_MENU (x = índice o -1, tecla = codigo). */
int32_t sis_menu_contextual(uint32_t id, uint32_t px, uint32_t py, uint32_t pitens, uint32_t codigo)
{
    janela_t *j = janela_do_processo((int)id);
    if (!j) return ERRO_INVALIDO;
    if (j != focada()) return ERRO_ACESSO;
    static char itens[MENU_MAX_ITENS * (MENU_MAX_TEXTO + 2) + 1];
    int r = usuario_texto((const char *)pitens, itens, sizeof itens);
    if (r < 0) return r;
    int x = (int)px, y = (int)py;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > j->larg) x = j->larg;
    if (y > j->alt) y = j->alt;
    if (j->tela_cheia) {   /* sin marco y quizá ampliado: convierte como mouse_tela_cheia, al revés */
        int ox, oy, dl, da;
        geometria_tela_cheia(j, &ox, &oy, &dl, &da);
        x = ox + x * dl / j->larg;
        y = oy + y * da / j->alt;
    } else {
        x += j->x + bd(j);
        y += j->y + td(j);
    }
    arrastando = redimensionando = captura = -1;
    if (!menu_abrir(x, y, itens, resposta_programa, j->id, (int)codigo)) return ERRO_INVALIDO;
    gui_mostrar();
    return 0;
}

/* ------------------------------------------------------------ cambio de modo de video (D059)
 *
 * Llamado con las interrupciones desactivadas, después de que el video, el fondo y el
 * ratón ya están en el tamaño nuevo. Las ventanas maximizadas y en pantalla completa
 * reciben EV_REDIMENSIONAR con el tamaño nuevo (el programa responde como
 * siempre); las demás sólo son empujadas hacia dentro de la pantalla y, si son
 * más grandes que ella y aceptan otro tamaño, reciben el pedido del mayor que
 * quepa. Todas reciben EV_TELA con la resolución nueva. */
void gui_tela_mudou(void)
{
    if (!ativa) return;
    sup_iniciar(&tela, video_buffer(), tela_l(), tela_a());
    menu_cancelar();   /* el menú abierto se colocó para la pantalla antigua */
    arrastando = redimensionando = captura = -1;
    for (int i = 0; i < MAX_JANELAS; i++) {
        janela_t *j = &janelas[i];
        if (!j->usada) continue;
        mon_t m = mon_da(j);   /* D290: la pantalla donde está la ventana */
        int ml = mc_l(m), ma = mc_a(m);
        if (j->min_l > ml) j->min_l = ml;
        if (j->min_a > ma) j->min_a = ma;
        if (j->rest_l > ml) j->rest_l = ml;
        if (j->rest_a > ma) j->rest_a = ma;
        if (j->tela_cheia && redimensionavel(j)) {
            pedir_tamanho(j, m.l, m.a, JAN_ESTADO_TELA_CHEIA);
        } else if (j->maximizada) {
            j->x = m.x; j->y = mon_topo(m);
            pedir_tamanho(j, ml, ma, JAN_ESTADO_MAXIMIZADA);
        } else if (redimensionavel(j) && !j->tela_cheia && (j->larg > ml || j->alt > ma)) {
            pedir_tamanho(j, j->larg < ml ? j->larg : ml, j->alt < ma ? j->alt : ma, JAN_ESTADO_NORMAL);
        }
        /* Posición (también la guardada para después de la pantalla completa). */
        int *px = j->tela_cheia ? &j->tc_x : &j->x, *py = j->tela_cheia ? &j->tc_y : &j->y;
        if (!j->maximizada) {
            int l = (j->larg < ml ? j->larg : ml) + 2 * bd(j);
            int a = (j->alt < ma ? j->alt : ma) + td(j) + bd(j);
            if (*px + l > tela_l()) *px = tela_l() - l;
            if (*py + a > tela_a() - barra_fundo()) *py = tela_a() - barra_fundo() - a;
            if (*px < 0) *px = 0;
            if (*py < BARRA_SUPERIOR) *py = BARRA_SUPERIOR;
        }
        if (j->rest_x + j->rest_l + 2 * BORDA > tela_l()) j->rest_x = tela_l() - j->rest_l - 2 * BORDA;
        if (j->rest_y + j->rest_a + BARRA_TITULO + BORDA > tela_a()) j->rest_y = tela_a() - j->rest_a - BARRA_TITULO - BORDA;
        if (j->rest_x < 0) j->rest_x = 0;
        if (j->rest_y < BARRA_SUPERIOR) j->rest_y = BARRA_SUPERIOR;
        evento_t e = { .tipo = EV_TELA, .x = m.l, .y = m.a };   /* D290: el programa ve la pantalla donde está */
        entregar(j, &e);
        klog("[GUI] janela %d em (%d,%d) %dx%d depois da troca para %dx%d\n", j->id, j->x, j->y, j->larg, j->alt,
             tela_l(), tela_a());
    }
    redesenhar_tudo();
    gui_mostrar();
}

/* D361: la barra cambió de sitio, de altura o se oculta sola: las ventanas maximizadas toman el área nueva y las
 * demás no dejan su título debajo de la barra. */
void janelas_area_mudou(void)
{
    if (!ativa) return;
    menu_cancelar();
    arrastando = redimensionando = captura = -1;
    int lim = tela_a() - barra_fundo();
    for (int i = 0; i < MAX_JANELAS; i++) {
        janela_t *j = &janelas[i];
        if (!j->usada || j->tela_cheia) continue;
        mon_t m = mon_da(j);
        if (j->maximizada) {
            j->x = m.x;
            j->y = mon_topo(m);
            pedir_tamanho(j, mc_l(m), mc_a(m), JAN_ESTADO_MAXIMIZADA);
            continue;
        }
        if (j->y < BARRA_SUPERIOR) j->y = BARRA_SUPERIOR;
        if (j->y + BARRA_TITULO > lim) j->y = lim - BARRA_TITULO;
        if (j->y < BARRA_SUPERIOR) j->y = BARRA_SUPERIOR;
        if (j->rest_y + BARRA_TITULO > lim) j->rest_y = lim - BARRA_TITULO;
        if (j->rest_y < BARRA_SUPERIOR) j->rest_y = BARRA_SUPERIOR;
    }
    redesenhar_tudo();
    gui_mostrar();
}

/* ------------------------------------------------------------ barra de tareas (D065)
 *
 * La barra (barra.c) no toca las ventanas directamente: pide la lista y manda las
 * acciones por aquí, por el id, como el menú de la barra de título. */

/* Las ventanas que tienen botón en la barra, en orden de creación (el id crece),
 * para que los botones no cambien de lugar cuando cambia el foco. Los diálogos
 * quedan fuera. */
int janelas_da_barra(janela_barra_t *v, int max)
{
    janela_t *f = focada();
    int n = 0, ultimo = 0;
    while (n < max) {
        janela_t *prox = NULL;
        for (int i = 0; i < MAX_JANELAS; i++) {
            janela_t *j = &janelas[i];
            if (j->usada && !dialogo(j) && !j->widget && j->id > ultimo && (!prox || j->id < prox->id)) prox = j;
        }
        if (!prox) break;
        ultimo = prox->id;
        janela_barra_t *b = &v[n++];
        b->id = prox->id;
        b->pid = prox->pid;
        b->minimizada = prox->minimizada;
        b->foco = prox == f;
        txt_copia_n(b->titulo, prox->titulo, sizeof b->titulo);
        processo_t *p = processo_por_pid(prox->pid);
        txt_copia_n(b->processo, p ? p->nome : "", sizeof b->processo);
    }
    return n;
}

void gui_janela_ativar(int id)
{
    janela_t *j = janela_por_id(id);
    if (!j) return;
    if (j->minimizada) { desminimizar(j); return; }
    int k = indice_na_ordem(j);
    if (k >= 0) trazer_para_cima(k);
    klog("[GUI] janela %d ativada pela barra\n", j->id);
}

void gui_janela_minimizar(int id)
{
    janela_t *j = janela_por_id(id);
    if (j) minimizar(j);
}

void gui_janela_menu(int id, int x, int y)
{
    janela_t *j = janela_por_id(id);
    if (j) menu_titulo(j, x, y);
}

void gui_sair_tela_cheia(void)
{
    janela_t *f = focada();
    if (f && f->tela_cheia) sair_tela_cheia(f);
}

/* ------------------------------------------------------------ atajos de teclado (D119)
 *
 * atalhos.c decide qué hacer con Alt+Tab, Alt+F4 y Windows+D; aquí sólo se
 * consultan y tocan las ventanas, por el id. */

/* Las ventanas en orden de uso reciente: primero las visibles de arriba hacia
 * abajo (la del foco es la primera) y después las minimizadas, también de
 * arriba hacia abajo. Incluye los diálogos (que no tienen botón en la barra). */
int janelas_por_uso(janela_barra_t *v, int max)
{
    janela_t *f = focada();
    int n = 0;
    for (int passo = 0; passo < 2; passo++)
        for (int k = qtd - 1; k >= 0 && n < max; k--) {
            janela_t *j = &janelas[ordem[k]];
            if (j->minimizada != (passo == 1) || j->widget) continue;   /* D365: los widgets no entran en Alt+Tab */
            janela_barra_t *b = &v[n++];
            b->id = j->id;
            b->pid = j->pid;
            b->minimizada = j->minimizada;
            b->foco = j == f;
            txt_copia_n(b->titulo, j->titulo, sizeof b->titulo);
            processo_t *p = processo_por_pid(j->pid);
            txt_copia_n(b->processo, p ? p->nome : "", sizeof b->processo);
        }
    return n;
}

int gui_janela_focada(void)
{
    janela_t *f = focada();
    return f ? f->id : 0;
}

/* Alt+F4: el mismo EV_FECHAR que la X de la barra de título. */
void gui_janela_pedir_fechar(int id)
{
    janela_t *j = janela_por_id(id);
    if (!j) return;
    evento_t e = { .tipo = EV_FECHAR };
    entregar(j, &e);
    klog("[GUI] Alt+F4: fechar janela %d (%s)\n", j->id, j->titulo);
}

/* SIS_CAPTURAR_TELA (D125): copia la imagen ya compuesta (sin cursor) de la pantalla, de la
 * ventana activa con su marco (id = -1) o de una ventana por su id. prect (4 enteros) entra
 * con x, y, l, a de un recorte en coordenadas de pantalla (l = 0: todo) y sale con lo copiado. */
int32_t sis_capturar_tela(uint32_t id, uint32_t prect, uint32_t pdest, uint32_t maxpx, uint32_t e)
{
    (void)e;
    if (!ativa) return ERRO_ACESSO;
    int32_t r[4];
    if (!usuario_ok((void *)prect, sizeof r, true)) return ERRO_ENDERECO;
    memcpy(r, (void *)prect, sizeof r);
    int L = tela_l(), A = tela_a();
    int x = 0, y = 0, l = L, a = A;
    if (id != 0) {
        janela_t *j = NULL;
        if (id == 0xFFFFFFFFu) j = focada();
        else j = janela_por_id((int)id);
        if (!j || j->minimizada) return ERRO_NAO_EXISTE;
        x = j->x; y = j->y; l = moldura_l(j); a = moldura_a(j);
    } else if (r[2] > 0) {
        x = r[0]; y = r[1]; l = r[2]; a = r[3];
    }
    if (x < 0) { l += x; x = 0; }
    if (y < 0) { a += y; y = 0; }
    if (x + l > L) l = L - x;
    if (y + a > A) a = A - y;
    if (l <= 0 || a <= 0) return ERRO_INVALIDO;
    if ((uint32_t)l * (uint32_t)a > maxpx) return ERRO_INVALIDO;
    if (!usuario_ok((void *)pdest, (uint32_t)l * (uint32_t)a * 4, true)) return ERRO_ENDERECO;
    gui_mostrar();   /* que el búfer de atrás esté al día */
    uint32_t *src = video_buffer(), *dst = (uint32_t *)pdest;
    for (int yy = 0; yy < a; yy++)
        memcpy(dst + (uint32_t)yy * (uint32_t)l, src + (uint32_t)(y + yy) * (uint32_t)L + (uint32_t)x, (size_t)l * 4);
    r[0] = x; r[1] = y; r[2] = l; r[3] = a;
    memcpy((void *)prect, r, sizeof r);
    return l * a;
}

bool janela_quer_imprpant(void)
{
    janela_t *j = focada();
    return j && (j->estilo & JAN_IMPRPANT);
}

int32_t sis_avisar(uint32_t ptexto, uint32_t pcaminho, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    char t[512], cam[160];
    int r = usuario_texto((const char *)ptexto, t, sizeof t);
    if (r < 0 && r != ERRO_INVALIDO) return r;   /* demasiado largo: se muestra recortado a 511 bytes */
    cam[0] = 0;
    if (pcaminho) { r = usuario_texto((const char *)pcaminho, cam, sizeof cam); if (r < 0) return r; }
    if (ativa) gui_aviso_ex(t, cam, false, 0);
    return 0;
}

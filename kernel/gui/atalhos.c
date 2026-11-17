// SPDX-License-Identifier: GPL-2.0-or-later
/* atalhos.c - atajos de teclado del escritorio (v6.0 beta, D119).
 *
 * El gestor de ventanas (janelas.c) pasa aquí cada tecla, con los
 * modificadores de cuando se pulsó, antes que a nadie. Si es un atajo, se
 * queda con ella y el programa no la ve:
 *   - Alt+F4: el mismo EV_FECHAR que la X (el programa puede preguntar si guarda);
 *   - Alt+Tab y Alt+Mayús+Tab: panel con las ventanas por orden de uso; al
 *     soltar Alt se activa la elegida;
 *   - tecla Windows sola (al soltarla): abre o cierra el menú Inicio;
 *   - Windows+D: minimiza todo; repetirlo lo restaura;
 *   - Ctrl+Alt+Supr: Administrador de tareas.
 * El AltGr no es Alt: no dispara Alt+Tab ni Alt+F4, y con Ctrl sólo cuenta
 * para Ctrl+Alt+Supr (hay teclados y anfitriones que mandan Ctrl+AltGr).
 *
 * Soltar una tecla no llega como tecla: en cada tic, atalhos_tique() mira
 * los modificadores actuales para saber si Alt o Windows ya se soltaron. */
#include "gui.h"
#include "aparencia.h"
#include "console.h"
#include "processo.h"
#include "teclado.h"
#include "texto.h"
#include "video.h"
#include "telas.h"

#define MAX_LISTA  32
#define CELDA      64      /* lado de cada casilla del panel de Alt+Tab */
#define MARGEN     14
#define ALTO_TITULO 28     /* franja de abajo con el título de la elegida */

/* ---- Alt+Tab */
static bool alternando;                 /* panel abierto: Alt sigue apretada */
static janela_barra_t lista[MAX_LISTA];
static int qtd_lista, elegida;
static int px, py, pl, pa, colunas;     /* rectángulo del panel y casillas por fila */

/* ---- tecla Windows */
static bool super_pendente;   /* se pulsó y todavía no se soltó */
static bool super_usada;      /* mientras estaba apretada se pulsó otra tecla */

/* ---- Windows+D: las ventanas que se minimizaron, de arriba hacia abajo */
static int escondidas[MAX_LISTA], qtd_escondidas;

static void sujar_painel(void) { if (pl > 0) gui_redesenhar(px - 1, py - 1, pl + 2, pa + 2); }

static void calcular_painel(void)
{
    int L, A;
    telas_principal_rect(&L, &A);   /* D290: el panel va en la pantalla principal */
    int max_col = (L - 2 * MARGEN - 32) / CELDA;
    if (max_col < 1) max_col = 1;
    colunas = qtd_lista < max_col ? qtd_lista : max_col;
    int filas = (qtd_lista + colunas - 1) / colunas;
    pl = colunas * CELDA + 2 * MARGEN;
    if (pl < 260 && L >= 300) pl = 260;   /* que quepa un título razonable aunque haya una sola ventana */
    pa = MARGEN + filas * CELDA + ALTO_TITULO + 6;
    px = (L - pl) / 2;
    py = (A - pa) / 2;
    if (py < BARRA_SUPERIOR + 4) py = BARRA_SUPERIOR + 4;
}

static void registrar_eleccion(void)
{
    klog("[GUI] alt+tab: %d janelas, escolhida %d (id %d, %s)\n", qtd_lista, elegida,
         lista[elegida].id, lista[elegida].titulo);
}

static void empezar_alternar(bool atras)
{
    qtd_lista = janelas_por_uso(lista, MAX_LISTA);
    if (qtd_lista == 0) return;
    if (menu_aberto()) menu_cancelar();
    alternando = true;
    /* La primera de la lista es la del foco (si la hay): un toque rápido de
     * Alt+Tab elige la segunda, es decir, alterna entre las dos últimas. */
    bool hay_foco = lista[0].foco;
    if (atras) elegida = qtd_lista - 1;
    else elegida = (hay_foco && qtd_lista > 1) ? 1 : 0;
    calcular_painel();
    sujar_painel();
    klog("[GUI] alt+tab: painel %dx%d em (%d,%d)\n", pl, pa, px, py);
    registrar_eleccion();
}

static void avanzar(bool atras)
{
    elegida = (elegida + (atras ? qtd_lista - 1 : 1)) % qtd_lista;
    sujar_painel();
    registrar_eleccion();
}

static void terminar_alternar(bool activar)
{
    if (!alternando) return;
    alternando = false;
    sujar_painel();
    pl = 0;
    if (!activar) { klog("[GUI] alt+tab: cancelado\n"); return; }
    int id = lista[elegida].id;
    /* Si la del foco estaba en pantalla completa, sale de ella: la otra
     * ventana tiene que verse con la barra (como el menú Inicio, D065). */
    if (gui_janela_focada() != id) gui_sair_tela_cheia();
    gui_janela_ativar(id);   /* trae arriba y restaura si estaba minimizada */
    gui_redesenhar(0, 0, (int)video_largura(), (int)video_altura());
    klog("[GUI] alt+tab: ativada janela %d\n", id);
}

/* ---- Windows+D */

static void mostrar_escritorio(void)
{
    janela_barra_t v[MAX_LISTA];
    int n = janelas_por_uso(v, MAX_LISTA);
    /* Segunda vez: si las que se escondieron siguen minimizadas, vuelven en el
     * mismo orden (de abajo hacia arriba, así la de arriba queda arriba). Si el
     * usuario ya restauró alguna, se esconde todo otra vez. */
    if (qtd_escondidas > 0) {
        bool todas = true, alguna = false;
        for (int e = 0; e < qtd_escondidas; e++)
            for (int i = 0; i < n; i++)
                if (v[i].id == escondidas[e]) { alguna = true; if (!v[i].minimizada) todas = false; }
        if (todas && alguna) {
            for (int e = qtd_escondidas - 1; e >= 0; e--) gui_janela_ativar(escondidas[e]);
            klog("[GUI] Windows+D: %d janelas restauradas\n", qtd_escondidas);
            qtd_escondidas = 0;
            return;
        }
    }
    qtd_escondidas = 0;
    gui_sair_tela_cheia();
    for (int i = 0; i < n; i++)
        if (!v[i].minimizada) {
            gui_janela_minimizar(v[i].id);   /* los diálogos no se minimizan: quedan fuera */
            escondidas[qtd_escondidas++] = v[i].id;
        }
    /* Sólo cuentan las que de verdad quedaron minimizadas. */
    n = janelas_por_uso(v, MAX_LISTA);
    int k = 0;
    for (int e = 0; e < qtd_escondidas; e++)
        for (int i = 0; i < n; i++)
            if (v[i].id == escondidas[e] && v[i].minimizada) escondidas[k++] = escondidas[e];
    qtd_escondidas = k;
    klog("[GUI] Windows+D: %d janelas minimizadas\n", qtd_escondidas);
}

/* ---- capturas de pantalla (D125): un programa aparte, /bin/captura, hace el trabajo */
static void lanzar_captura(const char *a1)
{
    char *argv[] = { "captura", (char *)a1, NULL };
    int pid = processo_executar("/bin/captura", a1 ? 2 : 1, argv, NULL);
    klog("[GUI] captura %s (pid %d)\n", a1 ? a1 : "pantalla", pid);
}

static void lanzar_recortes(void)
{
    char *argv[] = { "recortes", "--recorte", NULL };
    int pid = processo_executar("/bin/recortes", 2, argv, NULL);
    klog("[GUI] recortes (pid %d)\n", pid);
}

/* ---- teclas */

bool atalhos_tecla(int t, uint32_t mods)
{
    bool alt = (mods & MOD_ALT) != 0;
    bool shift = (mods & MOD_SHIFT) != 0;

    /* Con el panel de Alt+Tab abierto, el teclado es suyo. */
    if (alternando) {
        if (t == '\t') avanzar(shift);
        else if (t == TECLA_ESC) terminar_alternar(false);
        else if (t == TECLA_DIREITA) avanzar(false);
        else if (t == TECLA_ESQUERDA) avanzar(true);
        return true;
    }

    if (t == TECLA_SUPER) {
        if (!super_pendente) { super_pendente = true; super_usada = false; }   /* la repetición no cuenta */
        return true;
    }
    if (mods & MOD_SUPER) {
        /* Windows+otra tecla: ya no es la tecla sola. Windows+D muestra el
         * escritorio; las demás combinaciones quedan reservadas (no llegan
         * al programa). */
        super_usada = true;
        if (shift && (t == 's' || t == 'S' || t == 0x13)) { lanzar_recortes(); return true; }
        if (t == 'd' || t == 'D' || t == 4 /* Ctrl+D */) mostrar_escritorio();
        if (t == 'l' || t == 'L' || t == 0x0C /* Ctrl+L */) sessao_bloquear();   /* D371: bloquear la pantalla */
        return true;
    }

    if (t == TECLA_IMPRPANT) {
        if (janela_quer_imprpant()) return false;   /* un programa que la pide la recibe */
        lanzar_captura(alt ? "-v" : NULL);
        return true;
    }
    if (t == '\t' && alt) { empezar_alternar(shift); return true; }

    if (t == TECLA_F4 && alt) {
        if (menu_aberto()) { menu_cancelar(); return true; }   /* primero cierra el menú, como Esc */
        int id = gui_janela_focada();
        if (id) gui_janela_pedir_fechar(id);
        else klog("[GUI] Alt+F4 sem janela ativa\n");
        return true;
    }

    /* Ctrl+Alt+Supr. Con AltGr también (Ctrl+AltGr es como algunos teclados
     * mandan AltGr), pero sólo con Supr: Ctrl+AltGr+letra sigue escribiendo. */
    if (t == TECLA_DELETE && (mods & MOD_CTRL) && (mods & (MOD_ALT | MOD_ALTGR))) {
        if (menu_aberto()) menu_cancelar();
        klog("[GUI] Ctrl+Alt+Supr\n");
        gui_abrir_tarefas();
        return true;
    }
    return false;
}

void atalhos_tique(void)
{
    uint32_t agora = teclado_modificadores();
    if (alternando && !(agora & MOD_ALT)) terminar_alternar(true);
    if (super_pendente && !(agora & MOD_SUPER)) {
        super_pendente = false;
        if (!super_usada) {
            /* En pantalla completa la barra no se ve: sale de ella antes (D065). */
            gui_sair_tela_cheia();
            barra_abrir_inicio(true);
        }
    }
}

/* ---- dibujo */

static void desenhar_icone(superficie_t *s, int x, int y, const char *processo)
{
    const desenho_t *d = barra_icone_processo(processo);
    if (d) {
        int esc = d->largura <= 16 ? 2 : 1;
        sup_desenho(s, x + (32 - d->largura * esc) / 2, y + (32 - d->altura * esc) / 2, d, esc);
        return;
    }
    /* Sin icono propio: una ventanita. */
    sup_retangulo(s, x + 2, y + 5, 28, 22, 0xE8ECF0);
    sup_retangulo(s, x + 2, y + 5, 28, 6, aparencia_acento());
    sup_moldura(s, x + 2, y + 5, 28, 22, 0x101010);
}

bool atalhos_visivel(void) { return alternando && pl > 0; }   /* D195 */

void atalhos_desenhar(superficie_t *s)
{
    if (!alternando || pl <= 0) return;
    uint32_t acento = aparencia_acento();
    sup_retangulo(s, px, py, pl, pa, 0x18202A);
    sup_moldura(s, px, py, pl, pa, acento);
    sup_moldura(s, px + 1, py + 1, pl - 2, pa - 2, acento);
    int x0 = px + (pl - colunas * CELDA) / 2;
    for (int i = 0; i < qtd_lista; i++) {
        int cx = x0 + (i % colunas) * CELDA, cy = py + MARGEN + (i / colunas) * CELDA;
        if (i == elegida) {
            sup_retangulo(s, cx + 2, cy + 2, CELDA - 4, CELDA - 4, 0x34485C);
            sup_moldura(s, cx + 2, cy + 2, CELDA - 4, CELDA - 4, 0xFFDF4A);
            sup_moldura(s, cx + 3, cy + 3, CELDA - 6, CELDA - 6, 0xFFDF4A);
        }
        desenhar_icone(s, cx + (CELDA - 32) / 2, cy + (CELDA - 32) / 2, lista[i].processo);
        if (lista[i].minimizada)   /* una raya gris debajo: está minimizada */
            sup_retangulo(s, cx + 20, cy + CELDA - 12, CELDA - 40, 2, 0x8090A0);
    }
    /* Título de la elegida, recortado con «..» si no cabe. */
    char t[48];
    txt_copia_n(t, lista[elegida].titulo, sizeof t);
    int cabe = (pl - 2 * MARGEN) / 8;
    if (cabe < 1) return;
    if ((int)txt_tamanho(t) > cabe) {
        t[cabe] = 0;
        if (cabe >= 4) t[cabe - 1] = t[cabe - 2] = '.';
    }
    int tl = (int)txt_tamanho(t) * 8;
    sup_texto(s, px + (pl - tl) / 2, py + pa - ALTO_TITULO, t, 0xFFFFFF);
}

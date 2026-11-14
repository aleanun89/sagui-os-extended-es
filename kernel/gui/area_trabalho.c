// SPDX-License-Identifier: GPL-3.0-only
/* area_trabalho.c - el fondo, la mascota, los íconos y el menú del
 * escritorio (H20, H28, D051, D066).
 *
 * El fondo no cambia: se dibuja una vez en una superficie guardada (degradado
 * verde, amarillo y azul, el sagui en la esquina y los íconos), y en cada
 * redibujado sólo se copia la parte necesaria. La barra superior (barra.c) se
 * dibuja por encima, porque el reloj y las tareas cambian. */
#include "gui.h"
#include "arte.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "versao.h"
#include "video.h"
#include "disco.h"
#include "montagem.h"
#include "sistema.h"
#include "modos_video.h"

static uint32_t *fundo;
static uint32_t *base;   /* la imagen de fondo elegida (v5.2), del tamaño de la pantalla; NULL = degradado */
static int larg, alt;

typedef struct {
    const char *nome;
    const char *programa;
    const desenho_t *desenho;
} icone_t;

static const icone_t icones[] = {
    { "Archivos",    "/bin/arquivos",    &arte_icone_arquivos },
    { "Terminal",    "/bin/terminal",    &arte_icone_terminal },
    { "Editor",      "/bin/editor",      &arte_icone_editor },
    { "Calculadora", "/bin/calculadora", &arte_icone_calculadora },
    { "Banana",      "/bin/banana",      &arte_icone_banana },
    { "Sagui 3D",    "/bin/sagui3d",     &arte_icone_sagui3d },   /* bonus B4 */
    { "Acerca de",   "/bin/sobre-o-sistema",   &arte_icone_sobre },
    { "Pendrive",    "/bin/arquivos",    &arte_icone_pendrive },   /* sólo aparece con un pendrive conectado (H34) */
    { "Tareas",      "/bin/tareas",      &arte_icone_tarefas },    /* segunda columna (D043) */
    { "Visor",       "/bin/visor",       &arte_icone_visor },      /* "Visor de imágenes" (D055): el nombre entero chocaría con el de la columna 1 */
    { "Paint",       "/bin/paint",       &arte_icone_paint },      /* D056 */
    { "CHIP-8",      "/bin/chip8",       &arte_icone_chip8 },      /* D070 */
    { "Configuración", "/bin/configuracion", &arte_icone_configuracion },   /* D112 */
    { "Discos",      "/bin/discos",      &arte_icone_discos },           /* D121: sin argumentos y con salida no-terminal abre la ventana */
};
#define QTD_ICONES (int)(sizeof icones / sizeof icones[0])
#define ICONE_PENDRIVE 7
static bool pendrive_visivel;
static char nome_pendrive[24] = "Pendrive";

static bool icone_visivel(int i) { return i != ICONE_PENDRIVE || pendrive_visivel; }
#define ICONE_X 20
#define ICONE_Y0 (BARRA_SUPERIOR + 16)
#define ICONE_PASSO 84
#define ICONE_L 96
#define ICONE_A 88

/* Cuántos íconos caben en una columna (los demás van a la columna de al lado):
 * 8 en 768 líneas, como siempre; depende de la altura porque la pantalla puede
 * cambiar de modo (D059). */
static int icones_por_coluna(void)
{
    int n = (alt - ICONE_Y0) / ICONE_PASSO;
    return n < 1 ? 1 : n;
}

int area_qtd_icones(void) { return QTD_ICONES; }

void area_icone_retangulo(int i, int *x, int *y, int *l, int *a)
{
    *x = ICONE_X + (i / icones_por_coluna()) * (ICONE_L + 8);
    *y = ICONE_Y0 + (i % icones_por_coluna()) * ICONE_PASSO;
    *l = ICONE_L; *a = ICONE_A;
}

/* Interpolación de un canal en enteros. */
static uint32_t mistura(uint32_t a, uint32_t b, int num, int den)
{
    uint32_t r = 0;
    for (int s = 0; s <= 16; s += 8) {
        int ca = (int)((a >> s) & 255), cb = (int)((b >> s) & 255);
        r |= (uint32_t)(ca + (cb - ca) * num / den) << s;
    }
    return r;
}

static void texto_com_sombra(superficie_t *s, int x, int y, const char *t, uint32_t cor)
{
    sup_texto(s, x + 1, y + 1, t, 0x101010);
    sup_texto(s, x, y, t, cor);
}

static void desenhar_fundo(void)
{
    superficie_t s;
    sup_iniciar(&s, fundo, larg, alt);
    if (base) {   /* imagen elegida en Configuración: ya viene escalada a la pantalla */
        memcpy(fundo, base, (uint32_t)(larg * alt) * 4);
        goto iconos;
    }
    /* Degradado vertical en los colores de Brasil: verde arriba, amarillo en el medio,
     * azul abajo, en tonos más suaves que los de la bandera para que el texto y los
     * íconos sigan siendo legibles. */
    const uint32_t verde = 0x0B7A3E, amarelo = 0xE0B92A, azul = 0x14337E;
    for (int y = 0; y < alt; y++) {
        uint32_t c = y < alt / 2 ? mistura(verde, amarelo, y, alt / 2)
                                 : mistura(amarelo, azul, y - alt / 2, alt - alt / 2);
        uint32_t *p = fundo + y * larg;
        for (int x = 0; x < larg; x++) p[x] = c;
    }
    /* La mascota, grande, en la esquina de abajo a la derecha. */
    int esc = larg >= 1000 ? 5 : 3;
    int mx = larg - 32 * esc - 30, my = alt - 32 * esc - 40;
    sup_desenho(&s, mx, my, &arte_sagui, esc);
    texto_com_sombra(&s, mx + 16 * esc - texto_largura(SAGUI_NOME) / 2, my + 32 * esc + 6, SAGUI_NOME, 0xFFFFFF);
iconos:
    /* Íconos en la columna de la izquierda, con el nombre abajo. */
    for (int i = 0; i < QTD_ICONES; i++) {
        if (!icone_visivel(i)) continue;
        int x, y, l, a;
        area_icone_retangulo(i, &x, &y, &l, &a);
        sup_desenho(&s, x + (l - 64) / 2, y, icones[i].desenho, 2);
        const char *nome = i == ICONE_PENDRIVE ? nome_pendrive : icones[i].nome;
        texto_com_sombra(&s, x + (l - texto_largura(nome)) / 2, y + 68, nome, 0xFFFFFF);
    }
}

bool area_iniciar(void)
{
    larg = (int)video_largura();
    alt = (int)video_altura();
    fundo = kmalloc((uint32_t)(larg * alt) * 4);
    if (!fundo) return false;
    desenhar_fundo();
    return true;
}

/* El modo de video cambió (D059): fondo nuevo del tamaño de la pantalla. */
bool area_tela_mudou(void)
{
    int l = (int)video_largura(), a = (int)video_altura();
    uint32_t *novo = kmalloc((uint32_t)(l * a) * 4);
    if (!novo) return false;
    kfree(fundo);
    fundo = novo;
    if (base) {   /* se reescala (vecino más próximo); el próximo arranque la ajusta bien */
        uint32_t *nb = kmalloc((uint32_t)(l * a) * 4);
        if (nb) {
            for (int y = 0; y < a; y++) {
                const uint32_t *o = base + (y * alt / a) * larg;
                for (int x = 0; x < l; x++) nb[y * l + x] = o[x * larg / l];
            }
        }
        kfree(base);
        base = nb;
    }
    larg = l; alt = a;
    desenhar_fundo();
    return true;
}

/* Fondo personalizado (D112, SIS_FUNDO): la imagen llega ya escalada, por tandas de líneas. */
bool area_fundo_personalizado(void) { return base != NULL; }

void area_fundo_padrao(void)
{
    if (!base) return;
    kfree(base);
    base = NULL;
    desenhar_fundo();
}

int area_fundo_linhas(const uint32_t *px, int y, int n)
{
    if (y < 0 || n <= 0 || y + n > alt) return ERRO_INVALIDO;
    if (!base) {
        base = kmalloc((uint32_t)(larg * alt) * 4);
        if (!base) return ERRO_SEM_MEMORIA;
        memset(base, 0, (uint32_t)(larg * alt) * 4);
    }
    memcpy(base + y * larg, px, (uint32_t)(n * larg) * 4);
    return 0;
}

void area_fundo_fim(void)
{
    desenhar_fundo();
    gui_redesenhar(0, 0, larg, alt);
    gui_mostrar();
}

void area_fundo(superficie_t *s)
{
    sup_copiar(s, 0, 0, fundo, larg, alt, larg);
}

int area_icone_em(int x, int y)
{
    for (int i = 0; i < QTD_ICONES; i++) {
        if (!icone_visivel(i)) continue;
        int ix, iy, l, a;
        area_icone_retangulo(i, &ix, &iy, &l, &a);
        if (x >= ix && x < ix + l && y >= iy && y < iy + a) return i;
    }
    return -1;
}

/* Muestra u oculta el ícono del pendrive (H34). Devuelve true si cambió. */
bool area_pendrive(bool presente, const char *rotulo)
{
    char nome[24];
    txt_copia_n(nome, rotulo && rotulo[0] ? rotulo : "Pendrive", sizeof nome);
    if (presente == pendrive_visivel && txt_compara(nome, nome_pendrive) == 0) return false;
    pendrive_visivel = presente;
    txt_copia_n(nome_pendrive, nome, sizeof nome_pendrive);
    desenhar_fundo();
    return true;
}

static disco_t *primeiro_disco(void)
{
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *d = disco_indice(i);
        if (d && d->presente && !d->pai && d->removivel && !d->de_boot) return d;
    }
    return NULL;
}

/* "Quitar con seguridad" del pendrive: graba lo que falta y desmonta. */
static void remover_pendrive(void)
{
    disco_t *d = disco_pendrive_montado();
    if (d) disco_desmontar(d);
    else gui_aviso("El pendrive no está montado: puedes retirarlo.");
}

/* Respuesta del menú de un ícono (D051): "dado" es el ícono. */
static void resposta_icone(int indice, int janela, int i)
{
    (void)janela;
    if (indice == 0) area_icone_abrir(i);
    else if (indice == 2 && i == ICONE_PENDRIVE) remover_pendrive();
}

/* Botón derecho en un ícono: menú con "Abrir" y, en el pendrive, "Quitar con
 * seguridad" (antes el botón derecho en el pendrive ya desmontaba directo, sin
 * preguntar nada; ahora la persona ve lo que va a pasar). */
void area_icone_direito(int i, int x, int y)
{
    if (i < 0 || i >= QTD_ICONES) return;
    const char *itens = i == ICONE_PENDRIVE ? "Abrir\n-\nQuitar con seguridad" : "Abrir";
    menu_abrir(x, y, itens, resposta_icone, 0, i);
}

/* Menú del escritorio vacío (D051): los programas que ya tienen ícono,
 * el Administrador de tareas, la configuración de la pantalla con el submenú de las
 * resoluciones (D066) y el "Acerca de". Cada línea lleva una acción; las de los
 * íconos son el propio índice del ícono. */
enum { AF_TAREFAS = 100, AF_PANTALLA, AF_RESOLUCAO = 200 };
#define RES_MAX 24

static menu_lista_t menu_fundo;
static modo_video_t res_modos[RES_MAX];

/* "Configuración de pantalla": la ventana del programa pantalla, una sola. */
void area_abrir_pantalla(void)
{
    char *argv[] = { "pantalla", "--ventana", NULL };
    gui_abrir_unico("pantalla", "/bin/pantalla", 2, argv);
}

/* Submenú "Resolución": los modos de 32 bits que cambian en caliente, con el
 * actual marcado (D066). Sin cambio en caliente (la placa real, sólo VBE), no hay
 * submenú: la ventana "Pantalla" lo explica y graba para el próximo arranque. Si
 * la lista no cabe en la altura de la pantalla, quedan las menores, y la actual siempre. */
void area_menu_resolucao(menu_lista_t *ml)
{
    static modo_video_t todos[192];
    int n = modos_lista(todos, 192), k = 0, cabe = ((int)video_altura() - 8) / 22 - 1;
    bool tem_atual = false;
    if (cabe > RES_MAX) cabe = RES_MAX;
    for (int i = 0; i < n; i++) {
        if (todos[i].bpp != video_bpp_a_quente() || !(todos[i].flags & (MODO_A_QUENTE | MODO_ATUAL))) continue;
        bool e_atual = (todos[i].flags & MODO_ATUAL) != 0;
        if (k < cabe) { res_modos[k++] = todos[i]; tem_atual |= e_atual; }
        else if (e_atual && !tem_atual) { res_modos[cabe - 1] = todos[i]; tem_atual = true; }
    }
    if (k < 2) return;   /* sólo el modo actual: no hay qué elegir */
    char t[MENU_MAX_TEXTO + 1];
    snformatar(t, sizeof t, ">Resolución\t%ux%u", video_largura(), video_altura());
    menu_lista_linha(ml, t, MENU_NADA, NULL);
    for (int i = 0; i < k; i++) {
        modo_video_t *mv = &res_modos[i];
        bool a_quente = (mv->flags & MODO_A_QUENTE) != 0;
        snformatar(t, sizeof t, "%s%s%ux%u", (mv->flags & MODO_ATUAL) ? "*" : "", a_quente ? "" : "~", mv->largura,
                   mv->altura);
        menu_lista_linha(ml, t, AF_RESOLUCAO + i, NULL);
    }
    menu_lista_linha(ml, "<", MENU_NADA, NULL);
}

/* Cambia la resolución elegida en el submenú, por el mismo camino del
 * SIS_MUDAR_MODO (MUDAR_AGORA, sin grabar). */
bool area_resposta_resolucao(int acao)
{
    if (acao < AF_RESOLUCAO || acao >= AF_RESOLUCAO + RES_MAX) return false;
    modo_video_t *mv = &res_modos[acao - AF_RESOLUCAO];
    if (mv->flags & MODO_ATUAL) return true;
    int r = sis_mudar_modo((uint32_t)mv->largura | (uint32_t)mv->altura << 16, MUDAR_AGORA, 0, 0, 0);
    klog("[GUI] resolucao pelo menu: %ux%u -> %d\n", mv->largura, mv->altura, r);
    if (r < 0) gui_aviso("No se pudo cambiar la resolución.");
    return true;
}

static void resposta_fundo(int indice, int janela, int dado)
{
    (void)janela; (void)dado;
    int acao = menu_lista_acao(&menu_fundo, indice);
    if (acao == AF_TAREFAS) gui_abrir_tarefas();
    else if (acao == AF_PANTALLA) area_abrir_pantalla();
    else if (area_resposta_resolucao(acao)) {}
    else if (acao >= 0 && acao < QTD_ICONES) area_icone_abrir(acao);
}

void area_menu_fundo(int x, int y)
{
    menu_lista_t *ml = &menu_fundo;
    menu_lista_iniciar(ml);
    menu_lista_linha(ml, "Terminal", 1, NULL);
    menu_lista_linha(ml, "Archivos", 0, NULL);
    menu_lista_linha(ml, "Editor", 2, NULL);
    menu_lista_linha(ml, "Calculadora", 3, NULL);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    menu_lista_linha(ml, "Administrador de tareas", AF_TAREFAS, NULL);
    menu_lista_linha(ml, "Configuración de pantalla...", AF_PANTALLA, NULL);
    area_menu_resolucao(ml);
    menu_lista_linha(ml, "-", MENU_NADA, NULL);
    menu_lista_linha(ml, "Acerca de " SAGUI_NOME, 6, NULL);
    menu_abrir(x, y, ml->texto, resposta_fundo, 0, 0);
}

/* Abre el programa del ícono (H28). No tiene padre: nadie espera por él. */
void area_icone_abrir(int i)
{
    if (i < 0 || i >= QTD_ICONES) return;
    if (i == ICONE_PENDRIVE) {
        disco_t *d = primeiro_disco();
        if (!d) return;
        int r = disco_montar(d);
        if (r == ERRO_FORMATO) { gui_aviso("Este pendrive no está en FAT32 ni ext2/ext4: formato no soportado."); return; }
        if (r < 0 && r != ERRO_OCUPADO) { gui_aviso("No se pudo montar el pendrive."); return; }
        /* Abre donde el volumen esté montado (normalmente /usb, D040). */
        static char ponto[CAMINHO_MAX];
        txt_copia_n(ponto, "/usb", sizeof ponto);
        disco_t *v = d->volume;
        if (v && v->montagem) txt_copia_n(ponto, v->montagem->ponto, sizeof ponto);
        char *argv[] = { "arquivos", ponto, NULL };
        int pid = processo_executar("/bin/arquivos", 2, argv, NULL);
        klog("[GUI] icone Pendrive -> /bin/arquivos %s (pid %d)\n", ponto, pid);
        return;
    }
    char *argv[] = { (char *)icones[i].programa + 5, NULL };
    int pid = processo_executar(icones[i].programa, 1, argv, NULL);
    klog("[GUI] icone %s -> %s (pid %d)\n", icones[i].nome, icones[i].programa, pid);
}

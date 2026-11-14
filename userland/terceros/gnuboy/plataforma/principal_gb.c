// SPDX-License-Identifier: GPL-2.0-or-later
/* principal_gb.c - el punto de entrada de SDL2-GNUBoy en el SaguiOS (D235).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv)
 * (userland/libsagui/inicio.asm). El emulador (código del upstream, sin tocar)
 * tiene un main() de verdad (src/main.c); este fichero hace de puente y de
 * lanzador, como los de OpenJazz (D228) y Command & Conquer (D230):
 *
 *   - busca ROM de Game Boy y Game Boy Color (.gb y .gbc) en /jogos/gb (y en
 *     sus subcarpetas), en /home/gnuboy/roms y en los discos montados
 *     (jogos/gb, gb y roms de cada uno, y su raíz). Las ROM no vienen con el
 *     sistema: son del que las pone (docs/input_share.md);
 *   - con ROM, abre una ventana con la lista (el nombre de la cabecera del
 *     cartucho y el del archivo), se elige con flechas e Intro, doble clic o
 *     «Abrir»; con la ROM pasada en la línea de órdenes
 *     (gnuboy ROM.gb), arranca sin preguntar;
 *   - sin ROM, una ventana de ayuda que explica dónde ponerlas, con «Buscar
 *     otra vez» (por si se acaba de montar un pendrive);
 *   - la configuración (gnuboy.rc), la batería del cartucho (.sav), el reloj
 *     (.rtc) y los estados (.sv0 a .sv9) van a /home/gnuboy: el emulador los
 *     escribe en savedir. La primera vez se escribe un gnuboy.rc con lo que
 *     conviene en el SaguiOS (teclas, ventana de 480x432 = x3, sonido).
 *
 * Opciones propias (se quitan antes de llamar al emulador):
 *   --pantalla-completa   la ventana nace a pantalla completa (D204)
 *   --roms CARPETA        buscar solo en esa carpeta
 * El resto (--scale=2, --no-sound, ...) son las del emulador (--help).
 */
#include "sagui.h"
#include "gui.h"
#include "SDL.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]);   /* el main del emulador (src/main.c) */

#define CONFIG_GB   "/home/gnuboy"
#define PATH_GB     200
#define MAX_ROMS    400

typedef struct {
    char caminho[PATH_GB];
    char titulo[20];      /* de la cabecera; "" si no es válida */
    int  cgb;             /* 1 = Game Boy Color (solo o compatible) */
} rom_t;

static rom_t roms[MAX_ROMS];
static int n_roms;
static const char *so_pasta;   /* --roms */

static int tem_extensao(const char *nome)
{
    size_t n = strlen(nome);
    if (n < 4) return 0;
    const char *e = nome + n - 3;
    if (nome[n - 4] == '.' && (e[0] | 32) == 'g' && (e[1] | 32) == 'b' && (e[2] | 32) == 'c') return 1;
    return nome[n - 3] == '.' && (nome[n - 2] | 32) == 'g' && (nome[n - 1] | 32) == 'b';
}

/* Lee la cabecera del cartucho (0x100-0x14F): el logo, el título (0x134, 16 bytes; 15 si es
 * de Game Boy Color, 0x143) y la marca de Color. 0 = el archivo no se puede leer o es demasiado
 * corto para ser una ROM. */
static int ler_cabecalho(const char *caminho, rom_t *r)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return 0;
    unsigned char h[0x150];
    size_t n = fread(h, 1, sizeof h, f);
    fclose(f);
    if (n < sizeof h) return 0;
    r->cgb = (h[0x143] & 0x80) != 0;
    static const unsigned char logo[4] = { 0xCE, 0xED, 0x66, 0x66 };
    r->titulo[0] = 0;
    if (memcmp(h + 0x104, logo, 4) != 0) return 1;   /* sin logo de Nintendo: se lista por el nombre del archivo */
    int max = r->cgb ? 15 : 16, k = 0;
    for (int i = 0; i < max && h[0x134 + i]; i++)
        if (h[0x134 + i] >= 32 && h[0x134 + i] < 127) r->titulo[k++] = (char)h[0x134 + i];
    while (k > 0 && r->titulo[k - 1] == ' ') k--;
    r->titulo[k] = 0;
    return 1;
}

static void anotar(const char *caminho)
{
    if (n_roms >= MAX_ROMS) return;
    for (int i = 0; i < n_roms; i++)
        if (strcmp(roms[i].caminho, caminho) == 0) return;   /* la misma por dos caminos */
    rom_t *r = &roms[n_roms];
    strncpy(r->caminho, caminho, sizeof r->caminho - 1);
    r->caminho[sizeof r->caminho - 1] = 0;
    if (!ler_cabecalho(caminho, r)) return;
    n_roms++;
}

/* Las ROM de una carpeta y, hasta «prof» niveles, las de sus subcarpetas. */
static void varrer(const char *pasta, int prof)
{
    entrada_pasta_t e;
    char c[PATH_GB];
    for (int i = 0; listar(pasta, i, &e) > 0 && i < 1024; i++) {
        if (e.nome[0] == '.') continue;
        caminho_juntar(c, sizeof c, pasta, e.nome);
        if (e.tipo == TIPO_PASTA) {
            if (prof > 0) varrer(c, prof - 1);
        } else if (tem_extensao(e.nome)) {
            anotar(c);
        }
    }
}

static int comparar(const void *a, const void *b)
{
    const rom_t *x = a, *y = b;
    const char *p = x->titulo[0] ? x->titulo : nome_base(x->caminho);
    const char *q = y->titulo[0] ? y->titulo : nome_base(y->caminho);
    for (;; p++, q++) {
        int u = *p >= 'A' && *p <= 'Z' ? *p + 32 : *p, v = *q >= 'A' && *q <= 'Z' ? *q + 32 : *q;
        if (u != v) return u - v;
        if (!u) return strcmp(x->caminho, y->caminho);
    }
}

static int buscar_roms(void)
{
    n_roms = 0;
    if (so_pasta) {
        varrer(so_pasta, 2);
    } else {
        varrer("/jogos/gb", 2);
        varrer(CONFIG_GB "/roms", 2);
        info_disco_t d;
        for (int i = 0; listar_discos(i, &d) == 1; i++) {
            char c[PATH_GB];
            if (!d.ponto[0] || strcmp(d.ponto, "/jogos") == 0) continue;
            caminho_juntar(c, sizeof c, d.ponto, "jogos/gb");
            varrer(c, 2);
            caminho_juntar(c, sizeof c, d.ponto, "gb");
            varrer(c, 2);
            caminho_juntar(c, sizeof c, d.ponto, "roms");
            varrer(c, 1);
            varrer(d.ponto, 0);   /* también sueltas en la raíz del pendrive */
        }
    }
    if (n_roms > 1) qsort(roms, (size_t)n_roms, sizeof roms[0], comparar);
    registrar("[gnuboy] %d ROM encontradas", n_roms);
    return n_roms;
}

/* La ventana de «no hay ROM». 1 = se encontraron al buscar de nuevo. */
static int janela_sem_roms(void)
{
    static const char *const linhas[] = {
        "No se encuentra ninguna ROM de Game Boy.",
        "",
        "GNUBoy es el emulador de Game Boy y Game Boy Color; los juegos (las ROM, archivos",
        ".gb y .gbc) no vienen con el SaguiOS: son del que los pone.",
        "",
        "  - Copie las ROM a /jogos/gb (o a jogos/gb de un pendrive, o a /home/gnuboy/roms).",
        "  - O al compilar el sistema, póngalas en input_share/roms/gb/ (docs/input_share.md).",
        "  - O desde el terminal:  gnuboy /ruta/juego.gb",
        "",
        "Teclas: flechas, Z (A), X (B), Intro (Start), Mayús (Select), F5 guarda el estado,",
        "F7 lo carga, F11 pantalla completa, Esc sale. La batería del cartucho (.sav) y los",
        "estados quedan en /home/gnuboy.",
        "",
        "Cuando las ROM estén a mano, pulse «Buscar otra vez».",
    };
    jan_t *j = jan_abrir("Game Boy (GNUBoy): faltan las ROM", 640, 340);
    if (!j) return 0;
    botao_t buscar = { 170, 296, 140, 28, "Buscar otra vez", false };
    botao_t cerrar = { 340, 296, 110, 28, "Cerrar", false };
    registrar("[gnuboy] sin ROM: ventana de ayuda");
    int achou = 0, falhou = 0;
    for (;;) {
        g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        rotulo(j, 16, 14, linhas[0], COR_DESTAQUE);
        for (int i = 1; i < (int)(sizeof linhas / sizeof linhas[0]); i++)
            rotulo(j, 16, 14 + i * 18, linhas[i], COR_TEXTO);
        if (falhou) rotulo(j, 16, 272, "Todavía no hay: compruebe la carpeta y el montaje.", COR_TEXTO_FRACO);
        botao_desenhar(j, &buscar);
        botao_desenhar(j, &cerrar);
        jan_mostrar(j);
        evento_t e;
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (botao_tratar(j, &cerrar, &e)) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(j, &buscar, &e) || (e.tipo == EV_TECLA && e.tecla == 10)) {
            achou = buscar_roms() > 0;
            registrar("[gnuboy] buscar otra vez: %s", achou ? "hay ROM" : "sin ROM");
            if (achou) break;
            falhou = 1;
        }
    }
    jan_fechar(j);
    return achou;
}

/* La ventana para elegir ROM: -1 = cerrada sin elegir; si no, el índice. */
static int janela_escolher(void)
{
    static char itens[MAX_ROMS][64];
    for (int i = 0; i < n_roms; i++) {
        const rom_t *r = &roms[i];
        snprintf(itens[i], sizeof itens[i], "%s%s  (%s)", r->titulo[0] ? r->titulo : "(sin cabecera)",
                 r->cgb ? " [GBC]" : "", nome_base(r->caminho));
        /* Lo que muestra la lista, también en el registro (las pruebas lo leen de ahí:
         * el texto de la ventana no cae en la rejilla de 8x16 de su reconocedor). */
        if (i < 32) registrar("[gnuboy] lista: %s", itens[i]);
    }
    jan_t *j = jan_abrir("Game Boy (GNUBoy): elegir ROM", 560, 380);
    if (!j) return -1;
    lista_t lista = { 16, 40, 528, 272, itens, n_roms, 0, 0, NULL, NULL };
    botao_t abrir = { 330, 336, 100, 28, "Abrir", false };
    botao_t cerrar = { 444, 336, 100, 28, "Cerrar", false };
    int escolha = -1;
    for (;;) {
        g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        rotulo(j, 16, 12, "Elija un juego (nombre de la cabecera del cartucho y archivo):", COR_TEXTO);
        lista_desenhar(j, &lista);
        rotulo(j, 16, 344, "Flechas e Intro, o doble clic.", COR_TEXTO_FRACO);
        botao_desenhar(j, &abrir);
        botao_desenhar(j, &cerrar);
        jan_mostrar(j);
        evento_t e;
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(j, &cerrar, &e)) break;
        if (botao_tratar(j, &abrir, &e)) { escolha = lista.sel; break; }
        if (lista_tratar(j, &lista, &e) == LISTA_ATIVOU) { escolha = lista.sel; break; }
    }
    jan_fechar(j);
    return escolha;
}

/* El gnuboy.rc de la primera vez: el emulador lo lee de savedir/rcpath (la carpeta actual,
 * /home/gnuboy) después de sus valores por defecto, así que lo escrito aquí manda y el
 * usuario puede cambiarlo (se escribe solo si no existe). Las teclas son las del emulador:
 * «bind TECLA +boton» (up down left right a b start select). */
static void config_inicial(void)
{
    char c[PATH_GB];
    info_arquivo_t inf;
    caminho_juntar(c, sizeof c, CONFIG_GB, "gnuboy.rc");
    if (info_arquivo(c, &inf) >= 0) return;
    FILE *f = fopen(c, "w");
    if (!f) {
        registrar("[gnuboy] no se puede escribir %s", c);
        return;
    }
    fputs("# SDL2-GNUBoy en SaguiOS: la configuracion inicial. Se puede cambiar: es de este usuario.\n"
          "# Las teclas: bind TECLA +boton  (up down left right a b start select)\n"
          "bind up +up\n"
          "bind down +down\n"
          "bind left +left\n"
          "bind right +right\n"
          "bind z +a\n"
          "bind x +b\n"
          "bind enter +start\n"
          "# la Mayus (izquierda o derecha: el emulador no las distingue) es Select\n"
          "bind shift +select\n"
          "bind tab +select\n"
          "# estados (se guardan en la carpeta de savedir; 0 a 9 eligen la ranura)\n"
          "bind f5 savestate\n"
          "bind f7 loadstate\n"
          "# ventana de 160x144 por 3 = 480x432; F11 o Alt+Intro, pantalla completa con bandas\n"
          "set scale 3\n"
          "set sound 1\n"
          "set samplerate 44100\n"
          "set alert_on_quit 0\n"
          "# la bateria del cartucho (.sav), el reloj (.rtc) y los estados (.sv0-.sv9)\n"
          "set savedir " CONFIG_GB "\n", f);
    fclose(f);
    registrar("[gnuboy] configuracion inicial en %s", c);
}

static void ao_sair(void)
{
    registrar("[gnuboy] fin");
}

int principal(int argc, char **argv)
{
    static char *args[64];
    const char *rom = NULL;
    int n = 0, cheia = 0;
    args[n++] = (char *)"/apps/bin/gnuboy";
    for (int i = 1; i < argc && n < 60; i++) {
        if (strcmp(argv[i], "--pantalla-completa") == 0) { cheia = 1; continue; }
        if (strcmp(argv[i], "--roms") == 0 && i + 1 < argc) { so_pasta = argv[++i]; continue; }
        if (strcmp(argv[i], "--bind") == 0 && i + 2 < argc) {   /* con sus dos argumentos */
            args[n++] = argv[i];
            args[n++] = argv[++i];
            args[n++] = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) { args[n++] = argv[i]; args[n++] = argv[++i]; continue; }
        if (strcmp(argv[i], "--link") == 0 && i + 1 < argc) { args[n++] = argv[i]; args[n++] = argv[++i]; continue; }
        if (argv[i][0] != '-' && !rom) { rom = argv[i]; continue; }   /* la ROM */
        args[n++] = argv[i];
    }

    int ayuda = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "--showvars") == 0
            || strcmp(argv[i], "--copying") == 0) ayuda = 1;

    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) < 0) criar_pasta("/home");
    if (info_arquivo(CONFIG_GB, &inf) < 0) criar_pasta(CONFIG_GB);
    if (mudar_pasta(CONFIG_GB) < 0)
        registrar("[gnuboy] no se puede entrar en %s: no se guardara nada", CONFIG_GB);

    static char elegida[PATH_GB];
    if (ayuda) {
        rom = "-";   /* el emulador solo imprime y sale */
    } else if (!rom) {
        if (!buscar_roms() && !janela_sem_roms()) return 1;
        int k = janela_escolher();
        if (k < 0) return 0;
        strncpy(elegida, roms[k].caminho, sizeof elegida - 1);
        rom = elegida;
    }
    registrar("[gnuboy] ROM: %s", rom);

    config_inicial();
    args[n++] = (char *)rom;
    args[n] = NULL;

    sagui_sdl2_nome = "GNUBOY";
    sagui_sdl2_escala = 1;
    if (cheia) sagui_sdl2_cheia_inicial = 1;

    atexit(ao_sair);
    int r = main(n, args);
    registrar("[gnuboy] el emulador vuelve (%d)", r);
    fflush(NULL);
    return r;
}

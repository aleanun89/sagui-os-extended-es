/* principal_oj.c - el punto de entrada de OpenJazz en el SaguiOS (D228).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv)
 * (userland/libsagui/inicio.asm, que antes llama a los constructores
 * globales de C++). El motor (código del upstream, sin tocar) tiene un main()
 * de verdad; este fichero hace de puente y de lanzador, como el del
 * OpenTyrian (D180):
 *
 *   - busca los datos de Jazz Jackrabbit (los ficheros del juego original,
 *     que no son libres) en la partición de juegos: /jogos/openjazz y
 *     cualquier otra carpeta de /jogos (lo que el que compila puso en
 *     input_share/games/, D228: por ejemplo /jogos/Jazz con el juego
 *     completo), y en jogos/... de cada disco montado. Si hay varias, gana la
 *     versión completa sobre la shareware;
 *   - si no los encuentra, abre una ventana que explica cómo conseguirlos,
 *     con «Buscar otra vez» (por si se acaba de montar un pendrive);
 *   - la configuración (openjazz.cfg), las partidas guardadas y el registro
 *     van a /home/openjazz: el motor los escribe en la carpeta actual, y
 *     aquí se entra en ella antes de llamarlo.
 *
 * Opciones propias (se quitan antes de llamar al motor):
 *   --pantalla-completa   la ventana nace a pantalla completa (D204)
 *   --datos CARPETA       usar esa carpeta de datos (sin buscar)
 */
#include "sagui.h"
#include "gui.h"
#include "SDL.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]);   /* el main de OpenJazz (src/main.cpp) */

#define PATH_OJ 200
#define CONFIG_OJ "/home/openjazz"

static char achada[PATH_OJ];

static int existe(const char *pasta, const char *nome)
{
    char c[PATH_OJ];
    info_arquivo_t inf;
    caminho_juntar(c, sizeof c, pasta, nome);
    return info_arquivo(c, &inf) >= 0;
}

/* El motor busca cada fichero tal cual, en mayúsculas y en minúsculas. */
static int existe_jj(const char *pasta, const char *maius, const char *minus)
{
    return existe(pasta, maius) || existe(pasta, minus);
}

/* ¿Hay datos de Jazz Jackrabbit 1 en esa carpeta? 0 = no; 1 = el
 * shareware (solo el episodio 1); 2 = la versión completa. */
static int nota_dados(const char *pasta)
{
    if (!existe_jj(pasta, "MAINCHAR.000", "mainchar.000") || !existe_jj(pasta, "BLOCKS.000", "blocks.000")
        || !existe_jj(pasta, "PANEL.000", "panel.000"))
        return 0;
    /* El shareware trae los planetas 0, 1, 2 y 18; el juego entero, más. */
    if (existe_jj(pasta, "BLOCKS.003", "blocks.003") || existe_jj(pasta, "BLOCKS.010", "blocks.010"))
        return 2;
    return 1;
}

static int melhor_nota;

static void considerar(const char *pasta)
{
    int n = nota_dados(pasta);
    if (n > melhor_nota) {
        melhor_nota = n;
        strncpy(achada, pasta, sizeof achada - 1);
        achada[sizeof achada - 1] = 0;
    }
}

/* Mira «base/openjazz» y cada subcarpeta de «base». */
static void procurar_em(const char *base)
{
    char c[PATH_OJ];
    caminho_juntar(c, sizeof c, base, "openjazz");
    considerar(c);
    entrada_pasta_t e;
    for (int i = 0; listar(base, i, &e) > 0 && i < 256; i++) {
        if (e.tipo != TIPO_PASTA || e.nome[0] == '.') continue;
        caminho_juntar(c, sizeof c, base, e.nome);
        considerar(c);
    }
}

static int buscar_dados(void)
{
    melhor_nota = 0;
    achada[0] = 0;
    procurar_em("/jogos");
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        char c[PATH_OJ];
        if (!d.ponto[0] || strcmp(d.ponto, "/jogos") == 0) continue;
        caminho_juntar(c, sizeof c, d.ponto, "jogos");
        procurar_em(c);
    }
    return melhor_nota > 0;
}

/* La ventana de «faltan los datos». 1 = se encontraron al buscar de nuevo. */
static int janela_sem_dados(void)
{
    static const char *const linhas[] = {
        "No se encuentran los datos de Jazz Jackrabbit.",
        "",
        "OpenJazz es el motor libre del juego; los gráficos, la música y los niveles",
        "son los del juego original de Epic MegaGames, que no son libres y no vienen",
        "con el SaguiOS. Sirven la versión shareware (el episodio 1) o la completa:",
        "",
        "  - Copie la carpeta del juego (con MAINCHAR.000, BLOCKS.000...) a /jogos,",
        "    por ejemplo a /jogos/openjazz, o a jogos/ de un pendrive montado.",
        "  - Al compilar el sistema: tools/openjazz_datos.sh baja el shareware a",
        "    input_share/games/, o ponga ahí el juego completo (docs/input_share.md).",
        "",
        "Cuando los datos estén a mano, pulse «Buscar otra vez».",
    };
    jan_t *j = jan_abrir("OpenJazz: faltan los datos", 640, 290);
    if (!j) return 0;
    botao_t buscar = { 170, 244, 140, 28, "Buscar otra vez", false };
    botao_t cerrar = { 340, 244, 110, 28, "Cerrar", false };
    registrar("[OPENJAZZ] faltan los datos: ventana de ayuda");
    int achou = 0, falhou = 0;
    for (;;) {
        g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        rotulo(j, 16, 14, linhas[0], COR_DESTAQUE);
        for (int i = 1; i < (int)(sizeof linhas / sizeof linhas[0]); i++)
            rotulo(j, 16, 14 + i * 18, linhas[i], COR_TEXTO);
        if (falhou) rotulo(j, 16, 222, "Todavía no están: compruebe la carpeta y el montaje.", COR_TEXTO_FRACO);
        botao_desenhar(j, &buscar);
        botao_desenhar(j, &cerrar);
        jan_mostrar(j);
        evento_t e;
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (botao_tratar(j, &cerrar, &e)) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(j, &buscar, &e) || (e.tipo == EV_TECLA && e.tecla == 10)) {
            achou = buscar_dados();
            registrar("[OPENJAZZ] buscar otra vez: %s", achou ? "encontrados" : "sin datos");
            if (achou) break;
            falhou = 1;
        }
    }
    jan_fechar(j);
    return achou;
}

/* La carpeta de la configuración y las partidas: se crea si falta y se entra
 * en ella. Si /home no se puede escribir, el motor sigue igual (sin guardar).
 * Es un constructor con prioridad: corre antes que los constructores globales
 * de C++ (los de .init_array sin prioridad van detrás), y el del registro de
 * OpenJazz (Log::Log) abre «openjazz.log» en la carpeta actual. */
__attribute__((constructor(101))) static void entrar_em_config(void)
{
    info_arquivo_t inf;
    /* Sin partición de datos no hay /home (solo existe si se monta): se crea en la
     * RAM, y la configuración vale para esta sesión. */
    if (info_arquivo("/home", &inf) < 0) criar_pasta("/home");
    if (info_arquivo(CONFIG_OJ, &inf) < 0) criar_pasta(CONFIG_OJ);
    if (mudar_pasta(CONFIG_OJ) < 0)
        registrar("[OPENJAZZ] no se puede entrar en %s: la configuracion no se guardara", CONFIG_OJ);   /* el registro, en ASCII */
}

static void ao_mudar_cheia(int cheia)
{
    registrar("[OPENJAZZ] pantalla completa: %s", cheia ? "activada" : "desactivada");
}

int principal(int argc, char **argv)
{
    static char *args[64];
    const char *datos = NULL;
    int n = 0, cheia = 0;
    args[n++] = (char *)"OpenJazz";
    for (int i = 1; i < argc && n < 60; i++) {
        if (strcmp(argv[i], "--pantalla-completa") == 0) { cheia = 1; continue; }
        if (strcmp(argv[i], "--datos") == 0 && i + 1 < argc) { datos = argv[++i]; continue; }
        args[n++] = argv[i];
    }

    if (datos) {
        strncpy(achada, datos, sizeof achada - 1);
        if (!nota_dados(achada)) registrar("[OPENJAZZ] %s no parece tener los datos del juego", achada);
    } else if (!buscar_dados() && !janela_sem_dados()) {
        return 1;
    }
    registrar("[OPENJAZZ] datos en %s (%s)", achada,
              nota_dados(achada) == 2 ? "juego completo" : "shareware");

    sagui_sdl_nome = "OPENJAZZ";
    sagui_sdl_titulo = "OpenJazz";
    sagui_sdl_escala = 2;
    sagui_sdl_ao_mudar_cheia = ao_mudar_cheia;
    if (cheia) {
        sagui_cheia_inicial = 1;
        args[n++] = (char *)"--fullscreen";
    }
    /* La carpeta de los datos va la última, detrás de «--» (la sintaxis del motor). */
    args[n++] = (char *)"--";
    args[n++] = achada;
    args[n] = NULL;
    int r = main(n, args);
    registrar("[OPENJAZZ] fin (%d)", r);
    fflush(NULL);
    return r;
}

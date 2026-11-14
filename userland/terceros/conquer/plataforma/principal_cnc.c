/* principal_cnc.c - el punto de entrada de Command & Conquer (Vanilla Conquer) en el SaguiOS (D230).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv)
 * (userland/libsagui/inicio.asm, que antes llama a los constructores
 * globales de C++). El motor (código del upstream, sin tocar) tiene un main()
 * de verdad (tiberiandawn/startup.cpp); este fichero hace de puente y de
 * lanzador, como el de OpenJazz (D228):
 *
 *   - busca los datos de Command & Conquer (los .MIX del juego original, que
 *     no son libres) en la partición de juegos: /jogos/CnC (lo que saca
 *     tools/cnc_extraer.py de las ISO de input_share/games/CnC/) y cualquier
 *     otra carpeta de /jogos, y en jogos/... de cada disco montado. Hacen
 *     falta CONQUER.MIX y las misiones (GENERAL.MIX, en la carpeta o en sus
 *     subcarpetas gdi/ y nod/, las de los dos discos);
 *   - si no los encuentra, abre una ventana que explica cómo conseguirlos,
 *     con «Buscar otra vez» (por si se acaba de montar un pendrive);
 *   - la configuración (CONQUER.INI), las partidas y el registro del motor
 *     (vanillatd.log) van a /home/cnc (paths_sagui.cpp). La primera vez se
 *     escribe un CONQUER.INI con lo que conviene en el SaguiOS: en ventana,
 *     sin el recuadro propio del motor (la SDL 2 del sistema ya guarda la
 *     proporción), con el ratón absoluto (no hay ratón relativo) y sin la
 *     película de entrada (se llega al menú principal).
 *
 * Opciones propias (se quitan antes de llamar al motor):
 *   --pantalla-completa   la ventana nace a pantalla completa (D204)
 *   --datos CARPETA       usar esa carpeta de datos (sin buscar)
 *
 * Compilado con -DSAGUI_RED_ALERT es el lanzador de Red Alert (/apps/bin/redalert,
 * redalert/startup.cpp): busca REDALERT.MIX en /jogos/RA, guarda en /home/redalert
 * (REDALERT.INI) y sus discos son allied/ y soviet/ (con MAIN.MIX).
 */
#include "sagui.h"
#include "gui.h"
#include "SDL.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]);   /* el main del motor (tiberiandawn/startup.cpp o redalert/startup.cpp) */

extern const char *sagui_cnc_dados;     /* paths_sagui.cpp */
extern const char *sagui_cnc_usuario;

#define PATH_CNC 200
#ifdef SAGUI_RED_ALERT
#define JOGO_NOME     "Red Alert"
#define JOGO_REG      "RA"               /* en el registro: [RA] */
#define JOGO_BIN      "/apps/bin/redalert"
#define CONFIG_CNC    "/home/redalert"
#define PASTA_PADRAO  "RA"
#define MIX_PRINCIPAL "REDALERT.MIX"
#define MIX_MISSOES   "MAIN.MIX"
#define DISCO_1       "allied"
#define DISCO_2       "soviet"
#define INI_NOME      "REDALERT.INI"
#define REGISTRO_NOME "vanillara.log"
#else
#define JOGO_NOME     "Command & Conquer"
#define JOGO_REG      "CNC"
#define JOGO_BIN      "/apps/bin/conquer"
#define CONFIG_CNC    "/home/cnc"
#define PASTA_PADRAO  "CnC"
#define MIX_PRINCIPAL "CONQUER.MIX"
#define MIX_MISSOES   "GENERAL.MIX"
#define DISCO_1       "gdi"
#define DISCO_2       "nod"
#define INI_NOME      "CONQUER.INI"
#define REGISTRO_NOME "vanillatd.log"
#endif

static char achada[PATH_CNC];

static int existe(const char *pasta, const char *nome)
{
    char c[PATH_CNC];
    info_arquivo_t inf;
    caminho_juntar(c, sizeof c, pasta, nome);
    return info_arquivo(c, &inf) >= 0;
}

/* El motor busca los ficheros sin distinguir mayúsculas; aquí, las dos formas
 * habituales (como en la ISO y en minúsculas). */
static int existe_mix(const char *pasta, const char *maius)
{
    char minus[32];
    size_t i = 0;
    for (; maius[i] && i < sizeof minus - 1; i++) minus[i] = (char)(maius[i] >= 'A' && maius[i] <= 'Z' ? maius[i] + 32 : maius[i]);
    minus[i] = 0;
    return existe(pasta, maius) || existe(pasta, minus);
}

static int tem_missoes(const char *pasta, const char *disco)
{
    char c[PATH_CNC];
    caminho_juntar(c, sizeof c, pasta, disco);
    return existe_mix(c, MIX_MISSOES);
}

/* ¿Hay datos del juego en esa carpeta? 0 = no; 1 = un disco (o la carpeta
 * de un juego instalado); 2 = los dos discos (gdi/ y nod/; allied/ y soviet/). */
static int nota_dados(const char *pasta)
{
    if (!existe_mix(pasta, MIX_PRINCIPAL)) return 0;
    int d1 = tem_missoes(pasta, DISCO_1), d2 = tem_missoes(pasta, DISCO_2);
    if (d1 && d2) return 2;
    if (d1 || d2 || existe_mix(pasta, MIX_MISSOES)) return 1;
    return 0;
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

/* Mira «base/CnC» (o «base/RA») y cada subcarpeta de «base». */
static void procurar_em(const char *base)
{
    char c[PATH_CNC];
    caminho_juntar(c, sizeof c, base, PASTA_PADRAO);
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
        char c[PATH_CNC];
        if (!d.ponto[0] || strcmp(d.ponto, "/jogos") == 0) continue;
        caminho_juntar(c, sizeof c, d.ponto, "jogos");
        procurar_em(c);
    }
    return melhor_nota > 0;
}

/* La ventana de «faltan los datos». 1 = se encontraron al buscar de nuevo. */
static int janela_sem_dados(void)
{
#ifdef SAGUI_RED_ALERT
    static const char *const linhas[] = {
        "No se encuentran los datos de Red Alert.",
        "",
        "Vanilla Conquer es el motor libre del juego; los gráficos, los sonidos y las",
        "misiones son los del juego original de Westwood, que no son libres y no vienen",
        "con el SaguiOS. Sirve el Red Alert para Windows 95 que EA regaló en 2008:",
        "",
        "  - Copie a /jogos/RA (o a jogos/RA de un pendrive) la carpeta de un Red Alert",
        "    instalado: REDALERT.MIX y, de cada disco, MAIN.MIX en allied/ y soviet/.",
        "  - O al compilar el sistema, póngala en input_share/games/RA/",
        "    (docs/input_share.md).",
        "",
        "Cuando los datos estén a mano, pulse «Buscar otra vez».",
    };
#else
    static const char *const linhas[] = {
        "No se encuentran los datos de Command & Conquer.",
        "",
        "Vanilla Conquer es el motor libre del juego; los gráficos, los sonidos y las",
        "misiones son los del juego original de Westwood, que no son libres y no vienen",
        "con el SaguiOS. Sirve la edición C&C Gold para Windows 95, que EA regaló:",
        "",
        "  - Al compilar el sistema: ponga las dos ISO (GDI95 y NOD95) en",
        "    input_share/games/CnC/; tools/cnc_extraer.py saca lo necesario a /jogos/CnC.",
        "  - O copie a /jogos/CnC (o a jogos/CnC de un pendrive) la carpeta de un C&C95",
        "    instalado: CONQUER.MIX, GENERAL.MIX, UPDATE.MIX... (docs/input_share.md).",
        "",
        "Cuando los datos estén a mano, pulse «Buscar otra vez».",
    };
#endif
    jan_t *j = jan_abrir(JOGO_NOME ": faltan los datos", 660, 290);
    if (!j) return 0;
    botao_t buscar = { 180, 244, 140, 28, "Buscar otra vez", false };
    botao_t cerrar = { 350, 244, 110, 28, "Cerrar", false };
    registrar("[" JOGO_REG "] faltan los datos: ventana de ayuda");
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
            registrar("[" JOGO_REG "] buscar otra vez: %s", achou ? "encontrados" : "sin datos");
            if (achou) break;
            falhou = 1;
        }
    }
    jan_fechar(j);
    return achou;
}

/* El CONQUER.INI de la primera vez. El motor lo lee de /home/cnc (la ruta
 * del usuario va la primera) y lo vuelve a escribir al salir. */
static void config_inicial(void)
{
    char c[PATH_CNC];
    caminho_juntar(c, sizeof c, CONFIG_CNC, INI_NOME);
    if (existe_mix(CONFIG_CNC, INI_NOME)) return;
    FILE *f = fopen(c, "w");
    if (!f) {
        registrar("[" JOGO_REG "] no se puede escribir %s", c);
        return;
    }
    fputs("; " JOGO_NOME " (Vanilla Conquer) en SaguiOS: la configuracion inicial.\n"
          "; El juego la vuelve a escribir al salir con lo que se cambie en sus opciones.\n"
          "\n"
          "[Video]\n"
          "; En ventana (F11 o Alt+Intro: pantalla completa, con la proporcion 4:3)\n"
          "Windowed=yes\n"
          "WindowWidth=640\n"
          "WindowHeight=400\n"
          "; El recuadro del motor, no: la SDL 2 del SaguiOS ya guarda la proporcion\n"
          "Boxing=no\n"
          "HardwareCursor=no\n"
          "; 60 cuadros por segundo (el motor trae 120): la mitad de trabajo de copia\n"
          "FrameLimit=60\n"
          "\n"
          "[Mouse]\n"
          "; El SaguiOS da la posicion absoluta del raton (no hay raton relativo)\n"
          "RawInput=no\n"
          "\n"
          "[Language]\n"
          "Language=ENG\n"
          "\n"
          "[Intro]\n"
          "; Sin la pelicula de entrada (ademas, sin MOVIES.MIX no hay peliculas):\n"
          "; al menu principal (\"Intro & Sneak Peek\" la pone)\n"
          "PlayIntro=no\n", f);
    fclose(f);
    registrar("[" JOGO_REG "] configuracion inicial en %s", c);
}

/* La carpeta de la configuración: se crea si falta y se entra en ella (el
 * motor abre algunos ficheros relativos a la carpeta actual). Es un
 * constructor con prioridad: corre antes que los constructores globales de
 * C++ del motor (los de .init_array sin prioridad van detrás). */
__attribute__((constructor(101))) static void entrar_em_config(void)
{
    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) < 0) criar_pasta("/home");
    if (info_arquivo(CONFIG_CNC, &inf) < 0) criar_pasta(CONFIG_CNC);
    if (mudar_pasta(CONFIG_CNC) < 0)
        registrar("[" JOGO_REG "] no se puede entrar en %s: la configuracion no se guardara", CONFIG_CNC);   /* el registro, en ASCII */
}

/* El motor sale con exit() (al cerrar la ventana, SDL_QUIT): el aviso del
 * final, en atexit. */
static void ao_sair(void)
{
    registrar("[" JOGO_REG "] fin");
}

int principal(int argc, char **argv)
{
    static char *args[64];
    const char *datos = NULL;
    int n = 0, cheia = 0;
    args[n++] = (char *)JOGO_BIN;
    for (int i = 1; i < argc && n < 60; i++) {
        if (strcmp(argv[i], "--pantalla-completa") == 0) { cheia = 1; continue; }
        if (strcmp(argv[i], "--sdl-medir") == 0) { sagui_sdl2_medir = 1; continue; }   /* D247 */
        if (strcmp(argv[i], "--datos") == 0 && i + 1 < argc) { datos = argv[++i]; continue; }
        args[n++] = argv[i];
    }
    args[n] = NULL;

    if (datos) {
        strncpy(achada, datos, sizeof achada - 1);
        if (!nota_dados(achada)) registrar("[" JOGO_REG "] %s no parece tener los datos del juego", achada);
    } else if (!buscar_dados() && !janela_sem_dados()) {
        return 1;
    }
    int nota = nota_dados(achada);
    registrar("[" JOGO_REG "] datos en %s (%s)", achada,
              nota == 2 ? "los dos discos" : nota == 1 ? "un disco" : "incompletos");

    config_inicial();
    sagui_cnc_dados = achada;
    sagui_cnc_usuario = CONFIG_CNC;

    /* El registro del motor (DBG_INFO y compañía van a stderr). */
    char reg[PATH_CNC];
    caminho_juntar(reg, sizeof reg, CONFIG_CNC, REGISTRO_NOME);
    if (!freopen(reg, "w", stderr)) registrar("[" JOGO_REG "] no se puede abrir %s", reg);

    sagui_sdl2_nome = JOGO_REG;
    sagui_sdl2_escala = 1;
    if (cheia) sagui_sdl2_cheia_inicial = 1;

    registrar("[" JOGO_REG "] arranca Vanilla Conquer");
    atexit(ao_sair);
    int r = main(n, args);
    registrar("[" JOGO_REG "] el motor vuelve (%d)", r);
    fflush(NULL);
    return r;
}

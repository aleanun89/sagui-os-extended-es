/* principal_ot.c - el punto de entrada del OpenTyrian en el SaguiOS (D156, D180).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv) (userland/
 * libsagui/inicio.asm). El motor del juego (que no se toca) tiene un main() de
 * verdad, así que este fichero hace de puente. Además es el lanzador (D180):
 *
 *   - busca los datos del juego (tyrian1.lvl) en /jogos/opentyrian (donde los
 *     deja el instalador a disco duro) y en jogos/opentyrian de CADA disco
 *     montado (/usb, /discos/..., lo que sea), y se los indica al motor con
 *     custom_data_dir, sin tocar el código del upstream;
 *   - si no los encuentra, en vez de fallar abre una ventana propia que explica
 *     qué falta y cómo conseguirlo, con un botón «Buscar otra vez» (por si se
 *     acaba de montar el pendrive). */
#include "sagui.h"
#include "gui.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[]);   /* el main del OpenTyrian */

extern const char *custom_data_dir;
extern bool sagui_cheia_inicial;      /* plataforma/sdl_sagui.c: la primera ventana nace a pantalla completa (D204) */   /* jogo/file.c: la primera carpeta que prueba el motor */

#define SUBPASTA "jogos/opentyrian"
#define PATH_OT 160

static char achada[PATH_OT];

/* ¿Están los datos del juego en esa carpeta? */
static bool tem_dados(const char *pasta)
{
    char c[PATH_OT];
    info_arquivo_t inf;
    caminho_juntar(c, sizeof c, pasta, "tyrian1.lvl");
    return info_arquivo(c, &inf) >= 0;
}

/* Busca los datos en el sistema y en todos los discos montados; deja la
 * carpeta en «achada». */
static bool buscar_dados(void)
{
    if (tem_dados("/jogos/opentyrian")) {
        strncpy(achada, "/jogos/opentyrian", sizeof achada);
        return true;
    }
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        if (!d.ponto[0]) continue;
        caminho_juntar(achada, sizeof achada, d.ponto, SUBPASTA);
        if (tem_dados(achada)) return true;
    }
    return false;
}

/* La ventana de «faltan los datos». true = se encontraron al buscar de nuevo. */
static bool janela_sem_dados(void)
{
    static const char *const linhas[] = {
        "No se encuentran los datos de Tyrian.",
        "",
        "El juego necesita los ficheros de Tyrian 2.1 (la versión shareware es",
        "gratuita) en una carpeta llamada jogos/opentyrian, en el sistema o en",
        "un disco montado. Para conseguirlos:",
        "",
        "  - En el anfitrión, ejecute tools/imagem_tyrian.sh: arma un disco con",
        "    los datos que se conecta como pendrive.",
        "  - O copie la carpeta opentyrian a un pendrive, dentro de jogos/",
        "    (jogos/opentyrian/tyrian1.lvl...), y móntelo (orden montar).",
        "",
        "Cuando los datos estén a mano, pulse «Buscar otra vez».",
    };
    jan_t *j = jan_abrir("OpenTyrian: faltan los datos", 640, 290);
    if (!j) return false;
    botao_t buscar = { 170, 244, 140, 28, "Buscar otra vez", false };
    botao_t cerrar = { 340, 244, 110, 28, "Cerrar", false };
    registrar("[OPENTYRIAN] faltan los datos: ventana de ayuda");
    bool achou = false, falhou = false;
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
        if (botao_tratar(j, &buscar, &e) || (e.tipo == EV_TECLA && e.tecla == 10)) {
            achou = buscar_dados();
            registrar("[OPENTYRIAN] buscar otra vez: %s", achou ? "encontrados" : "sin datos");
            if (achou) break;
            falhou = true;
        }
    }
    jan_fechar(j);
    return achou;
}

/* D183 y D186: el motor del OpenTyrian usa más de 64 KB de pila al empezar
 * una partida. Hasta la D186 el main del juego corría aquí sobre una pila
 * propia de 1 MB del heap; desde la D186 la pila de todo programa crece bajo
 * demanda hasta 8 MB y el juego corre sobre la suya. */

int principal(int argc, char **argv)
{
    bool com_dados = false;
    /* D204: --pantalla-completa (o -f, --fullscreen) es nuestro: se quita de la
     * lista antes de dársela al motor, que no lo conoce y protestaría. */
    int n = 1;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--pantalla-completa") == 0 || strcmp(argv[i], "--fullscreen") == 0
            || strcmp(argv[i], "-f") == 0) { sagui_cheia_inicial = true; continue; }
        argv[n++] = argv[i];
    }
    argv[n] = NULL;
    argc = n;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--data") == 0 || strcmp(argv[i], "--ayuda") == 0
            || strcmp(argv[i], "--help") == 0) com_dados = true;   /* el usuario manda: sin búsqueda */
    if (!com_dados) {
        if (!buscar_dados() && !janela_sem_dados()) return 1;
        registrar("[OPENTYRIAN] datos en %s", achada);
        custom_data_dir = achada;
    }
    return main(argc, argv);
}

/* D204, D210: el motor guarda su configuración y las partidas en «./» (no hay
 * directorio actual en el SaguiOS: sería la raíz). Se redirigen a /home, que es
 * donde se puede escribir y se conserva; si no se puede (no hay /home), se usa
 * la ruta de siempre. Con la libc común (D210) esto ya no está en su fopen: el
 * enlace lleva --wrap=fopen y las llamadas a fopen() del motor llegan aquí. */
typedef struct ns_ficheiro FILE;
FILE *__real_fopen(const char *caminho, const char *modo);

FILE *__wrap_fopen(const char *caminho, const char *modo)
{
    if (caminho && caminho[0] == '.' && caminho[1] == '/' && strlen(caminho) < 200) {
        char alt[220];
        strcpy(alt, "/home/");
        strcat(alt, caminho + 2);
        FILE *f = __real_fopen(alt, modo);
        if (f) return f;
    }
    return __real_fopen(caminho, modo);
}

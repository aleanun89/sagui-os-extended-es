/* principal_quake.c - el lanzador de Quake en el SaguiOS (D229).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv). Este fichero
 * hace de lanzador, como el de OpenJazz (D228) y el de OpenTyrian (D180):
 *
 *   - busca los datos del juego (los .pak de id Software, que no son
 *     libres) en la partición de juegos: cada carpeta de /jogos (lo que el
 *     que compila puso en input_share/games/, D228, o el shareware que baja
 *     tools/quake_datos.sh) y jogos/<carpeta> de cada disco montado. Sirve
 *     «<carpeta>/id1/pak0.pak» (la estructura de Quake) o el pak0.pak suelto
 *     en la carpeta; sin distinguir mayúsculas (el shareware de DOS trae
 *     ID1/PAK0.PAK). Si hay varias, gana la versión registrada (con
 *     pak1.pak) sobre la shareware;
 *   - si no los encuentra, abre una ventana que explica cómo conseguirlos,
 *     con «Buscar otra vez» (por si se acaba de montar un pendrive);
 *   - lo que escribe el juego (config.cfg, las partidas, las demos y las
 *     capturas) va a /home/quake/id1: el motor recibe /home/quake como su
 *     carpeta base y arquivos_quake.c pone los datos debajo;
 *   - el error fatal del motor (Sys_Error) se enseña en una ventana.
 *
 * Opciones propias (se quitan antes de pasar el resto al motor, que admite
 * las suyas: -nosound, +map e1m1, -mem 16...):
 *   --pantalla-completa   la ventana nace a pantalla completa (D204)
 *   --640x480             dibuja a 640x480 en vez de 320x200
 *   --datos CARPETA       usar esa carpeta de datos (sin buscar)
 */
#include "sagui.h"
#include "gui.h"
#include "SDL.h"
#include "arquivos_quake.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#define PATH_Q 200

const char *quake_raiz = "/home/quake";

static char achada[PATH_Q];       /* la carpeta del juego: /jogos/Quake */
static char achada_id1[PATH_Q];   /* la de los .pak: /jogos/Quake/ID1, o la misma */
static int  melhor_nota;

/* El hijo «nome» de «pasta» sin distinguir mayúsculas; 1 = está (en «saida»). */
static int filho(const char *pasta, const char *nome, char *saida, size_t max)
{
    info_arquivo_t inf;
    caminho_juntar(saida, max, pasta, nome);
    if (info_arquivo(saida, &inf) >= 0) return 1;
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) > 0 && i < 512; i++) {
        if (strcasecmp(e.nome, nome) == 0) {
            caminho_juntar(saida, max, pasta, e.nome);
            return 1;
        }
    }
    return 0;
}

/* ¿Están los paquetes de Quake en «pasta» (la del id1)? 0 = no; 1 = el
 * shareware (solo pak0.pak); 2 = la versión registrada (pak0 y pak1). */
static int nota_pak(const char *pasta)
{
    char c[PATH_Q];
    if (!filho(pasta, "pak0.pak", c, sizeof c)) return 0;
    return filho(pasta, "pak1.pak", c, sizeof c) ? 2 : 1;
}

/* Mira una carpeta candidata: con id1/ dentro o con el pak0.pak suelto. */
static void considerar(const char *pasta)
{
    char id1[PATH_Q];
    const char *onde = pasta;
    if (filho(pasta, "id1", id1, sizeof id1)) onde = id1;
    int n = nota_pak(onde);
    if (n > melhor_nota) {
        melhor_nota = n;
        strncpy(achada, pasta, sizeof achada - 1);
        achada[sizeof achada - 1] = 0;
        strncpy(achada_id1, onde, sizeof achada_id1 - 1);
        achada_id1[sizeof achada_id1 - 1] = 0;
    }
}

/* Cada subcarpeta de «base». */
static void procurar_em(const char *base)
{
    char c[PATH_Q];
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
    achada[0] = achada_id1[0] = 0;
    procurar_em("/jogos");
    info_disco_t d;
    for (int i = 0; listar_discos(i, &d) == 1; i++) {
        char c[PATH_Q];
        if (!d.ponto[0] || strcmp(d.ponto, "/jogos") == 0) continue;
        caminho_juntar(c, sizeof c, d.ponto, "jogos");
        procurar_em(c);
    }
    return melhor_nota > 0;
}

/* Una ventana con unas líneas y botones; devuelve el botón elegido
 * (0 = el primero... ; -1 = cerrada o Esc). */
static int janela_texto(const char *titulo, const char *const *linhas, int n, int destaque,
                        const char *botao1, const char *botao2, const char *aviso)
{
    int alto = 60 + n * 18 + 50;
    jan_t *j = jan_abrir(titulo, 640, alto);
    if (!j) return -1;
    int by = alto - 46;
    botao_t b1 = { botao2 ? 170 : 265, by, 140, 28, botao1, false };
    botao_t b2 = { 340, by, 110, 28, botao2 ? botao2 : "", false };
    int r = -1;
    for (;;) {
        g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        for (int i = 0; i < n; i++)
            rotulo(j, 16, 14 + i * 18, linhas[i], i < destaque ? COR_DESTAQUE : COR_TEXTO);
        if (aviso) rotulo(j, 16, by - 22, aviso, COR_TEXTO_FRACO);
        botao_desenhar(j, &b1);
        if (botao2) botao_desenhar(j, &b2);
        jan_mostrar(j);
        evento_t e;
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) break;
        if (botao_tratar(j, &b1, &e) || (e.tipo == EV_TECLA && e.tecla == 10)) { r = 0; break; }
        if (botao2 && botao_tratar(j, &b2, &e)) { r = 1; break; }
    }
    jan_fechar(j);
    return r;
}

/* La ventana de «faltan los datos». 1 = se encontraron al buscar de nuevo. */
static int janela_sem_dados(void)
{
    static const char *const linhas[] = {
        "No se encuentran los datos de Quake (id1/pak0.pak).",
        "",
        "El motor de Quake es libre (GPL); los mapas, los gráficos y los sonidos son",
        "de id Software, no son libres y no vienen con el SaguiOS. Sirven el shareware",
        "(el episodio 1) o el juego completo:",
        "",
        "  - Copie la carpeta del juego (con id1/pak0.pak, o el pak0.pak suelto) a",
        "    /jogos, por ejemplo a /jogos/Quake, o a jogos/ de un pendrive montado.",
        "  - Al compilar el sistema: tools/quake_datos.sh baja el shareware, o",
        "    ponga el juego en input_share/games/Quake/ (docs/input_share.md).",
        "",
        "Cuando los datos estén a mano, pulse «Buscar otra vez».",
    };
    registrar("[QUAKE] faltan los datos: ventana de ayuda");
    const char *aviso = NULL;
    for (;;) {
        int r = janela_texto("Quake: faltan los datos", linhas, (int)(sizeof linhas / sizeof linhas[0]), 1,
                             "Buscar otra vez", "Cerrar", aviso);
        if (r != 0) return 0;
        int achou = buscar_dados();
        registrar("[QUAKE] buscar otra vez: %s", achou ? "encontrados" : "sin datos");
        if (achou) return 1;
        aviso = "Todavía no están: compruebe la carpeta y el montaje.";
    }
}

/* El error fatal del motor: la ventana del juego se cierra y otra explica el error. */
void quake_janela_erro(const char *mensagem)
{
    const char *linhas[4];
    char m1[80], m2[80];
    size_t l = strlen(mensagem);
    /* El mensaje en dos líneas de 76 letras como mucho (y en ASCII, como el registro). */
    snprintf(m1, sizeof m1, "%.76s", mensagem);
    snprintf(m2, sizeof m2, "%.76s", l > 76 ? mensagem + 76 : "");
    linhas[0] = "Quake se ha detenido por un error:";
    linhas[1] = "";
    linhas[2] = m1;
    linhas[3] = m2;
    SDL_Quit();
    janela_texto("Quake: error", linhas, 4, 1, "Cerrar", NULL, NULL);
}

static int criar_se_falta(const char *c)
{
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) >= 0) return 0;
    int r = criar_pasta(c);
    if (r < 0 && info_arquivo(c, &inf) >= 0) r = 0;   /* ya existía */
    if (r < 0) registrar("[QUAKE] criar_pasta(%s) = %d", c, r);
    return r;
}

static void preparar_pasta_usuario(void)
{
    char c[PATH_Q];
    caminho_juntar(c, sizeof c, quake_raiz, "id1");
    /* Sin partición de /home (la imagen de pruebas, el CD), /home no existe
     * en el ramdisk hasta que alguien la crea. */
    if (criar_se_falta("/home") < 0 || criar_se_falta(quake_raiz) < 0 || criar_se_falta(c) < 0)
        registrar("[QUAKE] no se puede crear %s: la configuracion y las partidas no se guardaran", c);
    mudar_pasta(quake_raiz);
}

int principal(int argc, char **argv)
{
    /* La x87 ya llega a 53 bits (D236, el estado limpio de la FPU del kernel):
     * la libm x87 lo necesita. Con los 64 bits del FNINIT, floor() y ceil()
     * (el truco de sumar y restar 2^52) devolvían el número sin redondear y
     * el primer cuadro de un mapa acababa en «D_SCAlloc: bad cache size 0» (D229). */

    static char *args[64];
    const char *datos = NULL;
    int n = 0, cheia = 0, alta = 0;
    args[n++] = (char *)"quake";
    for (int i = 1; i < argc && n < 56; i++) {
        if (strcmp(argv[i], "--pantalla-completa") == 0) { cheia = 1; continue; }
        if (strcmp(argv[i], "--640x480") == 0) { alta = 1; continue; }
        if (strcmp(argv[i], "--datos") == 0 && i + 1 < argc) { datos = argv[++i]; continue; }
        args[n++] = argv[i];
    }

    if (datos) {
        melhor_nota = 0;
        achada[0] = 0;
        considerar(datos);
        if (!melhor_nota) {
            registrar("[QUAKE] %s no parece tener los datos del juego", datos);
            strncpy(achada, datos, sizeof achada - 1);
            strncpy(achada_id1, datos, sizeof achada_id1 - 1);
        }
    } else if (!buscar_dados() && !janela_sem_dados()) {
        return 1;
    }
    registrar("[QUAKE] datos en %s (%s; paquetes en %s)", achada,
              melhor_nota == 2 ? "version registrada" : "shareware", achada_id1);

    preparar_pasta_usuario();
    quake_arquivos_iniciar(quake_raiz, achada, achada_id1);

    sagui_sdl_nome = "QUAKE";
    sagui_sdl_titulo = "Quake";
    if (cheia) {
        sagui_cheia_inicial = 1;
        args[n++] = (char *)"-fullscreen";
    }
    if (alta) {
        args[n++] = (char *)"-width";
        args[n++] = (char *)"640";
        args[n++] = (char *)"-height";
        args[n++] = (char *)"480";
    }
    args[n] = NULL;
    quake_correr(n, args);   /* no vuelve: Sys_Quit y Sys_Error salen con exit */
    return 0;
}

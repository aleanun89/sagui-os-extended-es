/* principal_dosbox.c - el punto de entrada de DOSBox en el SaguiOS (D294).
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv) (userland/libsagui/inicio.asm, que
 * antes llama a los constructores globales de C++). DOSBox (código del upstream con unos parches)
 * tiene un main() de verdad; este fichero hace de puente y de lanzador, como los de OpenJazz y
 * Quake:
 *
 *   - crea /home/.config y /home/dos si faltan y, si no hay /home/.config/dosbox.conf, lo escribe
 *     con los valores del sistema (núcleo auto: el recompilador dinámico donde se puede; Sound
 *     Blaster 16, AdLib y altavoz; la salida «surface», que la SDL del SaguiOS escala a la
 *     ventana; /home/dos montado como C:). Un fichero que ya existe no se toca;
 *   - la configuración, el mapa de teclas y las capturas van a /home/.config y /home/dos (el
 *     motor los busca en la carpeta actual o en $HOME: aquí se entra en /home/.config y se
 *     fija HOME);
 *   - un fichero .exe, .com o .bat (lo que pasa Archivos) se convierte en órdenes de DOSBox: se
 *     monta su carpeta como C:, se entra en ella y se ejecuta; al terminar, DOSBox se cierra
 *     (el programa se queda en la ventana con «--quedarse»).
 *
 * Opciones propias (se quitan antes de llamar al motor):
 *   --pantalla-completa   la ventana nace a pantalla completa (D204)
 *   --quedarse            no cerrar DOSBox al terminar el programa
 * Las demás son las de DOSBox (-conf, -c, -noautoexec, -exit, -fullscreen...).
 */
#include "sagui.h"
#include "gui.h"
#include "SDL.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[]);   /* el main de DOSBox (src/gui/sdlmain.cpp) */

#define DIR_HOME   "/home"
#define DIR_CONF   "/home/.config"
#define CONF_DB    "/home/.config/dosbox.conf"
#define DIR_DOS    "/home/dos"
#define MAX_ARGS   96

/* La configuración por defecto: solo lo que cambia respecto a la de fábrica de DOSBox. */
static const char *const conf_inicial =
    "# dosbox.conf del SaguiOS (D294). Solo lo que difiere de DOSBox 0.74; el resto, por defecto.\n"
    "# Más en /apps/dosbox/leeme.txt y docs/dosbox.md. DOSBox lo reescribe con «config -writeconf».\n"
    "\n"
    "[sdl]\n"
    "fullscreen=false\n"
    "output=surface\n"
    "autolock=true\n"
    "sensitivity=100\n"
    "waitonerror=true\n"
    "usescancodes=false\n"
    "mapperfile=/home/.config/dosbox-mapper.txt\n"
    "\n"
    "[dosbox]\n"
    "language=\n"
    "machine=svga_s3\n"
    "captures=/home/dos/capturas\n"
    "memsize=16\n"
    "\n"
    "[render]\n"
    "frameskip=0\n"
    "aspect=false\n"
    "scaler=normal2x\n"
    "\n"
    "[cpu]\n"
    "core=auto\n"
    "cputype=auto\n"
    "cycles=auto\n"
    "cycleup=10\n"
    "cycledown=20\n"
    "\n"
    "[mixer]\n"
    "nosound=false\n"
    "rate=44100\n"
    "blocksize=1024\n"
    "prebuffer=40\n"
    "\n"
    "[midi]\n"
    "mpu401=intelligent\n"
    "mididevice=default\n"
    "\n"
    "[sblaster]\n"
    "sbtype=sb16\n"
    "sbbase=220\n"
    "irq=7\n"
    "dma=1\n"
    "hdma=5\n"
    "sbmixer=true\n"
    "oplmode=auto\n"
    "oplemu=default\n"
    "oplrate=44100\n"
    "\n"
    "[gus]\n"
    "gus=false\n"
    "\n"
    "[speaker]\n"
    "pcspeaker=true\n"
    "pcrate=44100\n"
    "tandy=auto\n"
    "disney=true\n"
    "\n"
    "[joystick]\n"
    "# D307: «auto» usa el primer mando del sistema (USB, Xbox 360 o puerto de juegos) como el\n"
    "# joystick del PC (4 ejes con uno; 2 joysticks de 2 ejes con dos). «none» lo apaga.\n"
    "joysticktype=auto\n"
    "timed=true\n"
    "\n"
    "[serial]\n"
    "serial1=dummy\n"
    "serial2=disabled\n"
    "serial3=disabled\n"
    "serial4=disabled\n"
    "\n"
    "[dos]\n"
    "xms=true\n"
    "ems=true\n"
    "umb=true\n"
    "keyboardlayout=auto\n"
    "\n"
    "[ipx]\n"
    "ipx=false\n"
    "\n"
    "[autoexec]\n"
    "# Estas líneas se ejecutan al arrancar DOSBox sin programa.\n"
    "mount c /home/dos\n"
    "c:\n";

static int existe(const char *ruta)
{
    info_arquivo_t inf;
    return info_arquivo(ruta, &inf) >= 0;
}

static int e_pasta(const char *ruta)
{
    info_arquivo_t inf;
    return info_arquivo(ruta, &inf) >= 0 && inf.tipo == TIPO_PASTA;
}

/* ¿La ruta acaba en .exe, .com o .bat (sin mirar mayúsculas)? */
static int e_programa_dos(const char *ruta)
{
    size_t n = strlen(ruta);
    if (n < 5 || ruta[n - 4] != '.') return 0;
    char e[4];
    for (int i = 0; i < 3; i++) e[i] = (char)tolower((unsigned char)ruta[n - 3 + i]);
    e[3] = 0;
    return !strcmp(e, "exe") || !strcmp(e, "com") || !strcmp(e, "bat");
}

static void preparar_carpetas(void)
{
    if (!e_pasta(DIR_HOME)) criar_pasta(DIR_HOME);   /* sin partición de datos solo existe en la RAM */
    if (!e_pasta(DIR_CONF)) criar_pasta(DIR_CONF);
    if (!e_pasta(DIR_DOS)) criar_pasta(DIR_DOS);
    if (!existe(CONF_DB)) {
        FILE *f = fopen(CONF_DB, "w");
        if (f) {
            fputs(conf_inicial, f);
            fclose(f);
            registrar("[DOSBOX] creado %s", CONF_DB);
        } else {
            registrar("[DOSBOX] no se puede escribir %s: se usa la configuracion de fabrica", CONF_DB);
        }
    }
}

static void ao_mudar_cheia(int cheia)
{
    registrar("[DOSBOX] pantalla completa: %s", cheia ? "activada" : "desactivada");
}

int principal(int argc, char **argv)
{
    static char *args[MAX_ARGS];
    static char ordem_montar[320], ordem_pasta[320], ordem_prog[320], dir_prog[256];
    int n = 0, cheia = 0, quedar = 0, tem_conf = 0, tem_prog = 0, sem_autoexec = 0;
    const char *prog = NULL;

    preparar_carpetas();
    /* El motor guarda el mapa de teclas, la configuración... en la carpeta actual o en HOME. */
    setenv("HOME", DIR_CONF, 1);
    mudar_pasta(DIR_CONF);

    args[n++] = (char *)"dosbox";
    for (int i = 1; i < argc && n < MAX_ARGS - 12; i++) {
        if (!strcmp(argv[i], "--pantalla-completa")) { cheia = 1; continue; }
        if (!strcmp(argv[i], "--quedarse")) { quedar = 1; continue; }
        if (!strcmp(argv[i], "-conf")) tem_conf = 1;
        if (!strcmp(argv[i], "-noautoexec")) sem_autoexec = 1;
        if (argv[i][0] != '-' && !prog && e_programa_dos(argv[i])) { prog = argv[i]; tem_prog = 1; continue; }
        args[n++] = argv[i];
    }
    (void)sem_autoexec;
    if (!tem_conf && existe(CONF_DB)) {
        args[n++] = (char *)"-conf";
        args[n++] = (char *)CONF_DB;
    }
    if (tem_prog) {
        /* La carpeta del programa pasa a ser C:; luego se ejecuta y se cierra. */
        strncpy(dir_prog, prog, sizeof dir_prog - 1);
        char *barra = strrchr(dir_prog, '/');
        const char *nome = prog;
        if (barra) {
            nome = barra + 1;
            if (barra == dir_prog) strcpy(dir_prog, "/");
            else *barra = 0;
        } else {
            strcpy(dir_prog, ".");
        }
        snprintf(ordem_montar, sizeof ordem_montar, "mount c \"%s\"", dir_prog);
        snprintf(ordem_pasta, sizeof ordem_pasta, "c:");
        snprintf(ordem_prog, sizeof ordem_prog, "%s", nome);
        args[n++] = (char *)"-noautoexec";
        args[n++] = (char *)"-c"; args[n++] = ordem_montar;
        args[n++] = (char *)"-c"; args[n++] = ordem_pasta;
        args[n++] = (char *)"-c"; args[n++] = ordem_prog;
        if (!quedar) { args[n++] = (char *)"-c"; args[n++] = (char *)"exit"; }
        registrar("[DOSBOX] programa %s en %s", nome, dir_prog);
    }

    sagui_sdl_nome = "DOSBOX";
    sagui_sdl_titulo = "DOSBox";
    sagui_sdl_escala = 1;
    sagui_sdl_ao_mudar_cheia = ao_mudar_cheia;
    sagui_sdl_foco_ao_soltar = 1;   /* Ctrl+Alt suelta el ratón también para DOSBox */
    if (cheia) {
        /* La ventana nace a pantalla completa y DOSBox pide SDL_FULLSCREEN en cada modo de vídeo. */
        sagui_cheia_inicial = 1;
        args[n++] = (char *)"-fullscreen";
    }
    args[n] = NULL;
    int r = main(n, args);
    registrar("[DOSBOX] fin (%d)", r);
    fflush(NULL);
    return r;
}

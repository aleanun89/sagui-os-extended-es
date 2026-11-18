/* SPDX-License-Identifier: GPL-2.0-or-later */
/* principal_cd.c - punto de entrada de Chocolate Doom (y Heretic, Hexen, Strife) en SaguiOS (D383).
 *
 * Los programas empiezan en principal(argc, argv); el motor del upstream tiene main(). Este puente:
 *   - fija HOME y las rutas de los WAD (DOOMWADDIR/DOOMWADPATH): /jogos/DOOM (los WAD originales que el
 *     usuario puso en input_share/games/DOOM; solo imágenes privadas) y /apps/doom (Freedoom, libre);
 *   - elige el IWAD: gana el original; --freedoom fuerza Freedoom. Con CD_JUEGO distinto de 0 busca el WAD
 *     propio de Heretic, Hexen o Strife (no hay versión libre);
 *   - si no hay ningún WAD, abre un mensaje claro que dice dónde ponerlos (en vez de la salida de error);
 *   - la configuración va a $HOME/.config/chocolate-doom/ (Sagui_ConfigDir, usada por el parche del motor).
 */
#include "sagui.h"
#include "SDL.h"
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifndef CD_JUEGO
#define CD_JUEGO 0
#endif

int main(int argc, char *argv[]);

#define DIR_ORIGINALES "/jogos/DOOM"
#define DIR_FREEDOOM   "/apps/doom"

static const char *const nomes_doom[] = { "doom2.wad", "plutonia.wad", "tnt.wad", "doom.wad", "doom1.wad", NULL };
static const char *const nomes_freedoom[] = { "freedoom2.wad", "freedoom1.wad", NULL };
static const char *const nomes_heretic[] = { "heretic.wad", "heretic1.wad", NULL };
static const char *const nomes_hexen[] = { "hexen.wad", NULL };
static const char *const nomes_strife[] = { "strife1.wad", NULL };

/* Carpeta de configuración: $HOME/.config/<app>/ (con la barra final), creada si falta. */
char *Sagui_ConfigDir(const char *app)
{
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/home";
    char c[200];
    snprintf(c, sizeof c, "%s/.config", home);
    criar_pasta(c);
    snprintf(c, sizeof c, "%s/.config/%s", home, app);
    criar_pasta(c);
    size_t n = strlen(c);
    char *r = malloc(n + 2);
    if (!r) return NULL;
    memcpy(r, c, n);
    r[n] = '/';
    r[n + 1] = 0;
    return r;
}

/* Busca en la carpeta un fichero de la lista sin distinguir mayúsculas; deja la ruta en ruta. */
static int buscar(const char *dir, const char *const *nomes, char *ruta, size_t max)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    for (int i = 0; nomes[i]; i++) {
        rewinddir(d);
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcasecmp(e->d_name, nomes[i]) == 0) {
                snprintf(ruta, max, "%s/%s", dir, e->d_name);
                closedir(d);
                return 1;
            }
        }
    }
    closedir(d);
    return 0;
}

int principal(int argc, char **argv)
{
    static char *nargv[64];
    static char ruta[200];
    int forzar_libre = 0, tiene_iwad = 0, n = 0;

    if (!getenv("HOME")) setenv("HOME", "/home", 1);
    setenv("DOOMWADDIR", DIR_ORIGINALES, 0);
    setenv("DOOMWADPATH", DIR_ORIGINALES ":" DIR_FREEDOOM, 0);

    nargv[n++] = argv[0];
    for (int i = 1; i < argc && n < 60; i++) {
        if (strcmp(argv[i], "--freedoom") == 0) { forzar_libre = 1; continue; }
        if (strcmp(argv[i], "-iwad") == 0) tiene_iwad = 1;
        nargv[n++] = argv[i];
    }

    if (!tiene_iwad) {
        const char *const *lista = CD_JUEGO == 1 ? nomes_heretic : CD_JUEGO == 2 ? nomes_hexen :
                                   CD_JUEGO == 3 ? nomes_strife : nomes_doom;
        int hay = 0;
        if (CD_JUEGO == 0 && forzar_libre) {
            hay = buscar(DIR_FREEDOOM, nomes_freedoom, ruta, sizeof ruta);
        } else {
            hay = buscar(DIR_ORIGINALES, lista, ruta, sizeof ruta);
            if (!hay && CD_JUEGO == 0) hay = buscar(DIR_FREEDOOM, nomes_freedoom, ruta, sizeof ruta);
        }
        if (!hay) {
            SDL_Init(SDL_INIT_VIDEO);
            if (CD_JUEGO == 0)
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DOOM",
                    "No se encuentra ningun WAD de DOOM.\n\n"
                    "Freedoom (libre) va en /apps/doom, y los WAD originales (doom.wad, doom2.wad, plutonia.wad,\n"
                    "tnt.wad) se copian a /jogos/DOOM (en input_share/games/DOOM al compilar la imagen).", NULL);
            else
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Chocolate Doom",
                    "Este juego necesita su WAD original (heretic.wad, hexen.wad o strife1.wad),\n"
                    "que no es libre: copialo a /jogos/DOOM.", NULL);
            SDL_Quit();
            return 1;
        }
        nargv[n++] = "-iwad";
        nargv[n++] = ruta;
    }
    nargv[n] = NULL;
    return main(n, nargv);
}

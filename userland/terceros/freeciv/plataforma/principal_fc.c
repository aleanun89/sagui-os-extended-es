/* principal_fc.c - punto de entrada de Freeciv en SaguiOS (D374).
 * Los programas empiezan en principal(argc, argv); el servidor y el cliente del upstream tienen
 * main(). Este puente además fija el idioma (LANG, del idioma del sistema D313) y la carpeta de
 * datos antes de llamarlos. */
#include "sagui.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]);

int principal(int argc, char **argv)
{
    char lang[16];
    if (!getenv("LANG") && idioma_sistema_ler(lang, sizeof lang) < 0) strcpy(lang, "es"), setenv("LANG", lang, 1);
    else if (!getenv("LANG"))
        setenv("LANG", lang, 1);
    if (!getenv("HOME")) setenv("HOME", "/home", 1);
    if (!getenv("FREECIV_DATA_PATH")) setenv("FREECIV_DATA_PATH", "/apps/freeciv/data", 1);
    return main(argc, argv);
}

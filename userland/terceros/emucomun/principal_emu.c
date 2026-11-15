// SPDX-License-Identifier: GPL-2.0-or-later
/* principal_emu.c - el punto de entrada común de los emuladores de consolas (D321).
 *
 * Los programas del SaguiOS empiezan en principal(argc, argv) (userland/libsagui/inicio.asm, que antes
 * llama a los constructores de C++). Este fichero hace de puente con el main() del emulador:
 *   - quita --pantalla-completa (D204) y lo deja en sagui_emu_completa;
 *   - sin ROM explica cómo se usa (los emuladores se lanzan desde Recreativa o con «Abrir» de Archivos);
 *   - deja a mano registrar() para el código C++ (sagui_emu_registrar).
 */
#include "sagui.h"
#include "gui.h"

#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]);   /* el main del emulador */

int sagui_emu_completa;
extern const char *sagui_emu_nombre;   /* «LaiNES», «Gearsystem»: lo define cada emulador */

void sagui_emu_registrar(const char *texto) { registrar("%s", texto); }

int principal(int argc, char **argv)
{
    char *av[16];
    int n = 0;
    for (int i = 0; i < argc && n < 15; i++) {
        if (i > 0 && strcmp(argv[i], "--pantalla-completa") == 0) { sagui_emu_completa = 1; continue; }
        av[n++] = argv[i];
    }
    av[n] = NULL;
    if (n < 2) {
        mensagem(sagui_emu_nombre, "Uso: elija el juego desde Recreativa, o abra una ROM desde Archivos.\n\n"
                                   "En la consola: <emulador> [--pantalla-completa] ARCHIVO");
        return 1;
    }
    return main(n, av);
}

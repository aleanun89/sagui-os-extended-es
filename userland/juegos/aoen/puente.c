/* SPDX-License-Identifier: GPL-3.0-or-later */
/* puente.c - punto de entrada de AoEN en SaguiOS (D382).
 * Los programas de SaguiOS empiezan en principal(argc, argv); AoEN tiene main(). El puente fija
 * HOME (la configuración va a ~/.config/aoen) antes de llamarlo. */
#include <stdlib.h>
#include "sagui.h"

int main(int argc, char *argv[]);

int principal(int argc, char **argv)
{
    if (!getenv("HOME")) setenv("HOME", "/home", 1);
    return main(argc, argv);
}

/* Una línea al registro del sistema (el puerto serie en QEMU); las pruebas buscan las que empiezan por «[AOEN]». */
void aoen_registro(const char *texto)
{
    registrar_texto(texto);
}

// SPDX-License-Identifier: GPL-2.0-or-later
/* Una ventana con la libsagui: dibuja un degradado y espera a que la cierres. */
#include "sagui.h"
#include "gui.h"

int principal(int argc, char **argv)
{
    jan_t *v = jan_abrir("Ventana desde el Laboratorio", 320, 200);
    if (!v) return 1;
    for (int y = 0; y < 200; y++)
        g_retangulo(v, 0, y, 320, 1, 0x103060 + (y * 2) * 0x000100 + y);
    g_texto(v, 20, 20, "Hola desde mi ventana", 0xFFFFFF);
    jan_mostrar(v);
    evento_t e;
    for (;;) {
        if (esperar_evento(v->id, &e, 100) == 1 && e.tipo == EV_FECHAR) break;
    }
    jan_fechar(v);
    return 0;
}

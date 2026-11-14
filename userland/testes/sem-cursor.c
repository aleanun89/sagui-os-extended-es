// SPDX-License-Identifier: GPL-2.0-or-later
/* sem-cursor - prueba de la D230: una ventana que oculta el puntero del sistema
 * (JAN_SEM_CURSOR, jan_cursor_visivel). Pinta todo de un verde liso, para que
 * la prueba (h230 «cursor») vea si la flecha está dibujada encima o no.
 * Teclas: «v» puntero visible, «o» oculto, «m» abre un menú contextual (con
 * el menú abierto el puntero se ve), «q» o cerrar termina. Un clic escribe su
 * posición en el registro (para saber dónde está la flecha). */
#include "sagui.h"
#include "gui.h"

#define VERDE 0x3CDC6E

static void pintar(jan_t *j)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, VERDE);
    jan_mostrar(j);
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = jan_abrir("Sem cursor", 400, 300);
    if (!j) return 1;
    pintar(j);
    jan_cursor_visivel(j, false);
    registrar("sem-cursor: cursor oculto\n");
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 'q')) break;
        if (e.tipo == EV_TECLA && e.tecla == 'v') { jan_cursor_visivel(j, true); registrar("sem-cursor: cursor visivel\n"); }
        if (e.tipo == EV_TECLA && e.tecla == 'o') { jan_cursor_visivel(j, false); registrar("sem-cursor: cursor oculto\n"); }
        if (e.tipo == EV_TECLA && e.tecla == 'm') {
            menu_contextual(j->id, 10, 10, "Uno\nDos\nTres", 1);
            registrar("sem-cursor: menu aberto\n");
        }
        if (e.tipo == EV_MENU) registrar("sem-cursor: menu fechado (%d)\n", e.x);
        if (e.tipo == EV_CLIQUE) registrar("sem-cursor: clique em %d %d\n", e.x, e.y);
    }
    jan_fechar(j);
    registrar("sem-cursor: fim\n");
    return 0;
}

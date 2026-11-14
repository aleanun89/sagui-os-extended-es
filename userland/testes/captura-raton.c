// SPDX-License-Identifier: GPL-2.0-or-later
/* captura-raton - prueba de la D242: pide la captura del ratón (estilo_janela con
 * JAN_SO_CAPTURA | JAN_CAPTURA) y anota en el registro los desplazamientos crudos
 * que llegan (EV_MOVER_CRU) con su suma.
 * Teclas: «c» captura, «l» suelta, «q» o cerrar termina. EV_CAPTURA_SOLTA se anota. */
#include "sagui.h"
#include "gui.h"

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = jan_abrir("Captura raton", 400, 300);
    if (!j) return 1;
    g_retangulo(j, 0, 0, j->larg, j->alt, 0x3C6EDC);
    jan_mostrar(j);
    registrar("captura-raton: listo\n");
    int ax = 0, ay = 0;
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 'q')) break;
        if (e.tipo == EV_TECLA && e.tecla == 'c') {
            int r = estilo_janela(j->id, JAN_SO_CAPTURA | JAN_CAPTURA, 0, 0);
            ax = ay = 0;
            registrar("captura-raton: pedida r=%d\n", r);
        }
        if (e.tipo == EV_TECLA && e.tecla == 'l') {
            estilo_janela(j->id, JAN_SO_CAPTURA, 0, 0);
            registrar("captura-raton: soltada por el programa\n");
        }
        if (e.tipo == EV_MOVER_CRU) {
            ax += e.x; ay += e.y;
            registrar("captura-raton: delta %d %d acum %d %d\n", e.x, e.y, ax, ay);
        }
        if (e.tipo == EV_MOVER) registrar("captura-raton: mover normal %d %d\n", e.x, e.y);
        if (e.tipo == EV_CAPTURA_SOLTA) registrar("captura-raton: el sistema solto la captura\n");
        if (e.tipo == EV_CLIQUE) registrar("captura-raton: clique %d %d\n", e.x, e.y);
    }
    jan_fechar(j);
    registrar("captura-raton: fin\n");
    return 0;
}

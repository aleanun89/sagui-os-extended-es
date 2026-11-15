// SPDX-License-Identifier: GPL-2.0-or-later
/* gui-revisar <caso> - prueba del detector de problemas de la GUI (D300, tests/h300_gui_revisar.py).
 * Casos:
 *   solape    dos botones que se pisan y un botón cuyo texto no cabe;
 *   limpio    los mismos botones bien colocados y con texto que cabe;
 *   ignorado  como "solape", pero uno de los botones solapados se marca con gui_revisar_ignorar
 *             y el otro lo cubre el patrón de un fichero /etc/gui-revisar.ignorar si existe. */
#include "gui.h"

int principal(int argc, char **argv)
{
    const char *caso = argc > 1 ? argv[1] : "limpio";
    jan_t *j = jan_abrir("Prueba GUI", 400, 240);
    if (!j) return 1;
    botao_t a = { 20, 20, 120, 28, "Aceptar", false };
    botao_t b = { 100, 30, 120, 28, "Cancelar", false };      /* pisa a "Aceptar" en 40x18 */
    botao_t c = { 20, 100, 80, 28, "Texto larguisimo", false };   /* 16 letras = 128 px > 80 */
    botao_t d = { 290, 20, 100, 8, "Bajo", false };           /* más bajo que la fuente */
    if (compara_texto(caso, "limpio") == 0) {
        b.x = 160;
        c.l = 150;
        d.a = 28;
    } else if (compara_texto(caso, "ignorado") == 0) {
        gui_revisar_ignorar(&a);
        c.l = 150;
        d.a = 28;
    }
    botao_desenhar(j, &a);
    botao_desenhar(j, &b);
    botao_desenhar(j, &c);
    botao_desenhar(j, &d);
    jan_mostrar(j);
    registrar("gui-revisar: pronto %s\n", caso);
    evento_t e;
    for (int i = 0; i < 40; i++) {      /* unos 4 s o hasta que cierren la ventana */
        if (esperar_evento(j->id, &e, 100) == 1 && e.tipo == EV_FECHAR) break;
    }
    jan_fechar(j);
    return 0;
}

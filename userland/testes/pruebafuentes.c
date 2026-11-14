// SPDX-License-Identifier: GPL-2.0-or-later
/* pruebafuentes - prueba de la libfontes (D196, tests/h162_fuentes.py).
 *
 * Abre una ventana «Prueba de fuentes» de 560x360 con fondo blanco y escribe, en negro,
 * una muestra con cada una de las primeras fuentes del catálogo (hasta 5, repartidas), a
 * 30 px. Por el registro (serie) da, para cada línea, la fuente, el ancho medido con
 * fonte_medir y el devuelto al dibujar:
 *   pruebafuentes: n=<catálogo> fila <i> y=<y> x=<x> medido=<m> dibujado=<d> familia=<f>
 * Termina con EV_FECHAR. */
#include "sagui.h"
#include "gui.h"
#include "fontes.h"

#define LARG 560
#define ALT  360
#define X0   20
#define PASO 64

static fuente_info_t cat[FUENTES_MAX];

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    int n = fuentes_catalogo(cat, FUENTES_MAX);
    jan_t *j = jan_abrir("Prueba de fuentes", LARG, ALT);
    if (!j) return 1;
    g_retangulo(j, 0, 0, LARG, ALT, 0xFFFFFF);
    registrar("pruebafuentes: n=%d", n);
    int usadas = n < 5 ? n : 5;
    for (int i = 0; i < usadas; i++) {
        int k = n < 5 ? i : i * (n - 1) / 4;
        fonte_t *f = fonte_abrir_archivo(cat[k].ruta);
        if (!f) { registrar("pruebafuentes: no abre %s", cat[k].ruta); continue; }
        int y = 10 + i * PASO;
        const char *muestra = "Hamburgefonstiv 0123";
        int m = fonte_medir(f, muestra, 30);
        int d = fonte_texto(f, j, X0, y, muestra, 30, 0x000000);
        registrar("pruebafuentes: fila %d y=%d x=%d medido=%d dibujado=%d familia=%s", i, y, X0, m, d, cat[k].familia);
        g_texto(j, X0, y + 40, cat[k].familia, 0x606060);
        fonte_fechar(f);
    }
    jan_mostrar(j);
    registrar("pruebafuentes: lista");
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
    }
    jan_fechar(j);
    return 0;
}

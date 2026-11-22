/* SPDX-License-Identifier: GPL-3.0-only */
/* Prueba de unidad de los esquemas de apariencia (D428): vuelca los doce esquemas incluidos y comprueba lo que se puede
 * comprobar sin ejecutar el sistema (el generador es el mismo código que usan el kernel y la libsagui).
 *   gcc -I kernel/inc -o estilos_unidad tests/estilos_unidad.c && ./estilos_unidad  ->  una línea por color: «id nombre campo RRGGBB» */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esquemas.h"

int main(void)
{
    esquema_t e;
    for (int id = 0; id < ESQ_QTD; id++) {
        esq_incluido(&e, id, 0xFFFFFFFFu);
        if (e.id != (uint32_t)id) { printf("FALLO id %d da %u\n", id, e.id); return 1; }
#define VUELCA(n) printf("%d|%s|" #n "|%06X\n", id, e.nome, e.n);
        ESQ_CAMPOS(VUELCA)
        printf("%d|%s|escuro|%u\n%d|%s|barra_propia|%u\n%d|%s|escritorio_liso|%u\n", id, e.nome, e.escuro, id, e.nome, e.barra_propia, id, e.nome, e.escritorio_liso);
    }
    /* edición: cada elemento cambia su color y el esquema pasa a personalizado */
    for (int el = 0; el < EE_QTD; el++) {
        esq_incluido(&e, ESQ_DESIERTO, 0xFFFFFFFFu);
        esq_poner(&e, el, 0x123456);
        if (esq_dame(&e, el) != 0x123456 || e.id != ESQ_PERSONAL) { printf("FALLO edicion %d\n", el); return 1; }
    }
    printf("CAMPOS|%d\nELEMENTOS|%d\n", ESQ_NC, EE_QTD);
    return 0;
}

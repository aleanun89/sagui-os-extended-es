/* SPDX-License-Identifier: GPL-3.0-only */
/* ventana.c - una ventana con la libsagui, compilada con TCC (D384).
 *   tcc ventana.c -o ventana && ./ventana [segundos]
 * Los programas de SaguiOS empiezan en principal() (como los de /bin); también pueden usar main().
 * Dibuja un degradado, escribe en el registro cada clic y termina al cerrar la ventana (o pasados
 * los segundos que se le digan). */
#include "sagui.h"

#define ANCHO 320
#define ALTO  200

int principal(int argc, char **argv)
{
    int segundos = argc > 1 ? texto_para_int(argv[1]) : 0;
    uint32_t *px;
    int id = criar_janela("Ventana TCC", ANCHO, ALTO, &px);
    if (id < 0) {
        registrar("ventana: no se pudo abrir (%d)\n", id);
        return 1;
    }
    for (int y = 0; y < ALTO; y++)
        for (int x = 0; x < ANCHO; x++)
            px[y * ANCHO + x] = (uint32_t)((x * 255 / ANCHO) << 16 | (y * 255 / ALTO) << 8 | 0x60);
    atualizar_janela(id, NULL);
    registrar("ventana TCC: abierta (id %d)\n", id);
    uint32_t fin = tempo_ms() + (uint32_t)segundos * 1000;
    evento_t e;
    for (;;) {
        if (segundos && tempo_ms() >= fin)
            break;
        if (esperar_evento(id, &e, 200) != 1)
            continue;
        if (e.tipo == EV_FECHAR)
            break;
        if (e.tipo == EV_CLIQUE)
            registrar("ventana TCC: clic en (%d,%d)\n", e.x, e.y);
    }
    registrar("ventana TCC: cerrada\n");
    fechar_janela(id);
    return 0;
}

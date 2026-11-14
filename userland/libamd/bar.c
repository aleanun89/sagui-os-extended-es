// SPDX-License-Identifier: GPL-2.0-or-later
/* bar.c - mapear una ventana de un BAR de un dispositivo PCI (D158).
 *
 * El kernel hace el trabajo: elige las páginas (las del propio BAR), las
 * mapea sin caché en el espacio del programa y devuelve la dirección virtual.
 * Aquí solo está la llamada. */
#include "amd.h"

static inline int chamar5(unsigned n, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory");
    return r;
}

int amd_mapear_bar(unsigned barramento, unsigned dispositivo, unsigned funcao, unsigned bar,
                   unsigned desloc, unsigned tamanho, int escrever, uint32_t *endereco)
{
    pedido_bar_t p;
    uint32_t mapa = 0;
    p.barramento = barramento;
    p.dispositivo = dispositivo;
    p.funcao = funcao;
    p.indice = bar;
    p.desloc = desloc;
    p.tamanho = tamanho;
    p.permissao = escrever ? BAR_ESCRITA : BAR_LEITURA;
    int r = chamar5(SIS_MAPEAR_BAR, (unsigned)&p, (unsigned)&mapa, 0, 0, 0);
    if (r < 0) return r;
    if (endereco) *endereco = mapa;
    return 0;
}

void amd_desejar_bar(uint32_t endereco)
{
    chamar5(SIS_DESEJAR_BAR, endereco, 0, 0, 0, 0);
}

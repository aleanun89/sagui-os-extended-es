// SPDX-License-Identifier: GPL-3.0-only
/* pipe.c - comunicación entre procesos por un buffer circular en el kernel.
 *
 * Un pipe tiene dos extremos, cada uno un archivo abierto: quien escribe en un
 * extremo, otro lee en el otro. Reglas (H15):
 *   - leer con el pipe vacío: si todavía hay quien escriba, el lector duerme; si no
 *     queda ningún escritor, devuelve 0 (fin del archivo);
 *   - escribir con el pipe lleno: el escritor duerme hasta que alguien lea; escribe lo
 *     que quepa y devuelve cuánto escribió (la libsagui insiste con el resto);
 *   - escribir sin ningún lector: error "pipe roto".
 * Lectores y escritores duermen en el propio pipe como canal; cualquier cambio
 * despierta a todos, y cada uno repite la llamada (D010) y vuelve a comprobar. */
#include "pipe.h"
#include "abi.h"
#include "memoria.h"
#include "processo.h"
#include "sistema.h"

pipe_t *pipe_criar(void)
{
    pipe_t *p = kcalloc(sizeof(pipe_t));
    if (p) { p->leitores = 1; p->escritores = 1; }
    return p;
}

int32_t pipe_ler(pipe_t *p, char *destino, uint32_t n)
{
    if (n == 0) return 0;
    if (p->qtd == 0) {
        if (p->escritores == 0) return 0;
        bloquear(p);
        return SIS_REPETIR;
    }
    uint32_t k = 0;
    while (k < n && p->qtd) {
        destino[k++] = (char)p->buf[p->ini];
        p->ini = (p->ini + 1) % PIPE_TAM;
        p->qtd--;
    }
    acordar(p);   /* se abrió espacio: los escritores pueden continuar */
    return (int32_t)k;
}

int32_t pipe_escrever(pipe_t *p, const char *origem, uint32_t n)
{
    if (p->leitores == 0) return ERRO_PIPE_QUEBRADO;
    if (n == 0) return 0;
    if (p->qtd == PIPE_TAM) {
        bloquear(p);
        return SIS_REPETIR;
    }
    uint32_t k = 0;
    while (k < n && p->qtd < PIPE_TAM) {
        p->buf[(p->ini + p->qtd) % PIPE_TAM] = (uint8_t)origem[k++];
        p->qtd++;
    }
    acordar(p);   /* llegaron datos: los lectores pueden continuar */
    return (int32_t)k;
}

void pipe_fechar_ponta(pipe_t *p, bool escrita)
{
    if (escrita) p->escritores--; else p->leitores--;
    acordar(p);   /* un lector esperando necesita descubrir que terminó */
    if (p->leitores <= 0 && p->escritores <= 0)
        kfree(p);
}

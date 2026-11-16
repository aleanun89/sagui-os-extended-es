// SPDX-License-Identifier: GPL-2.0-or-later
/* pilha.c - pilas de usuario que crecen bajo demanda, con guarda (D186).
 *
 * Como en Linux o Windows, la pila de un programa es una reserva de
 * direcciones grande de la que solo se asigna memoria física al tocarla:
 *
 *   - el hilo principal tiene [USUARIO_PILHA, USUARIO_FIM): 8 MB. Al cargar
 *     se asignan solo las páginas de argv/envp y un margen pequeño
 *     (carregador.c). Por debajo, de USUARIO_MMIO_FIM a USUARIO_PILHA, hay
 *     4 MB que nunca se mapean: la guarda;
 *   - cada hilo creado sin pila propia (hilos.c) recibe un hueco de 1 MB en
 *     [USUARIO_HILOS, USUARIO_HILOS + 64 MB). Los 64 KB de abajo del hueco
 *     son su guarda; el resto, su pila.
 *
 * Un fallo de página "no presente" dentro de una pila se atiende aquí:
 * página nueva a cero, mapeada, y la instrucción se repite. Un fallo en la
 * guarda termina el proceso con «Desbordamiento de pila». Lo demás sigue
 * como antes (excecao_usuario o la pantalla de error del kernel).
 *
 * El kernel también toca la pila del programa en las llamadas al sistema
 * (un read() a un buffer local aún no tocado, por ejemplo): usuario_ok()
 * llama a pilha_crescer() para las páginas que faltan, así el kernel nunca
 * falla al copiar. Si aun así fallara en modo kernel dentro de una pila, el
 * manipulador también la hace crecer.
 *
 * SMP (D164-D169): el manipulador corre con el cerrojo general tomado
 * (tratar_interrupcao), así que dos hilos del mismo proceso que fallan a la
 * vez en la misma página se atienden uno detrás de otro: el segundo ya la
 * encuentra mapeada y solo repite la instrucción. Solo se añaden páginas,
 * nunca se quitan mientras vive el proceso (tampoco al acabar un hilo: su
 * hueco se reutiliza con lo que ya tenía), así que ninguna CPU puede tener
 * en su TLB una traducción vieja: no hace falta avisar a las demás. Las
 * páginas se liberan con el espacio entero (espaco_destruir). */
#include "processo.h"
#include "paginacao.h"
#include "memoria.h"
#include "console.h"
#include "cpu.h"
#include "texto.h"
#include "abi.h"

#define HILOS_FIM (USUARIO_HILOS + HILO_VAGAS * HILO_VAGA)

/* Bits del código de error del fallo de página (manual de Intel). */
#define PF_PRESENTE 1u
#define PF_ESCRITA  2u

pilha_zona_t pilha_zona(const processo_t *p, uint32_t virt)
{
    const processo_t *l = processo_lider((processo_t *)p);
    if (!l || !l->usuario) return PILHA_FORA;
    if (virt >= USUARIO_PILHA && virt < USUARIO_FIM) return PILHA_CRESCER;
    if (virt >= USUARIO_MMIO_FIM && virt < USUARIO_PILHA) return PILHA_GUARDA;
    if (virt >= USUARIO_HILOS && virt < HILOS_FIM) {
        uint32_t vaga = (virt - USUARIO_HILOS) / HILO_VAGA;
        if (!(l->vagas_hilos & ((uint64_t)1 << vaga))) return PILHA_FORA;   /* hueco sin hilo */
        return (virt - USUARIO_HILOS) % HILO_VAGA < HILO_GUARDA ? PILHA_GUARDA : PILHA_CRESCER;
    }
    return PILHA_FORA;
}

bool pilha_crescer(processo_t *p, uint32_t virt)
{
    if (pilha_zona(p, virt) != PILHA_CRESCER) return false;
    processo_t *l = processo_lider(p);
    if (l->estado == P_ZUMBI || l->estado == P_LIVRE || l->espaco == espaco_kernel()) return false;
    /* espaco_alocar no toca lo que ya existe: el segundo de dos hilos que
     * fallaron en la misma página no asigna otra. */
    return espaco_alocar(l->espaco, virt & ~(PAGINA - 1), PAGINA, PG_USUARIO | PG_ESCRITA | PG_NX);   /* D349: pila no ejecutable */
}

int pilha_vaga_reservar(processo_t *lider)
{
    for (uint32_t i = 0; i < HILO_VAGAS; i++)
        if (!(lider->vagas_hilos & ((uint64_t)1 << i))) {
            lider->vagas_hilos |= (uint64_t)1 << i;
            return (int)i;
        }
    return -1;
}

void pilha_vaga_liberar(processo_t *t)
{
    if (!t->vaga_pilha) return;
    processo_t *l = t->lider;
    if (l && l->estado != P_LIVRE)
        l->vagas_hilos &= ~((uint64_t)1 << (t->vaga_pilha - 1));
    t->vaga_pilha = 0;
}

/* Páginas asignadas en [ini, fim) del espacio "e". */
static uint32_t paginas_em(espaco_t e, uint32_t ini, uint32_t fim)
{
    uint32_t n = 0;
    for (uint32_t v = ini; v < fim; v += PAGINA)
        if (espaco_traduzir(e, v, NULL)) n++;
    return n;
}

uint32_t pilha_usada_kb(const processo_t *p)
{
    const processo_t *l = processo_lider((processo_t *)p);
    if (!l || !l->usuario || l->espaco == espaco_kernel()) return 0;
    if (!p->lider) return paginas_em(l->espaco, USUARIO_PILHA, USUARIO_FIM) * (PAGINA / 1024);
    if (!p->vaga_pilha) return 0;
    uint32_t ini = USUARIO_HILOS + (uint32_t)(p->vaga_pilha - 1) * HILO_VAGA;
    return paginas_em(l->espaco, ini + HILO_GUARDA, ini + HILO_VAGA) * (PAGINA / 1024);
}

/* El fallo de página (vector 14). */
static quadro_t *falta_pagina(quadro_t *q)
{
    uint32_t cr2 = le_cr2();
    processo_t *p = processo_atual();
    bool usuario = (q->cs & 3) == 3;
    pilha_zona_t z = p && p->pid != 0 ? pilha_zona(p, cr2) : PILHA_FORA;
    /* En modo kernel solo si el CR3 es el del programa (el kernel trabaja
     * para él en una llamada al sistema). */
    if (z != PILHA_FORA && !usuario && espaco_atual() != processo_lider(p)->espaco) z = PILHA_FORA;

    if (z == PILHA_CRESCER && !(q->erro & PF_PRESENTE)) {
        if (pilha_crescer(p, cr2)) return q;   /* página nueva: se repite la instrucción */
        if (usuario) {
            char motivo[64];
            snformatar(motivo, sizeof motivo, "Sin memoria para la pila (0x%08x)", cr2);
            kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: %s\n", processo_lider(p)->pid, p->nome, motivo);
            processo_encerrar(p, SAIDA_ENCERRADO_PELO_KERNEL, motivo);
            return escalonar(q);
        }
    }
    if (z == PILHA_GUARDA && usuario) {
        char motivo[64];
        snformatar(motivo, sizeof motivo, "Desbordamiento de pila: %s 0x%08x",
                   q->erro & PF_ESCRITA ? "escritura" : "lectura", cr2);
        if (p->lider)
            kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: %s (hilo %d)\n", processo_lider(p)->pid, p->nome,
                    motivo, p->pid);
        else
            kprintf("\n[SaguiOS] Proceso %d (%s) finalizado: %s\n", p->pid, p->nome, motivo);
        processo_encerrar(p, SAIDA_ENCERRADO_PELO_KERNEL, motivo);
        return escalonar(q);
    }
    if (usuario) return excecao_usuario(q);
    excecao_fatal(q);
}

void pilha_iniciar(void)
{
    idt_registrar(14, falta_pagina);
}

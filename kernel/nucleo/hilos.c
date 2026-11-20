// SPDX-License-Identifier: GPL-2.0-or-later
/* hilos.c - hilos de usuario y espera por dirección (futex) (D167).
 *
 * Un hilo es una entrada más de la tabla de procesos, como en el "clone" de
 * Linux: tiene su pila de kernel, su cuadro (los registros), su estado de
 * FPU, su CPU y su número (tid), y comparte con su líder (el hilo principal,
 * el proceso) el espacio de direcciones. Lo demás que se comparte (ficheros,
 * carpeta, área de datos, mapas de dispositivos, ventanas, el pid que ven los
 * demás procesos) solo existe en el líder: el kernel lo lee con
 * processo_recursos() / processo_lider().
 *
 * Así el planificador, la FPU perezosa, la migración entre CPU, el "na_cpu"
 * y el "tirar da CPU" del cerrojo general (D165) valen tal cual para cada
 * hilo: ya eran por entrada de la tabla.
 *
 * La pila de usuario de un hilo la pone el kernel o el programa (D186):
 *   - pilha = 0: el kernel le da un hueco de 1 MB de la zona de pilas de
 *     hilos (USUARIO_HILOS), con 64 KB de guarda abajo y páginas asignadas
 *     al tocarlas (pilha.c), y pone en su tope la dirección de retorno (0) y
 *     el argumento (el tercer parámetro). Es lo que usa thread_create;
 *   - pilha != 0: el tope de una pila del programa (de su malloc, como antes
 *     de la D186), que ya lleva la dirección de retorno y el argumento.
 *
 * Futex: futex_esperar(&palabra, valor) duerme si la palabra aún vale "valor"
 * y futex_acordar(&palabra, n) despierta a n de los que duermen en ella. La
 * clave es la dirección FÍSICA de la palabra (única aunque dos espacios la
 * vean en direcciones distintas). Las dos llamadas corren con el cerrojo
 * general del kernel, así que entre la comprobación y el dormir no puede
 * llegar un despertar de otra CPU: no se pierde ninguno. */
#include "sistema.h"
#include "processo.h"
#include "relogio.h"
#include "console.h"
#include "texto.h"
#include "memalta.h"

/* El canal de todos los que duermen en un futex; cuál es la palabra lo dice
 * processo_t.futex. */
static const char canal_futex = 0;

/* D186: un hueco de pila para el hilo "t" del proceso "l". Devuelve el tope
 * (ESP inicial) con el retorno 0 y "arg" encima, o 0 si no hay hueco o
 * memoria. Como al entrar en una función del ABI i386: (esp + 4) alineado a 16. */
static uint32_t pilha_de_hilo(processo_t *l, processo_t *t, uint32_t arg)
{
    int vaga = pilha_vaga_reservar(l);
    if (vaga < 0) return 0;
    t->vaga_pilha = (uint8_t)(vaga + 1);
    uint32_t fim = USUARIO_HILOS + (uint32_t)(vaga + 1) * HILO_VAGA;
    uint32_t topo = fim - 16u - 4u;
    if (!pilha_crescer(t, topo)) return 0;   /* la primera página; las demás, al tocarlas */
    /* D348: la página puede ser alta: se escribe por una ventana temporal. */
    uint32_t palavras[2] = { 0, arg };   /* retorno (la función del hilo no vuelve) y su argumento */
    /* D352: puede estar en la swap (hueco reutilizado); D401: o compartida con un hijo del fork. */
    fis_t f = espaco_traduzir_escrita(l->espaco, topo);
    if (!f) return 0;
    fisica_escrever(f, palavras, sizeof palavras);
    return topo;
}

/* hilo_criar(entrada, pilha, arg): un hilo nuevo del proceso que llama. */
int32_t sis_hilo_criar(uint32_t entrada, uint32_t pilha, uint32_t arg, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    processo_t *l = processo_recursos();
    if (!l || !l->usuario || l->estado == P_ZUMBI) return ERRO_ACESSO;
    if (!usuario_ok((void *)entrada, 1, false)) return ERRO_ENDERECO;
    if (pilha && ((pilha & 3) || pilha < 8 || !usuario_ok((void *)pilha, 8, true))) return ERRO_ENDERECO;
    processo_t *t = processo_novo(l->nome, l->pid);
    if (!t) return ERRO_LIMITE;
    t->lider = l;
    if (!pilha && !(pilha = pilha_de_hilo(l, t, arg))) {
        processo_liberar(t);   /* devuelve también el hueco, si lo llegó a reservar */
        return ERRO_SEM_MEMORIA;
    }
    t->espaco = l->espaco;
    t->grupo = l->grupo;
    t->sinais_mascara = processo_atual()->sinais_mascara;   /* D401: como pthread_create */
    t->mmio_fim = l->mmio_fim;
    txt_copia_n(t->pasta, "/", sizeof t->pasta);   /* no se usa: la carpeta es la del líder */
    processo_preparar_usuario(t, entrada, pilha);
    return t->pid;
}

/* hilo_sair(codigo): el hilo acaba; en el principal, acaba el proceso. El
 * hilo queda zombi hasta que otro lo espere (hilo_esperar) o acabe el
 * proceso; escalonar() despierta a quien lo espera cuando su CPU lo deja. */
int32_t sis_hilo_sair(uint32_t codigo, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    processo_t *eu = processo_atual();
    if (!eu->lider) {
        processo_encerrar(eu, (int)codigo, NULL);
        return 0;
    }
    fpu_esquecer(eu);
    eu->codigo_saida = (int)codigo;
    eu->estado = P_ZUMBI;
    eu->espera = NULL;
    eu->acordar_em = 0;
    return 0;
}

/* hilo_esperar(tid, &codigo): espera a que acabe un hilo del mismo proceso
 * y libera su entrada. No sirve para el principal ni para uno mismo. */
int32_t sis_hilo_esperar(uint32_t tid, uint32_t ptr_codigo, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    int *codigo = (int *)ptr_codigo;
    if (codigo && !usuario_ok(codigo, sizeof(int), true)) return ERRO_ENDERECO;
    processo_t *eu = processo_atual();
    processo_t *t = processo_por_pid((int)tid);
    if (!t || t == eu || !t->lider || t->lider != processo_lider(eu)) return ERRO_SEM_PROCESSO;
    /* Como un hijo (D165): no se libera mientras su CPU siga sobre su pila. */
    if (t->estado == P_ZUMBI && !t->na_cpu) {
        if (codigo) *codigo = t->codigo_saida;
        processo_liberar(t);
        return 0;
    }
    bloquear(t);
    return SIS_REPETIR;
}

/* ceder(): el hilo deja la CPU a otro listo; vuelve enseguida si no hay. */
int32_t sis_ceder(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    processo_atual()->estado = P_PRONTO;   /* sistema.c planifica al ver que ya no corre */
    return 0;
}

int32_t sis_hilo_id(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return processo_atual()->pid;
}

/* La dirección física de una palabra alineada del programa (0 si no vale). */
static uint64_t palavra_fisica(uint32_t ptr)
{
    if ((ptr & 3) || !usuario_ok((void *)ptr, 4, false)) return 0;
    /* D401: la clave es el marco: en una página de copia al escribir cambiaría al escribirla y el
     * que espera se quedaría en el viejo. Se copia antes (una palabra de futex siempre se escribe). */
    return espaco_traduzir_escrita(processo_atual()->espaco, ptr);
}

/* futex_esperar(&palabra, valor, ms): duerme si la palabra vale "valor". */
int32_t sis_futex_esperar(uint32_t ptr, uint32_t valor, uint32_t ms, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    uint64_t fis = palavra_fisica(ptr);
    if (!fis) return ERRO_ENDERECO;
    if (*(volatile uint32_t *)ptr != valor) return 1;   /* cambió: quien llama vuelve a mirar */
    processo_t *eu = processo_atual();
    eu->futex = fis;
    if (ms) eu->acordar_em = relogio_tiques() + (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    bloquear(&canal_futex);
    return 0;   /* resultado listo: al despertar no se repite */
}

/* futex_acordar(&palabra, n): despierta hasta n de los que duermen en ella. */
int32_t sis_futex_acordar(uint32_t ptr, uint32_t n, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    uint64_t fis = palavra_fisica(ptr);
    if (!fis) return ERRO_ENDERECO;
    int32_t acordados = 0;
    for (int i = 1; i < MAX_PROCESSOS && (uint32_t)acordados < n; i++) {
        processo_t *p = processo_indice(i);
        if (p->estado != P_BLOQUEADO || p->espera != &canal_futex || p->futex != fis) continue;
        p->futex = 0;
        p->acordar_em = 0;
        p->estado = P_PRONTO;
        p->espera = NULL;
        avisar_ociosa(p);
        acordados++;
    }
    return acordados;
}

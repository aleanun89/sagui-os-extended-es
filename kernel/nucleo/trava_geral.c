// SPDX-License-Identifier: GPL-2.0-or-later
/* trava_geral.c - el cerrojo general del kernel (D165).
 *
 * Hasta la D164 el kernel solo corría en la CPU 0 (maestro/esclavo): un
 * secundario devolvía a la CPU 0 cualquier llamada al sistema. Ahora cada CPU
 * atiende las suyas, pero el kernel sigue escrito para un solo procesador
 * (lo protegen las interrupciones apagadas), así que solo una CPU a la vez
 * puede estar dentro: la dueña del cerrojo.
 *
 * Reglas:
 *   - el cerrojo es de la CPU (cada CPU sabe si es suyo: por_cpu.minha);
 *   - se toma al entrar en el kernel si la CPU no lo tiene (desde modo
 *     usuario o desde la espera ociosa); una IRQ que interrumpe código del
 *     kernel de la misma CPU entra sin esperar (ya es suyo);
 *   - se suelta al salir hacia un marco que no lo tenía: modo usuario o la
 *     espera de cpu_espera() (kernel_saida, en la salida de vetores.asm);
 *   - cpu_espera() lo suelta durante el "hlt": quien espera una IRQ, que
 *     llega a la CPU 0, no puede quedarse con el kernel mientras tanto.
 *
 * La CPU 0 arranca dentro del kernel: el cerrojo empieza siendo suyo. */
#include "trava_geral.h"
#include "atomicas.h"
#include "cpu_dados.h"
#include "processo.h"
#include "smp.h"
#include "console.h"

/* El cerrojo reparte los turnos por orden de llegada (D169, "ticket lock"):
 * quien quiere entrar saca número (proximo, con un "lock xadd") y espera a que
 * "atendendo" llegue a su número; al soltar, se pasa al siguiente. Antes era
 * un CAS sobre el dueño, sin cola: al soltarlo, todas las CPU que esperaban
 * intentaban el CAS a la vez y lo ganaba cualquiera (a veces la misma que lo
 * acababa de soltar), así que una CPU podía quedarse esperando mucho más que
 * las otras. Bajo un hipervisor se sigue tomando así, sin cola
 * (trava_geral_escolher): los turnos van mal cuando la CPU virtual a la que
 * le toca no está corriendo.
 *
 * Los contadores dan vueltas (32 bits) sin problema: solo se comparan por
 * igualdad. La CPU 0 arranca con el turno 0: proximo = 1, atendendo = 0.
 * Los dos van en una línea de caché propia: es la que viaja entre las CPU. */
static struct {
    volatile uint32_t proximo;     /* el número que sacará el siguiente */
    volatile uint32_t atendendo;   /* el número que tiene el cerrojo */
} __attribute__((aligned(CPU_LINHA_CACHE))) turnos = { 1, 0 };   /* el tipo alineado ocupa la línea entera */

/* Lo de cada CPU, cada una en su línea (no viaja): si el cerrojo es suyo
 * (solo lo escribe ella, así que desde ella es fiable y da la reentrada) y
 * cuánto ha esperado. */
static struct {
    volatile uint32_t minha;   /* el cerrojo es de esta CPU */
    uint32_t esperas;          /* veces que tuvo que esperar */
    uint64_t ciclos;           /* ciclos (TSC) esperando */
#ifdef TRAVA_MEDIR
    uint64_t desde, retida;    /* cuándo lo tomó y ciclos con él */
#endif
} __attribute__((aligned(CPU_LINHA_CACHE))) por_cpu[CPU_MAX] = { [0] = { .minha = 1 } };

/* Pausas por turno que falta, entre dos lecturas de "atendendo" (D169: sin
 * pausa, la ganancia de "bench 4 s" con KVM caía a 1,1-1,6x; con 4 pausas
 * por turno no cambia de forma clara). */
#ifndef ESPERA_POR_TURNO
#define ESPERA_POR_TURNO 1
#endif

static inline void pausa(void) { __asm__ volatile("pause" ::: "memory"); }

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* Al tomar el cerrojo: si otra CPU quitó páginas de nuestro espacio mientras
 * esta CPU estaba fuera del kernel, la TLB de esta CPU aún las recuerda: se
 * recarga el CR3 antes de que el kernel toque la memoria del programa. */
static void depois_de_pegar(uint32_t cpu)
{
    por_cpu[cpu].minha = 1;
#ifdef TRAVA_MEDIR
    por_cpu[cpu].desde = tsc();
#endif
    if (!cpu_gs_lista()) return;
    cpu_dados_t *d = cpu_dados(cpu);
    if (d->recarregar_cr3) {
        d->recarregar_cr3 = 0;
        espaco_ativar(espaco_atual());
    }
}

/* Sin cola (bajo un hipervisor, ver trava_geral_escolher): se toma el turno
 * solo si nadie lo tiene ni lo espera, como el CAS sobre el dueño de la D165.
 * Mezclarlo con quien saca número es seguro: los dos toman el turno
 * "atendendo" y uno solo lo consigue. */
static bool tomar_se_livre(void)
{
    uint32_t vez = turnos.atendendo;
    return turnos.proximo == vez && atomic_cas32(&turnos.proximo, vez, vez + 1);
}

/* Por turnos o sin cola: lo decide trava_geral_escolher al arrancar las CPU
 * secundarias; hasta entonces no hay con quién competir. */
static volatile bool por_turnos = true;

/* Se llama siempre con las interrupciones apagadas (la entrada del kernel,
 * el tic, la vuelta de cpu_espera): entre sacar número y tomar el turno nada
 * de esta CPU puede volver a pedirlo. La espera solo depende de que las CPU
 * de delante lo suelten: el dueño nunca espera a una CPU de la cola
 * (processo_tirar_da_cpu espera a esperando_trava, que se pone antes de
 * esperar; ver la D169). */
void trava_geral_pegar(void)
{
    uint32_t cpu = cpu_atual();
    if (por_cpu[cpu].minha) return;
    if (por_turnos) {
        uint32_t vez = atomic_add32(&turnos.proximo, 1);
        if (turnos.atendendo != vez) {
            /* Hay cola: se espera leyendo (sin "lock") hasta que sea nuestro
             * turno. Solo pasa con varias CPU arrancadas (un Pentium o
             * posterior: hay TSC). La marca dice a la dueña que esta CPU ya
             * no ejecuta código del programa (processo_tirar_da_cpu). Entre
             * dos lecturas, tantas pausas como turnos faltan: quien está lejos
             * lee menos la línea que la dueña está a punto de escribir. */
            cpu_dados_t *d = cpu_gs_lista() ? cpu_dados(cpu) : NULL;
            uint64_t t0 = tsc();
            if (d) d->esperando_trava = 1;
            uint32_t at;
            while ((at = turnos.atendendo) != vez)
                for (uint32_t k = (vez - at) * ESPERA_POR_TURNO; k; k--)
                    pausa();
            if (d) d->esperando_trava = 0;
            por_cpu[cpu].esperas++;
            por_cpu[cpu].ciclos += tsc() - t0;
        }
    } else if (!tomar_se_livre()) {
        /* Sin cola: se espera leyendo hasta verlo libre y se intenta. */
        cpu_dados_t *d = cpu_gs_lista() ? cpu_dados(cpu) : NULL;
        uint64_t t0 = tsc();
        if (d) d->esperando_trava = 1;
        do {
            while (turnos.proximo != turnos.atendendo)
                pausa();
        } while (!tomar_se_livre());
        if (d) d->esperando_trava = 0;
        por_cpu[cpu].esperas++;
        por_cpu[cpu].ciclos += tsc() - t0;
    }
    depois_de_pegar(cpu);
}

/* Solo si está libre y no hay nadie en la cola (con turnos, no se adelanta
 * a nadie). */
bool trava_geral_tentar(void)
{
    uint32_t cpu = cpu_atual();
    if (por_cpu[cpu].minha) return true;
    if (!tomar_se_livre()) return false;
    depois_de_pegar(cpu);
    return true;
}

/* D169: por turnos en una máquina de verdad; sin cola bajo un hipervisor,
 * salvo que se pida (mkimagem.py --trava-turnos, para las pruebas). Con
 * turnos, si el hipervisor deja sin correr a la CPU virtual a la que le toca
 * (hay más CPU virtuales que núcleos libres), nadie más puede entrar hasta que
 * vuelva; sin cola, entra la primera que lo ve libre (lo mismo hace Linux,
 * virt_spin_lock, cuando no tiene cerrojos paravirtuales). Se llama con las
 * CPU secundarias ya arrancadas: hay CPUID. Cambiar de modo en marcha es
 * seguro (los dos toman el mismo turno), solo cambia el reparto. */
void trava_geral_escolher(bool forcar_turnos)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    bool hipervisor = (c >> 31) & 1;
    por_turnos = forcar_turnos || !hipervisor;
    klog("[SMP] cerrojo general: %s\n",
         por_turnos ? (hipervisor ? "por turnos (pedido con --trava-turnos)" : "por turnos")
                    : "sin cola (bajo un hipervisor; --trava-turnos para turnos)");
}

bool trava_geral_por_turnos(void) { return por_turnos; }

void trava_geral_soltar(void)
{
    uint32_t cpu = cpu_atual();
    if (!por_cpu[cpu].minha) return;
    /* Se borra la marca y se pasa el turno. La suma con lock es una barrera
     * completa: todo lo escrito dentro del kernel se ve antes de que la
     * siguiente CPU vea su turno. */
#ifdef TRAVA_MEDIR
    por_cpu[cpu].retida += tsc() - por_cpu[cpu].desde;
#endif
    por_cpu[cpu].minha = 0;
    atomic_inc32(&turnos.atendendo);
}

bool trava_geral_minha(void) { return por_cpu[cpu_atual()].minha != 0; }

uint32_t trava_geral_esperas(void)
{
    uint32_t n = 0;
    for (uint32_t c = 0; c < CPU_MAX; c++) n += por_cpu[c].esperas;
    return n;
}

void trava_geral_medida(uint32_t cpu, uint32_t *esperas, uint64_t *ciclos)
{
    if (cpu >= CPU_MAX) { *esperas = 0; *ciclos = 0; return; }
    *esperas = por_cpu[cpu].esperas;
    *ciclos = por_cpu[cpu].ciclos;
}

#ifdef TRAVA_MEDIR
/* Diagnóstico (D169): con "make VARIANTE_DEFS=-DTRAVA_MEDIR", el tic de la
 * CPU 0 escribe cada segundo, si hubo esperas, cuánto esperó y cuánto tuvo el
 * cerrojo cada CPU (en miles de ciclos del TSC, 1024). No va en el kernel
 * normal: escribir en el registro con el cerrojo tomado también cuesta. */
void trava_geral_relatorio(uint32_t agora)
{
    static uint32_t ultimo;
    if (agora % 100 != 0 || trava_geral_esperas() == ultimo) return;
    ultimo = trava_geral_esperas();
    for (uint32_t c = 0; c < CPU_MAX && c < cpu_numero(); c++)
        klog("[TRAVA] t=%u cpu %u esperas %u esperando %u retida %u\n", agora, c,
             por_cpu[c].esperas, (uint32_t)(por_cpu[c].ciclos >> 10),
             (uint32_t)(por_cpu[c].retida >> 10));
}
#endif

/* ¿El marco reanudado tenía el cerrojo? No, si vuelve a modo usuario o al
 * "hlt" de espera_sem_trava. Todo otro código del kernel lo tenía. */
static bool marco_sem_trava(const quadro_t *q)
{
    if ((q->cs & 3) == 3) return true;
    return q->eip == (uint32_t)ponto_hlt || q->eip == (uint32_t)ponto_depois_hlt;
}

void kernel_saida(quadro_t *q)
{
    if (marco_sem_trava(q))
        trava_geral_soltar();
    /* Las IPI de despertar salen con el cerrojo ya suelto: la CPU despertada
     * lo necesita para planificar. */
    if (!cpu_gs_lista()) return;
    cpu_dados_t *d = cpu_dados(cpu_atual());
    uint32_t m = d->acordar;
    if (m) {
        d->acordar = 0;
        for (uint32_t c = 0; c < CPU_MAX; c++)
            if (m & (1u << c)) smp_acordar(c);
    }
}

/* Duerme hasta la próxima interrupción. Con el cerrojo, lo suelta durante el
 * "hlt": la IRQ que se espera puede llegar a otra CPU (los dispositivos van a
 * la CPU 0) y esa CPU necesita el kernel para atenderla. Con las
 * interrupciones apagadas, como antes: un "hlt" (que solo despierta un NMI). */
void cpu_espera(void)
{
    bool ints = int_salva_desliga();
    if (!ints) {
        __asm__ volatile("hlt");
        return;
    }
    bool minha = trava_geral_minha();
    if (minha) trava_geral_soltar();
    espera_sem_trava();          /* sti; hlt; cli: la salida de la IRQ soltó el cerrojo */
    if (minha) {
        trava_geral_pegar();     /* puede ser otra CPU: el proceso pudo cambiar de CPU */
        /* Otra CPU terminó este proceso mientras dormía: su código del kernel
         * no sigue nunca (como con un procesador, un zombi no vuelve a correr). */
        processo_t *p = processo_atual();
        if (p && p->pid != 0 && p->estado == P_ZUMBI)
            ceder();
    }
    int_liga();
}

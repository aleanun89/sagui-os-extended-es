// SPDX-License-Identifier: GPL-2.0-or-later
/* smp.c - el arranque de los procesadores secundarios (D157, D164).
 *
 * El procesador principal hace tres cosas:
 *   1. lee la MADT del ACPI, donde el firmware enumera los procesadores y sus
 *      identificadores de APIC local;
 *   2. copia el trampolín a la memoria baja (0x7000) y escribe en él lo que
 *      necesita el secundario (su CR0, CR3 y CR4, su pila y la función de
 *      entrada);
 *   3. enciende cada secundario con la secuencia INIT-SIPI de Intel y espera a
 *      que diga que ha arrancado.
 *
 * El secundario entra en el kernel por smp_ap_entrada(), que carga su GDT, su
 * TSS, su GS y la IDT (el mismo camino que el principal, D164), se anuncia,
 * activa su APIC local y su temporizador y queda listo para planificar.
 *
 * Opción de arranque (D164): solo se encienden los secundarios si la imagen se
 * hizo con "mkimagem.py --smp" (OPC_BOOT_SMP). Sin ella el sistema usa un
 * solo procesador, como antes de la D157. */
#include "smp.h"
#include "planificador.h"
#include "cpu.h"
#include "cpu_dados.h"
#include "lapic.h"
#include "console.h"
#include "texto.h"
#include "paginacao.h"
#include "info_hardware.h"
#include "atomicas.h"
#include "wc.h"
#include "processo.h"
#include "relogio.h"
#include "fpu.h"
#include "trava_geral.h"

/* El trampolín (kernel/arch/ap_trampolim.asm) y sus "huecos". */
extern uint8_t _ap_trampolim[];
extern uint8_t _ap_tam[];
extern uint8_t _ap_gdt_local[];
extern uint32_t _ap_gdt_base[];
extern uint32_t _ap_cr0[];
extern uint32_t _ap_cr3[];
extern uint32_t _ap_cr4[];
extern uint32_t _ap_nxe[];
extern uint32_t _ap_xd[];
extern uint32_t _ap_pilha[];
extern uint32_t _ap_entrada[];
extern uint32_t _ap_indice[];
extern uint32_t _ap_progresso[];

/* Cuántos procesadores secundarios han arrancado. */
static volatile uint32_t arrancadas;

/* El TSC, para medir el tiempo (la frecuencia se midió en el arranque). */
static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void esperar_us(uint32_t us)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz) {
        for (volatile uint32_t i = 0; i < us * 20u; i++)
            ;
        return;
    }
    /* khz/1000 ciclos por microsegundo; en 32 bits (el kernel no tiene __udivdi3). */
    uint64_t alvo = ler_tsc() + (uint64_t)(khz / 1000u) * us;
    while (ler_tsc() < alvo)
        __asm__ volatile("pause");
}

static void esperar_ms(uint32_t ms)
{
    while (ms--)
        esperar_us(1000);
}

static inline uint32_t ler_cr0(void) { uint32_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline uint32_t ler_cr4(void) { uint32_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }

/* La dirección de un "hueco" en la copia del trampolín de la memoria baja: la
 * del símbolo más su desplazamiento dentro del bloque. */
static volatile uint32_t *buraco(uint32_t *simbolo)
{
    uint32_t deslocamento = (uint32_t)((uint8_t *)simbolo - (uint8_t *)_ap_trampolim);
    return (volatile uint32_t *)(SMP_TRAMPOLIM + deslocamento);
}

uint32_t smp_arrancadas(void) { return arrancadas; }

/* La IPI de despertar (D164): la CPU que la recibe, si está ociosa,
 * planifica ya en vez de esperar a su tic. */
static quadro_t *ao_acordar(quadro_t *q)
{
    lapic_fim();
    processo_t *a = processo_atual();
    if (a && (a->pid == 0 || a->estado != P_EXECUTANDO || planificador_troca_pendente(cpu_atual())))
        return escalonar(q);   /* D174: o el planificador pidió cambiar */   /* con el cerrojo general: lo tomó la entrada (D165) */
    return q;
}

void smp_acordar(uint32_t cpu)
{
    if (!arrancadas || cpu >= CPU_MAX || cpu == cpu_atual()) return;
    cpu_dados_t *d = cpu_dados(cpu);
    if (cpu != 0 && d->estado == CPU_LIVRE) return;   /* no arrancó */
    lapic_ipi(d->apic_id, VETOR_ACORDAR);
}

/* La entrada del procesador secundario: la llama el trampolín, con el número
 * de la CPU como argumento, la paginación activa y los segmentos planos de la
 * GDT local. Lo primero es cargar su GDT, su GS, su TSS y la IDT: hasta
 * entonces no se puede usar nada que lea los datos de la CPU por GS. */
void smp_ap_entrada(uint32_t indice)
{
    gdt_carregar(indice);
    idt_carregar();
    /* D381: un proceso con YMM puede pasar a cualquier CPU: si esta no puede encender AVX como
     * el principal, se queda parada y el kernel no la cuenta (como sin NX, paso 9). */
    if (!fpu_iniciar_secundaria()) {
        *buraco(_ap_progresso) = 10;
        for (;;) __asm__ volatile("cli; hlt");
    }
    uint64_t pat = wc_iniciar_secundaria();   /* D198: la misma PAT/MTRR que el principal (el framebuffer en WC también aquí) */
    cpu_definir_indice(indice);
    cpu_topologia_ler(indice);   /* D174: su núcleo físico, por CPUID en esta misma CPU */
    lapic_ativar();       /* su APIC local; los dispositivos siguen en la CPU 0 */
    /* Su ocioso: el contexto propio de este procesador (D157). */
    processo_ocioso_preparar(indice);
    cpu_dados(indice)->estado = CPU_PRONTA;
    atomic_inc32(&arrancadas);
    klog("[SAGUI] CPU %u arrancou (APIC %u)\n", indice, cpu_dados(indice)->apic_id);
    klog("[VIDEO] CPU %u: PAT %x:%x (D198: la misma que el principal)\n", indice, (uint32_t)(pat >> 32), (uint32_t)pat);
    /* Su propio tic: el temporizador del APIC local, medido contra el TSC
     * (D157). El planificador corre aquí; los dispositivos son del principal. */
    if (!lapic_temporizador_iniciar(VETOR_TIC_CPU))
        klog("[SAGUI] CPU %u sin tic propio (el principal le hace las veces)\n", indice);
    /* La espera ociosa, sin el cerrojo general (D165): las interrupciones solo
     * entran en el "hlt" de espera_sem_trava, así que esta CPU nunca está en
     * otro punto de este bucle con el cerrojo tomado. */
    for (;;)
        espera_sem_trava();
}

/* Lo que significa cada paso del trampolín (para el log). */
static const char *passo_nome(uint32_t p)
{
    switch (p) {
    case 0: return "no ha empezado (no recibió la SIPI)";
    case 1: return "modo real";
    case 2: return "modo protegido";
    case 3: return "CR4 y CR3 cargados";
    case 4: return "paginación activa";
    case 5: return "saltando a C";
    case 9: return "sin NX en esta CPU (XD desactivado): se deja parada";   /* D349 */
    case 10: return "sin AVX/XSAVE en esta CPU (el principal lo tiene): se deja parada";   /* D381 */
    default: return "?";
    }
}

/* Espera a que la CPU "indice" se anuncie, hasta "ms" milisegundos. Mientras
 * tanto apunta en el log cada paso nuevo del trampolín: si la CPU tumba la
 * máquina, el último paso escrito dice dónde. */
static bool esperar_anuncio(uint32_t indice, uint32_t ms, uint32_t *passo)
{
    volatile uint32_t *prog = buraco(_ap_progresso);
    volatile uint32_t *estado = &cpu_dados(indice)->estado;
    for (uint32_t t = 0; t < ms * 10; t++) {
        uint32_t p = *prog;
        if (p != *passo) {
            *passo = p;
            klog("[SMP] CPU %u: paso %u (%s)\n", indice, p, passo_nome(p));
        }
        if (*estado != CPU_LIVRE) return true;
        esperar_us(100);
    }
    return *estado != CPU_LIVRE;
}

uint32_t smp_iniciar(void)
{
    uint32_t ids[CPU_MAX];
    uint32_t n = cpu_lapic_ids(ids, CPU_MAX);
    if (n > CPU_MAX) n = CPU_MAX;

    /* Copia el trampolín a la memoria baja y escribe la base de la GDT local
     * (que el trampolín carga en modo real) y lo que es igual para todas las
     * CPU: los registros de control del principal y la entrada. */
    uint32_t tam = (uint32_t)(_ap_tam - _ap_trampolim);
    memcpy((void *)SMP_TRAMPOLIM, _ap_trampolim, tam);
    *buraco(_ap_gdt_base) = SMP_TRAMPOLIM + (uint32_t)(_ap_gdt_local - _ap_trampolim);
    *buraco(_ap_cr0) = ler_cr0();
    /* D381: sin OSXSAVE; fpu_iniciar_secundaria() lo pone después de mirar CPUID en esa CPU. */
    *buraco(_ap_cr4) = ler_cr4() & ~(uint32_t)0x40000;
    *buraco(_ap_cr3) = espaco_kernel();   /* con PAE, el PDPT (D349) */
    *buraco(_ap_nxe) = paginacao_nx() ? 1 : 0;   /* D349: EFER.NXE en cada CPU */
    *buraco(_ap_xd) = paginacao_xd_liberado() ? 1 : 0;
    *buraco(_ap_entrada) = (uint32_t)smp_ap_entrada;

    /* El vector del tic de cada procesador: el mismo para todos, porque la IDT
     * es compartida (D157). */
    idt_registrar(VETOR_TIC_CPU, relogio_tic_cpu);
    idt_registrar(VETOR_ACORDAR, ao_acordar);

    /* Nuestro propio identificador: la MADT puede no listar el principal. */
    uint32_t meu = lapic_id();
    cpu_dados(0)->apic_id = meu;
    /* El principal también recibe IPI (los secundarios lo despiertan cuando
     * le pasan un programa que pidió el kernel, D164). */
    if (!lapic_ativar_principal())
        klog("[SMP] el APIC local del principal estaba apagado: encendido en modo cable virtual\n");

    uint32_t indice = 1;     /* el principal es el 0 */
    uint32_t ligadas = 1;

    for (uint32_t i = 0; i < n && indice < CPU_MAX; i++) {
        uint32_t apic_id = ids[i];
        if (apic_id == meu) continue;

        gdt_preparar(indice);
        cpu_dados(indice)->apic_id = apic_id;
        cpu_dados(indice)->pilha_topo = cpu_pilha_topo(indice);
        cpu_dados(indice)->estado = CPU_LIVRE;

        *buraco(_ap_pilha) = cpu_pilha_topo(indice);
        *buraco(_ap_indice) = indice;
        *buraco(_ap_progresso) = 0;

        /* La secuencia de Intel (SDM vol. 3, 8.4.4): INIT, 10 ms, SIPI y,
         * si la CPU no ha empezado, otra SIPI. Los modos del ICR son 5 (INIT)
         * y 6 (SIPI) en el hardware, en el TCG y en el KVM. */
        uint32_t passo = 0;
        lapic_init(apic_id);
        esperar_ms(10);
        lapic_sipi(apic_id, SMP_VETOR_SIPI);
        esperar_us(200);
        if (*buraco(_ap_progresso) == 0)
            lapic_sipi(apic_id, SMP_VETOR_SIPI);

        if (!esperar_anuncio(indice, 500, &passo)) {
            klog("[SAGUI] SMP: la CPU %u (APIC %u) no arrancó: se quedó en el paso %u (%s)\n",
                 indice, apic_id, passo, passo_nome(passo));
            /* El índice no se reutiliza: si la CPU arrancara tarde, no puede
             * compartir los datos ni la pila con la siguiente. */
            indice++;
            continue;
        }
        ligadas++;
        indice++;
    }
    klog("[SAGUI] SMP: %u procesador(es) trabajando (%u declarados en la MADT)\n", ligadas, n);
    return ligadas;
}

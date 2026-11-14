// SPDX-License-Identifier: GPL-3.0-only
/* gdt.c - tabla de segmentos del kernel y la TSS (D157: una por CPU).
 *
 * SaguiOS usa el modelo "plano": todos los segmentos empiezan en 0 y cubren
 * 4 GB, y la protección de memoria de verdad viene de la paginación (fase 3). Aun
 * así la GDT es obligatoria, porque es ella la que define el nivel de privilegio:
 * código y datos del kernel en el anillo 0, código y datos de los programas en el anillo 3.
 *
 * La TSS existe por un único motivo: cuando un programa en el anillo 3 sufre una
 * interrupción, el procesador necesita saber qué pila del kernel usar, y lee
 * eso de los campos esp0/ss0 de la TSS. El planificador cambia esp0 en cada proceso.
 *
 * Con varios procesadores (D157), cada CPU tiene su propia copia de la GDT:
 *   - la TSS es de esa CPU (el esp0 lo cambia el planificador de esa CPU);
 *   - el descriptor de GS apunta para a los datos de esa CPU, así que el
 *     kernel lee "sus" variables con "mov %gs:offset" sin saber qué CPU es.
 * El descriptor de GS tiene o mesmo número em todas as cópias, porque cada
 * CPU carrega a sua: o mesmo selector, base diferente. */
#include "cpu.h"
#include "cpu_dados.h"
#include "texto.h"

typedef struct EMPACOTADO {
    uint32_t anterior;
    uint32_t esp0, ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, mapa_es;
} tss_t;

/* Quantas entradas tem a GDT: nulo, código do kernel, dados do kernel,
 * código do utilizador, dados do utilizador, TSS e GS desta CPU. */
#define GDT_ENTRADAS 7

/* Uma GDT e uma TSS por CPU. */
static uint64_t gdt_por_cpu[CPU_MAX][GDT_ENTRADAS] ALINHADO(16);
static tss_t tss_por_cpu[CPU_MAX] ALINHADO(16);

/* Os descritores que o lgdt precisa (limite e base). Uma por CPU. */
gdt_reg_t gdt_registro[CPU_MAX];

/* Monta un descriptor de segmento. 'acesso' trae presente/anillo/tipo y
 * 'flags' trae granularidad (4 KB) y tamaño (32 bits), como en el manual de Intel. */
static uint64_t descritor(uint32_t base, uint32_t limite, uint8_t acesso, uint8_t flags)
{
    uint64_t d = 0;
    d |= limite & 0xFFFF;
    d |= (uint64_t)(base & 0xFFFFFF) << 16;
    d |= (uint64_t)acesso << 40;
    d |= (uint64_t)((limite >> 16) & 0xF) << 48;
    d |= (uint64_t)(flags & 0xF) << 52;
    d |= (uint64_t)((base >> 24) & 0xFF) << 56;
    return d;
}

/* Prepara a GDT e a TSS da CPU indicada (sem as carregar: o processador
 * principal carrega a sua; os secundários carregam a deles no trampolín). */
void gdt_preparar(uint32_t indice)
{
    if (indice >= CPU_MAX) return;
    uint64_t *g = gdt_por_cpu[indice];
    tss_t *t = &tss_por_cpu[indice];

    g[0] = 0;
    g[1] = descritor(0, 0xFFFFF, 0x9A, 0xC);   /* código, anillo 0 */
    g[2] = descritor(0, 0xFFFFF, 0x92, 0xC);   /* datos, anillo 0 */
    g[3] = descritor(0, 0xFFFFF, 0xFA, 0xC);   /* código, anillo 3 */
    g[4] = descritor(0, 0xFFFFF, 0xF2, 0xC);   /* datos, anillo 3 */
    memset(t, 0, sizeof *t);
    t->ss0 = SEL_KDADOS;
    t->mapa_es = sizeof(tss_t);                    /* sin mapa de puertos: el anillo 3 no accede a E/S */
    g[5] = descritor((uint32_t)t, sizeof(tss_t) - 1, 0x89, 0x0);  /* TSS de 32 bits disponible */
    /* O GS desta CPU: base na linha de dados desta CPU, limite a linha. */
    g[6] = descritor((uint32_t)cpu_dados(indice), sizeof(cpu_dados_t) - 1, 0x92, 0x4);

    gdt_registro[indice].limite = sizeof gdt_por_cpu[0] - 1;
    gdt_registro[indice].base = (uint32_t)g;
}

void tss_definir_pilha(uint32_t esp0)
{
    uint32_t cpu = cpu_atual();
    if (cpu >= CPU_MAX) return;
    tss_por_cpu[cpu].esp0 = esp0;
}

/* Carga en la CPU que ejecuta esto su GDT, recarga CS y los segmentos de
 * datos, apunta GS a sus datos y carga su TSS (D164). La usan el principal
 * (gdt_iniciar) y los secundarios (smp_ap_entrada, ya con la paginación
 * activa): el mismo camino para todos. */
void gdt_carregar(uint32_t indice)
{
    if (indice >= CPU_MAX) return;
    __asm__ volatile(
        "lgdt %0\n\t"
        "ljmp %1, $1f\n\t"                          /* recarga CS con el selector nuevo */
        "1:\n\t"
        "mov %2, %%ax\n\t"
        "mov %%ax, %%ds\n\t mov %%ax, %%es\n\t mov %%ax, %%fs\n\t mov %%ax, %%ss\n\t"
        "mov %3, %%ax\n\t"
        "mov %%ax, %%gs\n\t"                        /* GS: os dados desta CPU */
        "mov %4, %%ax\n\t"
        "ltr %%ax"
        : : "m"(gdt_registro[indice]), "i"(SEL_KCODIGO), "i"(SEL_KDADOS), "i"(SEL_GS_CPU), "i"(SEL_TSS) : "eax", "memory");
}

void gdt_iniciar(void)
{
    gdt_preparar(0);
    cpu_definir_indice(0);
    gdt_carregar(0);
    cpu_gs_ligar();   /* el GS ya apunta a los datos de esta CPU (D157) */
}

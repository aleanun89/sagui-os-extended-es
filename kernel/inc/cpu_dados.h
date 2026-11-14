// SPDX-License-Identifier: GPL-2.0-or-later
/* cpu_dados.h - datos por CPU: la base del SMP (D150) y su uso real (D157).
 *
 * Cada procesador tiene su propia entrada en una tabla: una entrada por
 * CPU, cada una en su propia línea de caché (para que dos CPUs no
 * compartan línea y se estorben). En la entrada están el número de CPU,
 * su identificador de APIC, la pila del kernel, la rebanada del
 * planificador y el proceso que esa CPU está ejecutando.
 *
 * Cómo sabe el kernel qué CPU está ejecutando el código: cada CPU tiene
 * su propia GDT (con su propia TSS y su propio descriptor de GS), y el
 * descriptor de GS aponta para a la entrada desta CPU na tabela. Assim, o
 * kernel lê os seus dados com "mov %gs:offset" sem precisar de saber o
 * número da CPU: o segmento já o diz. É a mesma ideia do Linux com o GS.
 *
 * Com um só processador, cpu_atual() devolve sempre 0 e só se usa a
 * entrada 0 da tabela. */
#ifndef SAGUI_CPU_DADOS_H
#define SAGUI_CPU_DADOS_H
#include "tipos.h"

/* Cuántos procesadores se soportan. De sobra por ahora: la tabla es
 * estática y no cuesta nada ampliarla. */
#define CPU_MAX 8

/* Una línea de caché de 32 bits son 64 bytes: los datos de cada CPU
 * empiezan en una línea propia, para que dos CPUs no compartan la misma. */
#define CPU_LINHA_CACHE 64

/* La pila del kernel de cada CPU. Tiene que ser igual que PILHA_KERNEL
 * (proceso.h); cpu_dados.c lo comprueba al compilar. */
#define CPU_PILHA_KERNEL 8192

struct processo;

/* Los estados de una CPU. */
#define CPU_LIVRE      0   /* ainda não arrancou */
#define CPU_PRONTA     1   /* arrancou e está a trabalhar */
#define CPU_OCIOSA     2   /* arrancou e está à espera de trabalho */

/* A linha de cache tem 64 bytes: 16 palavras de 32 bits. A ordem dos
 * campos importa, porque o acesso por GS usa os deslocamentos fixos
 * CPU_OFF_INDICE e CPU_OFF_ATUAL (comprovados ao compilar). */
typedef struct cpu_dados {
    uint32_t indice;          /* 0: número de CPU (0..CPU_MAX-1) */
    uint32_t nucleo;          /* clave del núcleo físico por CPUID: dos CPUs pueden compartirla (D174) */
    uint32_t tiques;          /* tiques de CPU gastados: para repartir carga */
    uint32_t estado;          /* CPU_LIVRE, CPU_PRONTA, CPU_OCIOSA */
    uint32_t fatia;           /* tiques que le quedan al proceso actual */
    uint32_t pilha_topo;      /* topo da pilha de kernel desta CPU */
    uint32_t apic_id;         /* identificador do APIC local */
    struct processo *atual;   /* 28: o processo que esta CPU está a executar */
    uint32_t ultimo;          /* 32: o último processo escolhido nesta CPU */
    uint32_t tiques_ociosos;  /* 36: tiques em que esta CPU não tinha processo nenhum */
    uint32_t reservado;       /* 40: libre (era el proceso anterior de la D164) */
    uint32_t acordar;         /* 44: CPUs (un bit cada una) a despertar al salir del kernel (D165) */
    uint32_t em_irq;          /* 48: IRQ de hardware anidadas en esta CPU (D165) */
    volatile uint32_t esperando_trava;  /* 52: esperando el cerrojo general: no ejecuta el programa (D165) */
    volatile uint32_t recarregar_cr3;   /* 56: otra CPU quitó páginas del espacio de esta (D165) */
    uint32_t extra[1];        /* hasta completar la línea de caché (64 bytes) */
} cpu_dados_t;

/* Deslocamentos dentro da linha: o acesso por GS usa-os. */
#define CPU_OFF_INDICE 0
#define CPU_OFF_ATUAL  28

/* A leitura por GS: o segmento aponta para a entrada da CPU que executa
 * este código, así que não é preciso saber o número da CPU. */

/* O GS só está preparado depois de a GDT estar montada (gdt_iniciar). Antes
 * disso, o kernel ainda não tem processos e usa o valor global. */
void cpu_gs_ligar(void);
bool cpu_gs_lista(void);
static inline uint32_t cpu_indice_gs(void)
{
    uint32_t v;
    __asm__ volatile("movl %%gs:%c1, %0" : "=r"(v) : "i"(CPU_OFF_INDICE));
    return v;
}

static inline struct processo *cpu_processo_gs(void)
{
    uint32_t v;
    __asm__ volatile("movl %%gs:%c1, %0" : "=r"(v) : "i"(CPU_OFF_ATUAL));
    return (struct processo *)v;
}

/* A CPU que ejecuta este código (0 antes de que la GDT esté montada). */
uint32_t cpu_atual(void);

/* Anuncia qué CPU es: lo usará cada procesador secundario al arrancar. */
void cpu_definir_indice(uint32_t indice);

/* Los datos de esa CPU (NULL si el número no existe). */
cpu_dados_t *cpu_dados(uint32_t cpu);

/* La pila del kernel de esa CPU (NULL si el número no existe). */
uint8_t *cpu_pilha(uint32_t cpu);

/* El topo de la pila del kernel de esa CPU (0 si no existe). */
uint32_t cpu_pilha_topo(uint32_t cpu);

/* Cuántos procesadores dice la máquina que tiene. Se cuenta con la MADT
 * del ACPI (la tabla "APIC"), que es donde el firmware dice cuántos
 * procesadores hay. NO los arranca: eso es trabajo del SMP (D157). */
uint32_t cpu_numero(void);

/* Lê a MADT do ACPI e guarda os identificadores dos APIC locais que o
 * firmware declara. Tem de ser chamada ANTES da paginação: as tabelas do
 * ACPI podem estar fora do mapa 1:1 do kernel. */
void cpu_lapic_capturar(void);

/* A base física do APIC local (vem da MADT). */
uint32_t cpu_lapic_fisico(void);

/* Os identificadores dos APIC locais que a MADT declara, por ordem. São os
 * processadores que o firmware diz que existem (o principal é o primeiro).
 * Devolve quantos escreveu (no máximo "maximo"). */
uint32_t cpu_lapic_ids(uint32_t *destino, uint32_t maximo);

/* La topología (D174): qué CPU lógicas son hilos (Hyper-Threading/SMT) del
 * mismo núcleo físico y de qué paquete. Cada CPU la lee con CPUID sobre sí
 * misma al arrancar (la CPU 0 antes del SMP; cada secundario en su
 * entrada). Sin datos de CPUID, cada CPU es su propio núcleo. Es el sitio
 * común: el planificador y el driver de frecuencia leen lo mismo. */
void        cpu_topologia_ler(uint32_t cpu);     /* en la propia CPU "cpu" */
uint32_t    cpu_nucleo_fisico(uint32_t cpu);     /* núcleo físico, numerado desde 0 sin huecos */
uint32_t    cpu_pacote(uint32_t cpu);            /* paquete (zócalo), desde 0 */
uint32_t    cpu_hilo_do_nucleo(uint32_t cpu);    /* 0 el primer hilo del núcleo, 1 el hermano... */
bool        cpu_irmas(uint32_t a, uint32_t b);   /* dos CPU distintas del mismo núcleo físico */
const char *cpu_topologia_metodo(void);          /* de dónde salió (para el log) */

#endif

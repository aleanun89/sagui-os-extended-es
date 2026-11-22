// SPDX-License-Identifier: GPL-3.0-only
/* cpu.h - segmentos, interrupciones y el marco guardado en la pila en cada interrupción. */
#ifndef SAGUI_CPU_H
#define SAGUI_CPU_H
#include "tipos.h"
#include "cpu_dados.h"

/* Selectores de la GDT del kernel (índice * 8 + nivel de privilegio). */
#define SEL_KCODIGO  0x08
#define SEL_KDADOS   0x10
#define SEL_UCODIGO  0x1B   /* anillo 3 */
#define SEL_UDADOS   0x23   /* anillo 3 */
#define SEL_TSS      0x28   /* la TSS de esta CPU (cada CPU tiene la suya, D157) */
#define SEL_GS_CPU   0x30   /* el GS de esta CPU: apunta a sus datos (D157) */

/* El descritor que necesita el lgdt: el límite y la base de la GDT. Cada
 * CPU tiene el suyo, porque cada CPU carga su propia copia (D157). */
typedef struct EMPACOTADO {
    uint16_t limite;
    uint32_t base;
} gdt_reg_t;

/* El descriptor de cada CPU (D157): cada processador carrega o seu. */
extern gdt_reg_t gdt_registro[CPU_MAX];

#define VETOR_IRQ0   0x20   /* el PIC se remapea a 0x20-0x2F */
#define VETOR_SISTEMA 0x80  /* llamadas al sistema (fase 3) */
#define VETOR_CEDER   0x81
#define VETOR_ACORDAR 0x83   /* D164: IPI que despierta a una CPU ociosa para que planifique */
#define VETOR_PARAR   0x84   /* D450: IPI de apagar o reiniciar: el secundario se para (cli; hlt) sin el cerrojo general */
#define VETOR_TIC_CPU 0x82   /* o tic do temporizador do APIC local (D157) */  /* la tarea del kernel cede el turno (solo anillo 0) */

/* Todo lo que deja en la pila el código de entrada (kernel/arch/vetores.asm), desde
 * la dirección más baja a la más alta. El C recibe un puntero a esto y
 * devuelve el marco a reanudar, que puede ser de otro proceso (fase 3). */
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_kernel, ebx, edx, ecx, eax;  /* orden del pushad */
    uint32_t vetor, erro;
    uint32_t eip, cs, eflags;
    uint32_t esp_usuario, ss_usuario;  /* solo existen si la interrupción vino del anillo 3 */
} quadro_t;

typedef quadro_t *(*manipulador_t)(quadro_t *q);

void gdt_iniciar(void);
void gdt_preparar(uint32_t indice);   /* D157: prepara la GDT y la TSS de esa CPU */
void gdt_carregar(uint32_t indice);   /* D164: carga en esta CPU su GDT, sus segmentos, su GS y su TSS */
void idt_carregar(void);              /* D164: carga la IDT del kernel en esta CPU */
void tss_definir_pilha(uint32_t esp0);
void idt_iniciar(void);
const void *idt_descritor(void);   /* D157: el descriptor que necesita el lidt */
void idt_registrar(uint8_t vetor, manipulador_t m);
void irq_registrar(uint8_t irq, manipulador_t m);
void irq_registrar_extra(uint8_t irq, manipulador_t m);
bool irq_atendida_por_outro(void);   /* D338: dentro de un manipulador: un atendedor ya reconoció esta IRQ */   /* D095: atiende una línea que ya tiene dueño */
bool contexto_irq(void);        /* dentro de una IRQ de hardware: no puede dormir */
bool irq_ocupada(uint8_t irq);   /* ya tiene manipulador principal */
uint32_t irq_contagem(uint8_t irq);   /* D211: interrupciones atendidas en la línea */
uint32_t irq_por_segundo(uint8_t irq);   /* D244: interrupciones por segundo (última ventana) */
uint32_t irq_tormentas(uint8_t irq);     /* D244: tormentas detectadas en la línea */
bool     irq_mascarada(uint8_t irq);     /* D244: ahora enmascarada por una tormenta */
uint16_t irq_linhas_em_uso(void);     /* D211: líneas con manipulador, extra o atendedor */
void irq_compartilhar(uint8_t irq, bool (*atender)(void));   /* atendedor extra en la línea (D089) */
/* Máscara de una IRQ (0-15) en el controlador activo: el 8259 o el IOAPIC (D166). */
void irq_habilitar(uint8_t irq);
void irq_desabilitar(uint8_t irq);

void pic_iniciar(void);
void pic_habilitar(uint8_t irq);
void pic_desabilitar(uint8_t irq);
void pic_fim(uint8_t irq);
bool pic_espuria(uint8_t irq);
bool pic_em_servico(uint8_t irq);   /* D211: el bit de la IRQ en el ISR del 8259 */
uint16_t pic_pedidos(void);         /* D302: el IRR de los dos 8259 (bit = línea) */

SEM_RETORNO void excecao_fatal(quadro_t *q);
quadro_t *excecao_usuario(quadro_t *q);
const char *nome_excecao(uint32_t vetor);

static inline void int_liga(void)    { __asm__ volatile("sti"); }
static inline void int_desliga(void) { __asm__ volatile("cli"); }
void cpu_espera(void);   /* hlt; suelta el cerrojo general mientras duerme (D165, trava_geral.c) */

/* Desactiva las interrupciones y devuelve si estaban activadas, para restaurar después. */
static inline bool int_salva_desliga(void)
{
    uint32_t f;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(f));
    return (f & 0x200) != 0;
}
static inline void int_restaura(bool estavam)
{
    if (estavam) int_liga();
}

static inline uint32_t le_cr0(void) { uint32_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline uint32_t le_cr2(void) { uint32_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v; }
static inline uint32_t le_cr3(void) { uint32_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }

SEM_RETORNO void parar_maquina(void);

#endif

// SPDX-License-Identifier: GPL-3.0-only
/* idt.c - tabla de interrupciones y el despachador.
 *
 * Todas las 256 entradas apuntan a los trozos de kernel/arch/vetores.asm,
 * que llaman a tratar_interrupcao(). Quien quiera tratar un vector se registra en la
 * tabla "manipuladores"; el despachador decide el resto:
 *   - IRQ de hardware (0x20-0x2F): descarta las espurias, llama al
 *     manipulador y avisa del fin al controlador que la entregó (el PIC
 *     8259 o, con --ioapic, el APIC local, D166);
 *   - excepción de la CPU sin manipulador: pantalla de error y parada;
 *   - cualquier otra cosa sin manipulador: registra y ignora. */
#include "cpu.h"
#include "console.h"
#include "aleatorio.h"
#include "processo.h"
#include "lapic.h"
#include "ioapic.h"
#include "cpu_dados.h"
#include "trava_geral.h"
#include "info_hardware.h"
#include "irq_rotas.h"
#include "bootinfo.h"

typedef struct EMPACOTADO {
    uint16_t desloc_baixo;
    uint16_t seletor;
    uint8_t  zero;
    uint8_t  tipo;      /* presente, anillo permitido para "int n", tipo de puerta */
    uint16_t desloc_alto;
} porta_idt_t;

extern const uint32_t tabela_vetores[256];

static porta_idt_t idt[256] ALINHADO(8);
static manipulador_t manipuladores[256];

/* El descriptor que necesita el lidt. Lo usan también los procesadores
 * secundarios, que cargan la misma IDT (D157). */
static gdt_reg_t idt_reg;
/* Segundo manipulador por IRQ, para líneas PCI compartidas (D095): el
 * driver principal (red, sonido...) sigue siendo dueño de la línea y el EHCI atiende a
 * misma línea sin tomarla. Cada uno comprueba si la interrupción es de su
 * dispositivo. */
static manipulador_t extras[16];
/* Las IRQ anidadas son de cada CPU (cpu_dados->em_irq, D165): antes del GS
 * (el arranque) se usa este contador. */
static uint32_t em_irq_arranque;

static uint32_t *em_irq(void)
{
    return cpu_gs_lista() ? &cpu_dados(cpu_atual())->em_irq : &em_irq_arranque;
}

static void definir_porta(int v, uint32_t endereco, uint8_t tipo)
{
    idt[v].desloc_baixo = endereco & 0xFFFF;
    idt[v].desloc_alto = endereco >> 16;
    idt[v].seletor = SEL_KCODIGO;
    idt[v].zero = 0;
    idt[v].tipo = tipo;
}

void idt_iniciar(void)
{
    /* 0x8E = puerta de interrupción de 32 bits, solo el kernel puede usar "int n".
     * Puerta de interrupción (y no de "trap") desactiva IF en la entrada: el kernel
     * no es interrumpido a mitad del tratamiento de otra interrupción. */
    for (int v = 0; v < 256; v++)
        definir_porta(v, tabela_vetores[v], 0x8E);
    /* La llamada al sistema necesita ser alcanzable desde el anillo 3. */
    definir_porta(VETOR_SISTEMA, tabela_vetores[VETOR_SISTEMA], 0xEE);

    idt_reg.limite = sizeof idt - 1;
    idt_reg.base = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idt_reg));
}

/* Carga la IDT ya montada en la CPU que ejecuta esto (los secundarios, D164). */
void idt_carregar(void)
{
    __asm__ volatile("lidt %0" : : "m"(idt_reg));
}

/* El descriptor de la IDT, para los que la cargan (los secundarios). */
const void *idt_descritor(void) { return &idt_reg; }

void idt_registrar(uint8_t vetor, manipulador_t m)
{
    manipuladores[vetor] = m;
}

/* La capa entre los drivers y el controlador (D166): con el IOAPIC activo
 * la máscara y el fin de interrupción van al IOAPIC y al APIC local; si no,
 * al 8259 de siempre. */
void irq_habilitar(uint8_t irq)
{
    if (irq >= 16) return;
    if (ioapic_ativo()) ioapic_mascarar(irq, false);
    else pic_habilitar(irq);
}

void irq_desabilitar(uint8_t irq)
{
    if (irq >= 16) return;
    if (ioapic_ativo()) ioapic_mascarar(irq, true);
    else pic_desabilitar(irq);
}

/* Las espurias del 8259 son las IRQ 7 y 15 sin bit en servicio; con el
 * IOAPIC no hay: el APIC local tiene su propio vector espurio (0xFF), que no
 * lleva EOI. Salvo si alguna línea entra por el 8259 (modo mixto, D211):
 * entonces sus espurias también pueden llegar a la CPU 0. */
static bool irq_espuria(uint8_t irq)
{
    if (!ioapic_ativo()) return pic_espuria(irq);
    if (ioapic_linhas_pic() && (irq == 7 || irq == 15) && cpu_atual() == 0 &&
        !lapic_em_servico((uint8_t)(VETOR_IRQ0 + irq)))
        return pic_espuria(irq);
    return false;
}

static void irq_fim(uint8_t irq)
{
    if (!ioapic_ativo()) { pic_fim(irq); return; }
    if (ioapic_linhas_pic()) {
        /* D211: el mismo vector pudo llegar por el IOAPIC (bit en el ISR del
         * APIC local) o por el 8259 en ExtINT (bit en el ISR del 8259, solo
         * en la CPU 0, la del cable virtual). Cada uno recibe su fin. */
        if (lapic_em_servico((uint8_t)(VETOR_IRQ0 + irq))) lapic_fim();
        if (cpu_atual() == 0 && pic_em_servico(irq)) pic_fim(irq);
        return;
    }
    lapic_fim();   /* el EOI del APIC local llega al IOAPIC (nivel) */
}

/* D211: interrupciones atendidas por línea (infopc y la autoprueba). */
static volatile uint32_t contagem[16];
uint32_t irq_contagem(uint8_t irq) { return irq < 16 ? contagem[irq] : 0; }

/* D244: detector de tormentas. Una línea que da más de LIMITE_TORMENTA interrupciones en un
 * segundo (el TSC mide el segundo) se enmascara unos segundos para que no se coma la CPU: los
 * drivers de esa línea (USB, red, sonido) ya saben sondear sin interrupción. Pasado el plazo se
 * desenmascara y, si vuelve a ocurrir, el plazo se duplica (hasta 40 s). La línea 0 (reloj) y la
 * 8 (RTC) nunca se enmascaran. Con CAB_IRQ_PRUEBA = IRQ_PRUEBA_TORMENTA se simula una en la IRQ 11
 * a los 2 s de la primera interrupción de hardware, para las pruebas. */
#define LIMITE_TORMENTA 20000u
static uint64_t jan_t0[16];                 /* TSC del principio de la ventana de medida */
static uint32_t jan_n0[16];                 /* contagem al principio de la ventana */
static volatile uint32_t taxa[16];          /* interrupciones por segundo de la última ventana */
static volatile uint32_t n_tormentas[16];   /* tormentas detectadas en la línea */
static uint64_t mascarada_ate[16];          /* TSC en que se vuelve a probar (0 = no enmascarada) */
static uint32_t espera_ms[16];              /* plazo de la próxima mascara */
static volatile uint16_t mascaradas;
static uint64_t sim_ate;
static bool sim_feita;

static inline uint64_t tsc_agora(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void tormenta_mascarar(uint8_t irq, uint32_t por_s, bool simulada)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz || irq == 0 || irq == 8) return;
    if (!espera_ms[irq]) espera_ms[irq] = 5000;
    else if (espera_ms[irq] < 40000) espera_ms[irq] *= 2;
    irq_desabilitar(irq);
    mascarada_ate[irq] = tsc_agora() + (uint64_t)khz * espera_ms[irq];
    mascaradas |= (uint16_t)(1u << irq);
    n_tormentas[irq]++;
    klog("[IRQ] tormenta en la IRQ %u (%u/s)%s: enmascarada; los drivers de esa linea pasan a sondeo y se reintenta en %u s\n",
         irq, por_s, simulada ? " (simulada)" : "", espera_ms[irq] / 1000);
}

/* En cada IRQ de hardware: desenmascara las que cumplieron su plazo y, con la opción de prueba,
 * simula la tormenta. Barato: una máscara y una comparación. */
static void tormenta_vigiar(void)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz) return;
    if (!sim_feita && (cabecalho_boot[CAB_IRQ_PRUEBA] & IRQ_PRUEBA_TORMENTA)) {
        uint64_t agora = tsc_agora();
        if (!sim_ate) sim_ate = agora + (uint64_t)khz * 2000;
        else if (agora >= sim_ate) { sim_feita = true; tormenta_mascarar(11, 50000, true); }
    }
    if (!mascaradas) return;
    uint64_t agora = tsc_agora();
    for (uint8_t i = 1; i < 16; i++)
        if ((mascaradas >> i) & 1 && agora >= mascarada_ate[i]) {
            mascaradas &= (uint16_t)~(1u << i);
            jan_t0[i] = agora; jan_n0[i] = contagem[i];
            irq_habilitar(i);
            klog("[IRQ] IRQ %u desenmascarada: se vuelve a probar\n", i);
        }
}

/* Cada 256 interrupciones de la línea: ¿pasó la ventana de un segundo? Si hubo más del límite, tormenta. */
static void tormenta_medir(uint8_t irq)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz) return;
    uint64_t agora = tsc_agora();
    if (!jan_t0[irq]) { jan_t0[irq] = agora; jan_n0[irq] = contagem[irq]; return; }
    uint64_t dt = agora - jan_t0[irq];
    uint32_t n = contagem[irq] - jan_n0[irq];
    if (dt >= (uint64_t)khz * 1000) {
        taxa[irq] = (uint32_t)((uint64_t)n * 1000 * khz / (dt ? dt : 1));
        jan_t0[irq] = agora; jan_n0[irq] = contagem[irq];
    } else if (n >= LIMITE_TORMENTA) {
        uint32_t por_s = (uint32_t)((uint64_t)n * 1000 * khz / (dt ? dt : 1));
        taxa[irq] = por_s;
        jan_t0[irq] = agora; jan_n0[irq] = contagem[irq];
        tormenta_mascarar(irq, por_s, false);
    }
}

uint32_t irq_por_segundo(uint8_t irq)
{
    if (irq >= 16) return 0;
    uint32_t khz = hw_tsc_khz();
    if (khz && jan_t0[irq]) {
        uint64_t dt = tsc_agora() - jan_t0[irq];
        if (dt >= (uint64_t)khz * 1000) {   /* ventana vencida: se cierra aquí */
            taxa[irq] = (uint32_t)((uint64_t)(contagem[irq] - jan_n0[irq]) * 1000 * khz / dt);
            jan_t0[irq] = tsc_agora(); jan_n0[irq] = contagem[irq];
        }
    }
    return taxa[irq];
}
uint32_t irq_tormentas(uint8_t irq) { return irq < 16 ? n_tormentas[irq] : 0; }
bool irq_mascarada(uint8_t irq) { return irq < 16 && ((mascaradas >> irq) & 1); }

bool irq_ocupada(uint8_t irq)
{
    return irq >= 16 || manipuladores[VETOR_IRQ0 + irq] != NULL;
}

void irq_registrar(uint8_t irq, manipulador_t m)
{
    manipuladores[VETOR_IRQ0 + irq] = m;
    irq_habilitar(irq);
}

/* Líneas PCI compartidas (D089): en QEMU y en muchos PCs la tarjeta de sonido
 * cae en la misma IRQ de la red o del USB. Un "atendedor" extra solo mira su
 * tarjeta y limpia la causa; se ejecuta antes del manipulador principal de la línea (que
 * puede cambiar de proceso) y no cambia de proceso. */
#define ATENDENTES 4
static bool (*atendentes[16][ATENDENTES])(void);

void irq_compartilhar(uint8_t irq, bool (*atender)(void))
{
    if (irq >= 16) return;
    for (int k = 0; k < ATENDENTES; k++)
        if (!atendentes[irq][k]) {
            atendentes[irq][k] = atender;
            irq_habilitar(irq);
            return;
        }
}

uint16_t irq_linhas_em_uso(void)
{
    uint16_t m = 0;
    for (int irq = 0; irq < 16; irq++)
        if (manipuladores[VETOR_IRQ0 + irq] || extras[irq] || atendentes[irq][0]) m |= (uint16_t)(1u << irq);
    return m;
}

void irq_registrar_extra(uint8_t irq, manipulador_t m)
{
    if (irq >= 16) return;
    extras[irq] = m;
    irq_habilitar(irq);
}

/* ¿Estamos tratando una IRQ de hardware en esta CPU? Quien puede dormir
 * (E/S USB) no puede hacerlo ahí: ningún otro proceso corre mientras la IRQ
 * no termina. */
bool contexto_irq(void) { return *em_irq() != 0; }

/* D165: cualquier CPU atiende las llamadas al sistema, los fallos y las
 * excepciones de sus programas, con el cerrojo general del kernel (un solo
 * dueño del kernel a la vez, trava_geral.c). Lo único que no lo toma de
 * entrada es el tic de un secundario: casi siempre solo cuenta tiempo, y lo
 * toma él si tiene que planificar. Las IRQ de dispositivos siguen llegando
 * a la CPU 0 por el PIC. */
quadro_t *tratar_interrupcao(quadro_t *q)
{
    uint32_t v = q->vetor;

    if (v == LAPIC_ESPURIO && !manipuladores[v])
        return q;                      /* espuria del APIC local: sin EOI ni aviso */
    if (v == VETOR_TIC_CPU && manipuladores[v])
        return manipuladores[v](q);    /* relogio_tic_cpu: toma el cerrojo si lo necesita */

    trava_geral_pegar();

    /* Un programa que otra CPU terminó mientras corría aquí (D165): su
     * llamada o su excepción ya no se atiende; la CPU pasa a otro. */
    processo_t *a = processo_atual();
    if ((q->cs & 3) == 3 && a && a->pid != 0 && a->estado == P_ZUMBI &&
        (v < 32 || v == VETOR_SISTEMA))
        return escalonar(q);

    quadro_t *r = q;
    if (v >= VETOR_IRQ0 && v < VETOR_IRQ0 + 16) {
        uint8_t irq = (uint8_t)(v - VETOR_IRQ0);
        if (irq_espuria(irq))
            return q;
        aleatorio_evento(irq ^ q->eip);   /* el instante de cada IRQ alimenta el generador (D061) */
        uint32_t *n = em_irq();
        (*n)++;
        contagem[irq]++;
        if (!(contagem[irq] & 255)) tormenta_medir(irq);
        tormenta_vigiar();
        for (int k = 0; k < ATENDENTES && atendentes[irq][k]; k++)
            atendentes[irq][k]();
        if (manipuladores[v])
            r = manipuladores[v](r);
        if (extras[irq])
            r = extras[irq](r);
        (*n)--;
        irq_fim(irq);
        uint32_t cpu = cpu_atual();
        if (cpu != 0) {
            /* D168: una IRQ repartida a un secundario. Lo que esperaba el
             * dispositivo puede estar en la CPU 0 (las tareas del kernel
             * duermen allí en cpu_espera): se la despierta al salir. */
            cpu_dados(cpu)->acordar |= 1u;
            static volatile uint16_t vista[CPU_MAX];
            if (cpu < CPU_MAX && !(vista[cpu] & (1u << irq))) {
                vista[cpu] |= (uint16_t)(1u << irq);
                klog("[IOAPIC] CPU %u atiende la IRQ %u\n", cpu, irq);
            }
        }
    } else if (manipuladores[v]) {
        r = manipuladores[v](q);
    } else if (v < 32) {
        if ((q->cs & 3) == 3)
            return excecao_usuario(q);   /* error de un programa: solo él termina */
        excecao_fatal(q);                /* error del kernel: no hay cómo continuar */
    } else {
        klog("[SAGUI] interrupcao %u sem manipulador\n", v);
    }
    /* Nunca se vuelve al código de un proceso terminado (por ejemplo, el
     * Ctrl+C que mata al proceso que estaba corriendo en esta CPU). */
    a = processo_atual();
    if (a && a->pid != 0 && a->estado == P_ZUMBI)
        r = escalonar(r);
    return r;
}

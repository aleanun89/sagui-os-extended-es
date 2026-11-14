// SPDX-License-Identifier: GPL-2.0-or-later
/* lapic.c - el APIC local de cada procesador (D157).
 *
 * Tres cosas:
 *   1. leer y escribir los registros del APIC local (0xFEE00000);
 *   2. la secuencia INIT-SIPI que enciende los procesadores secundarios;
 *   3. el temporizador local, que da a cada CPU su propio tic.
 *
 * El APIC está apagado hasta que se llama a lapic_ativar(). Con un solo
 * procesador no hace falta nada de esto: el PIC de siempre lleva los
 * dispositivos y el PIT da el tic. */
#include "lapic.h"
#include "cpu.h"
#include "info_hardware.h"
#include "texto.h"
#include "sensores_conv.h"

/* El registro: el índice es (dirección >> 4) & 0xFF. */
static inline volatile uint32_t *registro(uint32_t deslocamento)
{
    return (volatile uint32_t *)(LAPIC_BASE + deslocamento);
}

uint32_t lapic_ler(uint32_t deslocamento)   { return *registro(deslocamento); }
void     lapic_escrever(uint32_t d, uint32_t v) { *registro(d) = v; }

uint32_t lapic_id(void)
{
    return (lapic_ler(LAPIC_ID) >> 24) & 0xFF;
}

/* El TSC, para medir el tiempo. */
static inline uint64_t ler_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void lapic_ativar(void)
{
    /* Las dos líneas externas (LINT0 y LINT1) se enmascaran: los
     * dispositivos de entrada y salida los lleva el procesador principal
     * con el PIC de siempre. El temporizador se deja así hasta que se
     * calibre su frecuencia. */
    lapic_escrever(LAPIC_LINT0, 0x10000);        /* LINT0: enmascarada */
    lapic_escrever(LAPIC_LINT1, 0x10000);        /* LINT1: enmascarada */
    lapic_escrever(LAPIC_SVR, LAPIC_ESPURIO | 0x100);
}

/* Espera a que el registro de comando quede libre (bit 12: "entrega en
 * curso"). En el QEMU el comando se atiende al momento, pero en una máquina
 * de verdad hay que esperar. */
static void esperar_icr(void)
{
    for (int i = 0; i < 1000; i++) {
        if (!(lapic_ler(LAPIC_ICR) & 0x1000)) return;
        __asm__ volatile("nop");
    }
}

void lapic_init(uint32_t destino)
{
    esperar_icr();
    lapic_escrever(LAPIC_ICR_ALTO, (destino & 0xFF) << 24);
    /* Modo INIT (5), disparo por nivel (bit 15) con el nivel afirmado
     * (bit 14), como hace Linux: el procesador se reinicia y queda esperando
     * la SIPI (no ejecuta nada hasta entonces). */
    lapic_escrever(LAPIC_ICR, LAPIC_MODO_INIT | 0xC000);
    esperar_icr();
}

void lapic_sipi(uint32_t destino, uint8_t vetor)
{
    /* El que llama espera los 10 ms de Intel entre el INIT y la SIPI (D164). */
    esperar_icr();
    lapic_escrever(LAPIC_ICR_ALTO, (destino & 0xFF) << 24);
    /* Modo SIPI (6): el procesador empieza en modo real en la página
     * vector << 12 (aquí, 0x7000). */
    lapic_escrever(LAPIC_ICR, LAPIC_MODO_SIPI | (uint32_t)vetor);
    esperar_icr();
}

void lapic_ipi(uint32_t destino, uint8_t vetor)
{
    esperar_icr();
    lapic_escrever(LAPIC_ICR_ALTO, (destino & 0xFF) << 24);
    lapic_escrever(LAPIC_ICR, LAPIC_MODO_FIXO | (uint32_t)vetor);
}

bool lapic_ativar_principal(void)
{
    /* El principal recibe los dispositivos por el PIC a través de LINT0
     * (ExtINT, el "cable virtual" que deja la BIOS): eso no se toca. Solo
     * hace falta que el APIC esté activo por software para aceptar IPI. Si
     * la BIOS lo dejó apagado, se enciende con LINT0 en ExtINT y LINT1 en NMI,
     * que es la configuración del cable virtual. */
    uint32_t svr = lapic_ler(LAPIC_SVR);
    if (svr & 0x100) return true;
    lapic_escrever(LAPIC_LINT0, 0x700);           /* ExtINT */
    lapic_escrever(LAPIC_LINT1, 0x400);           /* NMI */
    lapic_escrever(LAPIC_SVR, LAPIC_ESPURIO | 0x100);
    return false;
}

void lapic_ipi_todos(uint8_t vetor)
{
    esperar_icr();
    /* Atajo de destino 3: "todos menos yo" (bits 19-18). El modo de
     * entrega es el fijo (0): una interrupción normal. */
    lapic_escrever(LAPIC_ICR, (3u << 18) | LAPIC_MODO_FIXO | (uint32_t)vetor);
}

void lapic_fim(void)
{
    /* O EOI tira o bit "em serviço" do vector: e' o que permite ao APIC
     * voltar a entregar esse vector (e nao bloqueia os de prioridade menor
     * enquanto o manipulador nao acaba). */
    lapic_escrever(LAPIC_EOI, 0);
}

bool lapic_temporizador_iniciar(uint8_t vetor)
{
    uint32_t khz = hw_tsc_khz();
    if (!khz) return false;   /* sin TSC não há como medir: fica sem timer */

    bool i = int_salva_desliga();
    /* O timer em modo periódico, sem divide, com a conta ao máximo: assim
     * não expira durante a medida (seria preciso mais de 400 GHz para
     * queimar a conta em 10 ms). */
    lapic_escrever(LAPIC_DIV, LAPIC_DIVISOR_1);     /* divide por 1 */
    /* Periódico (o bit 17 no QEMU) e sem máscara (o bit 16 é a máscara, não
     * o modo): com a máscara o temporizador nunca tocava, e sem o bit 17 era
     * de um só disparo e também não repetia (D157). */
    lapic_escrever(LAPIC_LVT_TIMER, LAPIC_LVT_PERIODICO | vetor);
    lapic_escrever(LAPIC_TIMER_INI, 0xFFFFFFFFu);

    uint32_t c1 = lapic_ler(LAPIC_TIMER_CNT);
    uint64_t t1 = ler_tsc();
    uint64_t alvo = t1 + (uint64_t)khz * 10;   /* 10 ms */
    while (ler_tsc() < alvo)
        __asm__ volatile("pause");
    uint32_t c2 = lapic_ler(LAPIC_TIMER_CNT);
    uint64_t t2 = ler_tsc();

    uint32_t desceu = c1 - c2;          /* quantos tiques o timer baixou */
    uint64_t ciclos = t2 - t1;          /* ciclos do TSC */
    if (!desceu || !ciclos) { int_restaura(i); return false; }

    /* frequência do timer em Hz: tiques por ciclo do TSC. A divisão é de
     * 64 bits, e o kernel não tem a biblioteca do GCC: usa a sua. */
    uint64_t hz = sconv_div64((uint64_t)desceu * (uint64_t)khz * 1000ull, (uint32_t)ciclos);
    uint64_t conta = hz / 100;          /* 100 Hz: um tic cada 10 ms */
    if (!conta || conta > 0xFFFFFFFFull) { int_restaura(i); return false; }

    lapic_escrever(LAPIC_TIMER_INI, (uint32_t)conta);
    int_restaura(i);
    return true;
}

/* D211: el ISR son ocho registros de 32 bits (0x100, 0x110... 0x170). */
bool lapic_em_servico(uint8_t vetor)
{
    return (lapic_ler(0x100u + 0x10u * (vetor >> 5)) & (1u << (vetor & 31))) != 0;
}

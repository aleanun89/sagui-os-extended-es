// SPDX-License-Identifier: GPL-2.0-or-later
/* msr.c - lectura de MSR que no derriba el kernel (D092).
 *
 * El monitor de hardware (mejora 050) lee MSRs que cambian de una CPU a otra
 * y que las máquinas virtuales suelen no emularlos. Aunque se compruebe el CPUID
 * antes, una tabla errónea, un modelo nuevo o un hipervisor que anuncia el
 * recurso sin emularlo bastarían para que un rdmsr genere #GP (excepción 13) y
 * en el anillo 0, la pantalla de error. Por eso todo rdmsr del monitor pasa por
 * msr_ler_cru(), escrita en ensamblador con una etiqueta exacta en la instrucción:
 *   - el tratador del vector 13 comprueba si el EIP del #GP es esa etiqueta; si lo es,
 *     pone a cero EAX/EDX, pone 1 en EBX (el "falló") y continúa en la etiqueta siguiente,
 *     como hace _ASM_EXTABLE de Linux con rdmsr_safe;
 *   - cualquier otro #GP sigue el camino de siempre (programa terminado o
 *     pantalla de error), sin cambios.
 * El MSR que falló entra en una lista pequeña y no se vuelve a intentar: cada
 * consulta siguiente cuesta solo la búsqueda en la lista.
 *
 * Escritura (D106, mejora 061): el driver de frecuencia es el único que
 * escribe, y siempre por msr_escrever_seguro(), con el mismo mecanismo: un
 * #GP en la instrucción wrmsr marcada se absorbe, la función devuelve false
 * y quien escribía (el driver) se desactiva. El monitor sigue sin escribir. */
#include "msr.h"
#include "console.h"
#include "cpu.h"

/* uint32_t msr_ler_cru(uint32_t msr, uint32_t *lo, uint32_t *hi)
 * -> 0 = leyó; 1 = el rdmsr generó #GP (lo y hi quedan 0). */
__asm__(".text\n"
        ".globl msr_ler_cru\n"
        "msr_ler_cru:\n"
        "    push %ebx\n"
        "    mov 8(%esp), %ecx\n"
        "    xor %ebx, %ebx\n"
        ".globl msr_ponto_rdmsr\n"
        "msr_ponto_rdmsr:\n"
        "    rdmsr\n"
        ".globl msr_ponto_volta\n"
        "msr_ponto_volta:\n"
        "    mov 12(%esp), %ecx\n"
        "    mov %eax, (%ecx)\n"
        "    mov 16(%esp), %ecx\n"
        "    mov %edx, (%ecx)\n"
        "    mov %ebx, %eax\n"
        "    pop %ebx\n"
        "    ret\n");

uint32_t msr_ler_cru(uint32_t msr, uint32_t *lo, uint32_t *hi);
extern const char msr_ponto_rdmsr[], msr_ponto_volta[];

/* uint32_t msr_escrever_cru(uint32_t msr, uint32_t lo, uint32_t hi)
 * -> 0 = escrito; 1 = el wrmsr generó #GP (D106). */
__asm__(".text\n"
        ".globl msr_escrever_cru\n"
        "msr_escrever_cru:\n"
        "    push %ebx\n"
        "    mov 8(%esp), %ecx\n"
        "    mov 12(%esp), %eax\n"
        "    mov 16(%esp), %edx\n"
        "    xor %ebx, %ebx\n"
        ".globl msr_ponto_wrmsr\n"
        "msr_ponto_wrmsr:\n"
        "    wrmsr\n"
        ".globl msr_ponto_volta_w\n"
        "msr_ponto_volta_w:\n"
        "    mov %ebx, %eax\n"
        "    pop %ebx\n"
        "    ret\n");

uint32_t msr_escrever_cru(uint32_t msr, uint32_t lo, uint32_t hi);
extern const char msr_ponto_wrmsr[], msr_ponto_volta_w[];

#define RUINS_MAX 32
static uint32_t ruins[RUINS_MAX];
static uint32_t qtd_ruins, falhas;
static bool tem_msr;   /* CPUID.01H:EDX[5]; sin él el rdmsr daría #UD, que aquí no se trata */

static quadro_t *protecao_geral(quadro_t *q)
{
    if (!(q->cs & 3) && q->eip == (uint32_t)msr_ponto_rdmsr) {
        q->eax = q->edx = 0;
        q->ebx = 1;
        q->eip = (uint32_t)msr_ponto_volta;
        falhas++;
        return q;
    }
    if (!(q->cs & 3) && q->eip == (uint32_t)msr_ponto_wrmsr) {   /* escritura protegida (D106) */
        q->ebx = 1;
        q->eip = (uint32_t)msr_ponto_volta_w;
        falhas++;
        return q;
    }
    /* El mismo destino que daría el despachador sin tratador registrado. */
    if ((q->cs & 3) == 3) return excecao_usuario(q);
    excecao_fatal(q);
}

void msr_iniciar(void)
{
    idt_registrar(13, protecao_geral);
    /* El CPUID existe si el bit ID (21) del EFLAGS puede cambiarse (los 486 antiguos no lo tienen). */
    uint32_t antes, depois, a, b, c, d;
    __asm__ volatile("pushf; pop %0; mov %0, %1; xor $0x200000, %1; push %1; popf; pushf; pop %1; push %0; popf"
                     : "=&r"(antes), "=&r"(depois));
    if (!((antes ^ depois) & 0x200000)) return;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1) return;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    tem_msr = (d >> 5) & 1;
}

bool msr_marcado_ruim(uint32_t msr)
{
    for (uint32_t i = 0; i < qtd_ruins; i++)
        if (ruins[i] == msr) return true;
    return false;
}

uint32_t msr_falhas(void) { return falhas; }

bool msr_ler_seguro(uint32_t msr, uint64_t *valor)
{
    *valor = 0;
    if (!tem_msr || msr_marcado_ruim(msr)) return false;
    uint32_t lo, hi;
    if (msr_ler_cru(msr, &lo, &hi) != 0) {
        if (qtd_ruins < RUINS_MAX) ruins[qtd_ruins++] = msr;
        klog("[MSR] 0x%x: #GP na leitura; marcado como nao suportado\n", msr);
        return false;
    }
    *valor = ((uint64_t)hi << 32) | lo;
    return true;
}

/* Escritura protegida (D106). Solo la usa el driver de frecuencia, y solo
 * en los MSR de control de P-state (y en la prueba del #GP de la imagen
 * simulada); un #GP se absorbe y devuelve false. */
bool msr_escrever_seguro(uint32_t msr, uint64_t valor)
{
    if (!tem_msr) return false;
    if (msr_escrever_cru(msr, (uint32_t)valor, (uint32_t)(valor >> 32)) != 0) {
        klog("[MSR] 0x%x: #GP en la escritura (valor %x%08x)\n", msr, (uint32_t)(valor >> 32), (uint32_t)valor);
        return false;
    }
    return true;
}

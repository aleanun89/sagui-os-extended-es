// SPDX-License-Identifier: GPL-2.0-or-later
/* simd - muestra y prueba la FPU x87, MMX, SSE y SSE2 (D032).
 *
 * Se compila sin -mgeneral-regs-only (ver el Makefile), pero sin -msse ni
 * -msse2 en todo el archivo: sólo cada función de prueba activa su recurso
 * (__attribute__((target))), y principal() sólo la llama después de que CPUID
 * diga que existe. Así ninguna instrucción SSE2 llega al código común, y simd
 * funciona en un Pentium II o III (D076).
 *   simd                     prueba cada recurso que tiene la CPU
 *   simd estres <n> <veces>  llena XMM0-XMM6 con un patrón que depende de n
 *                            y comprueba en cada vuelta que sigan iguales
 *                            después de dormir 1 ms (= cambio de proceso)
 *   simd excecao             desenmascara la división por cero (MXCSR y, si
 *                            SSE no genera el #XM, en la FPU x87) y divide:
 *                            el kernel debe terminar sólo este programa */
#include "sagui.h"

#define R_FPU  (1u << 0)
#define R_MMX  (1u << 23)
#define R_FXSR (1u << 24)
#define R_SSE  (1u << 25)
#define R_SSE2 (1u << 26)

static void cpuid(uint32_t folha, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(folha), "c"(0));
}

static int falhas;

static void conferir(bool ok, const char *nome, const char *detalhe)
{
    imprimir("  %-6s %s  %s\n", nome, ok ? "ok   " : "FALLO", detalhe);
    if (!ok) falhas++;
}

static void testar_x87(void)
{
    /* raíz de 2 con la FPU, por 1000, redondeada a entero. */
    int r;
    static const int mil = 1000;
    __asm__ volatile("fld1; fadd %%st(0), %%st(0); fsqrt; fimull %1; fistpl %0" : "=m"(r) : "m"(mil));
    char s[48];
    formatar_texto(s, sizeof s, "sqrt(2) * 1000 = %d", r);
    conferir(r == 1414, "x87", s);
}

__attribute__((target("mmx"))) static void testar_mmx(void)
{
    static const uint16_t a[4] = { 1, 2, 3, 4 }, b[4] = { 10, 20, 30, 40 };
    uint16_t c[4];
    __asm__ volatile("movq %1, %%mm0; paddw %2, %%mm0; movq %%mm0, %0; emms"
                     : "=m"(c) : "m"(a), "m"(b) : "mm0");
    char s[48];
    formatar_texto(s, sizeof s, "paddw = %u %u %u %u", c[0], c[1], c[2], c[3]);
    conferir(c[0] == 11 && c[1] == 22 && c[2] == 33 && c[3] == 44, "MMX", s);
}

__attribute__((target("sse"))) static void testar_sse(void)
{
    static const float a[4] __attribute__((aligned(16))) = { 1.5f, 2.5f, 3.5f, 4.5f };
    static const float b[4] __attribute__((aligned(16))) = { 2.0f, 2.0f, 2.0f, 2.0f };
    float c[4] __attribute__((aligned(16)));
    __asm__ volatile("movaps %1, %%xmm0; mulps %2, %%xmm0; movaps %%xmm0, %0"
                     : "=m"(c) : "m"(a), "m"(b) : "xmm0");
    char s[48];
    formatar_texto(s, sizeof s, "mulps = %d %d %d %d", (int)c[0], (int)c[1], (int)c[2], (int)c[3]);
    conferir(c[0] == 3.0f && c[1] == 5.0f && c[2] == 7.0f && c[3] == 9.0f, "SSE", s);
}

__attribute__((target("sse2,fpmath=sse"))) static void testar_sse2(void)
{
    /* Serie de Leibniz en double (GCC usa SSE2 aquí, fpmath=sse). */
    double pi = 0;
    for (int k = 0; k < 100000; k++)
        pi += (k & 1 ? -4.0 : 4.0) / (2 * k + 1);
    int r = (int)(pi * 10000);
    char s[48];
    formatar_texto(s, sizeof s, "pi (double) = %d.%04d", r / 10000, r % 10000);
    conferir(r == 31415, "SSE2", s);
}

/* Compara un XMM con el patrón guardado en OFF(%esi); cuenta la diferencia en ECX. */
#define CONFERE(off, reg) \
    "movdqa %%" reg ", %%xmm7\n\t" \
    "pcmpeqd " #off "(%%esi), %%xmm7\n\t" \
    "pmovmskb %%xmm7, %%edx\n\t" \
    "cmp $0xffff, %%edx\n\t" \
    "je 2f\n\t" \
    "inc %%ecx\n" \
    "2:\n\t"

__attribute__((target("sse2"))) static int estres(uint32_t n, int vezes)
{
    static uint32_t padrao[28] __attribute__((aligned(16)));
    for (int i = 0; i < 28; i++)
        padrao[i] = n * 0x9E3779B9u + (uint32_t)i * 0x01010101u;
    int erradas;
    int voltas = vezes;
    /* Todo en un único asm: entre la carga y la comprobación sólo se ejecuta la llamada de
     * sistema (dormir 1 ms), así que la única forma de que los XMM cambien es que el
     * kernel cambie de proceso sin guardar ni restaurar el estado. */
    __asm__ volatile(
        "movdqa 0(%%esi), %%xmm0\n\t"
        "movdqa 16(%%esi), %%xmm1\n\t"
        "movdqa 32(%%esi), %%xmm2\n\t"
        "movdqa 48(%%esi), %%xmm3\n\t"
        "movdqa 64(%%esi), %%xmm4\n\t"
        "movdqa 80(%%esi), %%xmm5\n\t"
        "movdqa 96(%%esi), %%xmm6\n\t"
        "xor %%ecx, %%ecx\n"
        "1:\n\t"
        "mov %[dormir], %%eax\n\t"
        "mov $1, %%ebx\n\t"
        "int $0x80\n\t"
        CONFERE(0, "xmm0") CONFERE(16, "xmm1") CONFERE(32, "xmm2") CONFERE(48, "xmm3")
        CONFERE(64, "xmm4") CONFERE(80, "xmm5") CONFERE(96, "xmm6")
        "dec %%edi\n\t"
        "jnz 1b\n\t"
        : "=c"(erradas), "+D"(voltas)
        : "S"(padrao), [dormir] "i"(SIS_DORMIR)
        : "eax", "ebx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "memory", "cc");
    if (erradas) {
        imprimir("estres %u: FALLO, %d registros XMM cambiados en %d vueltas\n", n, erradas, vezes);
        return 1;
    }
    imprimir("estres %u: ok, XMM0-XMM6 intactos tras %d cambios de proceso\n", n, vezes);
    return 0;
}

__attribute__((target("sse"))) static void excecao_sse(void)
{
    imprimir("SSE: desenmascarando la división por cero en el MXCSR y dividiendo 1/0...\n");
    uint32_t mxcsr = 0x1F80 & ~(1u << 9);   /* ZM = 0 */
    static const float um[4] __attribute__((aligned(16))) = { 1, 1, 1, 1 };
    static const float zero[4] __attribute__((aligned(16))) = { 0, 0, 0, 0 };
    __asm__ volatile("ldmxcsr %0; movaps %1, %%xmm0; divps %2, %%xmm0"
                     : : "m"(mxcsr), "m"(um), "m"(zero) : "xmm0");
    /* QEMU sin KVM no genera el #XM; en un PC de verdad no llega aquí. */
    imprimir("SSE: el procesador (o el emulador) no ha generado la excepción\n");
}

static int excecao(bool sse)
{
    if (sse) excecao_sse();
    imprimir("x87: desenmascarando la división por cero y dividiendo 1/0...\n");
    uint16_t cw = 0x037F & ~(1u << 2);   /* ZM = 0 */
    __asm__ volatile("fldcw %0; fld1; fldz; fdivrp; fwait; fstp %%st(0)" : : "m"(cw));
    imprimir("x87: el procesador no ha generado la excepción\n");
    return 1;
}

int principal(int argc, char **argv)
{
    uint32_t r[4], fab[4];
    cpuid(0, fab);
    char nome[13];
    memcpy(nome, &fab[1], 4); memcpy(nome + 4, &fab[3], 4); memcpy(nome + 8, &fab[2], 4);
    nome[12] = 0;
    cpuid(1, r);
    uint32_t d = r[3];

    if (argc >= 2 && compara_texto(argv[1], "estres") == 0) {
        if (!(d & R_SSE2)) { imprimir_em(ERROS, "simd: este procesador no tiene SSE2\n"); return 1; }
        uint32_t n = argc >= 3 ? (uint32_t)texto_para_int(argv[2]) : 1;
        int vezes = argc >= 4 ? texto_para_int(argv[3]) : 200;
        return estres(n, vezes > 0 ? vezes : 200);
    }
    if (argc >= 2 && compara_texto(argv[1], "excecao") == 0) {
        return excecao(d & R_SSE);
    }
    if (argc >= 2) {
        imprimir_em(ERROS, "uso: simd [estres <n> <vueltas> | excecao]\n");
        return 1;
    }

    imprimir("CPU %s, familia %u, modelo %u\n", nome, (r[0] >> 8) & 0xF, (r[0] >> 4) & 0xF);
    if (d & R_FPU)  testar_x87();  else imprimir("  x87    no disponible\n");
    if (d & R_MMX)  testar_mmx();  else imprimir("  MMX    no disponible\n");
    if (d & R_SSE)  testar_sse();  else imprimir("  SSE    no disponible\n");
    if (d & R_SSE2) testar_sse2(); else imprimir("  SSE2   no disponible\n");
    imprimir(falhas ? "simd: %d prueba(s) han fallado\n" : "simd: todo correcto\n", falhas);
    return falhas ? 1 : 0;
}

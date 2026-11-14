/* inicio_python.c - ponto de entrada do python: escolhe a FPU antes de
 * qualquer conta de ponto flutuante (D038, D082).
 *
 * O python é compilado duas vezes a partir dos mesmos fontes:
 *   /bin/python      com -msse2 -mfpmath=sse (D038);
 *   /bin/python-x87  com -mfpmath=387, para CPUs sem SSE2 (Pentium III,
 *                    Athlon XP...).
 * Nos dois, o GCC pode usar a FPU em qualquer função (até para copiar
 * estruturas). Por isso este arquivo, e só ele, é compilado como a
 * libsagui (-mgeneral-regs-only): ele roda antes de tudo.
 *
 * - No /bin/python, se o CPUID diz que não há SSE2, a primeira instrução
 *   SSE2 daria #UD: em vez disso ele executa o /bin/python-x87 com os
 *   mesmos argumentos e o mesmo ambiente, espera e devolve o código dele.
 * - No /bin/python-x87, a precisão da x87 passa a 53 bits (a de um double):
 *   cada soma, produto, divisão e raiz é arredondada como no SSE2, e os
 *   resultados (print(1/3), math...) são os mesmos. */
#include "sagui.h"

int mp_principal(int argc, char **argv);

#ifdef SAGUI_X87

int principal(int argc, char **argv)
{
    /* Palavra de controle 0x027F: exceções mascaradas (como o MXCSR
     * 0x1F80 do SSE), arredondamento ao mais próximo, precisão de 53 bits
     * (bits 8-9 = 10b) em vez dos 64 bits do FNINIT. */
    unsigned short cw = 0x027F;
    __asm__ volatile("fldcw %0" : : "m"(cw));
    return mp_principal(argc, argv);
}

#else

#define PYTHON_X87 "/bin/python-x87"

static int tem_sse2(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1) return 0;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d >> 26) & 1;   /* CPUID.1:EDX bit 26 = SSE2 */
}

int principal(int argc, char **argv)
{
    if (tem_sse2() && !obter_env("PYTHON_X87")) return mp_principal(argc, argv);
    /* Sem SSE2 (ou PYTHON_X87=1, para testar): a variante x87. O filho
     * herda os descritores e o grupo (o Ctrl+C também chega a ele). */
    char *args[64];
    int n = 0;
    args[n++] = PYTHON_X87;
    for (int i = 1; i < argc && n < 63; i++) args[n++] = argv[i];
    args[n] = NULL;
    int pid = executar(PYTHON_X87, args);
    if (pid < 0) {
        escrever_texto(ERROS, "python: este procesador no tiene SSE2 y no se pudo ejecutar " PYTHON_X87 "\n");
        return 1;
    }
    int codigo = 1;
    esperar(pid, &codigo);
    return codigo;
}

#endif

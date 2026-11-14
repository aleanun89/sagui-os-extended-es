// SPDX-License-Identifier: GPL-2.0-or-later
/* lscpu - información de la CPU en el formato de Linux (D232), sobre CPUID y lo que da el kernel. */
#include "sagui.h"

static const char *AYUDA =
    "uso: lscpu\n"
    "  Muestra fabricante, modelo, familia, CPU lógicas, hilos por núcleo, frecuencia medida y las instrucciones\n"
    "  que admite (flags), con el formato de lscpu de Linux. Para más detalle (caches, temperatura): infopc -t cpu.\n";

static void cpuid(uint32_t folha, uint32_t sub, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(folha), "c"(sub));
}

typedef struct { int reg, bit; const char *nombre; } flag_t;

int principal(int argc, char **argv)
{
    if (argc > 1) {
        if (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        imprimir_em(ERROS, "lscpu: opción desconocida: %s (lscpu --help)\n", argv[1]);
        return 2;
    }
    uint32_t r[4], max, maxx;
    char fab[13], marca[49];
    cpuid(0, 0, r);
    max = r[0];
    memcpy(fab, &r[1], 4); memcpy(fab + 4, &r[3], 4); memcpy(fab + 8, &r[2], 4);
    fab[12] = 0;
    cpuid(0x80000000u, 0, r);
    maxx = r[0] >= 0x80000000u && r[0] < 0x8000FFFFu ? r[0] : 0;
    marca[0] = 0;
    if (maxx >= 0x80000004u)
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002u + i, 0, r); memcpy(marca + 16 * i, r, 16); marca[48] = 0; }
    const char *m = marca;
    while (*m == ' ') m++;
    uint32_t s1[4] = { 0 }, s7[4] = { 0 }, sx[4] = { 0 };
    if (max >= 1) cpuid(1, 0, s1);
    if (max >= 7) cpuid(7, 0, s7);
    if (maxx >= 0x80000001u) cpuid(0x80000001u, 0, sx);
    uint32_t base_fam = (s1[0] >> 8) & 0xF, base_mod = (s1[0] >> 4) & 0xF;
    uint32_t fam = base_fam == 0xF ? base_fam + ((s1[0] >> 20) & 0xFF) : base_fam;
    uint32_t mod = (base_fam == 6 || base_fam == 0xF) ? (((s1[0] >> 16) & 0xF) << 4) + base_mod : base_mod;
    static info_nucleo_t nuc[64];
    int ncpu = info_sistema(INFO_NUCLEOS, nuc, 64);
    if (ncpu < 1) ncpu = 1;
    unsigned hilos = 1, nucleos_fis = (unsigned)ncpu;
    {
        bool visto[64] = { false };
        int fisicos = 0;
        for (int i = 0; i < ncpu; i++) { unsigned k = nuc[i].paquete * 8 + nuc[i].nucleo; if (k < 64 && !visto[k]) { visto[k] = true; fisicos++; } }
        if (fisicos > 0 && ncpu > 0 && (unsigned)ncpu % (unsigned)fisicos == 0) { hilos = (unsigned)ncpu / (unsigned)fisicos; nucleos_fis = (unsigned)fisicos; }
    }
    imprimir("%-24s %s\n", "Arquitectura:", "i686");
    imprimir("%-24s %s\n", "Modo(s) de operación:", "32-bit");
    imprimir("%-24s %s\n", "Orden de los bytes:", "Little Endian");
    imprimir("%-24s %d\n", "CPU(s):", ncpu);
    imprimir("%-24s %u\n", "Hilo(s) por núcleo:", hilos);
    imprimir("%-24s %u\n", "Núcleo(s) por socket:", nucleos_fis);
    imprimir("%-24s %s\n", "ID de fabricante:", fab);
    imprimir("%-24s %s\n", "Nombre del modelo:", *m ? m : "(la CPU no lo informa)");
    imprimir("%-24s %u\n", "Familia de CPU:", fam);
    imprimir("%-24s %u\n", "Modelo:", mod);
    imprimir("%-24s %u\n", "Stepping:", s1[0] & 0xF);
    info_tsc_t tsc;
    memset(&tsc, 0, sizeof tsc);
    if (info_sistema(INFO_CPU_MHZ, &tsc, sizeof tsc) >= 0 && tsc.khz)
        imprimir("%-24s %u,%03u\n", "MHz de la CPU:", tsc.khz / 1000, tsc.khz % 1000);
    if (s1[2] & (1u << 31)) imprimir("%-24s %s\n", "Hipervisor:", "sí (virtualizado)");
    /* fuente: 0 = CPUID.1 EDX, 1 = CPUID.1 ECX, 2 = CPUID.7 EBX, 3 = CPUID.80000001 ECX, 4 = CPUID.80000001 EDX */
    static const flag_t flags[] = {
        { 0, 0, "fpu" }, { 0, 4, "tsc" }, { 0, 5, "msr" }, { 0, 6, "pae" }, { 0, 8, "cx8" }, { 0, 9, "apic" }, { 0, 11, "sep" },
        { 0, 15, "cmov" }, { 0, 23, "mmx" }, { 0, 24, "fxsr" }, { 0, 25, "sse" }, { 0, 26, "sse2" }, { 0, 28, "ht" },
        { 1, 0, "pni" }, { 1, 5, "vmx" }, { 1, 9, "ssse3" }, { 1, 13, "cx16" }, { 1, 19, "sse4_1" }, { 1, 20, "sse4_2" },
        { 1, 23, "popcnt" }, { 1, 25, "aes" }, { 1, 28, "avx" }, { 1, 31, "hypervisor" }, { 2, 5, "avx2" },
        { 3, 2, "svm" }, { 3, 6, "sse4a" }, { 4, 20, "nx" }, { 4, 29, "lm" } };
    uint32_t fuentes[5] = { s1[3], s1[2], s7[1], sx[2], sx[3] };
    char f[600];
    int n = 0;
    f[0] = 0;
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++)
        if ((fuentes[flags[i].reg] >> flags[i].bit) & 1) n += formatar_texto(f + n, sizeof f - (size_t)n, "%s%s", n ? " " : "", flags[i].nombre);
    imprimir("%-24s %s\n", "Flags:", f);
    return 0;
}

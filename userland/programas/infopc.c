// SPDX-License-Identifier: GPL-2.0-or-later
/* infopc - información del PC, al estilo de CPU-Z (D041).
 *
 *   infopc              abre la ventana (o imprime todo, si no hay escritorio)
 *   infopc -t [sección]   imprime en el terminal: cpu, memoria, placa, pci, video, sistema
 *   infopc --informe [archivo]
 *                       informe de compatibilidad (D076): un .txt con todo esto
 *                       y lo que se sabe de cada dispositivo PCI, los modos
 *                       VESA, los discos, las tablas ACPI, el último SaguiMark y
 *                       el final del log del kernel
 *
 * La pestaña CPU incluye el monitor de hardware (temperatura, frecuencia real,
 * multiplicador, VID..., INFO_SENSORES, D092) y la pestaña Benchmark ejecuta el
 * SaguiMark (D093) con el mismo motor que el comando "saguimark".
 *
 * La pestaña Rendimiento (D176) lanza /bin/benchmark como proceso aparte (--maquina) y lee su
 * salida por un pipe: la ventana no se congela mientras mide y puede cerrarse.
 *
 * La CPU se lee aquí mismo, en el anillo 3: CPUID y RDTSC no son privilegiadas.
 * El resto llega del kernel por SIS_INFO_SISTEMA (INFO_E820, INFO_SMBIOS,
 * INFO_PCI, INFO_VIDEO, INFO_CPU_MHZ, además de los existentes; para el informe,
 * INFO_LOG, INFO_ACPI e INFO_ARRANQUE). La tabla SMBIOS llega cruda y se
 * decodifica aquí. Nada se escribe en el hardware. */
#include "sagui.h"
#include "gui.h"
#include "saguimark.h"
#include "versao.h"
#include "sensores.h"
#include "gpuid.h"

/* ------------------------------------------------------------ texto acumulado */

typedef struct {
    char *p;
    int   tam, cap;
} texto_t;

static void ad(texto_t *t, const char *fmt, ...)
{
    char linha[512];
    va_list ap;
    va_start(ap, fmt);
    int n = formatar_lista(linha, sizeof linha, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof linha - 1) n = sizeof linha - 1;
    if (t->tam + n + 1 > t->cap) {
        int nova = t->cap ? t->cap * 2 : 4096;
        while (nova < t->tam + n + 1) nova *= 2;
        char *q = realoca(t->p, (size_t)nova);
        if (!q) return;
        t->p = q;
        t->cap = nova;
    }
    memcpy(t->p + t->tam, linha, (size_t)n);
    t->tam += n;
    t->p[t->tam] = 0;
}

/* Añade un texto ya listo, de cualquier tamaño (ad() corta en 512). */
static void ad_texto(texto_t *t, const char *txt, int n)
{
    if (t->tam + n + 1 > t->cap) {
        int nova = t->cap ? t->cap : 4096;
        while (nova < t->tam + n + 1) nova *= 2;
        char *q = realoca(t->p, (size_t)nova);
        if (!q) return;
        t->p = q;
        t->cap = nova;
    }
    memcpy(t->p + t->tam, txt, (size_t)n);
    t->tam += n;
    t->p[t->tam] = 0;
}

/* Tamaño legible: "639 KB", "512 MB", "3,5 GB". */
static const char *tamanho(char *d, unsigned max, uint64_t bytes)
{
    uint32_t mb = (uint32_t)(bytes >> 20);
    if (mb >= 1024) {
        uint32_t gb10 = (uint32_t)((uint64_t)mb * 10 / 1024);
        if (gb10 % 10) formatar_texto(d, max, "%u,%u GB", gb10 / 10, gb10 % 10);
        else formatar_texto(d, max, "%u GB", gb10 / 10);
    } else if (bytes >= (1u << 20)) formatar_texto(d, max, "%u MB", mb);
    else if (bytes >= 1024) formatar_texto(d, max, "%u KB", (uint32_t)(bytes >> 10));
    else formatar_texto(d, max, "%u B", (uint32_t)bytes);
    return d;
}

/* ------------------------------------------------------------ CPU */

static void cpuid(uint32_t folha, uint32_t sub, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(folha), "c"(sub));
}

/* Microarquitectura por familia y modelo (tabla pequeña, sólo las comunes). */
static const char *microarquitetura(bool intel, bool amd, uint32_t fam, uint32_t mod)
{
    if (intel) {
        if (fam == 4) return "i486";
        if (fam == 5) return "P5 (Pentium)";
        if (fam == 15) return "NetBurst (Pentium 4)";
        if (fam != 6) return NULL;
        switch (mod) {
        case 0x01: return "P6 (Pentium Pro)";
        case 0x03: case 0x05: case 0x06: return "P6 (Pentium II / Celeron)";
        case 0x07: case 0x08: case 0x0A: case 0x0B: return "P6 (Pentium III)";
        case 0x09: case 0x0D: return "Pentium M";
        case 0x0E: return "Core (Yonah)";
        case 0x0F: case 0x16: return "Core (Merom)";
        case 0x17: case 0x1D: return "Core (Penryn)";
        case 0x1A: case 0x1E: case 0x1F: case 0x2E: return "Nehalem";
        case 0x25: case 0x2C: case 0x2F: return "Westmere";
        case 0x2A: case 0x2D: return "Sandy Bridge";
        case 0x3A: case 0x3E: return "Ivy Bridge";
        case 0x3C: case 0x3F: case 0x45: case 0x46: return "Haswell";
        case 0x3D: case 0x47: case 0x4F: case 0x56: return "Broadwell";
        case 0x4E: case 0x5E: case 0x55: return "Skylake";
        case 0x8E: case 0x9E: return "Kaby Lake / Coffee Lake";
        case 0xA5: case 0xA6: return "Comet Lake";
        case 0x7D: case 0x7E: case 0x6A: case 0x6C: return "Ice Lake";
        case 0x8C: case 0x8D: return "Tiger Lake";
        case 0x97: case 0x9A: return "Alder Lake";
        case 0xB7: case 0xBA: case 0xBF: return "Raptor Lake";
        case 0x1C: case 0x26: case 0x27: case 0x35: case 0x36: return "Atom (Bonnell/Saltwell)";
        case 0x37: case 0x4A: case 0x4D: case 0x5A: case 0x4C: return "Atom (Silvermont/Airmont)";
        case 0x5C: case 0x5F: case 0x7A: return "Atom (Goldmont)";
        default: return NULL;
        }
    }
    if (amd) {
        if (fam == 4) return "Am486 / 5x86";
        if (fam == 5) return mod >= 6 ? "K6" : "K5";
        if (fam == 6) return "K7 (Athlon)";
        if (fam == 0x0F) return "K8 (Athlon 64)";
        if (fam == 0x10) return "K10 (Phenom)";
        if (fam == 0x11) return "K8/K10 (Turion)";
        if (fam == 0x12) return "K10 (Llano)";
        if (fam == 0x14) return "Bobcat";
        if (fam == 0x15) return mod < 0x10 ? "Bulldozer" : mod < 0x30 ? "Piledriver" : mod < 0x60 ? "Steamroller" : "Excavator";
        if (fam == 0x16) return "Jaguar / Puma";
        if (fam == 0x17) return mod < 0x30 ? "Zen / Zen+" : "Zen 2";
        if (fam == 0x19) return (mod >= 0x10 && mod < 0x20) || mod >= 0x60 ? "Zen 4" : "Zen 3";
        if (fam == 0x1A) return "Zen 5";
    }
    return NULL;
}

static const char *nome_hipervisor(const char *sig)
{
    if (comeca_com(sig, "TCGTCGTCGTCG")) return "QEMU (TCG, emulación por software)";
    if (comeca_com(sig, "KVMKVMKVM")) return "KVM";
    if (comeca_com(sig, "VBoxVBoxVBox")) return "VirtualBox";
    if (comeca_com(sig, "VMwareVMware")) return "VMware";
    if (comeca_com(sig, "Microsoft Hv")) return "Hyper-V";
    if (comeca_com(sig, "XenVMMXenVMM")) return "Xen";
    if (comeca_com(sig, " lrpepyh  vr")) return "Parallels";
    if (comeca_com(sig, "bhyve bhyve ")) return "bhyve";
    return "desconocido";
}

typedef struct { uint8_t reg, bit; const char *nome; } recurso_t;
enum { R1_EDX, R1_ECX, R7_EBX, RX_EDX, RX_ECX };
static const recurso_t recursos[] = {
    { R1_EDX, 0, "FPU" }, { R1_EDX, 3, "PSE" }, { R1_EDX, 4, "TSC" }, { R1_EDX, 5, "MSR" }, { R1_EDX, 6, "PAE" },
    { R1_EDX, 8, "CX8" }, { R1_EDX, 9, "APIC" }, { R1_EDX, 11, "SEP" }, { R1_EDX, 12, "MTRR" },
    { R1_EDX, 13, "PGE" }, { R1_EDX, 15, "CMOV" }, { R1_EDX, 16, "PAT" },
    { R1_EDX, 23, "MMX" }, { R1_EDX, 24, "FXSR" }, { R1_EDX, 25, "SSE" }, { R1_EDX, 26, "SSE2" },
    { R1_EDX, 28, "HTT" },
    { R1_ECX, 0, "SSE3" }, { R1_ECX, 1, "PCLMUL" }, { R1_ECX, 5, "VT-x" }, { R1_ECX, 9, "SSSE3" },
    { R1_ECX, 12, "FMA" }, { R1_ECX, 13, "CX16" }, { R1_ECX, 19, "SSE4.1" }, { R1_ECX, 20, "SSE4.2" },
    { R1_ECX, 22, "MOVBE" }, { R1_ECX, 23, "POPCNT" }, { R1_ECX, 25, "AES" }, { R1_ECX, 26, "XSAVE" },
    { R1_ECX, 28, "AVX" }, { R1_ECX, 29, "F16C" }, { R1_ECX, 30, "RDRAND" },
    { R7_EBX, 3, "BMI1" }, { R7_EBX, 5, "AVX2" }, { R7_EBX, 8, "BMI2" }, { R7_EBX, 16, "AVX-512F" },
    { R7_EBX, 18, "RDSEED" }, { R7_EBX, 29, "SHA" },
    { RX_EDX, 11, "SYSCALL" }, { RX_EDX, 20, "NX" }, { RX_EDX, 29, "x86-64" },
    { RX_ECX, 0, "LAHF" }, { RX_ECX, 2, "SVM" }, { RX_ECX, 5, "LZCNT" }, { RX_ECX, 6, "SSE4a" },
};

static void cache_linha(texto_t *t, const char *nome, uint32_t kb, uint32_t vias, uint32_t linha, uint32_t partilha)
{
    char tam[24];
    ad(t, "  %-5s %s", nome, tamanho(tam, sizeof tam, (uint64_t)kb * 1024));
    if (vias) ad(t, ", %u vías", vias);
    if (linha) ad(t, ", línea de %u B", linha);
    if (partilha > 1) ad(t, ", compartida por %u hilos", partilha);
    ad(t, "\n");
}

/* D208: qué variante de las rutas calientes eligió el kernel (la línea "[RAPIDO] escolhido" del
 * log del arranque) y cuál elige la libsagui en este programa (por medida, entre las que da
 * CPUID). Dos líneas cortas, porque ad() corta en 512 bytes. */
static void rutas_rapidas(texto_t *t)
{
    static char log[32 * 1024 + 1];
    char k[96], u[160];
    k[0] = 0;
    int n = info_sistema(INFO_LOG, log, sizeof log - 1);
    if (n > 0) {
        log[n] = 0;
        const char *marca = "[RAPIDO] escolhido: ", *ult = NULL;
        for (char *p = log; (p = acha_char(p, '[')) != NULL; p++)
            if (comeca_com(p, marca)) ult = p + tam_texto(marca);
        if (!ult) {
            for (char *p = log; (p = acha_char(p, '[')) != NULL; p++)
                if (comeca_com(p, "[RAPIDO] sem SSE")) ult = "rep movsd/stosd (la CPU no tiene SSE)";
        }
        if (ult) {
            copia_texto(k, ult, sizeof k);
            char *f = acha_char(k, '\n');
            if (f) *f = 0;
        }
    }
    mem_descrever(u, sizeof u);
    ad(t, "Rutas rápidas:\n");
    ad(t, "  kernel (gráficos): %s\n", k[0] ? k : "(sin dato)");
    ad(t, "  programas:         %s\n", u);
}

static void secao_cpu(texto_t *t)
{
    uint32_t r[4], max, maxx;
    char fab[13], marca[49], sig[13];
    cpuid(0, 0, r);
    max = r[0];
    memcpy(fab, &r[1], 4); memcpy(fab + 4, &r[3], 4); memcpy(fab + 8, &r[2], 4);
    fab[12] = 0;
    bool intel = compara_texto(fab, "GenuineIntel") == 0;
    bool amd = compara_texto(fab, "AuthenticAMD") == 0;
    ad(t, "Fabricante:        %s%s\n", fab, intel ? " (Intel)" : amd ? " (AMD)" : "");

    cpuid(0x80000000u, 0, r);
    maxx = r[0] >= 0x80000000u && r[0] < 0x8000FFFFu ? r[0] : 0;
    marca[0] = 0;
    if (maxx >= 0x80000004u) {
        for (uint32_t i = 0; i < 3; i++) {
            cpuid(0x80000002u + i, 0, r);
            memcpy(marca + 16 * i, r, 16);
        }
        marca[48] = 0;
    }
    const char *m = marca;
    while (*m == ' ') m++;
    ad(t, "Nombre comercial:  %s\n", *m ? m : "(la CPU no lo informa)");

    uint32_t s1[4] = { 0 }, s7[4] = { 0 }, sx[4] = { 0 };
    if (max >= 1) cpuid(1, 0, s1);
    if (max >= 7) cpuid(7, 0, s7);
    if (maxx >= 0x80000001u) cpuid(0x80000001u, 0, sx);
    uint32_t base_fam = (s1[0] >> 8) & 0xF, base_mod = (s1[0] >> 4) & 0xF;
    uint32_t fam = base_fam == 0xF ? base_fam + ((s1[0] >> 20) & 0xFF) : base_fam;
    uint32_t mod = (base_fam == 6 || base_fam == 0xF) ? (((s1[0] >> 16) & 0xF) << 4) + base_mod : base_mod;
    ad(t, "Familia/modelo:    familia %u (0x%x), modelo %u (0x%02x), stepping %u  [firma 0x%08x]\n",
       fam, fam, mod, mod, s1[0] & 0xF, s1[0]);
    const char *ua = microarquitetura(intel, amd, fam, mod);
    ad(t, "Microarquitectura: %s\n", ua ? ua : "(no está en la tabla)");

    /* Núcleos y hilos lógicos por encapsulado. */
    uint32_t logicos = (s1[3] & (1u << 28)) ? (s1[1] >> 16) & 0xFF : 1, nucleos = 0;
    if (logicos == 0) logicos = 1;
    if (intel && max >= 0xB) {
        uint32_t a[4], b[4];
        cpuid(0xB, 0, a);
        cpuid(0xB, 1, b);
        if (((a[2] >> 8) & 0xFF) == 1 && ((b[2] >> 8) & 0xFF) == 2 && (a[1] & 0xFFFF) && (b[1] & 0xFFFF)) {
            logicos = b[1] & 0xFFFF;
            nucleos = logicos / (a[1] & 0xFFFF);
        }
    }
    if (!nucleos && intel && max >= 4) { cpuid(4, 0, r); if (r[0] & 0x1F) nucleos = ((r[0] >> 26) & 0x3F) + 1; }
    if (!nucleos && amd && maxx >= 0x80000008u) { cpuid(0x80000008u, 0, r); nucleos = (r[2] & 0xFF) + 1; }
    if (!nucleos) nucleos = 1;
    if (nucleos > logicos) logicos = nucleos;
    /* Cuántas usa SaguiOS: las CPU arrancadas (con --smp, D164); sin --smp, una. */
    {
        static info_nucleo_t cpus_n[8];
        int n = info_sistema(INFO_NUCLEOS, cpus_n, 8), usadas = 0;
        for (int i = 0; i < n; i++) if (cpus_n[i].estado != 0) usadas++;
        if (usadas < 1) usadas = 1;
        ad(t, "Núcleos / hilos:   %u núcleo(s), %u hilo(s) lógico(s) por procesador (SaguiOS usa %d%s)\n",
           nucleos, logicos, usadas, usadas == 1 && logicos > 1 ? "; imagen sin --smp" : "");
    }

    info_tsc_t tsc;
    memset(&tsc, 0, sizeof tsc);
    info_sistema(INFO_CPU_MHZ, &tsc, sizeof tsc);
    if (tsc.khz)
        ad(t, "Frecuencia (TSC):  %u,%02u MHz, medida contra el PIT durante %u ms al arrancar\n",
           tsc.khz / 1000, tsc.khz % 1000 / 10, tsc.janela_ms);
    else
        ad(t, "Frecuencia (TSC):  no medida (CPU sin TSC)\n");

    /* Cachés: hoja 4 en Intel; 0x80000005/6 en AMD (y la L2 de los Intel antiguos). */
    ad(t, "Cachés:\n");
    bool alguma = false;
    if (intel && max >= 4) {
        for (uint32_t i = 0; i < 16; i++) {
            cpuid(4, i, r);
            uint32_t tipo = r[0] & 0x1F;
            if (tipo == 0) break;
            uint32_t nivel = (r[0] >> 5) & 7, vias = (r[1] >> 22) + 1, part = ((r[1] >> 12) & 0x3FF) + 1;
            uint32_t linha = (r[1] & 0xFFF) + 1, conj = r[2] + 1;
            uint32_t kb = vias * part * linha * conj / 1024;
            char nome[8];
            formatar_texto(nome, sizeof nome, "L%u%s", nivel, tipo == 1 ? "d" : tipo == 2 ? "i" : "");
            cache_linha(t, nome, kb, vias, linha, ((r[0] >> 14) & 0xFFF) + 1);
            alguma = true;
        }
    }
    if (!alguma && maxx >= 0x80000005u && amd) {
        cpuid(0x80000005u, 0, r);
        if (r[2] >> 24) cache_linha(t, "L1d", r[2] >> 24, (r[2] >> 16) & 0xFF, r[2] & 0xFF, 0);
        if (r[3] >> 24) cache_linha(t, "L1i", r[3] >> 24, (r[3] >> 16) & 0xFF, r[3] & 0xFF, 0);
        alguma = (r[2] >> 24) || (r[3] >> 24);
    }
    if ((!alguma || amd) && maxx >= 0x80000006u) {
        cpuid(0x80000006u, 0, r);
        if (r[2] >> 16) { cache_linha(t, "L2", r[2] >> 16, 0, r[2] & 0xFF, 0); alguma = true; }
        if (amd && (r[3] >> 18)) cache_linha(t, "L3", (r[3] >> 18) * 512, 0, r[3] & 0xFF, 0);
    }
    if (!alguma) ad(t, "  (la CPU no informa las cachés)\n");

    /* Conjunto de instrucciones, partido en líneas. */
    const uint32_t regs[] = { s1[3], s1[2], s7[1], sx[3], sx[2] };
    char linha[80];
    int n = 0;
    linha[0] = 0;
    ad(t, "Instrucciones:\n");
    for (unsigned i = 0; i < sizeof recursos / sizeof recursos[0]; i++) {
        if (!((regs[recursos[i].reg] >> recursos[i].bit) & 1)) continue;
        int k = (int)tam_texto(recursos[i].nome);
        if (n + k + 1 > 64) { ad(t, "  %s\n", linha); n = 0; linha[0] = 0; }
        if (n) linha[n++] = ' ';
        memcpy(linha + n, recursos[i].nome, (size_t)k + 1);
        n += k;
    }
    if (n) ad(t, "  %s\n", linha);
    if ((s1[2] >> 28) & 1) ad(t, "  (AVX existe en la CPU, pero SaguiOS solo activa FPU, MMX y SSE: D032)\n");
    rutas_rapidas(t);

    if ((s1[2] >> 31) & 1) {
        cpuid(0x40000000u, 0, r);
        memcpy(sig, &r[1], 4); memcpy(sig + 4, &r[2], 4); memcpy(sig + 8, &r[3], 4);
        sig[12] = 0;
        for (int i = 0; i < 12; i++) if (sig[i] && (sig[i] < 0x20 || sig[i] > 0x7E)) sig[i] = '?';
        ad(t, "Hipervisor:        sí, firma \"%s\" = %s\n", sig, nome_hipervisor(sig));
    } else {
        ad(t, "Hipervisor:        no detectado (máquina real o hipervisor oculto)\n");
    }

    /* Monitor de hardware (D092): sólo lectura, como máximo una vez por segundo. */
    info_sensores_t sen;
    static char st[2048];
    ad(t, "Sensores (monitor de hardware, solo lectura):\n");
    if (sensores_ler(&sen)) {
        sensores_texto(&sen, st, sizeof st);
        char *l = st;
        while (*l) {
            char *f = acha_char(l, '\n');
            if (f) *f = 0;
            ad(t, "  %s\n", l);
            if (!f) break;
            l = f + 1;
        }
    } else {
        ad(t, "  no disponible (el kernel no tiene INFO_SENSORES)\n");
    }
    /* Driver de frecuencia (D106): el único que escribe, aparte del monitor. */
    info_frequencia_t fq;
    if (frequencia_ler(&fq)) {
        ad(t, "Frecuencia (driver de P-states, D106):\n");
        frequencia_texto(&fq, st, sizeof st);
        char *l = st;
        while (*l) {
            char *f = acha_char(l, '\n');
            if (f) *f = 0;
            ad(t, "  %s\n", l);
            if (!f) break;
            l = f + 1;
        }
    }
}

/* ------------------------------------------------------------ memória */

static const char *nome_e820(uint32_t t)
{
    switch (t) {
    case 1: return "usable (libre)";
    case 2: return "reservada";
    case 3: return "ACPI (recuperable)";
    case 4: return "ACPI NVS";
    case 5: return "defectuosa";
    case 7: return "persistente";
    default: return "otra";
    }
}

static void secao_memoria(texto_t *t)
{
    static info_e820_t r[64];
    char a[24], b[24], c[24];
    int n = info_sistema(INFO_E820, r, 64);
    if (n < 0) n = 0;
    uint64_t total = 0, abaixo = 0;
    ad(t, "Mapa de memoria E820 (BIOS), %d regiones:\n", n);
    ad(t, "  %-18s  %-18s  %-10s %s\n", "Inicio", "Fin", "Tamaño", "Tipo");
    for (int i = 0; i < n; i++) {
        uint64_t fim = r[i].base + r[i].tamanho - 1;
        bool valida = (r[i].extra & 1) != 0;
        ad(t, "  0x%08x%08x  0x%08x%08x  %-10s %s%s\n", (uint32_t)(r[i].base >> 32), (uint32_t)r[i].base,
           (uint32_t)(fim >> 32), (uint32_t)fim, tamanho(a, sizeof a, r[i].tamanho), nome_e820(r[i].tipo),
           valida ? "" : " (ignorada)");
        if (r[i].tipo == 1 && valida) {
            total += r[i].tamanho;
            uint64_t ini = r[i].base, f = r[i].base + r[i].tamanho;
            if (ini < 0x100000000ull) abaixo += (f > 0x100000000ull ? 0x100000000ull : f) - ini;
        }
    }
    info_memoria_t m;
    info_sistema(INFO_MEMORIA, &m, sizeof m);
    ad(t, "\nRAM usable detectada:        %s\n", tamanho(a, sizeof a, total));
    ad(t, "Usable por debajo de 4 GB:   %s\n", tamanho(a, sizeof a, abaixo));
    ad(t, "Gestionada por el kernel:    %s (usada %s, libre %s)\n", tamanho(a, sizeof a, (uint64_t)m.gerida_kb << 10),
       tamanho(b, sizeof b, (uint64_t)m.usada_kb << 10), tamanho(c, sizeof c, (uint64_t)m.livre_kb << 10));
    ad(t, "\nPor qué el kernel gestiona como mucho ~1,9 GB (D008, D049): el kernel accede\n"
          "a la memoria física por su propia dirección (mapeo 1:1 de los 2 primeros GB\n"
          "virtuales, en páginas de 4 MB) y los programas empiezan en 0x80000000; de esos\n"
          "2 GB, los últimos 128 MB son ventanas para el framebuffer y el USB. Usar el\n"
          "resto: mejoras/012-memoria-alta.md.\n");
}

/* ------------------------------------------------------------ SMBIOS */

static const uint8_t *smb_fim;   /* fin de los datos copiados */

/* Texto número "idx" (1, 2, ...) de la estructura "e"; 0 = no informado. */
static const char *smb_texto(const uint8_t *e, uint8_t idx)
{
    static char buf[4][64];
    static int vez;
    char *d = buf[vez++ & 3];
    d[0] = 0;
    if (idx == 0) return "(no informado)";
    const uint8_t *s = e + e[1];
    for (uint8_t k = 1; k < idx && s < smb_fim; k++) {
        while (s < smb_fim && *s) s++;
        s++;
        if (s < smb_fim && *s == 0) return "(no informado)";
    }
    int n = 0;
    while (s < smb_fim && *s && n < 63) { d[n++] = (char)(*s >= 0x20 && *s < 0x7F ? *s : '?'); s++; }
    while (n > 0 && d[n - 1] == ' ') n--;
    d[n] = 0;
    return n ? d : "(no informado)";
}

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static const char *tipo_memoria(uint8_t t)
{
    switch (t) {
    case 0x03: return "DRAM"; case 0x07: return "RAM"; case 0x0F: return "SDRAM";
    case 0x12: return "DDR"; case 0x13: return "DDR2"; case 0x14: return "DDR2 FB-DIMM";
    case 0x18: return "DDR3"; case 0x1A: return "DDR4"; case 0x1B: return "LPDDR";
    case 0x1C: return "LPDDR2"; case 0x1D: return "LPDDR3"; case 0x1E: return "LPDDR4";
    case 0x22: return "DDR5"; case 0x23: return "LPDDR5";
    default: return "tipo desconocido";
    }
}

static void secao_placa(texto_t *t)
{
    uint32_t cap = sizeof(info_smbios_t) + 16 * 1024;
    uint8_t *buf = aloca(cap);
    if (!buf) { ad(t, "Sin memoria\n"); return; }
    int r = info_sistema(INFO_SMBIOS, buf, cap);
    info_smbios_t *h = (info_smbios_t *)buf;
    if (r < (int)sizeof *h || !h->achou) {
        ad(t, "No se encontró la tabla SMBIOS/DMI (anclas _SM_ y _SM3_ en 0xF0000-0xFFFFF).\n");
        libera(buf);
        return;
    }
    ad(t, "SMBIOS %u.%u (entrada de %u bits) en 0x%08x, %u bytes\n", h->versao_maior, h->versao_menor,
       h->entrada_64 ? 64 : 32, h->endereco, h->tamanho);
    const uint8_t *p = buf + sizeof *h;
    smb_fim = p + h->copiado;
    char a[24];
    int modulos = 0;
    while (p + 4 <= smb_fim && p[1] >= 4) {
        uint8_t tipo = p[0], len = p[1];
        const uint8_t *e = p;
        if (tipo == 0) {
            ad(t, "\nBIOS (tipo 0)\n");
            ad(t, "  Fabricante:   %s\n", smb_texto(e, e[4]));
            ad(t, "  Versión:      %s\n", smb_texto(e, e[5]));
            ad(t, "  Fecha:        %s\n", smb_texto(e, e[8]));
            if (len > 9) ad(t, "  Tamaño ROM:   %u KB\n", (e[9] + 1) * 64);
            if (len > 0x15 && e[0x14] != 0xFF) ad(t, "  Revisión:     %u.%u\n", e[0x14], e[0x15]);
        } else if (tipo == 1) {
            ad(t, "\nSistema (tipo 1)\n");
            ad(t, "  Fabricante:   %s\n", smb_texto(e, e[4]));
            ad(t, "  Producto:     %s\n", smb_texto(e, e[5]));
            ad(t, "  Versión:      %s\n", smb_texto(e, e[6]));
            ad(t, "  N.º de serie: (omitido por privacidad)\n");
            if (len > 0x1A) ad(t, "  Familia:      %s\n", smb_texto(e, e[0x1A]));
        } else if (tipo == 2) {
            ad(t, "\nPlaca base (tipo 2)\n");
            ad(t, "  Fabricante:   %s\n", smb_texto(e, e[4]));
            ad(t, "  Producto:     %s\n", smb_texto(e, e[5]));
            ad(t, "  Versión:      %s\n", smb_texto(e, e[6]));
        } else if (tipo == 4 && len >= 0x1A) {
            ad(t, "\nProcesador (tipo 4)\n");
            ad(t, "  Zócalo:       %s\n", smb_texto(e, e[4]));
            ad(t, "  Fabricante:   %s\n", smb_texto(e, e[7]));
            ad(t, "  Versión:      %s\n", smb_texto(e, e[0x10]));
            ad(t, "  Reloj externo: %u MHz; velocidad máx.: %u MHz; actual: %u MHz\n",
               u16(e + 0x12), u16(e + 0x14), u16(e + 0x16));
            if (len > 0x25) ad(t, "  Núcleos: %u; hilos: %u\n", e[0x23], e[0x25]);
        } else if (tipo == 16 && len >= 0x0F) {
            uint32_t kb = u32(e + 7);
            ad(t, "\nMemoria física (tipo 16): máximo %s, %u ranura(s)\n",
               kb == 0x80000000u ? "(extendido)" : tamanho(a, sizeof a, (uint64_t)kb << 10), u16(e + 0x0D));
        } else if (tipo == 17 && len >= 0x15) {
            uint16_t tam = u16(e + 0x0C);
            uint64_t bytes;
            if (tam == 0x7FFF && len >= 0x20) bytes = (uint64_t)(u32(e + 0x1C) & 0x7FFFFFFF) << 20;
            else if (tam & 0x8000) bytes = (uint64_t)(tam & 0x7FFF) << 10;
            else bytes = (uint64_t)tam << 20;
            if (!modulos++) ad(t, "\nMódulos de memoria (tipo 17)\n");
            const char *ranura = smb_texto(e, e[0x10]);
            const char *banco = smb_texto(e, e[0x11]);
            if (tam == 0) {
                ad(t, "  %s / %s: vacía\n", ranura, banco);
            } else {
                ad(t, "  %s / %s: %s %s", ranura, banco, tam == 0xFFFF ? "tamaño desconocido" : tamanho(a, sizeof a, bytes),
                   tipo_memoria(e[0x12]));
                if (len >= 0x17 && u16(e + 0x15)) ad(t, " a %u MT/s", u16(e + 0x15));
                ad(t, "\n");
                if (len >= 0x1B)
                    ad(t, "     fabricante %s, referencia %s\n", smb_texto(e, e[0x17]), smb_texto(e, e[0x1A]));
            }
        }
        if (tipo == 127) break;
        /* Salta la parte formateada y los textos (terminan con dos ceros). */
        const uint8_t *s = p + len;
        while (s + 1 < smb_fim && !(s[0] == 0 && s[1] == 0)) s++;
        p = s + 2;
    }
    libera(buf);
}

/* ------------------------------------------------------------ PCI */

static const char *nome_classe(uint8_t c, uint8_t s, uint8_t i)
{
    switch ((c << 8) | s) {
    case 0x0001: return "VGA antigua";
    case 0x0100: return "Controlador SCSI";
    case 0x0101: return "Controlador IDE";
    case 0x0102: return "Controlador disquete";
    case 0x0104: return "Controlador RAID";
    case 0x0105: return "Controlador ATA";
    case 0x0106: return i == 1 ? "SATA (AHCI)" : "Controlador SATA";
    case 0x0108: return "Controlador NVMe";
    case 0x0180: return "Almacenamiento";
    case 0x0200: return "Ethernet";
    case 0x0280: return "Red (Wi-Fi u otra)";
    case 0x0300: return "Vídeo VGA";
    case 0x0302: return "Vídeo 3D";
    case 0x0380: return "Vídeo";
    case 0x0401: return "Audio";
    case 0x0403: return "Audio HD";
    case 0x0480: return "Multimedia";
    case 0x0500: return "Memoria RAM";
    case 0x0600: return "Puente host";
    case 0x0601: return "Puente ISA";
    case 0x0604: return "Puente PCI-PCI";
    case 0x0607: return "Puente CardBus";
    case 0x0680: return "Puente";
    case 0x0700: return "Puerto serie";
    case 0x0703: return "Módem";
    case 0x0780: return "Comunicación";
    case 0x0805: return "Lector SD";
    case 0x0880: return "Periférico del sistema";
    case 0x0C00: return "FireWire";
    case 0x0C03: return i == 0x00 ? "USB 1.1 (UHCI)" : i == 0x10 ? "USB 1.1 (OHCI)" : i == 0x20 ? "USB 2.0 (EHCI)"
                      : i == 0x30 ? "USB 3.0 (xHCI)" : "USB";
    case 0x0C05: return "SMBus";
    case 0x0D11: return "Bluetooth";
    case 0x1080: return "Cifrado";
    case 0x1180: return "Procesamiento de señal";
    }
    switch (c) {
    case 0x01: return "Almacenamiento"; case 0x02: return "Red"; case 0x03: return "Vídeo";
    case 0x04: return "Multimedia"; case 0x06: return "Puente"; case 0x08: return "Periférico del sistema";
    case 0x0C: return "Bus serie"; case 0x0D: return "Inalámbrico";
    default: return "Otro";
    }
}

static const char *nome_fabricante(uint16_t v)
{
    switch (v) {
    case 0x8086: return "Intel"; case 0x1022: return "AMD"; case 0x1002: return "AMD/ATI";
    case 0x10DE: return "NVIDIA"; case 0x10EC: return "Realtek"; case 0x168C: return "Atheros";
    case 0x1969: return "Atheros"; case 0x14E4: return "Broadcom"; case 0x1AF4: return "Red Hat (virtio)";
    case 0x1B36: return "Red Hat (QEMU)"; case 0x1234: return "Bochs/QEMU"; case 0x80EE: return "VirtualBox";
    case 0x15AD: return "VMware"; case 0x1106: return "VIA"; case 0x1039: return "SiS";
    case 0x197B: return "JMicron"; case 0x1180: return "Ricoh"; case 0x1217: return "O2 Micro";
    case 0x104C: return "Texas Instr."; case 0x11AB: return "Marvell"; case 0x1B21: return "ASMedia";
    case 0x1414: return "Microsoft"; case 0x10B7: return "3Com"; case 0x1033: return "NEC";
    default: return NULL;
    }
}

static const char *nome_dispositivo(uint16_t v, uint16_t d)
{
    uint32_t id = ((uint32_t)v << 16) | d;
    switch (id) {
    case 0x80861237: return "440FX (i440FX PMC)";
    case 0x80867000: return "PIIX3 ISA";
    case 0x80867010: return "PIIX3 IDE";
    case 0x80867020: return "PIIX3 USB";
    case 0x80867113: return "PIIX4 ACPI";
    case 0x808629C0: return "Q35 (MCH)";
    case 0x80862918: return "ICH9 LPC";
    case 0x80862922: return "ICH9 AHCI";
    case 0x8086100E: return "82540EM Gigabit (e1000)";
    case 0x808624CD: return "82801DB EHCI";
    case 0x8086293A: return "ICH9 EHCI";
    case 0x80862668: return "ICH6 HD Audio";
    case 0x80860104: return "Sandy Bridge (host)";
    case 0x80860106: case 0x80860116: case 0x80860126: return "HD Graphics (Sandy Bridge)";
    case 0x80861C3A: return "6 Series MEI";
    case 0x80861C26: case 0x80861C2D: return "6 Series EHCI";
    case 0x80861C20: return "6 Series HD Audio";
    case 0x80861C49: return "HM65 LPC";
    case 0x80861C03: return "6 Series SATA AHCI";
    case 0x80861C22: return "6 Series SMBus";
    case 0x12341111: return "VGA estándar (Bochs)";
    case 0x10EC8136: return "RTL810xE Fast Ethernet";
    case 0x10EC8168: return "RTL8111/8168 Gigabit";
    case 0x10EC8139: return "RTL8139";
    case 0x19692060: return "AR8152 v1.1 Fast Ethernet";
    case 0x19692062: return "AR8152 v2.0 Fast Ethernet";
    case 0x19691073: return "AR8151 v1.0 Gigabit";
    case 0x19691083: return "AR8151 v2.0 Gigabit";
    case 0x19691063: return "AR8131 Gigabit";
    case 0x19691062: return "AR8132 Fast Ethernet";
    case 0x168C002B: return "AR9285 Wi-Fi";
    case 0x168C0032: return "AR9485 Wi-Fi";
    case 0x80EEBEEF: return "VGA de VirtualBox";
    case 0x80EECAFE: return "Guest Service";
    default: return NULL;
    }
}

/* D211: la vía de cada IRQ: controlador, GSI (de la línea de la BIOS o del
 * _PRT), CPU que la recibe y el resultado de la autoprueba del arranque. */
static void secao_irq(texto_t *t)
{
    static info_irq_via_t v[64];
    int n = info_sistema(INFO_IRQ_VIAS, v, 64);
    if (n <= 0) return;
    static const char *const prova[] = { "", "autoprueba ok", "autoprueba: no llegaba por el IOAPIC, va por el 8259",
                                         "autoprueba: no llega por ninguna vía (sondeo)", "autoprueba: ocupado" };
    ad(t, "\nInterrupciones (la vía de cada IRQ):\n");
    for (int i = 0; i < n; i++) {
        info_irq_via_t *e = &v[i];
        char quem[24], via[64];
        if (e->bus == 0xFF) formatar_texto(quem, sizeof quem, "ISA");
        else formatar_texto(quem, sizeof quem, "%02x:%02x.%u INT%c", e->bus, e->disp, e->fun, 'A' + (e->pino ? e->pino - 1 : 0));
        if (e->irq > 15) {
            ad(t, "  ---     %-14s %-6s sin línea de interrupción\n", quem, e->nome);
            continue;
        }
        if (e->via == IRQ_VIA_PIC)
            formatar_texto(via, sizeof via, "8259 -> CPU 0");
        else if (e->via == IRQ_VIA_MISTA)
            formatar_texto(via, sizeof via, "8259 (dentro del IOAPIC) -> CPU 0");
        else if (e->gsi == 0xFFFFFFFFu)
            formatar_texto(via, sizeof via, "IOAPIC sin entrada");
        else
            formatar_texto(via, sizeof via, "IOAPIC GSI %u%s -> APIC %u", e->gsi, e->prt ? " (_PRT)" : "", e->apic);
        ad(t, "  IRQ %-3u %-14s %-6s %-34s %u atendidas, %u/s%s%s\n", e->irq, quem, e->nome, via, e->atendidas,
           e->por_segundo, e->prova ? "; " : "", e->prova < 5 ? prova[e->prova] : "?");
        if (e->tormentas)
            ad(t, "          tormenta de interrupciones: %u vez/veces%s\n", e->tormentas,
               e->tormenta ? "; ahora enmascarada (los drivers sondean)" : "; ya se reintentó");
    }
}

static void secao_pci(texto_t *t)
{
    static info_pci_t v[64];
    int n = info_sistema(INFO_PCI, v, 64);
    if (n < 0) n = 0;
    ad(t, "%d funciones PCI (bus:disp.fun, fabricante:dispositivo, clase):\n", n);
    for (int i = 0; i < n; i++) {
        info_pci_t *p = &v[i];
        const char *f = nome_fabricante(p->fabricante), *d = nome_dispositivo(p->fabricante, p->modelo);
        ad(t, "  %02x:%02x.%u  %04x:%04x  %-22s %s%s%s\n", p->barramento, p->dispositivo, p->funcao,
           p->fabricante, p->modelo, nome_classe(p->classe, p->subclasse, p->interface),
           f ? f : "?", d ? " " : "", d ? d : "");
    }
    secao_irq(t);
}

/* ------------------------------------------------------------ vídeo */

static void secao_video(texto_t *t)
{
    info_video_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VIDEO, &v, sizeof v);
    char a[24];
    if (!v.grafico) {
        ad(t, "Modo gráfico:      no (modo texto VGA 80x25)\n");
    } else {
        if (v.modo) ad(t, "Modo gráfico:      VESA, modo 0x%x\n", v.modo);
        else ad(t, "Modo gráfico:      cambiado en caliente (tarjeta Bochs/QEMU)\n");
        ad(t, "Resolución:        %ux%u, %u bits por píxel\n", v.largura, v.altura, v.bpp);
        ad(t, "Bytes por línea:   %u (pitch)\n", v.pitch);
        ad(t, "Framebuffer:       0x%08x (%s en pantalla)\n", v.framebuffer,
           tamanho(a, sizeof a, (uint64_t)v.pitch * v.altura));
        ad(t, "Colores:           rojo %u bits en %u, verde %u en %u, azul %u en %u\n",
           v.verm_tam, v.verm_pos, v.verde_tam, v.verde_pos, v.azul_tam, v.azul_pos);
    }
    if (v.vbe_versao) {
        ad(t, "VBE:               versión %u.%u, %s de memoria de vídeo\n", v.vbe_versao >> 8, v.vbe_versao & 0xFF,
           tamanho(a, sizeof a, (uint64_t)v.memoria_kb << 10));
        if (v.oem[0]) ad(t, "OEM:               %s\n", v.oem);
        if (v.fabricante[0]) ad(t, "Fabricante:        %s\n", v.fabricante);
        if (v.produto[0]) ad(t, "Producto:          %s %s\n", v.produto, v.revisao);
    }
    ad(t, "Escritorio:        %s\n", v.escritorio ? "activo" : "no (terminal de pantalla completa)");
    /* D195: 2D por la GPU (copiar y rellenar rectángulos del escritorio). */
    info_acel2d_t ac;
    memset(&ac, 0, sizeof ac);
    if (v.grafico && info_sistema(INFO_ACEL2D, &ac, sizeof ac) == 0) {
        /* D195: «<motor> (por qué)» o «CPU (por qué)»: lo primero que hay que mirar en
         * un PC real (el detalle, en las líneas [ACEL2D] del log). */
        if (ac.ativo) ad(t, "Aceleración 2D:    %s (%s)\n", ac.nome, ac.motivo);
        else ad(t, "Aceleración 2D:    CPU (%s)\n", ac.motivo);
    }
    /* D057-D059: write-combining, cambio en caliente y preferencia (pantalla). */
    info_tela_t te;
    memset(&te, 0, sizeof te);
    if (v.grafico && info_sistema(INFO_TELA, &te, sizeof te) == 0) {
        if (te.wc) ad(t, "Write-combining:   activo (vía %s), framebuffer a %u MB/s\n", te.wc_via, te.wc_mbs);
        else ad(t, "Write-combining:   no (%s), framebuffer a %u MB/s\n", te.wc_motivo, te.wc_mbs);
        if (te.fps_tela_cheia) ad(t, "Pantalla completa:  %u fps\n", te.fps_tela_cheia);
        if (te.a_quente) ad(t, "Cambio de modo:    en caliente (Bochs/QEMU), hasta %ux%u\n", te.max_largura, te.max_altura);
        else ad(t, "Cambio de modo:    al reiniciar (programa pantalla)\n");
        if (te.pref_largura) ad(t, "Al arrancar:       %ux%u (guardada con pantalla)\n", te.pref_largura, te.pref_altura);
        else ad(t, "Al arrancar:       automática\n");
    }
    /* Familia de la GPU por su identificador PCI y motor 3D de SaguiGL (D104). */
    static info_pci_t pv[64];
    int np = info_sistema(INFO_PCI, pv, 64);
    for (int i = 0; i < np; i++) {
        const info_pci_t *p = &pv[i];
        if (p->classe != 0x03) continue;
        gpu_id_t g = gpu_identificar(p->fabricante, p->modelo);
        ad(t, "GPU:               %04x:%04x, %s\n", p->fabricante, p->modelo, g.familia);
        if (g.detalle[0]) ad(t, "  Chips:          %s\n", g.detalle);
        ad(t, "  SaguiGL 3D:     %s\n", g.motor);
    }
}

/* ------------------------------------------------------------ sistema */

/* El sonido (D088): la placa, el códec y la salida que activó el driver. */
static void secao_som(texto_t *t)
{
    info_audio_t au;
    memset(&au, 0, sizeof au);
    info_sistema(INFO_AUDIO, &au, sizeof au);
    if (au.estado == AUDIO_SEM_DRIVER) {
        ad(t, "Sonido:            tarjeta %04x:%04x sin driver (solo el altavoz del PC)\n", au.fabricante, au.modelo);
        return;
    }
    if (au.estado != AUDIO_FUNCIONANDO) {
        ad(t, "Sonido:            solo el altavoz del PC (no hay tarjeta de sonido)\n");
        return;
    }
    ad(t, "Sonido:            %s (driver %s), %u Hz\n", au.placa, au.driver, au.taxa);
    ad(t, "  Códec:           %s\n", au.codec);
    if (au.irq != 0xFF) ad(t, "  Interrupción:    IRQ %u (%u atendidas)\n", au.irq, au.interrupcoes);
    else ad(t, "  Interrupción:    no (sondeo a cada tic del reloj)\n");
    ad(t, "  Volumen:         %u %%%s; %u flujo(s) abiertos; %u ms de margen; %u cortes\n", au.volume,
       au.silencio ? " (silencio)" : "", au.nfluxos, au.latencia_ms, au.falhas);
    if (!au.eq_etapas) ad(t, "  Ecualizador:     plano\n");
    else {
        char v[5][4];
        ad(t, "  Ecualizador:     %s, %s, %s, %s, %s dB (60 Hz, 250 Hz, 1 kHz, 4 kHz, 12 kHz)\n", eq_db_texto(v[0], au.eq_db[0]),
           eq_db_texto(v[1], au.eq_db[1]), eq_db_texto(v[2], au.eq_db[2]), eq_db_texto(v[3], au.eq_db[3]), eq_db_texto(v[4], au.eq_db[4]));
    }
}

/* "Apagado ACPI: disponible (S5 = 5/5)" (D113). */
static void linha_energia(texto_t *t)
{
    static info_energia_t e;
    memset(&e, 0, sizeof e);
    if (info_sistema(INFO_ENERGIA, &e, sizeof e) < 0) return;
    static const char *const estados[] = { "no hay ACPI", "tablas erróneas", "sin \\_S5_ legible", "disponible",
                                           "desactivado" };
    ad(t, "Apagado ACPI:      %s", e.estado <= ENERGIA_DESATIVADO ? estados[e.estado] : "?");
    if (e.origem_s5[0]) ad(t, " (S5 = %u/%u)", e.slp_typ_a, e.slp_typ_b);
    ad(t, "\nBotón encendido:   %s\n", e.botao ? "atendido" : e.botao_metodo ? "dispositivo AML (no atendido)"
                                                                     : "no atendido");
}

/* D215: el Wi-Fi (pincho Realtek RTL8188EU), por SIS_WIFI. */
static void linha_wifi(texto_t *t)
{
    static info_wifi_t w;
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(SIS_WIFI), "b"(WIFI_ESTADO), "c"(&w), "d"(sizeof w), "S"(0), "D"(0) : "memory");
    if (r < 0) return;
    static const char *const est[] = { "sin adaptador", "iniciando", "listo, sin conectar", "conectando",
                                       "comprobando la clave", "conectado", "error", "sin firmware" };
    ad(t, "Wi-Fi:             %s, MAC %02x:%02x:%02x:%02x:%02x:%02x, driver rtl8188eu%s%s\n",
       w.chip[0] ? w.chip : "Realtek RTL8188EU", w.mac[0], w.mac[1], w.mac[2], w.mac[3], w.mac[4], w.mac[5],
       w.firmware[0] ? ", firmware " : "", w.firmware);
    /* D220: la seguridad de la conexión (WEP y TKIP, con su aviso) */
    static const char *const seg[] = { "abierta", "WEP: insegura", "WPA", "WPA2", "?", "WPA3", "WPA2/WPA3", "WPA/WPA2" };
    if (w.estado == WIFI_CONECTADO)
        ad(t, "                   conectado a %s (canal %u, %d dBm, %u Mb/s, %s%s)\n", w.ssid, w.canal, w.sinal, w.taxa_mbps,
           w.seguranca < 8 ? seg[w.seguranca] : "?", (w.detalhe & WIFI_DET_TKIP) ? " con TKIP: débil" : "");
    else ad(t, "                   %s%s%s\n", w.estado < 8 ? est[w.estado] : "?", w.erro[0] ? ": " : "", w.erro);
}

/* D218: el bloqueo de publicidad y rastreo por DNS. */
static void linha_bloqueo(texto_t *t)
{
    info_bloqueo_t b;
    memset(&b, 0, sizeof b);
    if (bloqueo_dns(BLOQ_ESTADO, &b, sizeof b) < 0) return;
    if (!b.activo) {
        ad(t, "Bloqueo DNS:       desactivado%s\n", b.pedido ? " (pedido, pero sin tabla: bloqueo estado)" : "");
        return;
    }
    ad(t, "Bloqueo DNS:       activado, %u dominios (%u KB), %u de %u consultas bloqueadas\n",
       b.bloqueados, (b.bytes + 1023) / 1024, b.bloqueadas, b.consultas);
}

static void secao_sistema(texto_t *t)
{
    info_versao_t v;
    info_memoria_t m;
    static info_processo_t p[64];
    char a[24], b[24];
    info_sistema(INFO_VERSAO, &v, sizeof v);
    info_sistema(INFO_MEMORIA, &m, sizeof m);
    int procs = info_sistema(INFO_PROCESSOS, p, 64);
    ad(t, "Sistema:           " SAGUI_NOME " %s (imagen de %u KB)\n", v.versao, v.imagem_bytes / 1024);
    ad(t, "Compilación:       " SAGUI_VARIANTE "\n");
#ifdef SAGUI_SEM_GPL
    ad(t, "                   sin GPL: Realtek solo con inicio clásico; temperaturas sin tablas por modelo\n");
#endif
    ad(t, "Encendido hace:    %u h %02u min %02u s\n", v.segundos_ligado / 3600, v.segundos_ligado / 60 % 60,
       v.segundos_ligado % 60);
    ad(t, "Procesos:          %d\n", procs);
    /* D157: os processadores (o Administrador de tarefas tambem os mostra). */
    static info_nucleo_t cpus[8];
    int ncpu = info_sistema(INFO_NUCLEOS, cpus, 8);
    if (ncpu < 1) ncpu = 1;
    ad(t, "Processadores:     %d\n", ncpu);
    for (int i = 0; i < ncpu; i++) {
        unsigned oci = cpus[i].tiques ? cpus[i].tiques_ociosos * 100 / cpus[i].tiques : 0;
        ad(t, "  CPU %u: %s, %u tiques, %u%% ocioso, processo %d, núcleo %u hilo %u\n", cpus[i].indice,
           cpus[i].estado == 1 ? "arrancada" : cpus[i].estado == 2 ? "ociosa" : "livre",
           cpus[i].tiques, oci, (int)cpus[i].pid, cpus[i].nucleo, cpus[i].hilo);   /* D174 */
    }
    int lay = teclado_layout(-1);
    ad(t, "Teclado:           %s\n", lay == TECLADO_ES ? "ES (español)" : lay == TECLADO_ABNT2 ? "ABNT2 (Brasil)" : "?");
    linha_energia(t);
    secao_som(t);
    ad(t, "Memoria:           libre %s de %s\n", tamanho(a, sizeof a, (uint64_t)m.livre_kb << 10),
       tamanho(b, sizeof b, (uint64_t)m.gerida_kb << 10));
    ad(t, "Discos:\n");
    info_disco_t d;
    int k = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++, k++) {
        if (d.particao) continue;
        /* D207: el tipo, como en el informe ([CD] para los lectores de CD/DVD). */
        const char *tipo = (d.flags & DISCO_CD) ? "[CD]" : (d.flags & DISCO_NVME) ? "[NVMe]" : (d.flags & DISCO_INTERNO) ? "[ATA]" : (d.flags & DISCO_REMOVIVEL) ? "[USB]" : "[?]";
        bool mt = d.montado && d.ponto[0];
        ad(t, "  %s %s: %s %s, %s%s, %s%s%s%s\n", d.nome, tipo, d.fabricante, d.produto,
           tamanho(a, sizeof a, (uint64_t)d.setores * 512), (d.flags & DISCO_BLOCO4K) ? ", bloque 4096" : (d.flags & DISCO_NVME) ? ", bloque 512" : "",
           d.formato, mt ? " (montado en " : "", mt ? d.ponto : "", mt ? ")" : "");
    }
    if (!k) ad(t, "  ninguno\n");
    static char usb[2048];
    int n = info_sistema(INFO_USB, usb, sizeof usb - 1);
    if (n > 0) {
        usb[n] = 0;
        ad(t, "USB:\n");
        for (char *l = usb; *l;) {
            char *f = acha_char(l, '\n');
            if (f) *f = 0;
            if (*l) ad(t, "  %s\n", l);
            if (!f) break;
            l = f + 1;
        }
    }
    linha_wifi(t);
    linha_bloqueo(t);
}

/* ------------------------------------------------------------ SaguiMark (D093) */

static sm_resultado_t marca;       /* el último resultado de esta ventana */
static bool tem_marca;
static char marca_arquivo[96];

/* El último resultado guardado (el nombre tiene la fecha: el mayor es el más nuevo). */
static bool ultimo_prefixo(const char *prefixo, bool sem_gpu, char *caminho, int max)
{
    static const char *const pastas[] = { "/home", "/tmp" };
    char melhor[NOME_MAX] = "";
    const char *pasta = NULL;
    for (int k = 0; k < 2; k++) {
        entrada_pasta_t e;
        for (int i = 0; listar(pastas[k], i, &e) == 1; i++)
            if (comeca_com(e.nome, prefixo) && !(sem_gpu && comeca_com(e.nome, "saguimark-gpu-")) &&
                compara_texto(e.nome, melhor) > 0) {
                copia_texto(melhor, e.nome, sizeof melhor);
                pasta = pastas[k];
            }
    }
    if (!pasta) return false;
    formatar_texto(caminho, (size_t)max, "%s/%s", pasta, melhor);
    return true;
}
/* Los resultados del SaguiMark general no incluyen los de la pestaña GPU (D243). */
static bool ultimo_resultado(char *caminho, int max) { return ultimo_prefixo("saguimark-", true, caminho, max); }
static bool ultimo_gpu(char *caminho, int max) { return ultimo_prefixo("saguimark-gpu-", false, caminho, max); }

/* Copia hasta "linhas" líneas de un archivo de texto. */
static void ad_arquivo(texto_t *t, const char *caminho, int linhas)
{
    static char b[8192];
    int fd = abrir(caminho, ABRIR_LER), n = 0;
    if (fd >= 0) { n = ler(fd, b, sizeof b - 1); fechar(fd); }
    if (n <= 0) { ad(t, "  (no se ha podido leer %s)\n", caminho); return; }
    b[n] = 0;
    char *l = b;
    for (int k = 0; *l && (linhas <= 0 || k < linhas); k++) {
        char *f = acha_char(l, '\n');
        if (f) *f = 0;
        ad(t, "%s\n", l);
        if (!f) break;
        l = f + 1;
    }
}

static void secao_benchmark(texto_t *t)
{
    static char b[8192];
    if (tem_marca) {
        sm_texto(&marca, b, sizeof b);
        ad_texto(t, b, (int)tam_texto(b));
        ad(t, "\n");
        sm_comparar(&marca, b, sizeof b);
        ad_texto(t, b, (int)tam_texto(b));
        if (marca_arquivo[0]) ad(t, "\nResultado guardado en %s\n", marca_arquivo);
        return;
    }
    ad(t, "SaguiMark %u: pruebas propias de enteros, FPU x87, SIMD (SSE2), memoria y gráficos 2D, con el\n", SM_VERSAO);
    ad(t, "resultado verificado. 1000 puntos = la máquina de referencia (por ahora, provisional:\n");
    ad(t, "una máquina virtual QEMU/KVM; la definitiva será el HP dv7).\n\n");
    ad(t, "  Rápido:  3 repeticiones de cada prueba.\n");
    ad(t, "  Preciso: 9 repeticiones (recomendado para comparar PC).\n\n");
    ad(t, "Cierra los demás programas antes de medir. Mientras mide, la ventana solo\n");
    ad(t, "dibuja la barra de progreso. También existe el comando \"saguimark\".\n");
    ad(t, "Gráficos 2D: abre una ventanita de 128x128 en la esquina de abajo a la izquierda y pinta\n");
    ad(t, "líneas, rectángulos y bloques (también hasta la pantalla). Las puntuaciones de versiones\n");
    ad(t, "distintas de SaguiMark no se pueden comparar.\n");
    char c[96];
    if (ultimo_resultado(c, sizeof c)) {
        ad(t, "\nÚltimo resultado guardado (%s):\n\n", c);
        ad_arquivo(t, c, 0);
    } else {
        ad(t, "\n");
        static sm_resultado_t vazio;
        sm_comparar(&vazio, b, sizeof b);
        ad_texto(t, b, (int)tam_texto(b));
    }
}

/* ------------------------------------------------------------ Rendimiento (D176) */

#define BENCH_HISTORIAL  "/home/.benchmark"
#define BENCH_HISTORIAL2 "/tmp/.benchmark"

/* Una línea del historial de /bin/benchmark: fecha|cpu|núcleos|planificador|modo|mono|procesos|hilos|... */
typedef struct {
    char     data[20], cpu[64], modo[10];
    unsigned nucleos, mono, mp, mh;
} bench_linha_t;

/* Lee el campo n (de 0) de una línea separada por '|' en "dst". */
static const char *campo_linha(const char *l, int n, char *dst, int max)
{
    while (n-- > 0) {
        while (*l && *l != '|' && *l != '\n') l++;
        if (*l != '|') { dst[0] = 0; return NULL; }
        l++;
    }
    int k = 0;
    while (*l && *l != '|' && *l != '\n' && k < max - 1) dst[k++] = *l++;
    dst[k] = 0;
    return l;
}

static unsigned campo_num(const char *l, int n)
{
    char b[24];
    int v = 0;
    campo_linha(l, n, b, sizeof b);
    texto_para_numero(b, &v);
    return (unsigned)v;
}

/* La última línea del historial que sirva: con multi, la que tenga puntuación multinúcleo. */
static bool bench_ultima(bool multi, bench_linha_t *o)
{
    static char buf[16384];
    static const char *const caminhos[] = { BENCH_HISTORIAL, BENCH_HISTORIAL2 };
    bool achou = false;
    char melhor[20] = "";
    for (int k = 0; k < 2; k++) {
        int fd = abrir(caminhos[k], ABRIR_LER);
        if (fd < 0) continue;
        int n = 0, r;
        while ((r = ler(fd, buf + n, (unsigned)(sizeof buf - 1 - (unsigned)n))) > 0) {
            n += r;
            if (n >= (int)sizeof buf - 1) {   /* archivo grande: me quedo con la segunda mitad */
                memmove(buf, buf + n / 2, (size_t)(n - n / 2));
                n -= n / 2;
            }
        }
        fechar(fd);
        buf[n] = 0;
        for (char *l = buf; *l;) {
            char *fim = l;
            while (*fim && *fim != '\n') fim++;
            char salvo = *fim;
            *fim = 0;
            unsigned mono = campo_num(l, 5), mp = campo_num(l, 6), mh = campo_num(l, 7);
            char data[20];
            campo_linha(l, 0, data, sizeof data);
            if (data[0] && (multi ? (mp || mh) : mono > 0) && compara_texto(data, melhor) >= 0) {
                copia_texto(melhor, data, sizeof melhor);
                copia_texto(o->data, data, sizeof o->data);
                campo_linha(l, 1, o->cpu, sizeof o->cpu);
                campo_linha(l, 4, o->modo, sizeof o->modo);
                o->nucleos = campo_num(l, 2);
                o->mono = mono; o->mp = mp; o->mh = mh;
                achou = true;
            }
            *fim = salvo;
            l = salvo ? fim + 1 : fim;
        }
    }
    return achou;
}

static void secao_rendimiento(texto_t *t)
{
    ad(t, "Rendimiento (D176): el programa /bin/benchmark mide ocho cargas (entero, coma flotante,\n");
    ad(t, "memoria, cifrado y llamadas al sistema) con un núcleo y con todos a la vez, con procesos y con\n");
    ad(t, "hilos. 1000 puntos por carga = QEMU con KVM y una CPU; el total es la media geométrica.\n");
    ad(t, "En la ventana, los botones \"Prueba de un núcleo\" y \"Prueba multinúcleo\" la lanzan.\n");
    ad(t, "En el terminal: benchmark [--mono | --multi [N]] [--rapido]\n\n");
    bench_linha_t l;
    memset(&l, 0, sizeof l);
    if (bench_ultima(false, &l))
        ad(t, "Última ejecución guardada (%s, %u núcleos): un núcleo %u, procesos %u, hilos %u\n",
           l.data, l.nucleos, l.mono, l.mp, l.mh);
    else
        ad(t, "Todavía no hay ninguna ejecución guardada en /home/.benchmark.\n");
}

/* ------------------------------------------------------------ GPU (D243) */

/* Seis pruebas 2D. Las tres primeras (rellenos, copias, copias solapadas) las hace el kernel por
 * el acelerador de la GPU y por la CPU con las mismas órdenes y con la suma de la zona de cada
 * camino (INFO_ACEL2D_PROVA): si no coinciden, no hay puntuación. Sin acelerador se hacen aquí,
 * por software, con la libsagui sobre un búfer en la RAM y se comprueban contra un
 * dibujo ingenuo e independiente. Líneas, texto y ventana no los acelera ninguna GPU de las que
 * soporta el kernel (solo copiar y rellenar): salen solo por la CPU. */
#define GB_N 6
#define GB_RELLENOS 0
#define GB_COPIAS   1
#define GB_SOLAPES  2
#define GB_LINEAS   3
#define GB_TEXTO    4
#define GB_VENTANA  5

typedef struct {
    const char *nome;
    unsigned ops, pix;          /* órdenes y píxeles movidos (0 = no cuenta) */
    unsigned us_gpu, us_cpu;    /* 0 = no medido */
    unsigned ref_taxa;          /* órdenes por segundo de la referencia provisional (1000 puntos) */
    bool feito, soma_ok;
    unsigned pontos;
} gb_prova_t;

typedef struct {
    gb_prova_t p[GB_N];
    int acel;                   /* 1 = hay acelerador y dio los mismos píxeles; 0 = por software; -1 = el acelerador falló */
    char motivo[160];
    unsigned total;
    int erros;
    unsigned tsc_khz;
    data_t data;
} gb_t;

static gb_t gb_res;
static bool gb_tem;
static char gb_arquivo[96];

/* Órdenes por segundo de la referencia provisional (QEMU con KVM = 1000 puntos por prueba). */
static const char *const gb_nomes[GB_N] = { "Rellenos de rectángulos", "Copias de bloques", "Copias solapadas (desplazar)",
                                            "Líneas", "Texto", "Ventana (redibujar y mover)" };
static const unsigned gb_ref[GB_N] = { 4400000, 3300000, 1850000, 6000000, 560000, 16500 };

static inline uint64_t gb_rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t gb_div64(uint64_t n, uint32_t d)
{
    if (!d) return 0;
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

static unsigned gb_us(uint64_t ciclos, unsigned khz)
{
    unsigned u = (unsigned)gb_div64(ciclos * 1000, khz);
    return u ? u : 1;
}

static uint32_t gb_sem;
static uint32_t gb_aleat(uint32_t n)
{
    gb_sem ^= gb_sem << 13; gb_sem ^= gb_sem >> 17; gb_sem ^= gb_sem << 5;
    return gb_sem % n;
}

static uint32_t gb_soma(const uint32_t *b, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (b[i] & 0xFFFFFF)) * 16777619u;
    return h;
}

#define GB_L 512
#define GB_A 64
#define GB_REP 20      /* veces que se repite la tanda de órdenes por software */

/* Tandas por software (sin acelerador): g_* de la libsagui contra un dibujo ingenuo aparte. */
static void gb_software(gb_prova_t *p, int t, unsigned khz)
{
    static uint32_t px[GB_L * GB_A], ref[GB_L * GB_A], tmp[GB_L * GB_A];
    jan_t off = { 0, px, GB_L, GB_A, 0, 0, 0 };
    for (int i = 0; i < GB_L * GB_A; i++) px[i] = ref[i] = (uint32_t)i * 2654435761u & 0xFFFFFF;
    uint32_t orden[120][8];
    gb_sem = 0xBE7C0u + (uint32_t)t;
    unsigned pix = 0;
    for (int k = 0; k < 120; k++) {
        uint32_t *o = orden[k];
        o[2] = 16 + gb_aleat(GB_L / 2 - 16);     /* l */
        o[3] = 4 + gb_aleat(GB_A / 2 - 4);       /* a */
        o[0] = gb_aleat(GB_L - o[2] + 1);        /* ox */
        o[1] = gb_aleat(GB_A - o[3] + 1);        /* oy */
        o[6] = gb_aleat(0x1000000);              /* color */
        if (t == GB_SOLAPES) {
            int dx = (int)o[0] + (int)gb_aleat(9) - 4, dy = (int)o[1] + (int)gb_aleat(5) - 2;
            if (dx < 0) dx = 0;
            if (dy < 0) dy = 0;
            if (dx > GB_L - (int)o[2]) dx = GB_L - (int)o[2];
            if (dy > GB_A - (int)o[3]) dy = GB_A - (int)o[3];
            o[4] = (uint32_t)dx; o[5] = (uint32_t)dy;
        } else {
            o[4] = gb_aleat(GB_L - o[2] + 1);
            o[5] = gb_aleat(GB_A - o[3] + 1);
        }
        pix += o[2] * o[3];
    }
    uint64_t t0 = gb_rdtsc();
    for (int r = 0; r < GB_REP; r++)
        for (int k = 0; k < 120; k++) {
            const uint32_t *o = orden[k];
            if (t == GB_RELLENOS) g_retangulo(&off, (int)o[0], (int)o[1], (int)o[2], (int)o[3], o[6]);
            else g_copiar_area(&off, (int)o[0], (int)o[1], (int)o[2], (int)o[3], (int)o[4], (int)o[5]);
        }
    uint64_t t1 = gb_rdtsc();
    for (int r = 0; r < GB_REP; r++)
        for (int k = 0; k < 120; k++) {
            const uint32_t *o = orden[k];
            if (t == GB_RELLENOS) {
                for (uint32_t y = 0; y < o[3]; y++)
                    for (uint32_t x = 0; x < o[2]; x++) ref[(o[1] + y) * GB_L + o[0] + x] = o[6] | 0xFF000000u;
            } else {   /* copiar por un búfer intermedio: no depende de la dirección del solape */
                for (uint32_t y = 0; y < o[3]; y++)
                    for (uint32_t x = 0; x < o[2]; x++) tmp[y * o[2] + x] = ref[(o[1] + y) * GB_L + o[0] + x];
                for (uint32_t y = 0; y < o[3]; y++)
                    for (uint32_t x = 0; x < o[2]; x++) ref[(o[5] + y) * GB_L + o[4] + x] = tmp[y * o[2] + x];
            }
        }
    p->ops = 120 * GB_REP;
    p->pix = pix * GB_REP;
    p->us_cpu = gb_us(t1 - t0, khz);
    p->soma_ok = gb_soma(px, GB_L * GB_A) == gb_soma(ref, GB_L * GB_A);
    p->feito = true;
}

/* Líneas y texto sobre el búfer: se comprueba que el resultado es el esperado (suma FNV). */
#define GB_SOMA_LINEAS 0x05A2D6FBu
#define GB_SOMA_TEXTO  0xE1C40346u
static void gb_linhas_texto(gb_prova_t *p, int t, unsigned khz)
{
    static uint32_t px[256 * 128];
    jan_t off = { 0, px, 256, 128, 0, 0, 0 };
    memset(px, 0, sizeof px);
    uint64_t t0 = gb_rdtsc();
    unsigned n;
    if (t == GB_LINEAS) {
        n = 40000;
        gb_sem = 31337;
        for (unsigned i = 0; i < n; i++) {
            int x0 = (int)gb_aleat(288) - 16, y0 = (int)gb_aleat(160) - 16, x1 = (int)gb_aleat(288) - 16, y1 = (int)gb_aleat(160) - 16;
            g_linha(&off, x0, y0, x1, y1, gb_aleat(0x1000000));
        }
    } else {
        n = 600;
        char s[40];
        for (unsigned i = 0; i < n; i++) {
            formatar_texto(s, sizeof s, "SaguiOS GPU %05u texto de prueba", i);
            g_texto(&off, (int)(i * 7) % 40, (int)(i * 13) % 112, s, 0x102030u + i * 4099u);
        }
    }
    uint64_t t1 = gb_rdtsc();
    p->ops = n;
    p->pix = 0;
    p->us_cpu = gb_us(t1 - t0, khz);
    uint32_t s = gb_soma(px, 256 * 128);
    uint32_t esp = t == GB_LINEAS ? GB_SOMA_LINEAS : GB_SOMA_TEXTO;
    p->soma_ok = s == esp;
    registrar("infopc: gpu suma %s %08x", t == GB_LINEAS ? "lineas" : "texto", s);
    p->feito = true;
}

/* Redibujar una ventana completa y moverla: va por el camino que use el sistema (la GPU si hay). */
static void gb_ventana(gb_prova_t *p, unsigned khz)
{
    info_video_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VIDEO, &v, sizeof v);
    if (!v.escritorio) return;
    jan_t *j = jan_abrir("GPU 2D", 320, 200);
    if (!j) return;
    char s[48];
    uint64_t t0 = gb_rdtsc();
    for (int i = 0; i < 40; i++) {
        g_retangulo(j, 0, 0, 320, 200, COR_FUNDO);
        for (int k = 0; k < 8; k++) {
            formatar_texto(s, sizeof s, "Cuadro %d, linea %d", i, k);
            g_texto(j, 8, 6 + k * 20, s, COR_TEXTO);
            g_retangulo(j, 200, 6 + k * 20, 20 + k * 12, 12, 0x0B7A3E + (uint32_t)(i * 5));
        }
        jan_mostrar(j);
    }
    uint64_t t1 = gb_rdtsc();
    for (int i = 0; i < 60; i++) mover_janela(j->id, 40 + (i * 7) % 300, 40 + (i * 5) % 200);
    uint64_t t2 = gb_rdtsc();
    jan_fechar(j);
    p->ops = 100;
    p->pix = 40 * 320 * 200 + 60 * 320 * 200;
    p->us_cpu = gb_us(t2 - t0, khz);
    (void)t1;
    p->soma_ok = true;
    p->feito = true;
}

static void gb_executar(gb_t *r)
{
    memset(r, 0, sizeof *r);
    ler_data(&r->data);
    info_tsc_t ts;
    memset(&ts, 0, sizeof ts);
    info_sistema(INFO_CPU_MHZ, &ts, sizeof ts);
    r->tsc_khz = ts.khz;
    if (!r->tsc_khz) { r->erros = 1; copia_texto(r->motivo, "la CPU no tiene TSC medido", sizeof r->motivo); return; }
    for (int i = 0; i < GB_N; i++) { r->p[i].nome = gb_nomes[i]; r->p[i].ref_taxa = gb_ref[i]; }
    /* El kernel: las mismas órdenes por la GPU y por la CPU (tres tandas). */
    static info_acel2d_prova_t k;
    memset(&k, 0, sizeof k);
    info_sistema(INFO_ACEL2D_PROVA, &k, sizeof k);
    copia_texto(r->motivo, k.texto, sizeof r->motivo);
    r->acel = k.resultado == 1 && k.bn == 3 ? 1 : k.resultado == 0 ? -1 : 0;
    for (int t = 0; t < 3; t++) {
        gb_prova_t *p = &r->p[t];
        if (r->acel == 1) {
            p->ops = k.b[t].ops;
            p->pix = k.b[t].pixeis;
            p->us_gpu = k.b[t].us_gpu ? k.b[t].us_gpu : 1;
            p->us_cpu = k.b[t].us_cpu ? k.b[t].us_cpu : 1;
            p->soma_ok = k.b[t].soma_gpu == k.b[t].soma_cpu && k.b[t].us_gpu;
            p->feito = true;
        } else {
            gb_software(p, t, r->tsc_khz);
        }
    }
    gb_linhas_texto(&r->p[GB_LINEAS], GB_LINEAS, r->tsc_khz);
    gb_linhas_texto(&r->p[GB_TEXTO], GB_TEXTO, r->tsc_khz);
    gb_ventana(&r->p[GB_VENTANA], r->tsc_khz);
    /* Puntos: lo que tarda el camino más rápido, contra la referencia (ops por segundo normalizadas). */
    uint64_t soma = 0;
    int n = 0;
    for (int i = 0; i < GB_N; i++) {
        gb_prova_t *p = &r->p[i];
        if (!p->feito) continue;
        if (!p->soma_ok) { r->erros++; continue; }
        unsigned us = p->us_gpu && p->us_gpu < p->us_cpu ? p->us_gpu : p->us_cpu;
        uint64_t taxa = gb_div64((uint64_t)p->ops * 1000000u, us);
        p->pontos = (unsigned)gb_div64(taxa * 1000, p->ref_taxa);
        soma += p->pontos;
        n++;
    }
    r->total = n && !r->erros ? (unsigned)gb_div64(soma, (uint32_t)n) : 0;
}

static void gb_taxa(char *d, size_t m, const gb_prova_t *p, unsigned us)
{
    if (!us) { copia_texto(d, "-", m); return; }
    unsigned ops = (unsigned)gb_div64((uint64_t)p->ops * 1000000u, us);
    if (p->pix) {
        unsigned mb = (unsigned)gb_div64((uint64_t)p->pix * 4, us);   /* píxeles*4 B / us = MB/s */
        formatar_texto(d, m, "%u op/s %u MB/s", ops, mb);
    } else {
        formatar_texto(d, m, "%u op/s", ops);
    }
}

static void gb_texto(texto_t *t, const gb_t *r)
{
    ad(t, "Benchmark 2D (SaguiOS GPU %u; referencia provisional: QEMU con KVM = 1000 por prueba)\n", 1);
    ad(t, "Medido el %02u/%02u/%04u a las %02u:%02u, TSC %u,%02u MHz\n", r->data.dia, r->data.mes, r->data.ano,
       r->data.hora, r->data.minuto, r->tsc_khz / 1000, r->tsc_khz % 1000 / 10);
    if (r->acel == 1) ad(t, "Camino GPU: %s\n", r->motivo);
    else if (r->acel < 0) ad(t, "Camino GPU: el acelerador FALLÓ la autoprueba (%s); solo por software\n", r->motivo);
    else ad(t, "Camino GPU: no hay acelerador 2D (%s); solo por software (CPU)\n", r->motivo);
    ad(t, "\n%-30s %-24s %-24s %6s %s\n", "Prueba", "GPU", "CPU (software)", "Ganan.", "Suma  Puntos");
    for (int i = 0; i < GB_N; i++) {
        const gb_prova_t *p = &r->p[i];
        if (!p->feito) { ad(t, "%-30s n/d (sin escritorio)\n", p->nome); continue; }
        char g[40], c[40], x[16] = "-";   /* sin GPU en la prueba: sin ganancia (no vale «x1,00») */
        gb_taxa(g, sizeof g, p, p->us_gpu);
        gb_taxa(c, sizeof c, p, p->us_cpu);
        if (p->us_gpu && p->us_cpu) {
            unsigned q = (unsigned)gb_div64((uint64_t)p->us_cpu * 100, p->us_gpu);
            formatar_texto(x, sizeof x, "x%u,%02u", q / 100, q % 100);
        }
        const char *gg = p->us_gpu ? g : i == GB_VENTANA ? "(camino del sistema)" : i >= GB_LINEAS ? "solo CPU (sin acelerador)" : "sin acelerador";
        ad(t, "%-30s %-24s %-24s %6s %-4s %u\n", p->nome, gg, c, x, p->soma_ok ? "OK" : "MAL", p->pontos);
    }
    ad(t, "\nPuntuación 2D total: %u%s\n", r->total, r->erros ? " (no hay: alguna suma no coincide)" : "");
    ad(t, "GPU y CPU hacen las mismas órdenes; las sumas de lo dibujado tienen que ser iguales para puntuar.\n");
    ad(t, "La CPU incluye escribir el destino en la tarjeta (lo que haría el escritorio sin acelerador).\n");
}

/* D285: el 3D por la GPU: estado de la capa del kernel y, si se midió, su puntuación. */
static info_gpu3d_prova_t g3_res;
static bool g3_tem;

static void g3_texto(texto_t *t)
{
    info_gpu3d_t g;
    memset(&g, 0, sizeof g);
    ad(t, "\n3D\n");
    if (info_sistema(INFO_GPU3D, &g, sizeof g) != 0) {
        ad(t, "  3D por GPU:      no disponible (el kernel no lo sabe)\n");
        ad(t, "  SaguiGL:         SaguiGL por software (CPU)\n");
        return;
    }
    if (g.estado == GPU3D_ATIVO)
        ad(t, "  3D por GPU:      activo: driver %s, %s; órdenes %s; memoria de GPU %u KB (%u libres)\n", g.nome, g.placa,
           g.variante ? "en el anillo" : "por lotes", g.memoria_kb, g.livre_kb);
    else if (g.estado == GPU3D_SIN_PROBAR)
        ad(t, "  3D por GPU:      driver %s para %s, sin probar todavía (se prueba con el primer uso o al medir)\n", g.nome, g.placa);
    else
        ad(t, "  3D por GPU:      no disponible: %s\n", g.estado == GPU3D_SEM_GPU ? "ninguna tarjeta con driver 3D" :
           g.estado == GPU3D_DESATIVADO ? "desactivado por la opción de arranque" :
           g.estado == GPU3D_APAGADO ? "apagado a mano (gpu3d encender)" : "apagado por un fallo (gpu3d encender lo repite)");
    ad(t, "  Motivo:          %s\n", g.motivo);
    if (g.estado == GPU3D_ATIVO)
        ad(t, "  SaguiGL:         por la GPU en las ventanas mientras el motor esté libre%s; si falla, SaguiGL por software\n",
           g.dono ? " (ahora lo usa un programa)" : "");
    else
        ad(t, "  SaguiGL:         SaguiGL por software (CPU)\n");
    if (g.triangulos || g.falhas)
        ad(t, "  Uso:             %u triángulos en %u entregas, %u cuadros, %u fallos\n", g.triangulos, g.lotes, g.quadros, g.falhas);
    if (g3_tem && g3_res.resultado == 1) {
        ad(t, "  Autoprueba 3D:   las %d pruebas dan los píxeles esperados (%u us)\n", GPU3D_N_PROVAS, g3_res.us_autoprova);
        ad(t, "  Triángulos:      %u/s (%u de 8x8 en %u us)\n", g3_res.tri_s, g3_res.tri_pequenos, g3_res.us_pequenos);
        ad(t, "  Relleno:         %u kpíxeles/s con mezcla (%u píxeles en %u us)\n", g3_res.kpix_s, g3_res.pix_grandes, g3_res.us_grandes);
        ad(t, "  Traer el cuadro: %u us por %u píxeles\n", g3_res.us_leitura, g3_res.pix_leitura);
        ad(t, "  Puntuación 3D:   %u (provisional: 100 000 triángulos/s y 100 Mpíxeles/s dan 1000)\n", g3_res.pontos);
    } else if (g3_tem && g3_res.resultado == 0)
        ad(t, "  Puntuación 3D:   no hay: %s\n", g3_res.texto);
    else if (g.pontos)
        ad(t, "  Puntuación 3D:   %u (última medida: %u triángulos/s, %u kpíxeles/s)\n", g.pontos, g.tri_s, g.kpix_s);
    else if (g.estado == GPU3D_ATIVO || g.estado == GPU3D_SIN_PROBAR)
        ad(t, "  Puntuación 3D:   sin medir (botón \"Medir\", \"infopc --bench-gpu\" o \"gpu3d probar\")\n");
    else
        ad(t, "  Puntuación 3D:   no hay (sin 3D por GPU)\n");
}

/* Mide el 3D por la GPU si hay driver y el motor está libre (enciende y prueba el driver si hace falta). */
static void g3_medir(void)
{
    info_gpu3d_t g;
    memset(&g, 0, sizeof g);
    g3_tem = false;
    if (info_sistema(INFO_GPU3D, &g, sizeof g) != 0) return;
    if ((g.estado != GPU3D_ATIVO && g.estado != GPU3D_SIN_PROBAR) || g.dono) return;
    memset(&g3_res, 0, sizeof g3_res);
    if (info_sistema(INFO_GPU3D_PROVA, &g3_res, sizeof g3_res) != 0) return;
    g3_tem = g3_res.resultado >= 0;
    registrar("infopc: gpu3d bench resultado %d pontos %u", g3_res.resultado, g3_res.pontos);
}

/* Guarda el resultado en /home (o /tmp si no se puede escribir). */
static int gb_salvar(const gb_t *r, char *caminho, int max)
{
    static info_montagem_t mt[10];
    const char *pasta = "/tmp";
    int n = info_sistema(INFO_MONTAGENS, mt, 10);
    for (int i = 0; i < n; i++)
        if (!(mt[i].opcoes & MONTAR_SO_LEITURA) && compara_texto(mt[i].ponto, "/home") == 0) pasta = "/home";
    formatar_texto(caminho, (size_t)max, "%s/saguimark-gpu-%04u%02u%02u-%02u%02u.txt", pasta, r->data.ano, r->data.mes,
                   r->data.dia, r->data.hora, r->data.minuto);
    texto_t t = { 0 };
    gb_texto(&t, r);
    g3_texto(&t);
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    int e = fd < 0 ? fd : escrever_tudo(fd, t.p, (unsigned)t.tam);
    if (fd >= 0) fechar(fd);
    libera(t.p);
    return e;
}

static void gb_medir_e_guardar(void)
{
    registrar("infopc: gpu bench inicio");
    gb_executar(&gb_res);
    g3_medir();
    gb_tem = true;
    gb_arquivo[0] = 0;
    if (gb_salvar(&gb_res, gb_arquivo, sizeof gb_arquivo) < 0) gb_arquivo[0] = 0;
    registrar("infopc: gpu bench total %u erros %d acel %d guardado %s", gb_res.total, gb_res.erros, gb_res.acel,
              gb_arquivo[0] ? gb_arquivo : "-");
}

static void secao_gpu(texto_t *t)
{
    info_video_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VIDEO, &v, sizeof v);
    static info_pci_t pv[64];
    int np = info_sistema(INFO_PCI, pv, 64), achou = 0;
    char a[24];
    for (int i = 0; i < np; i++) {
        const info_pci_t *p = &pv[i];
        if (p->classe != 0x03) continue;
        achou++;
        gpu_id_t g = gpu_identificar(p->fabricante, p->modelo);
        const char *f = nome_fabricante(p->fabricante), *d = nome_dispositivo(p->fabricante, p->modelo);
        ad(t, "GPU:               %04x:%04x  %s%s%s\n", p->fabricante, p->modelo, f ? f : "?", d ? " " : "", d ? d : "");
        ad(t, "  Subsistema:      %04x:%04x, revisión %02x, PCI %02x:%02x.%u\n", p->sub_fabricante, p->sub_modelo, p->revisao,
           p->barramento, p->dispositivo, p->funcao);
        ad(t, "  Familia:         %s\n", g.familia);
        if (g.detalle[0]) ad(t, "  Chips:           %s\n", g.detalle);
    }
    if (!achou) ad(t, "GPU:               ninguna tarjeta de vídeo en el PCI\n");
    if (v.vbe_versao) ad(t, "Memoria de vídeo: %s (VBE %u.%u%s%s)\n", tamanho(a, sizeof a, (uint64_t)v.memoria_kb << 10),
                         v.vbe_versao >> 8, v.vbe_versao & 0xFF, v.oem[0] ? ", " : "", v.oem);
    else ad(t, "Memoria de vídeo: no informada (sin VBE)\n");
    if (v.grafico) {
        info_tela_t te;
        memset(&te, 0, sizeof te);
        info_sistema(INFO_TELA, &te, sizeof te);
        ad(t, "Modo de vídeo:     %ux%u, %u bits por píxel, pitch %u, framebuffer 0x%08x\n", v.largura, v.altura, v.bpp, v.pitch,
           v.framebuffer);
        ad(t, "Write-combining:   %s\n", te.wc ? "activo" : "no");
    } else {
        ad(t, "Modo de vídeo:     modo texto VGA 80x25\n");
    }
    info_acel2d_t ac;
    memset(&ac, 0, sizeof ac);
    if (v.grafico && info_sistema(INFO_ACEL2D, &ac, sizeof ac) == 0) {
        if (ac.ativo) ad(t, "Acelerador 2D:    %s (%s)\n", ac.nome, ac.motivo);
        else ad(t, "Acelerador 2D:    por software (CPU): %s\n", ac.motivo);
        if (ac.modelo) ad(t, "  Tarjeta:         %s; espera máxima de la GPU %u ms\n", ac.confirmada ? "confirmada" : "por autoprueba", ac.prazo_ms);
        if (ac.apagado) ad(t, "  Apagado:         %s (acel2d encender lo repite)\n", ac.apagado == 2 ? "a mano" : "por el vigilante: la GPU no respondió a tiempo");
        if (ac.ativo)
            ad(t, "  Órdenes de la GPU: %u copias, %u rellenos, %u fallos\n", ac.copias, ac.rellenos, ac.falhas);
        if (ac.us_gpu || ac.us_cpu) ad(t, "  Velocidad al arrancar: GPU %u us, CPU %u us\n", ac.us_gpu, ac.us_cpu);
    } else {
        ad(t, "Acelerador 2D:    por software (CPU): sin modo gráfico\n");
    }
    g3_texto(t);
    ad(t, "\nBenchmark 2D: botón \"Medir\" (tecla M) o \"infopc --bench-gpu\". Compara el acelerador 2D de la GPU\n");
    ad(t, "con el camino por software (CPU) en rellenos, copias, solapes, líneas, texto y ventana.\n\n");
    if (gb_tem) {
        gb_texto(t, &gb_res);
        if (gb_arquivo[0]) ad(t, "\nResultado guardado en %s\n", gb_arquivo);
    } else {
        char c[96];
        if (ultimo_gpu(c, sizeof c)) { ad(t, "Último resultado guardado (%s):\n\n", c); ad_arquivo(t, c, 0); }
        else ad(t, "Todavía no se ha medido.\n");
    }
}

/* ------------------------------------------------------------ secciones */

typedef struct {
    const char *nome, *titulo;
    void (*gerar)(texto_t *t);
} secao_t;

static const secao_t secoes[] = {
    { "cpu", "CPU", secao_cpu },
    { "memoria", "Memoria", secao_memoria },
    { "placa", "Placa/BIOS", secao_placa },
    { "pci", "PCI", secao_pci },
    { "video", "Vídeo", secao_video },
    { "sistema", "Sistema", secao_sistema },
    { "benchmark", "Benchmark", secao_benchmark },
    { "rendimiento", "Rendimiento", secao_rendimiento },
    { "gpu", "GPU", secao_gpu },
};
#define QTD_SECOES (int)(sizeof secoes / sizeof secoes[0])

static int achar_secao(const char *nome)
{
    for (int i = 0; i < QTD_SECOES; i++)
        if (compara_texto(nome, secoes[i].nome) == 0) return i;
    if (compara_texto(nome, "mem") == 0) return 1;
    if (compara_texto(nome, "smbios") == 0 || compara_texto(nome, "bios") == 0) return 2;
    if (compara_texto(nome, "vídeo") == 0) return 4;
    return -1;
}

static void gerar(int s, texto_t *t)
{
    t->tam = 0;
    if (t->p) t->p[0] = 0;
    secoes[s].gerar(t);
}

static int modo_texto(int s)
{
    texto_t t = { 0 };
    for (int i = 0; i < QTD_SECOES; i++) {
        if (s >= 0 && i != s) continue;
        gerar(i, &t);
        imprimir("== %s ==\n", secoes[i].titulo);
        escrever_tudo(SAIDA, t.p, (unsigned)t.tam);
        imprimir("\n");
    }
    libera(t.p);
    return 0;
}

/* ------------------------------------------------------------ informe (D076) */

/* Lo que se sabe de cada dispositivo PCI: por identificador (fabricante y
 * modelo) o, si modelo = 0xFFFF, por fabricante; y, si fabricante = 0xFFFF,
 * por clase (clase, subclase, interfaz; 0xFF = cualquiera). La primera
 * línea que coincide vale, así que los identificadores exactos van antes que las clases.
 * Es el mismo criterio que los drivers del kernel (e1000.c, rtl8139.c, r8169.c, alc.c,
 * ehci.c, ata.c, bga.c): cuando entre un driver nuevo, la línea cambia aquí. */
enum { SUPORTADO, EXPERIMENTAL, SEM_DRIVER, NAO_PRECISA, PLANEJADO };
static const char *const nome_estado[] = { "soportado", "experimental", "sin driver", "no necesita driver",
                                           "sin driver, previsto" };

typedef struct {
    uint16_t fab, mod;
    uint8_t  classe, sub, intf;
    uint8_t  estado;
    const char *driver;   /* nombre del driver o, sin driver, la mejora que lo prevé */
} conhecido_t;

static const conhecido_t conhecidos[] = {
    /* Red (D046): e1000, RTL8139 y los experimentales r8169 y alc (REDE_EXPERIMENTAL). */
    { 0x8086, 0x100E, 0, 0, 0, SUPORTADO, "e1000" }, { 0x8086, 0x100F, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x8086, 0x1004, 0, 0, 0, SUPORTADO, "e1000" }, { 0x8086, 0x1015, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x8086, 0x1016, 0, 0, 0, SUPORTADO, "e1000" }, { 0x8086, 0x1017, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x8086, 0x101E, 0, 0, 0, SUPORTADO, "e1000" }, { 0x8086, 0x1019, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x8086, 0x1026, 0, 0, 0, SUPORTADO, "e1000" }, { 0x8086, 0x1076, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x8086, 0x107C, 0, 0, 0, SUPORTADO, "e1000" },
    { 0x10EC, 0x8139, 0, 0, 0, SUPORTADO, "rtl8139" },
    { 0x10EC, 0x8136, 0, 0, 0, EXPERIMENTAL, "r8169 (RTL810x)" },
    { 0x10EC, 0x8168, 0, 0, 0, EXPERIMENTAL, "r8169" }, { 0x10EC, 0x8169, 0, 0, 0, EXPERIMENTAL, "r8169" },
    /* Atheros AR813x/AR815x (D200): driver alc, experimental como el r8169. */
    { 0x1969, 0x2060, 0, 0, 0, EXPERIMENTAL, "alc" }, { 0x1969, 0x2062, 0, 0, 0, EXPERIMENTAL, "alc" },
    { 0x1969, 0x1073, 0, 0, 0, EXPERIMENTAL, "alc" }, { 0x1969, 0x1083, 0, 0, 0, EXPERIMENTAL, "alc" },
    { 0x1969, 0x1063, 0, 0, 0, EXPERIMENTAL, "alc" }, { 0x1969, 0x1062, 0, 0, 0, EXPERIMENTAL, "alc" },
    /* Redes de la época, previstas en la fase B de la mejora 036. */
    { 0x8086, 0x1229, 0, 0, 0, PLANEJADO, "mejora 036 (Intel e100)" },
    { 0x8086, 0x1209, 0, 0, 0, PLANEJADO, "mejora 036 (Intel e100)" },
    { 0x8086, 0x103A, 0, 0, 0, PLANEJADO, "mejora 036 (Intel e100)" },
    { 0x10B7, 0x9200, 0, 0, 0, PLANEJADO, "mejora 036 (3Com 3c905)" },
    { 0x10B7, 0x9055, 0, 0, 0, PLANEJADO, "mejora 036 (3Com 3c905)" },
    { 0x1106, 0x3065, 0, 0, 0, PLANEJADO, "mejora 036 (VIA Rhine)" },
    { 0x1106, 0x3106, 0, 0, 0, PLANEJADO, "mejora 036 (VIA Rhine)" },
    /* Vídeo: la placa de Bochs/QEMU y la de VirtualBox también cambian de modo en caliente (D059). */
    { 0x1234, 0x1111, 0, 0, 0, SUPORTADO, "vesa + bga (cambio en caliente)" },
    { 0x80EE, 0xBEEF, 0, 0, 0, SUPORTADO, "vesa + bga (cambio en caliente)" },
    { 0x1002, 0xFFFF, 0x03, 0xFF, 0xFF, SUPORTADO, "vesa (3D: SaguiGL, mejora 036 fase D)" },
    /* ACPI y gestión de energía. */
    { 0x8086, 0x7113, 0, 0, 0, SUPORTADO, "acpi (apagado S5 y botón, D113)" },
    /* Por clase. */
    { 0xFFFF, 0, 0x01, 0x01, 0xFF, SUPORTADO, "ata (PIO; DMA: mejora 036)" },
    { 0xFFFF, 0, 0x01, 0x06, 0x01, SUPORTADO, "ahci (SATA; D155)" },
    { 0xFFFF, 0, 0x01, 0x08, 0x02, SUPORTADO, "nvme (D265)" },
    { 0xFFFF, 0, 0x03, 0xFF, 0xFF, SUPORTADO, "vesa (framebuffer de la BIOS)" },
    { 0xFFFF, 0, 0x0C, 0x03, 0x20, SUPORTADO, "ehci" },
    { 0xFFFF, 0, 0x0C, 0x03, 0x00, PLANEJADO, "mejora 029 (UHCI)" },
    { 0xFFFF, 0, 0x0C, 0x03, 0x10, PLANEJADO, "mejora 029 (OHCI)" },
    { 0xFFFF, 0, 0x0C, 0x03, 0x30, SUPORTADO, "xhci (USB 3.0: pendrives, hubs, teclado y ratón; D214)" },
    { 0xFFFF, 0, 0x04, 0x01, 0xFF, SUPORTADO, "ac97 (Intel ICH y compatibles; Sound Blaster: no)" },
    { 0xFFFF, 0, 0x04, 0x03, 0xFF, SUPORTADO, "hda" },
    { 0xFFFF, 0, 0x06, 0xFF, 0xFF, NAO_PRECISA, "puente" },
    { 0xFFFF, 0, 0x05, 0xFF, 0xFF, NAO_PRECISA, "memoria" },
    { 0xFFFF, 0, 0x08, 0xFF, 0xFF, NAO_PRECISA, "periférico del chipset" },
};

static const conhecido_t *conhecer(const info_pci_t *p)
{
    for (unsigned i = 0; i < sizeof conhecidos / sizeof conhecidos[0]; i++) {
        const conhecido_t *c = &conhecidos[i];
        if (c->fab != 0xFFFF) {
            if (c->fab != p->fabricante || (c->mod != 0xFFFF && c->mod != p->modelo)) continue;
            if (c->mod == 0xFFFF && c->classe != p->classe) continue;
            return c;
        }
        if (c->classe == p->classe && (c->sub == 0xFF || c->sub == p->subclasse) &&
            (c->intf == 0xFF || c->intf == p->interface))
            return c;
    }
    return NULL;
}

static void informe_pci(texto_t *t, int *com, int *sem)
{
    static info_pci_t v[64];
    int n = info_sistema(INFO_PCI, v, 64);
    if (n < 0) n = 0;
    *com = *sem = 0;
    ad(t, "%d funciones PCI. Formato: bus:disp.fun  fabricante:modelo  subsistema  clase.sub.if  rev  IRQ\n", n);
    ad(t, "  nombre / driver de SaguiOS y estado\n\n");
    for (int i = 0; i < n; i++) {
        info_pci_t *p = &v[i];
        const char *f = nome_fabricante(p->fabricante), *d = nome_dispositivo(p->fabricante, p->modelo);
        const conhecido_t *c = conhecer(p);
        ad(t, "%02x:%02x.%u  %04x:%04x  sub %04x:%04x  %02x.%02x.%02x  rev %02x  irq %u\n", p->barramento,
           p->dispositivo, p->funcao, p->fabricante, p->modelo, p->sub_fabricante, p->sub_modelo, p->classe,
           p->subclasse, p->interface, p->revisao, p->irq);
        ad(t, "    %s; %s%s%s\n", nome_classe(p->classe, p->subclasse, p->interface), f ? f : "fabricante ?",
           d ? " " : "", d ? d : "");
        if (!c) {
            ad(t, "    sin driver\n");
            (*sem)++;
        } else if (c->estado == SUPORTADO || c->estado == EXPERIMENTAL) {
            ad(t, "    driver: %s (%s)\n", c->driver, nome_estado[c->estado]);
            (*com)++;
        } else if (c->estado == NAO_PRECISA) {
            ad(t, "    %s (%s)\n", nome_estado[c->estado], c->driver);
        } else {
            ad(t, "    %s: %s\n", nome_estado[c->estado], c->driver);
            (*sem)++;
        }
    }
}

static void informe_modos(texto_t *t)
{
    static modo_video_t m[192];
    int n = modos_video(m, 192), k = 0;
    if (n < 0) n = 0;
    ad(t, "Modos de la BIOS (VBE) aceptados por el cargador (color directo y framebuffer lineal):\n");
    for (int i = 0; i < n; i++) {
        if (!(m[i].flags & MODO_VBE)) continue;
        ad(t, "  0x%03x  %4ux%-4u  %2u bits%s%s\n", m[i].modo, m[i].largura, m[i].altura, m[i].bpp,
           (m[i].flags & MODO_ATUAL) ? "  (actual)" : "", (m[i].flags & MODO_PREFERIDO) ? "  (preferida)" : "");
        k++;
    }
    if (!k) ad(t, "  ninguno (sin VESA: modo texto)\n");
    k = 0;
    for (int i = 0; i < n; i++) if (!(m[i].flags & MODO_VBE) && (m[i].flags & MODO_A_QUENTE)) k++;
    if (k) ad(t, "Además, %d resoluciones en caliente por la tarjeta Bochs/QEMU/VirtualBox (pantalla las lista).\n", k);
}

static void informe_arranque(texto_t *t)
{
    info_arranque_t a;
    memset(&a, 0, sizeof a);
    if (info_sistema(INFO_ARRANQUE, &a, sizeof a) < 0) return;
    ad(t, "Disco de la BIOS:  0x%02x%s\n", a.disco, a.cd ? " (la imagen de dentro del CD, El Torito)" : "");
    ad(t, "Desde CD:          %s\n", a.cd ? "sí (solo lectura: pantalla --guardar no funciona)" : "no");
    ad(t, "Lectura de disco:  %s\n", a.leitura_chs ? "CHS (la BIOS no tiene las extensiones del INT 13h)"
                                                   : "LBA (extensiones del INT 13h)");
    if (a.res_pref) ad(t, "Preferencia:       %ux%u", a.res_pref & 0xFFFF, a.res_pref >> 16);
    else ad(t, "Preferencia:       resolución automática");
    if (a.bpp_pref) ad(t, ", %u bits\n", a.bpp_pref); else ad(t, ", profundidad automática\n");
    /* D201: cuánto tardó cada fase (ms desde el encendido, por el TSC). */
    if (a.fases && a.fases <= ARRANQUE_MAX_FASES) {
        ad(t, "Tiempos (ms desde el encendido; la 1.ª incluye BIOS y lectura de la imagen):\n");
        for (unsigned i = 0; i < a.fases; i++) {
            a.fase_nome[i][15] = 0;
            ad(t, "  %-14s %6u ms  (+%u)\n", a.fase_nome[i], a.fase_ms[i],
               i ? a.fase_ms[i] - a.fase_ms[i - 1] : a.fase_ms[i]);
        }
    }
}

static void informe_discos(texto_t *t)
{
    info_disco_t d;
    char a[24];
    int k = 0;
    for (int i = 0; listar_discos(i, &d) == 1; i++, k++) {
        const char *tipo = (d.flags & DISCO_CD) ? "[CD]" : (d.flags & DISCO_NVME) ? "[NVMe]" : (d.flags & DISCO_INTERNO) ? "[ATA]" : (d.flags & DISCO_REMOVIVEL) ? "[USB]" : "[?]";
        if (d.particao)
            ad(t, "    %-8s %s, %s%s%s%s\n", d.nome, tamanho(a, sizeof a, (uint64_t)d.setores * 512), d.formato,
               d.rotulo[0] ? ", \"" : "", d.rotulo, d.rotulo[0] ? "\"" : "");
        else
            ad(t, "  %-10s %s %s %s, %s%s, %s%s\n", d.nome, tipo, d.fabricante, d.produto,
               tamanho(a, sizeof a, (uint64_t)d.setores * 512), (d.flags & DISCO_BLOCO4K) ? ", bloque 4096" : (d.flags & DISCO_NVME) ? ", bloque 512" : "",
               d.formato, (d.flags & DISCO_BOOT) ? " (arranque)" : "");
        if (d.montado && d.ponto[0])
            ad(t, "             montado en %s%s\n", d.ponto, (d.flags & DISCO_SO_LEITURA) ? " (solo lectura)" : "");
    }
    if (!k) ad(t, "  ninguno (ni ATA ni USB)\n");
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    if (n > 0) {
        ad(t, "Montajes:\n");
        for (int i = 0; i < n; i++)
            ad(t, "  %-8s en %-10s %s (%s)\n", m[i].dispositivo, m[i].ponto, m[i].tipo,
               (m[i].opcoes & MONTAR_SO_LEITURA) ? "ro" : "rw");
    }
    static char usb[2048];
    int u = info_sistema(INFO_USB, usb, sizeof usb - 1);
    if (u > 0) {
        usb[u] = 0;
        ad(t, "USB:\n");
        for (char *l = usb; *l;) {
            char *f = acha_char(l, '\n');
            if (f) *f = 0;
            if (*l) ad(t, "  %s\n", l);
            if (!f) break;
            l = f + 1;
        }
    }
}

static void informe_acpi(texto_t *t)
{
    static info_acpi_t a;
    memset(&a, 0, sizeof a);
    if (info_sistema(INFO_ACPI, &a, sizeof a) < 0 || !a.achou) {
        ad(t, "No se ha encontrado el RSDP (sin ACPI, o fuera de 0xE0000-0xFFFFF y de la EBDA).\n");
        return;
    }
    ad(t, "RSDP en 0x%08x, revisión %u (%s), OEM \"%s\"\n", a.rsdp, a.revisao,
       a.revisao >= 2 ? "ACPI 2.0 o posterior" : "ACPI 1.0", a.oem);
    ad(t, "RSDT en 0x%08x", a.rsdt);
    if (a.xsdt_baixo || a.xsdt_alto) ad(t, ", XSDT en 0x%08x%08x", a.xsdt_alto, a.xsdt_baixo);
    ad(t, "; lista de la %s:\n", a.usou_xsdt ? "XSDT" : "RSDT");
    for (unsigned i = 0; i < a.qtd && i < ACPI_MAX_TABELAS; i++) {
        info_acpi_tabela_t *e = &a.tabela[i];
        ad(t, "  %-4s  0x%08x  %6u bytes  rev %u  OEM \"%s\"%s\n", e->assinatura, e->endereco, e->tamanho,
           e->revisao, e->oem, e->soma_ok ? "" : "  (suma incorrecta)");
    }
    static info_energia_t en;
    memset(&en, 0, sizeof en);
    if (info_sistema(INFO_ENERGIA, &en, sizeof en) < 0) return;
    linha_energia(t);
    if (en.motivo[0]) ad(t, "  motivo: %s\n", en.motivo);
    ad(t, "  FADT rev %u: PM1a_CNT 0x%x, PM1b_CNT 0x%x, PM1a_EVT 0x%x (%u bytes), PM1b_EVT 0x%x\n", en.fadt_revisao,
       en.pm1a_cnt, en.pm1b_cnt, en.pm1a_evt, en.pm1_evt_len, en.pm1b_evt);
    ad(t, "  SCI en la IRQ %u, SMI_CMD 0x%x, ACPI_ENABLE 0x%x, modo ACPI: %s%s%s\n", en.sci, en.smi_cmd, en.acpi_enable,
       en.modo_acpi == 2 ? "activado por " SAGUI_NOME : en.modo_acpi == 1 ? "ya estaba" : "no",
       en.reduzido ? ", hardware reducido" : "", en.origem_s5[0] ? (en.origem_s5[0] == 'S' ? ", \\_S5_ en una SSDT" : "") : "");
    ad(t, "  Botón: %u pulsaciones, %u SCI atendidos\n", en.pulsacoes, en.sci_total);
}

#define INFORME_LOG_LINHAS 200

static void informe_log(texto_t *t)
{
    static char log[32 * 1024 + 1];
    int n = info_sistema(INFO_LOG, log, sizeof log);
    if (n <= 0) { ad(t, "(log vacío)\n"); return; }
    log[n] = 0;
    /* Sólo las últimas líneas: el principio del arranque ya está en las otras secciones. */
    int linhas = 0;
    char *ini = log + n;
    while (ini > log) {
        if (ini[-1] == '\n' && ini != log + n && ++linhas >= INFORME_LOG_LINHAS) break;
        ini--;
    }
    ad(t, "(últimas %d líneas, como en el puerto serie)\n", linhas < INFORME_LOG_LINHAS ? linhas + 1 : INFORME_LOG_LINHAS);
    /* ad() corta en 512 bytes: línea por línea. */
    for (char *l = ini; *l;) {
        char *f = acha_char(l, '\n');
        if (f) *f = 0;
        ad(t, "%s\n", l);
        if (!f) break;
        l = f + 1;
    }
}

/* Dónde grabar: /home si está montado para escritura (la partición de datos o
 * el fstab), si no el primer pendrive en /usb, si no /tmp (en la RAM: avisa). */
static const char *pasta_informe(bool *persistente)
{
    static info_montagem_t m[10];
    static char achou[64];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int passo = 0; passo < 2; passo++)
        for (int i = 0; i < n; i++) {
            if (m[i].opcoes & MONTAR_SO_LEITURA) continue;
            bool home = compara_texto(m[i].ponto, "/home") == 0;
            bool usb = comeca_com(m[i].ponto, "/usb");
            if ((passo == 0 && home) || (passo == 1 && usb)) {
                copia_texto(achou, m[i].ponto, sizeof achou);
                *persistente = true;
                return achou;
            }
        }
    *persistente = false;
    return "/tmp";
}

static int informe(const char *destino)
{
    char caminho[160];
    data_t dt;
    memset(&dt, 0, sizeof dt);
    ler_data(&dt);
    bool persistente = true;
    if (destino) {
        copia_texto(caminho, destino, sizeof caminho);
    } else {
        const char *pasta = pasta_informe(&persistente);
        formatar_texto(caminho, sizeof caminho, "%s/informe-%04u%02u%02u-%02u%02u.txt", pasta, dt.ano, dt.mes, dt.dia,
                       dt.hora, dt.minuto);
    }

    texto_t t = { 0 }, s = { 0 };
    info_versao_t v;
    info_sistema(INFO_VERSAO, &v, sizeof v);
    ad(&t, SAGUI_NOME " - informe de compatibilidad\n");
    ad(&t, "===================================\n");
    ad(&t, "Generado el %02u/%02u/%04u a las %02u:%02u con \"infopc --informe\" (" SAGUI_NOME " %s).\n", dt.dia, dt.mes,
       dt.ano, dt.hora, dt.minuto, v.versao);
    ad(&t, "Compilación: " SAGUI_VARIANTE "\n");
    ad(&t, "Cómo enviarlo: docs/compatibilidad.md. Texto en ISO-8859-1 (Latin-1).\n");

    /* Resumen primero: quien lee el informe ve enseguida lo que falta. */
    uint32_t r1[4];
    cpuid(1, 0, r1);
    bool sse2 = (r1[3] >> 26) & 1;
    info_video_t vi;
    memset(&vi, 0, sizeof vi);
    info_sistema(INFO_VIDEO, &vi, sizeof vi);
    texto_t pci = { 0 };
    int com = 0, sem = 0;
    informe_pci(&pci, &com, &sem);
    ad(&t, "\n== Resumen ==\n");
    ad(&t, "SSE2:              %s\n", sse2 ? "sí (cc usa SSE2; python funciona)"
                                           : "no (cc usa la FPU x87; python con float: pendiente, mejora 036)");
    if (vi.grafico) ad(&t, "Vídeo:             %ux%u, %u bits\n", vi.largura, vi.altura, vi.bpp);
    else ad(&t, "Vídeo:             modo texto (sin VESA utilizable)\n");
    ad(&t, "PCI:               %d con driver, %d sin driver\n", com, sem);
    info_audio_t au;
    memset(&au, 0, sizeof au);
    info_sistema(INFO_AUDIO, &au, sizeof au);
    if (au.estado == AUDIO_FUNCIONANDO) ad(&t, "Sonido:            %s; %s\n", au.placa, au.codec);
    else if (au.estado == AUDIO_SEM_DRIVER) ad(&t, "Sonido:            %04x:%04x sin driver\n", au.fabricante, au.modelo);
    else ad(&t, "Sonido:            sin tarjeta (solo el altavoz)\n");

    for (int i = 0; i < QTD_SECOES; i++) {
        if (compara_texto(secoes[i].nome, "benchmark") == 0) continue;   /* va al final, resumido */
        if (compara_texto(secoes[i].nome, "pci") == 0) {
            ad(&t, "\n== PCI ==\n");
            if (pci.p) ad_texto(&t, pci.p, pci.tam);
            continue;
        }
        gerar(i, &s);
        ad(&t, "\n== %s ==\n", secoes[i].titulo);
        if (s.p) ad_texto(&t, s.p, s.tam);
        if (compara_texto(secoes[i].nome, "video") == 0) {
            ad(&t, "\n");
            informe_modos(&t);
        }
    }
    ad(&t, "\n== Arranque ==\n");
    informe_arranque(&t);
    ad(&t, "\n== Discos y USB ==\n");
    informe_discos(&t);
    ad(&t, "\n== ACPI ==\n");
    informe_acpi(&t);
    secao_irq(&t);   /* D211: la vía de cada IRQ (IOAPIC, _PRT, 8259) */
    ad(&t, "\n== SaguiMark (último resultado) ==\n");
    {
        char c[96];
        if (ultimo_resultado(c, sizeof c)) { ad(&t, "Archivo: %s\n", c); ad_arquivo(&t, c, 14); }
        else ad(&t, "Sin resultados: ejecuta \"saguimark\" (o la pestaña Benchmark de infopc) y repite el informe.\n");
    }
    ad(&t, "\n== Log del kernel ==\n");
    informe_log(&t);

    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    int r = fd < 0 ? fd : escrever_tudo(fd, t.p, (unsigned)t.tam);
    if (fd >= 0) fechar(fd);
    /* D120: con la partición SAGUI-LOGS montada para escribir, una copia también en /logs. */
    if (!destino && fd >= 0 && r >= 0 && comeca_com(caminho, "/logs/") == 0) {
        static info_montagem_t mm[10];
        int nm = info_sistema(INFO_MONTAGENS, mm, 10);
        for (int i = 0; i < nm; i++)
            if (compara_texto(mm[i].ponto, "/logs") == 0 && !(mm[i].opcoes & MONTAR_SO_LEITURA)) {
                char copia[160];
                formatar_texto(copia, sizeof copia, "/logs/informe-%04u%02u%02u-%02u%02u.txt", dt.ano, dt.mes, dt.dia, dt.hora, dt.minuto);
                int fl = abrir(copia, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
                if (fl >= 0 && escrever_tudo(fl, t.p, (unsigned)t.tam) >= 0) imprimir("Copia también en %s.\n", copia);
                if (fl >= 0) fechar(fl);
            }
    }
    libera(t.p); libera(s.p); libera(pci.p);
    if (fd < 0 || r < 0) {
        imprimir_em(ERROS, "infopc: no se ha podido escribir %s: %s\n", caminho, nome_erro(fd < 0 ? fd : r));
        return 1;
    }
    imprimir("Informe de compatibilidad escrito en %s (%d bytes).\n", caminho, t.tam);
    if (!persistente)
        imprimir("Aviso: ni /home ni un pendrive en /usb se pueden escribir; /tmp está en la RAM y se pierde\n"
                 "al apagar. Conecta un pendrive (FAT32) y repite, o copia el archivo antes de apagar.\n");
    registrar("infopc: informe %s, %d bytes, pci com driver %d, sem %d", caminho, t.tam, com, sem);
    return 0;
}

/* ------------------------------------------------------------ ventana */

#define JL 900
#define JA 560
#define ABA_L 92
#define ABA_A 28

static void desenhar_abas(jan_t *j, int sel)
{
    g_retangulo(j, 0, 0, JL, 8 + ABA_A + 4, COR_FUNDO);
    for (int i = 0; i < QTD_SECOES; i++) {
        int x = 10 + i * (ABA_L + 6);
        bool s = i == sel;
        g_retangulo(j, x, 8, ABA_L, ABA_A, s ? COR_DESTAQUE : COR_BOTAO);
        g_relevo(j, x, 8, ABA_L, ABA_A, s);
        g_moldura(j, x - 1, 7, ABA_L + 2, ABA_A + 2, 0x5A6470);
        g_texto(j, x + (ABA_L - g_largura_texto(secoes[i].titulo)) / 2, 8 + (ABA_A - LETRA_A) / 2,
                secoes[i].titulo, s ? COR_BRANCO : COR_TEXTO);
    }
    jan_mostrar_area(j, 0, 0, JL, 8 + ABA_A + 4);
}

/* Franja de abajo: en la pestaña Benchmark, los botones y, durante la medida, la barra. */
#define BASE_Y (JA - 42)
#define ABA_BENCH (QTD_SECOES - 3)
#define ABA_REND  (QTD_SECOES - 2)
#define ABA_GPU   (QTD_SECOES - 1)
static botao_t b_rapido = { 10, BASE_Y + 6, 150, 28, "Medir (rápido)", false };
static botao_t b_preciso = { 170, BASE_Y + 6, 150, 28, "Medir (preciso)", false };
static botao_t b_um = { 10, BASE_Y + 6, 190, 28, "Prueba de un núcleo", false };
static botao_t b_gpu = { 10, BASE_Y + 6, 150, 28, "Medir", false };
static botao_t b_multi = { 210, BASE_Y + 6, 190, 28, "Prueba multinúcleo", false };

static void desenhar_base(jan_t *j, int sel)
{
    g_retangulo(j, 0, BASE_Y, JL, JA - BASE_Y, COR_FUNDO);
    if (sel == ABA_BENCH) {
        botao_desenhar(j, &b_rapido);
        botao_desenhar(j, &b_preciso);
        g_texto(j, 340, BASE_Y + 12, "Teclas: R rápido, P preciso", COR_TEXTO_FRACO);
    } else if (sel == ABA_GPU) {
        botao_desenhar(j, &b_gpu);
        g_texto(j, 180, BASE_Y + 12, "Tecla: M mide el 2D (GPU y CPU)", COR_TEXTO_FRACO);
    } else if (sel == ABA_REND) {
        botao_desenhar(j, &b_um);
        botao_desenhar(j, &b_multi);
        g_texto(j, 420, BASE_Y + 12, "Teclas: U un núcleo, M multinúcleo", COR_TEXTO_FRACO);
    }
    jan_mostrar_area(j, 0, BASE_Y, JL, JA - BASE_Y);
}

static void rd_desenhar(jan_t *j);

static void mostrar_secao(jan_t *j, area_t *a, texto_t *t, int s)
{
    desenhar_abas(j, s);
    if (s == ABA_REND) {   /* esta pestaña se dibuja a mano, no es un texto */
        rd_desenhar(j);
        desenhar_base(j, s);
        registrar("infopc: secao %s", secoes[s].nome);
        return;
    }
    gerar(s, t);
    area_definir(a, t->p ? t->p : "", t->tam);
    area_desenhar(j, a);
    desenhar_base(j, s);
    registrar("infopc: secao %s (%d bytes)", secoes[s].nome, t->tam);
}

/* Durante la medida la ventana no procesa eventos ni se redibuja: sólo esta
 * barra cambia, entre una prueba y otra (fuera del tiempo medido). */
static void progresso_marca(void *ctx, int feito, int total, const sm_prova_t *p)
{
    jan_t *j = ctx;
    g_retangulo(j, 0, BASE_Y, JL, JA - BASE_Y, COR_FUNDO);
    int l = 300, x = 10, y = BASE_Y + 10;
    g_retangulo(j, x, y, l, 20, 0xFFFFFF);
    g_retangulo(j, x, y, total ? l * feito / total : l, 20, COR_DESTAQUE);
    g_moldura(j, x, y, l, 20, COR_BORDA);
    char t[96];
    if (p) formatar_texto(t, sizeof t, "Midiendo %d/%d: %s - %s", feito + 1, total, sm_nome_grupo(p->grupo), p->nome);
    else copia_texto(t, "Calculando la puntuación...", sizeof t);
    g_texto_n(j, x + l + 12, y + 2, t, (JL - x - l - 20) / LETRA_L, COR_TEXTO);
    jan_mostrar_area(j, 0, BASE_Y, JL, JA - BASE_Y);
    if (p) registrar("infopc: saguimark %d/%d %s", feito + 1, total, p->nome);
}

static void medir(jan_t *j, area_t *a, texto_t *t, int modo)
{
    registrar("infopc: saguimark inicio %s", modo == SM_PRECISO ? "preciso" : "rapido");
    {
        static const char aviso[] = "Midiendo... no toques nada hasta que termine.\n";
        area_definir(a, aviso, (int)sizeof aviso - 1);
    }
    area_desenhar(j, a);
    if (!sm_executar(&marca, modo, SM_TODOS, progresso_marca, j)) {
        mensagem("SaguiMark", "No se puede medir: la CPU no tiene TSC o falta memoria.");
        desenhar_base(j, ABA_BENCH);
        return;
    }
    tem_marca = true;
    marca_arquivo[0] = 0;
    int e = sm_salvar(&marca, marca_arquivo, sizeof marca_arquivo);
    if (e < 0) marca_arquivo[0] = 0;
    registrar("infopc: saguimark total %u erro %d grupos %u %u %u %u %u guardado %s", marca.total, marca.erro,
              marca.pontos_grupo[0], marca.pontos_grupo[1], marca.pontos_grupo[2], marca.pontos_grupo[3],
              marca.pontos_grupo[SM_2D],
              marca_arquivo[0] ? marca_arquivo : "-");
    mostrar_secao(j, a, t, ABA_BENCH);
}

static void medir_gpu(jan_t *j, area_t *a, texto_t *t)
{
    static const char aviso[] = "Midiendo el 2D (GPU y CPU)... no toques nada hasta que termine.\n";
    area_definir(a, aviso, (int)sizeof aviso - 1);
    area_desenhar(j, a);
    jan_mostrar(j);
    gb_medir_e_guardar();
    mostrar_secao(j, a, t, ABA_GPU);
}

/* ---- pestaña Rendimiento (D176) ---- */

#define RD_CARGAS 8
static const struct { const char *nome, *grupo; } rd_nomes[RD_CARGAS] = {
    { "hash", "Entero" }, { "criba", "Entero" }, { "mandel", "Flotante" }, { "ncuerpos", "Flotante" },
    { "copia", "Memoria" }, { "aleat", "Memoria" }, { "sha256", "Cifrado" }, { "llamadas", "Sistema" },
};

static struct {
    bool     tem, multi, correndo;
    int      estado[RD_CARGAS];             /* 0 = n/d, 1 = ok, 2 = error */
    unsigned mono[RD_CARGAS], var[RD_CARGAS], mp[RD_CARGAS], mh[RD_CARGAS];
    unsigned tmono, tmp, tmh, esc, efi;     /* esc = escala x100, efi = % */
    int      ncpu, erros, feito, total;
    char     cpu[64], fase[12], atual[16], aviso[96];
    bool     tem_ant;
    bench_linha_t ant;
} rd;

#define RD_Y0 (8 + ABA_A + 6)

static void rd_barra(jan_t *j, int x, int y, int l, int a, unsigned v, unsigned max, uint32_t cor)
{
    g_retangulo(j, x, y, l, a, 0xE6E9EC);
    if (max && v) {
        int w = (int)((unsigned long)l * v / max);
        g_retangulo(j, x, y, w < 2 ? 2 : (w > l ? l : w), a, cor);
    }
    g_moldura(j, x, y, l, a, COR_BORDA);
}

static void rd_numero(jan_t *j, int x, int y, unsigned v, uint32_t cor)
{
    char b[16];
    formatar_texto(b, sizeof b, v ? "%u" : "-", v);
    g_texto(j, x, y, b, cor);
}

static void rd_desenhar(jan_t *j)
{
    int y0 = RD_Y0, altura = BASE_Y - y0;
    g_retangulo(j, 0, y0, JL, altura, COR_FUNDO);
    g_texto(j, 14, y0 + 4, "Rendimiento: ocho cargas con resultado comprobado. 1000 puntos = QEMU con KVM y una CPU.", COR_TEXTO_FRACO);
    char t[160];
    if (rd.cpu[0]) {
        formatar_texto(t, sizeof t, "%s - %d núcleo%s", rd.cpu, rd.ncpu, rd.ncpu == 1 ? "" : "s");
        g_texto_n(j, 14, y0 + 24, t, (JL - 28) / LETRA_L, COR_TEXTO);
    } else {
        g_texto(j, 14, y0 + 24, "Sin medir todavía: pulsa una de las dos pruebas.", COR_TEXTO);
    }
    const int yc = y0 + 54, yf = yc + 22, paso = 32;
    g_texto(j, 14, yc, "Carga", COR_TEXTO_FRACO);
    g_texto(j, 96, yc, "Grupo", COR_TEXTO_FRACO);
    g_texto(j, 180, yc, rd.correndo ? "Progreso" : "Un núcleo (azul) / todos (verde)", COR_TEXTO_FRACO);
    g_texto(j, 490, yc, "Un núcl.", COR_TEXTO_FRACO);
    g_texto(j, 562, yc, "Procesos", COR_TEXTO_FRACO);
    g_texto(j, 634, yc, "Hilos", COR_TEXTO_FRACO);
    g_texto(j, 698, yc, "Escala", COR_TEXTO_FRACO);
    g_retangulo(j, 10, yc + 16, JL - 20, 1, COR_BORDA);
    unsigned gmax = 1;
    for (int i = 0; i < RD_CARGAS; i++) {
        if (rd.mono[i] > gmax) gmax = rd.mono[i];
        if (rd.mp[i] > gmax) gmax = rd.mp[i];
        if (rd.mh[i] > gmax) gmax = rd.mh[i];
    }
    int fases = rd.multi ? 3 : 1;
    for (int i = 0; i < RD_CARGAS; i++) {
        int y = yf + i * paso;
        g_texto(j, 14, y + 8, rd_nomes[i].nome, COR_TEXTO);
        g_texto(j, 96, y + 8, rd_nomes[i].grupo, COR_TEXTO_FRACO);
        if (rd.correndo) {
            int sw = 240 / fases;
            for (int f = 0; f < fases; f++) {
                int pos = f * RD_CARGAS + i;
                bool feito = pos < rd.feito, agora = pos == rd.feito;
                g_retangulo(j, 180 + f * sw, y + 8, sw - 4, 14, feito ? (f == 0 ? COR_SELECAO : COR_DESTAQUE) : 0xE6E9EC);
                g_moldura(j, 180 + f * sw, y + 8, sw - 4, 14, agora ? COR_DESTAQUE : COR_BORDA);
            }
        } else if (rd.tem && rd.estado[i] == 1) {
            unsigned mm = rd.mp[i] > rd.mh[i] ? rd.mp[i] : rd.mh[i];
            rd_barra(j, 180, y + 4, 240, 10, rd.mono[i], gmax, COR_SELECAO);
            if (mm) rd_barra(j, 180, y + 16, 240, 10, mm, gmax, COR_DESTAQUE);
            rd_numero(j, 490, y + 8, rd.mono[i], COR_TEXTO);
            rd_numero(j, 562, y + 8, rd.mp[i], COR_TEXTO);
            rd_numero(j, 634, y + 8, rd.mh[i], COR_TEXTO);
            if (mm && rd.mono[i]) {
                unsigned e = mm * 100 / rd.mono[i];
                formatar_texto(t, sizeof t, "%u.%02ux", e / 100, e % 100);
                g_texto(j, 698, y + 8, t, COR_TEXTO);
            }
        } else if (rd.tem) {
            g_texto(j, 180, y + 8, rd.estado[i] == 2 ? "ERROR: la suma de comprobación no coincide" : "n/d", 0xB03030);
        }
        g_retangulo(j, 10, y + paso - 1, JL - 20, 1, 0xDDE0E3);
    }
    int yt = yf + RD_CARGAS * paso + 8;
    if (rd.tem && !rd.correndo) {
        g_texto(j, 14, yt, "TOTAL (media geométrica)", COR_TEXTO);
        rd_numero(j, 490, yt, rd.tmono, COR_DESTAQUE);
        rd_numero(j, 562, yt, rd.tmp, COR_DESTAQUE);
        rd_numero(j, 634, yt, rd.tmh, COR_DESTAQUE);
        if (rd.multi && rd.esc) {
            formatar_texto(t, sizeof t, "Factor de escala (todos / uno): %u.%02ux con %d núcleos - eficiencia %u%%", rd.esc / 100,
                           rd.esc % 100, rd.ncpu, rd.efi);
            g_texto(j, 14, yt + 22, t, COR_TEXTO);
        }
        if (rd.tem_ant) {
            unsigned antes = rd.multi ? (rd.ant.mp > rd.ant.mh ? rd.ant.mp : rd.ant.mh) : rd.ant.mono;
            unsigned ahora = rd.multi ? (rd.tmp > rd.tmh ? rd.tmp : rd.tmh) : rd.tmono;
            if (antes && ahora) {
                int dif = (int)((long)ahora * 100 / (long)antes) - 100;
                formatar_texto(t, sizeof t, "Anterior (%s, %u núcleos): %u puntos; ahora %u (%s%d%%)", rd.ant.data, rd.ant.nucleos,
                               antes, ahora, dif > 0 ? "+" : "", dif);
                g_texto_n(j, 14, yt + 44, t, (JL - 28) / LETRA_L, COR_TEXTO);
            }
        } else {
            g_texto(j, 14, yt + 44, "Sin ejecución anterior guardada con la que comparar.", COR_TEXTO_FRACO);
        }
    } else if (!rd.correndo && rd.aviso[0]) {
        g_texto_n(j, 14, yt, rd.aviso, (JL - 28) / LETRA_L, 0xB03030);
    }
    jan_mostrar_area(j, 0, y0, JL, altura);
}

static void rd_base(jan_t *j)
{
    g_retangulo(j, 0, BASE_Y, JL, JA - BASE_Y, COR_FUNDO);
    int l = 300, x = 10, y = BASE_Y + 10;
    g_retangulo(j, x, y, l, 20, 0xFFFFFF);
    g_retangulo(j, x, y, rd.total ? l * rd.feito / rd.total : 0, 20, COR_DESTAQUE);
    g_moldura(j, x, y, l, 20, COR_BORDA);
    char t[96];
    formatar_texto(t, sizeof t, "Midiendo %d/%d: %s - %s", rd.feito < rd.total ? rd.feito + 1 : rd.total, rd.total, rd.fase, rd.atual);
    g_texto_n(j, x + l + 12, y + 2, t, (JL - x - l - 20) / LETRA_L, COR_TEXTO);
    jan_mostrar_area(j, 0, BASE_Y, JL, JA - BASE_Y);
}

/* Una línea de la salida de "benchmark --maquina": C cpu / I núcleos khz pasos / P hecho total fase carga
 * / R carga grupo estado mono var procesos hilos / T mono procesos hilos escala eficiencia núcleos errores. */
static void rd_linha(char *l)
{
    if (l[0] == 'C' && l[1] == ' ') { copia_texto(rd.cpu, l + 2, sizeof rd.cpu); return; }   /* el nombre de la CPU lleva espacios */
    char *w[10];
    int n = 0;
    for (char *p = l; *p && n < 10;) {
        while (*p == ' ') p++;
        if (!*p) break;
        w[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    if (n < 2) return;
    int v[8];
    for (int k = 1; k < n && k < 9; k++) { v[k - 1] = 0; texto_para_numero(w[k], &v[k - 1]); }
    switch (w[0][0]) {
    case 'I': rd.ncpu = v[0]; rd.total = v[2]; break;
    case 'P':
        if (n >= 5) { rd.feito = v[0]; rd.total = v[1]; copia_texto(rd.fase, w[3], sizeof rd.fase); copia_texto(rd.atual, w[4], sizeof rd.atual); }
        break;
    case 'R':
        if (n >= 8)
            for (int i = 0; i < RD_CARGAS; i++)
                if (compara_texto(w[1], rd_nomes[i].nome) == 0) {
                    rd.estado[i] = v[2]; rd.mono[i] = (unsigned)v[3]; rd.var[i] = (unsigned)v[4];
                    rd.mp[i] = (unsigned)v[5]; rd.mh[i] = (unsigned)v[6];
                }
        break;
    case 'T':
        if (n >= 8) {
            rd.tmono = (unsigned)v[0]; rd.tmp = (unsigned)v[1]; rd.tmh = (unsigned)v[2];
            rd.esc = (unsigned)v[3]; rd.efi = (unsigned)v[4]; rd.ncpu = v[5]; rd.erros = v[6];
            rd.tem = true;
        }
        break;
    }
}

/* Lanza /bin/benchmark con la salida en un pipe y la lee entre evento y evento: la ventana no se
 * congela y se puede cerrar. Devuelve true si piden cerrar la ventana mientras mide. */
static bool rd_correr(jan_t *j, bool multi)
{
    memset(&rd.estado, 0, sizeof rd.estado);
    memset(rd.mono, 0, sizeof rd.mono); memset(rd.mp, 0, sizeof rd.mp); memset(rd.mh, 0, sizeof rd.mh);
    memset(rd.var, 0, sizeof rd.var);
    rd.tem = false; rd.erros = 0; rd.feito = 0; rd.esc = 0; rd.efi = 0; rd.aviso[0] = 0;
    rd.tmono = rd.tmp = rd.tmh = 0;
    rd.multi = multi;
    rd.total = multi ? 3 * RD_CARGAS : RD_CARGAS;
    rd.fase[0] = rd.atual[0] = 0;
    rd.tem_ant = bench_ultima(multi, &rd.ant);   /* antes de medir: la medida añade su línea */
    registrar("infopc: rendimiento inicio %s", multi ? "multinucleo" : "un nucleo");
    if (rd.tem_ant)
        registrar("infopc: rendimiento anterior %s mono %u procesos %u hilos %u", rd.ant.data, rd.ant.mono, rd.ant.mp, rd.ant.mh);
    else
        registrar("infopc: rendimiento sin anterior");

    int p[2];
    if (criar_pipe(p) < 0) { copia_texto(rd.aviso, "No se puede crear el pipe.", sizeof rd.aviso); rd_desenhar(j); return false; }
    int g = -1;
    if (tipo_descritor(SAIDA) >= 0) {
        for (int fd = 10; fd < 32; fd++) if (tipo_descritor(fd) < 0) { g = fd; break; }
        if (g >= 0) duplicar(SAIDA, g);
    }
    duplicar(p[1], SAIDA);
    char *args[6];
    int na = 0;
    args[na++] = "benchmark";
    args[na++] = "--maquina";
    args[na++] = "--rapido";
    if (!multi) args[na++] = "--mono";
    args[na] = NULL;
    int pid = executar("/bin/benchmark", args);
    if (g >= 0) { duplicar(g, SAIDA); fechar(g); } else fechar(SAIDA);
    fechar(p[1]);
    if (pid < 0) {
        fechar(p[0]);
        copia_texto(rd.aviso, "No se puede ejecutar /bin/benchmark.", sizeof rd.aviso);
        rd_desenhar(j);
        return false;
    }
    rd.correndo = true;
    rd_desenhar(j);
    rd_base(j);

    char buf[512];
    int n = 0;
    bool cerrar = false;
    for (;;) {
        int d = disponivel(p[0]);
        if (d > 0) {
            int r = ler(p[0], buf + n, (unsigned)(sizeof buf - 1 - (unsigned)n));
            if (r <= 0) break;
            n += r;
            buf[n] = 0;
            bool mudou = false;
            char *ini = buf;
            for (char *q = buf; q < buf + n; q++)
                if (*q == '\n') {
                    *q = 0;
                    if (ini[0] == 'P') mudou = true;
                    rd_linha(ini);
                    ini = q + 1;
                }
            n = (int)(buf + n - ini);
            memmove(buf, ini, (size_t)n);
            if (mudou) { rd_desenhar(j); rd_base(j); }
            continue;
        }
        if (d < 0) break;   /* el benchmark terminó */
        evento_t e;
        if (esperar_evento(j->id, &e, 40) == 1 && e.tipo == EV_FECHAR) { cerrar = true; break; }
    }
    fechar(p[0]);
    if (cerrar) matar(pid);
    int codigo = 0;
    esperar(pid, &codigo);
    rd.correndo = false;
    if (!rd.tem && !cerrar) copia_texto(rd.aviso, "La prueba no terminó (mira el registro del sistema).", sizeof rd.aviso);
    registrar("infopc: rendimiento total mono %u procesos %u hilos %u escala %u eficiencia %u errores %d codigo %d",
              rd.tmono, rd.tmp, rd.tmh, rd.esc, rd.efi, rd.erros, codigo);
    if (!cerrar) { rd_desenhar(j); desenhar_base(j, ABA_REND); }
    return cerrar;
}

static int modo_janela(void)
{
    jan_t *j = jan_abrir("Información del PC", JL, JA);
    if (!j) return 3;
    area_t a;
    if (!area_iniciar(&a, 10, 8 + ABA_A + 10, JL - 20, BASE_Y - (8 + ABA_A + 10) - 4)) return 1;
    a.somente_leitura = true;
    a.foco = true;
    texto_t t = { 0 };
    int sel = 0;
    g_retangulo(j, 0, 0, JL, JA, COR_FUNDO);
    mostrar_secao(j, &a, &t, sel);
    jan_mostrar(j);
    registrar("infopc: janela aberta");
    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        int nova = -1;
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y >= 8 && e.y < 8 + ABA_A) {
            int i = (e.x - 10) / (ABA_L + 6);
            if (e.x >= 10 && i < QTD_SECOES && (e.x - 10) % (ABA_L + 6) < ABA_L) nova = i;
        } else if (e.tipo == EV_TECLA) {
            if (e.tecla == TECLA_ESC) break;
            if (e.tecla == '\t') nova = (sel + ((e.modificadores & 1) ? QTD_SECOES - 1 : 1)) % QTD_SECOES;
            else if (e.tecla >= '1' && e.tecla < '1' + (unsigned)QTD_SECOES) nova = (int)(e.tecla - '1');
            else if (sel == ABA_BENCH && (e.tecla == 'r' || e.tecla == 'R')) { medir(j, &a, &t, SM_RAPIDO); continue; }
            else if (sel == ABA_BENCH && (e.tecla == 'p' || e.tecla == 'P')) { medir(j, &a, &t, SM_PRECISO); continue; }
            else if (sel == ABA_GPU && (e.tecla == 'm' || e.tecla == 'M')) { medir_gpu(j, &a, &t); continue; }
            else if (sel == ABA_REND && (e.tecla == 'u' || e.tecla == 'U')) { if (rd_correr(j, false)) break; continue; }
            else if (sel == ABA_REND && (e.tecla == 'm' || e.tecla == 'M')) { if (rd_correr(j, true)) break; continue; }
        }
        if (nova >= 0) {
            sel = nova;
            mostrar_secao(j, &a, &t, sel);
            continue;
        }
        if (sel == ABA_GPU && botao_tratar(j, &b_gpu, &e)) { medir_gpu(j, &a, &t); continue; }
        if (sel == ABA_REND && botao_tratar(j, &b_um, &e)) { if (rd_correr(j, false)) break; continue; }
        if (sel == ABA_REND && botao_tratar(j, &b_multi, &e)) { if (rd_correr(j, true)) break; continue; }
        if (sel == ABA_REND) continue;
        if (sel == ABA_BENCH && botao_tratar(j, &b_rapido, &e)) { medir(j, &a, &t, SM_RAPIDO); continue; }
        if (sel == ABA_BENCH && botao_tratar(j, &b_preciso, &e)) { medir(j, &a, &t, SM_PRECISO); continue; }
        area_tratar(j, &a, &e);
    }
    jan_fechar(j);
    return 0;
}

static void uso(void)
{
    imprimir("Uso: infopc               abre la ventana con la información del PC\n"
             "     infopc -t [sección]  la imprime en el terminal\n"
             "     infopc --informe [archivo]\n"
             "                          escribe un informe de compatibilidad (.txt) en /home,\n"
             "                          en el pendrive (/usb) o en /tmp; ver docs/compatibilidad.md\n"
             "     infopc --bench-gpu   mide el 2D por la GPU y por software y lo guarda en /home/saguimark-gpu-*.txt\n"
             "Secciones: cpu, memoria, placa, pci, video, sistema, benchmark, rendimiento, gpu\n");
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && compara_texto(argv[1], "-t") == 0) {
        int s = -1;
        if (argc >= 3 && (s = achar_secao(argv[2])) < 0) { uso(); return 1; }
        return modo_texto(s);
    }
    if (argc >= 2 && (compara_texto(argv[1], "--informe") == 0 || compara_texto(argv[1], "-i") == 0))
        return informe(argc >= 3 ? argv[2] : NULL);
    if (argc >= 2 && compara_texto(argv[1], "--bench-gpu") == 0) {
        texto_t t = { 0 };
        gb_medir_e_guardar();
        secao_gpu(&t);
        escrever_tudo(SAIDA, t.p, (unsigned)t.tam);
        libera(t.p);
        return gb_res.erros ? 1 : 0;
    }
    if (argc >= 2 && compara_texto(argv[1], "-v") == 0)   /* uso interno: la ventana, ya en segundo plano */
        return modo_janela();
    if (argc >= 2) { uso(); return argc >= 2 && compara_texto(argv[1], "-h") == 0 ? 0 : 1; }

    /* Sin argumentos: con escritorio, la ventana se ejecuta en un proceso
     * separado para no bloquear el shell; sin él, el informe va al
     * terminal. */
    info_video_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VIDEO, &v, sizeof v);
    if (!v.escritorio) return modo_texto(-1);
    char *args[] = { "infopc", "-v", NULL };
    int pid = executar("/bin/infopc", args);
    if (pid < 0) return modo_janela();
    imprimir("Abriendo la información del PC (proceso %d)...\n", pid);
    return 0;
}

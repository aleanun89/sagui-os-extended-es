// SPDX-License-Identifier: GPL-2.0-or-later
/* wc.c - write-combining (WC) para el framebuffer. Ver D057.
 *
 * La memoria de la placa de vídeo suele quedar sin caché (UC): la BIOS deja la
 * franja del PCI como UC en el MTRR por defecto. En UC cada escritura se convierte en una
 * transacción en el bus, del tamaño de la instrucción (4 bytes en un rep movsd).
 * En WC la CPU junta las escrituras en un búfer de 64 bytes y envía la línea
 * entera de una vez: es el tipo pensado para framebuffers. Leer sigue siendo lento
 * (no hay caché), pero SaguiOS nunca lee la placa: dibuja en un búfer de
 * atrás en la RAM y solo copia (video.c).
 *
 * Dos caminos, del mejor al peor:
 *   - PAT (Pentium III en adelante, el G470 lo tiene): la tabla de 8 tipos del MSR
 *     IA32_PAT se indexa por los bits PWT, PCD y PAT de cada PTE. La entrada 1
 *     (solo PWT) viene de fábrica como WT, que nadie usa en SaguiOS; esa pasa a
 *     WC y las páginas del framebuffer se mapean con PWT. Según el manual de
 *     Intel, PAT=WC gana a un MTRR=UC en la misma página, así que no hace falta
 *     tocar los MTRR de la BIOS.
 *   - MTRR variable: sin PAT, solo un MTRR libre puede marcar la franja como WC.
 *     El tamaño tiene que ser potencia de 2 y la base alineada a él; y si un
 *     MTRR de la BIOS ya cubre parte de la franja, nos rendimos: en un solapamiento UC
 *     gana a WC, y WC con WB es indefinido. El cambio sigue el procedimiento del
 *     manual (caché desactivada, wbinvd, TLB vacía, MTRR desactivados durante la
 *     escritura).
 * Con SMP (D164) cada procesador tiene su propia PAT y sus propios MTRR, y el
 * manual exige que sean iguales en todos: wc_iniciar_secundaria() repite en cada
 * secundario lo que hizo el principal (D198). Sin eso, en el Amilo T2400 el
 * segundo núcleo escribía en el framebuffer sin combinar y el escritorio iba
 * lentísimo (en QEMU no se nota: no hay tipos de memoria).
 *
 * Desactivar: opción OPC_SEM_WC de la cabecera de arranque (mkimagem.py --sem-wc).
 * La medida de rapido.c se hace después del mapeo, así que ya mide la placa con el
 * tipo de memoria final. */
#include "wc.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "paginacao.h"
#include "texto.h"
#include "prazo.h"
#include "info_hardware.h"

#define MSR_MTRRCAP        0x0FE
#define MSR_PAT            0x277
#define MSR_MTRR_PADRAO    0x2FF
#define MSR_MTRR_BASE(i)   (0x200u + 2u * (uint32_t)(i))
#define MSR_MTRR_MASCARA(i) (0x201u + 2u * (uint32_t)(i))
#define MTRR_VALIDO        0x800u   /* bit 11 de la máscara; en el MSR por defecto, E (MTRR activados) */
#define MTRRCAP_WC         0x400u   /* bit 10: acepta el tipo WC */
#define TIPO_WC            1
/* Valor de fábrica: WB WT UC- UC WB WT UC- UC (entrada 0 en el byte bajo).
 * SaguiOS cambia solo la entrada 1 (WT -> WC); las usadas hoy (0: WB, sin
 * bits; PCD en ningún lugar) siguen iguales. */
#define PAT_SAGUI          0x0007040600070106ull

static bool desligado, com_pat, com_mtrr, ativo;
/* D198: lo que el principal programó, para repetirlo en los secundarios. */
static bool pat_gravada;
static int mtrr_indice = -1;
static uint64_t mtrr_base, mtrr_mascara;
static const char *via = "";
static char motivo[80] = "todavía no se ha mapeado el framebuffer";

static inline uint64_t ler_msr(uint32_t msr)
{
    uint32_t a, d;
    __asm__ volatile("rdmsr" : "=a"(a), "=d"(d) : "c"(msr));
    return ((uint64_t)d << 32) | a;
}

static inline void gravar_msr(uint32_t msr, uint64_t v)
{
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)) : "memory");
}

static inline void cpuid(uint32_t f, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(f), "c"(0));
}

static inline void esvaziar_cache(void) { __asm__ volatile("wbinvd" : : : "memory"); }
static inline void esvaziar_tlb(void)   { __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" : : : "eax", "memory"); }
static inline uint32_t le_cr4(void)     { uint32_t v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static inline void grava_cr4(uint32_t v) { __asm__ volatile("mov %0, %%cr4" : : "r"(v) : "memory"); }
static inline void grava_cr0(uint32_t v) { __asm__ volatile("mov %0, %%cr0" : : "r"(v) : "memory"); }

bool        wc_ativo(void)  { return ativo; }
const char *wc_via(void)    { return via; }
const char *wc_motivo(void) { return ativo ? "" : motivo; }

void wc_iniciar(void)
{
    if (info_boot.opcoes & OPC_BOOT_SEM_WC) {
        desligado = true;
        txt_copia_n(motivo, "desactivado en la cabecera de arranque (mkimagem --sem-wc)", sizeof motivo);
        klog("[VIDEO] WC desligado pelo cabecalho de boot\n");
        return;
    }
    uint32_t a, b, c, d;
    cpuid(0, &a, &b, &c, &d);
    if (a < 1) { txt_copia_n(motivo, "la CPU no informa sus capacidades (CPUID)", sizeof motivo); return; }
    cpuid(1, &a, &b, &c, &d);
    bool msr = (d >> 5) & 1;
    com_mtrr = msr && ((d >> 12) & 1);
    com_pat = msr && ((d >> 16) & 1);
    if (!com_pat) return;
    /* Nadie usa la entrada 1 todavía; aun así, el manual pide caché y TLB
     * vacías después de cambiar la PAT, para que ninguna línea quede con el tipo viejo. */
    uint64_t antes = ler_msr(MSR_PAT);
    bool i = int_salva_desliga();
    esvaziar_cache();
    gravar_msr(MSR_PAT, PAT_SAGUI);
    esvaziar_cache();
    esvaziar_tlb();
    int_restaura(i);
    pat_gravada = true;
    klog("[VIDEO] PAT %x:%x -> %x:%x (entrada 1 = WC)\n", (uint32_t)(antes >> 32), (uint32_t)antes,
         (uint32_t)(PAT_SAGUI >> 32), (uint32_t)PAT_SAGUI);
}

/* Anchura de la dirección física (la máscara del MTRR llega hasta ella). */
static uint32_t bits_fisicos(void)
{
    uint32_t a, b, c, d;
    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a < 0x80000008u) return 36;
    cpuid(0x80000008u, &a, &b, &c, &d);
    return (a & 0xFF) ? (a & 0xFF) : 36;
}

/* Procedimiento del manual de Intel (SDM vol. 3, 11.11.7.2 y 11.11.8) para
 * cambiar un MTRR con un solo núcleo: caché desactivada (CD=1, NW=0), wbinvd,
 * TLB vacía (incluidas las páginas globales: quita el PGE), MTRR desactivados
 * mientras se escribe, y todo de vuelta en orden inverso. */
static void gravar_mtrr(int i, uint64_t base, uint64_t mascara)
{
    bool ints = int_salva_desliga();
    uint32_t cr0 = le_cr0(), cr4 = le_cr4();
    grava_cr0((cr0 | 0x40000000u) & ~0x20000000u);
    esvaziar_cache();
    if (cr4 & 0x80) grava_cr4(cr4 & ~0x80u);
    esvaziar_tlb();
    uint64_t padrao = ler_msr(MSR_MTRR_PADRAO);
    gravar_msr(MSR_MTRR_PADRAO, padrao & ~(uint64_t)MTRR_VALIDO);
    gravar_msr(MSR_MTRR_BASE(i), base);
    gravar_msr(MSR_MTRR_MASCARA(i), mascara);
    gravar_msr(MSR_MTRR_PADRAO, padrao);
    esvaziar_cache();
    esvaziar_tlb();
    grava_cr0(cr0);
    if (cr4 & 0x80) grava_cr4(cr4);
    int_restaura(ints);
}

static bool mtrr_wc(uint32_t fis, uint32_t bytes)
{
    uint64_t cap = ler_msr(MSR_MTRRCAP);
    int qtd = (int)(cap & 0xFF);
    if (!(cap & MTRRCAP_WC)) { txt_copia_n(motivo, "sin PAT y las MTRR de esta CPU no admiten WC", sizeof motivo); return false; }
    if (!(ler_msr(MSR_MTRR_PADRAO) & MTRR_VALIDO)) { txt_copia_n(motivo, "sin PAT y la BIOS dejó las MTRR apagadas", sizeof motivo); return false; }
    /* Franja cubierta: potencia de 2 que contiene todo el framebuffer, con la base
     * alineada a ella (exigencia del par base/máscara). */
    uint64_t tam = 4096;
    while (tam < bytes) tam <<= 1;
    if ((uint64_t)fis & (tam - 1)) {
        snformatar(motivo, sizeof motivo, "sin PAT; el framebuffer en %x no está alineado a %u KB", fis, (uint32_t)(tam >> 10));
        return false;
    }
    uint64_t limite = (1ull << bits_fisicos()) - 1;
    uint64_t ini = fis, fim = ini + tam;
    int livre = -1;
    for (int i = 0; i < qtd; i++) {
        uint64_t m = ler_msr(MSR_MTRR_MASCARA(i));
        if (!(m & MTRR_VALIDO)) { if (livre < 0) livre = i; continue; }
        uint64_t b = ler_msr(MSR_MTRR_BASE(i));
        uint64_t mascara = m & limite & ~0xFFFull;
        uint64_t rb = b & mascara, rtam = (~mascara & limite) + 1;   /* máscaras contiguas, como las que usan las BIOS */
        if (rb < fim && ini < rb + rtam) {
            if ((b & 0xFF) == TIPO_WC && rb <= ini && rb + rtam >= fim) {
                klog("[VIDEO] MTRR %d da BIOS ja e WC sobre o framebuffer\n", i);
                return true;
            }
            snformatar(motivo, sizeof motivo, "sin PAT; la MTRR %d de la BIOS (tipo %u) solapa el framebuffer",
                       i, (uint32_t)(b & 0xFF));
            return false;
        }
    }
    if (livre < 0) { txt_copia_n(motivo, "sin PAT y sin ninguna MTRR variable libre", sizeof motivo); return false; }
    mtrr_indice = livre;
    mtrr_base = ini | TIPO_WC;
    mtrr_mascara = (~(tam - 1) & limite & ~0xFFFull) | MTRR_VALIDO;
    gravar_mtrr(livre, mtrr_base, mtrr_mascara);
    klog("[VIDEO] MTRR %d: %x, %u KB, WC\n", livre, fis, (uint32_t)(tam >> 10));
    return true;
}

/* D245: velocidad real de escritura al framebuffer, en MB/s (0 = no se pudo medir: sin TSC
 * calibrado). Escribe hasta 1 MB de ceros al principio de la ventana (el contenido aún no
 * importa: la consola lo pinta después). */
static uint32_t wc_mbs;
static uint32_t medir_escrita(uint32_t v, uint32_t bytes)
{
    uint32_t khz = hw_tsc_khz();
    if (!v || !khz) return 0;
    uint32_t n = bytes < (1u << 20) ? bytes : (1u << 20);
    n &= ~63u;
    if (n < 4096) return 0;
    volatile uint32_t *p = (volatile uint32_t *)v;
    bool ints = int_salva_desliga();
    uint64_t t0 = prazo_tsc();
    for (uint32_t i = 0; i < n / 4; i++) p[i] = 0;
    uint64_t dt = prazo_tsc() - t0;
    int_restaura(ints);
    if (!dt) dt = 1;
    return (uint32_t)(((uint64_t)n * khz) / (dt * 1000u));   /* bytes por ms / 1000 = MB/s */
}

#define WC_LENTO_MBS 200u   /* por debajo, el framebuffer va casi sin combinar (UC): se prueba un MTRR */

uint32_t wc_mapear(uint32_t fis, uint32_t bytes)
{
    uint32_t v;
    if (!desligado && com_pat) {
        v = janela_mapear_tipo(fis, bytes, PG_PWT);   /* PWT solo = entrada 1 de la PAT = WC */
        if (v) { ativo = true; via = "PAT"; }
    } else {
        v = janela_mapear(fis, bytes);
        if (v && !desligado) {
            if (!com_mtrr) txt_copia_n(motivo, "la CPU no tiene PAT ni MTRR", sizeof motivo);
            else if (mtrr_wc(fis, bytes)) { ativo = true; via = "MTRR"; }
        }
    }
    wc_mbs = medir_escrita(v, bytes);
    /* D245: con PAT pero lento (PAT mal programada, MTRR de la BIOS en UC...), se prueba un
     * MTRR variable WC sobre el framebuffer y se queda lo mejor. */
    if (v && !desligado && ativo && via[0] == 'P' && com_mtrr && wc_mbs && wc_mbs < WC_LENTO_MBS && mtrr_indice < 0) {
        uint32_t antes = wc_mbs;
        if (mtrr_wc(fis, bytes) && mtrr_indice >= 0) {
            uint32_t depois = medir_escrita(v, bytes);
            klog("[VIDEO] framebuffer lento por PAT (%u MB/s): con MTRR WC, %u MB/s\n", antes, depois);
            if (depois > antes) { wc_mbs = depois; via = "MTRR"; }
            else { gravar_mtrr(mtrr_indice, 0, 0); mtrr_indice = -1; }   /* peor: se deshace */
        }
    }
    if (ativo) klog("[VIDEO] WC ativo via %s: framebuffer %x, %u KB\n", via, fis, bytes >> 10);
    else klog("[VIDEO] WC inativo: %s\n", motivo);
    klog("[VIDEO] framebuffer: WC por %s, %u MB/s\n", ativo ? via : "ninguno", wc_mbs);
    return v;
}

uint32_t wc_mbs_medido(void) { return wc_mbs; }

/* D198: cada procesador secundario, al arrancar, deja su PAT y sus MTRR como
 * los del principal. Se llama desde smp_ap_entrada(), con las interrupciones
 * todavía apagadas y antes de que la CPU dibuje nada. */
uint64_t wc_iniciar_secundaria(void)
{
    if (desligado) return 0;
    if (pat_gravada) {
        esvaziar_cache();
        gravar_msr(MSR_PAT, PAT_SAGUI);
        esvaziar_cache();
        esvaziar_tlb();
    }
    if (mtrr_indice >= 0)
        gravar_mtrr(mtrr_indice, mtrr_base, mtrr_mascara);
    return com_pat ? ler_msr(MSR_PAT) : 0;   /* releída: para el log y la prueba */
}


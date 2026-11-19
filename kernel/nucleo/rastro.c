// SPDX-License-Identifier: GPL-2.0-or-later
/* rastro.c - marcas que sobreviven a un reinicio en caliente (ver rastro.h). Depuración del volcado por GPU en
 * el Amilo (dbg5, 2026-11-19): con dbg4, «acel2d volcado encender» reinició el equipo sin dejar nada. */
#include "rastro.h"
#include "console.h"
#include "paginacao.h"
#include "pci.h"
#include "portas.h"
#include "texto.h"
#include "msr.h"

#define RASTRO_MAGICO 0x53524131u   /* «SRA1» */
#define RASTRO_N      960           /* entradas del anillo (la página: 16 de cabecera + 960 + sobra) */

/* Cabecera: [0] mágico, [1] arranque (contador), [2] entradas escritas en total, [3] mágico invertido. */
static volatile uint32_t *pg;

void rastro_marca(uint32_t v)
{
    if (!pg) return;
    uint32_t n = pg[2];
    pg[16 + n % RASTRO_N] = v;
    pg[2] = n + 1;
    __asm__ volatile("" ::: "memory");
}

/* Los bits de error del registro de estado del PCI (15 paridad, 14 SERR enviado, 13 abort de maestro recibido,
 * 12 abort de destino recibido, 11 abort de destino enviado) de todas las funciones que tengan alguno. */
static void rastro_pci_erros(bool so_log)
{
    static pci_disp_t v[64];
    int n = pci_listar(v, 64);
    for (int i = 0; i < n; i++) {
        uint32_t st = pci_le32(&v[i], 0x04) >> 16;
        uint32_t sec = (v[i].classe == 0x06 && v[i].subclasse == 0x04) ? pci_le32(&v[i], 0x1C) >> 16 : 0;   /* estado secundario */
        if (!(st & 0xF900u) && !(sec & 0xF900u)) continue;
        klog("[RASTRO] PCI %x:%x.%x (%04x:%04x): estado %x, estado secundario %x\n", v[i].barramento, v[i].dispositivo,
             v[i].funcao, v[i].fabricante, v[i].modelo, st, sec);
        if (!so_log) {
            rastro_marca(RASTRO_VALOR(0x50) | ((uint32_t)v[i].barramento << 8) | ((uint32_t)v[i].dispositivo << 3) | v[i].funcao);
            rastro_marca(st | (sec << 16));
        }
    }
}

void rastro_pci_erros_log(const char *quando)
{
    klog("[RASTRO] errores del PCI (%s):\n", quando);
    rastro_pci_erros(true);
}

/* El TCO del ICH de Intel (puente LPC 0:1f.0): TCO1_STS y TCO2_STS dicen si hubo un NMI del TCO, si el perro
 * guardián llegó a la segunda vuelta (SECOND_TO_STS, bit 1 de TCO2_STS: reinicio por el TCO) o si falló el
 * arranque. Solo se leen. */
static void rastro_tco(void)
{
    static pci_disp_t v[64];
    int n = pci_listar(v, 64);
    for (int i = 0; i < n; i++) {
        if (v[i].fabricante != 0x8086 || v[i].barramento || v[i].dispositivo != 0x1F || v[i].funcao || v[i].classe != 0x06 ||
            v[i].subclasse != 0x01)
            continue;
        uint32_t pmbase = pci_le32(&v[i], 0x40) & 0xFF80u;
        uint32_t pmcon = pci_le32(&v[i], 0xA0);   /* GEN_PMCON_1/2 */
        uint32_t pmcon3 = pci_le32(&v[i], 0xA4);
        if (!pmbase || pmbase > 0xFF00u) { klog("[RASTRO] TCO: PMBASE sin sentido (%x)\n", pmbase); return; }
        uint16_t tco = (uint16_t)(pmbase + 0x60);
        uint16_t s1 = porta_le16((uint16_t)(tco + 4)), s2 = porta_le16((uint16_t)(tco + 6)), c1 = porta_le16((uint16_t)(tco + 8));
        klog("[RASTRO] ICH %04x: PMBASE %x, TCO1_STS %x, TCO2_STS %x (segunda vuelta del perro guardian: %s), TCO1_CNT %x "
             "(temporizador %s); GEN_PMCON_1/2 %x, GEN_PMCON_3 %x\n", v[i].modelo, pmbase, s1, s2, (s2 & 2) ? "SI" : "no", c1,
             (c1 & 0x800) ? "parado" : "EN MARCHA", pmcon, pmcon3);
        return;
    }
}

/* Comprobación de máquina (#MC, vector 18). Sin CR4.MCE (el kernel no la encendía) un error de máquina (un acceso
 * al bus que no termina, un error fatal del bus) no da excepción: la CPU se para («shutdown») y el ICH lo
 * convierte en un REINICIO, que es justo lo visto con dbg4. Aquí:
 *   - se escriben en el log los MCi_STATUS que hayan quedado del arranque anterior (en Intel sobreviven a un
 *     reinicio en caliente si la BIOS no los borra), y se borran;
 *   - se enciende CR4.MCE: la siguiente vez sale la pantalla de excepción 18 con los MCi_STATUS en el log y en
 *     el rastro, en vez de un reinicio mudo.
 * Solo si la CPU dice tener MCE y MCA (CPUID 1, EDX bits 7 y 14). No se tocan los MCi_CTL (los deja la BIOS). */
#define MSR_MCG_CAP    0x179u
#define MSR_MCG_STATUS 0x17Au
#define MSR_MC0_STATUS 0x401u
#define CR4_MCE        0x40u
static uint32_t mce_bancos;

static bool mce_tem(void)
{
    uint64_t cap;
    if (!msr_ler_seguro(MSR_MCG_CAP, &cap)) return false;   /* sin MSR (o sin CPUID): nada */
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    (void)a; (void)b; (void)c;
    if (!(d & (1u << 7)) || !(d & (1u << 14))) return false;
    mce_bancos = (uint32_t)(cap & 0xFF);
    if (mce_bancos > 32) mce_bancos = 32;
    return true;
}

static void mce_cr4(void)
{
    uint32_t v;
    __asm__ volatile("mov %%cr4, %0" : "=r"(v));
    v |= CR4_MCE;
    __asm__ volatile("mov %0, %%cr4" : : "r"(v) : "memory");
}

/* Los bancos con el bit VAL (63): al log y, si «marcar», al rastro. Devuelve cuántos. */
static int mce_bancos_log(const char *quando, bool marcar)
{
    int n = 0;
    uint64_t g = 0;
    (void)msr_ler_seguro(MSR_MCG_STATUS, &g);
    for (uint32_t i = 0; i < mce_bancos; i++) {
        uint64_t st = 0, ad = 0;
        if (!msr_ler_seguro(MSR_MC0_STATUS + 4 * i, &st) || !(st >> 63)) continue;
        if ((st >> 58) & 1) (void)msr_ler_seguro(MSR_MC0_STATUS + 4 * i + 1, &ad);   /* ADDRV: MCi_ADDR vale */
        klog("[RASTRO] MCE (%s): banco %u STATUS %x:%x ADDR %x:%x (MCG_STATUS %x)\n", quando, i, (uint32_t)(st >> 32), (uint32_t)st,
             (uint32_t)(ad >> 32), (uint32_t)ad, (uint32_t)g);
        if (marcar) {
            rastro_marca(RASTRO_VALOR(0x70) | i);
            rastro_marca((uint32_t)(st >> 32));
            rastro_marca((uint32_t)st);
            rastro_marca((uint32_t)ad);
        }
        n++;
    }
    return n;
}

static void mce_iniciar(void)
{
    if (!mce_tem()) { klog("[RASTRO] MCE: la CPU no tiene MCE/MCA: un error de maquina seguira siendo un reinicio\n"); return; }
    int n = mce_bancos_log("del arranque anterior", false);
    for (uint32_t i = 0; i < mce_bancos; i++) (void)msr_escrever_seguro(MSR_MC0_STATUS + 4 * i, 0);
    mce_cr4();
    klog("[RASTRO] MCE: %u bancos, %d con error del arranque anterior; CR4.MCE encendido (un error de maquina da la "
         "excepcion 18 en vez de un reinicio)\n", mce_bancos, n);
}

/* Los procesadores secundarios (fpu_iniciar_secundaria): el mismo CR4.MCE. */
void rastro_mce_secundaria(void)
{
    if (mce_bancos) mce_cr4();
}

void rastro_iniciar(void)
{
    uint64_t f = rastro_pagina();
    if (!f || f >= 0x100000000ull) { klog("[RASTRO] sin pagina para las marcas\n"); return; }
    volatile uint32_t *p = (volatile uint32_t *)janela_mapear_tipo((uint32_t)f, 4096, PG_PCD | PG_PWT);
    if (!p) { klog("[RASTRO] sin ventana para la pagina %x\n", (uint32_t)f); return; }
    uint32_t arranque = 0;
    if (p[0] == RASTRO_MAGICO && p[3] == ~RASTRO_MAGICO) {
        uint32_t n = p[2], ini = n > 48 ? n - 48 : 0;
        arranque = p[1];
        klog("[RASTRO] marcas del arranque anterior (n.o %u, pagina %x): %u en total; las ultimas %u:\n", arranque, (uint32_t)f, n,
             n - ini);
        char linha[200];
        int l = 0;
        for (uint32_t k = ini; k < n; k++) {
            snformatar(linha + l, sizeof linha - (size_t)l, " %x", p[16 + k % RASTRO_N]);
            while (linha[l]) l++;
            if (l > 150 || k + 1 == n) { klog("[RASTRO]  %s\n", linha); l = 0; }
        }
        if (n) {
            uint32_t u = p[16 + (n - 1) % RASTRO_N];
            kprintf("[RASTRO] ultima marca del arranque anterior: %x (dmesg | grep RASTRO)\n", u);
        }
    } else
        klog("[RASTRO] sin marcas del arranque anterior en la pagina %x (magico %x: primer arranque, apagado o RAM borrada)\n",
             (uint32_t)f, p[0]);
    rastro_tco();
    rastro_pci_erros(true);
    mce_iniciar();
    p[0] = RASTRO_MAGICO; p[1] = arranque + 1; p[2] = 0; p[3] = ~RASTRO_MAGICO;
    pg = p;
}

void rastro_excecao(uint32_t vetor, uint32_t erro, uint32_t eip)
{
    rastro_marca(0xE0000000u | vetor);
    rastro_marca(erro);
    rastro_marca(eip);
    if (vetor == 2) {   /* NMI: el puerto 0x61 dice si fue SERR (bit 7) o IOCHK (bit 6) */
        uint8_t nmi = porta_le8(0x61);
        rastro_marca(RASTRO_VALOR(0x61) | nmi);
        klog("[RASTRO] NMI: puerto 61h %x (SERR %s, IOCHK %s)\n", nmi, (nmi & 0x80) ? "si" : "no", (nmi & 0x40) ? "si" : "no");
    }
    if (vetor == 18) (void)mce_bancos_log("excepcion 18", true);
    rastro_pci_erros(false);
}

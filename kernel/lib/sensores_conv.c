/* sensores_conv.c - fórmulas do monitor de hardware (D092).
 *
 * Archivo sin acceso a hardware, para probarlo en el anfitrión
 * (tests/h71_sensores.py lo compila con -DSAGUI_HOST). Los campos y las tablas
 * vienen de los manuales de Intel (SDM, vol. 4: MSRs) y de AMD (BKDG de las familias
 * 0Fh, 10h, 11h, 15h y 16h; PPR de la 17h), contrastados con los drivers coretemp,
 * k8temp y k10temp de Linux, usados solo como referencia de las tablas (D073).
 * Los valores que dependen del modelo y que el manual no da con certeza (VID,
 * TjMax de los Core 2) quedan marcados como aproximados por quien los muestra. */
#include "sensores_conv.h"
#ifndef SAGUI_HOST
#include "abi.h"
#else
#define SENS_THR_TERMICO   0x01
#define SENS_THR_REGISTRO  0x02
#define SENS_THR_PROCHOT   0x04
#define SENS_THR_ENERGIA   0x08
#define SENS_THR_SABE      0x80
#endif

/* División de 64 por 32 bits sin la libgcc (el kernel no la enlaza). */
uint64_t sconv_div64(uint64_t n, uint32_t d)
{
    if (d == 0) return 0;
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

/* ------------------------------------------------------------ Intel */

/* IA32_THERM_STATUS / IA32_PACKAGE_THERM_STATUS: bits 22:16 = cuántos grados
 * faltan para el TjMax; bit 31 = lectura válida. */
bool sconv_intel_temp(uint64_t st, int tjmax_mc, int *mc)
{
    if (!((st >> 31) & 1) || tjmax_mc <= 0) return false;
    *mc = tjmax_mc - (int)((st >> 16) & 0x7F) * 1000;
    return true;
}

uint32_t sconv_intel_throttling(uint64_t st, bool tem_pln)
{
    uint32_t t = SENS_THR_SABE;
    if (st & 1) t |= SENS_THR_TERMICO;        /* bit 0: thermal status */
    if (st & 2) t |= SENS_THR_REGISTRO;       /* bit 1: thermal status log */
    if (st & 4) t |= SENS_THR_PROCHOT;        /* bit 2: PROCHOT# / FORCEPR# */
    if (tem_pln && (st & (1u << 10))) t |= SENS_THR_ENERGIA;   /* bit 10: power limit */
    return t;
}

/* MSR_TEMPERATURE_TARGET (0x1A2), bits 23:16, desde el Nehalem. */
int sconv_intel_tjmax_msr(uint64_t v)
{
    int t = (int)((v >> 16) & 0xFF);
    return (t >= 70 && t <= 130) ? t * 1000 : 0;
}

bool sconv_intel_core2(uint32_t m)
{
    return m == 0x0E || m == 0x0F || m == 0x16 || m == 0x17 || m == 0x1D;
}

/* Sin el MSR 0x1A2: Core Duo, Core 2 y los Atom antiguos. El criterio por
 * modelo es el de coretemp(4) de FreeBSD (BSD-2-Clause, D259) y vale en todas
 * las variantes de la imagen. */
#include "../terceros/bsd/coretemp/coretemp_tjmax.h"
int sconv_intel_tjmax_tabela(uint32_t m, uint32_t stepping, bool movel, bool ee_85)
{
    return tabela_bsd_intel_tjmax(m, stepping, movel, ee_85);
}

/* IA32_PERF_STATUS (0x198). Core/Core 2: bits 12:8 + medio (bit 14); desde
 * el Nehalem en adelante, bits 15:8. */
uint32_t sconv_intel_mult_x10(uint32_t m, uint64_t v)
{
    if (sconv_intel_core2(m)) return (uint32_t)((v >> 8) & 0x1F) * 10 + (uint32_t)((v >> 14) & 1) * 5;
    return (uint32_t)((v >> 8) & 0xFF) * 10;
}

/* Core 2: el multiplicador máximo viene en el mismo MSR, bits 44:40 (+ medio en el 46). */
uint32_t sconv_intel_mult_max_core2_x10(uint64_t v)
{
    return (uint32_t)((v >> 40) & 0x1F) * 10 + (uint32_t)((v >> 46) & 1) * 5;
}

static bool intel_sandy_ou_depois(uint32_t m)
{
    static const uint8_t modelos[] = { 0x2A, 0x2D, 0x3A, 0x3E, 0x3C, 0x3F, 0x45, 0x46, 0x3D, 0x47, 0x4F, 0x56,
                                       0x4E, 0x5E, 0x55, 0x8E, 0x9E, 0xA5, 0xA6, 0x7D, 0x7E, 0x6A, 0x6C, 0x8C,
                                       0x8D, 0x97, 0x9A, 0xA7, 0xB7, 0xBA, 0xBF };
    for (unsigned i = 0; i < sizeof modelos; i++)
        if (modelos[i] == m) return true;
    return false;
}

/* Tensión pedida: Core/Core 2 (IMVP-6: 1,5 V - 12,5 mV por paso del VID en los
 * bits 6:0); Sandy Bridge en adelante: bits 47:32 en 1/8192 V. */
uint32_t sconv_intel_vid_mv(uint32_t m, uint64_t v)
{
    if (sconv_intel_core2(m)) {
        uint32_t vid = (uint32_t)(v & 0x7F);
        int mv = 1500 - (int)(vid * 25 / 2);
        return mv >= 300 ? (uint32_t)mv : 0;
    }
    if (intel_sandy_ou_depois(m)) {
        uint32_t c = (uint32_t)((v >> 32) & 0xFFFF);
        return (uint32_t)((uint64_t)c * 1000 / 8192);
    }
    return 0;
}

/* ------------------------------------------------------------ AMD */

/* K8, F3xE4 (Thermtrip Status): bits 23:14 en cuartos de grado, menos 49 °C.
 * En las revisiones antiguas los bits 15:14 valen 0 (resolución de 1 °C). */
int sconv_amd_k8(uint32_t r)
{
    return (int)((r >> 14) & 0x3FF) * 250 - 49000;
}

/* Familias 10h-16h, F3xA4 (Reported Temperature Control): CurTmp en los bits
 * 31:21, en octavos de grado. Es el Tctl, una escala de AMD para el control del
 * ventilador, cerca pero no igual a la temperatura real. */
int sconv_amd_k10(uint32_t r)
{
    return (int)(r >> 21) * 125;
}

/* 17h/19h, SMN 0x59800: el mismo campo; con el bit 19 (CurTmpRangeSel) la
 * escala empieza en -49 °C. */
int sconv_amd_zen(uint32_t r)
{
    int t = (int)(r >> 21) * 125;
    if (r & (1u << 19)) t -= 49000;
    return t;
}

/* Modelos cuyo Tctl está desplazado del Tdie (la temperatura real del chip): la tabla
 * es la de k10temp de Linux (GPL-2.0-or-later), en kernel/terceros/linux/hwmon/
 * (D109, D259). Sin GPL: 0. */
#ifndef SAGUI_SEM_GPL
#include "../terceros/linux/hwmon/sensores_tabelas.h"
#endif
int sconv_amd_zen_desvio(const char *marca)
{
#ifdef SAGUI_SEM_GPL
    (void)marca;
    return 0;
#else
    return tabela_linux_amd_desvio(marca);
#endif
}

/* F3x64 (Hardware Thermal Control): HtcEn no bit 0; limite = 52 °C + medio
 * grado por unidad de los bits 22:16. */
int sconv_amd_htc_limite(uint32_t r)
{
    if (!(r & 1)) return 0;
    return 52000 + (int)((r >> 16) & 0x7F) * 500;
}

/* MSR 0xC0010064 + n (P-state n). 10h, 15h, 16h: 100 MHz * (FID + 16) /
 * 2^DID; 11h: 100 MHz * (FID + 8) / 2^DID; 17h/19h: 200 MHz * FID / DFS.
 * 12h y 14h usan otra codificación de los divisores y quedan sin fórmula. */
uint32_t sconv_amd_pstate_khz(uint32_t fam, uint64_t v)
{
    if (!(v >> 63)) return 0;   /* PstateEn */
    if (fam == 0x10 || fam == 0x15 || fam == 0x16 || fam == 0x11) {
        uint32_t fid = (uint32_t)(v & 0x3F), did = (uint32_t)((v >> 6) & 7);
        if (did > 4) return 0;
        return (100000u * (fid + (fam == 0x11 ? 8u : 16u))) >> did;
    }
    if (fam == 0x17 || fam == 0x19) {
        uint32_t fid = (uint32_t)(v & 0xFF), dfs = (uint32_t)((v >> 8) & 0x3F);
        if (!dfs) return 0;
        return 200000u * fid / dfs;
    }
    return 0;
}

/* VID del P-state: SVI1 (10h, 11h, 15h hasta el modelo 2Fh) 1,55 V - 12,5 mV por
 * paso en los bits 15:9; SVI2 (15h modelos 30h+, 16h) y 17h/19h (bits 21:14)
 * 1,55 V - 6,25 mV por paso. Es la tensión pedida al regulador, no la medida. */
uint32_t sconv_amd_vid_mv(uint32_t fam, uint64_t v)
{
    int mv;
    if (!(v >> 63)) return 0;
    if (fam == 0x17 || fam == 0x19) mv = 1550 - (int)(((v >> 14) & 0xFF) * 25 / 4);
    else if (fam == 0x16) mv = 1550 - (int)(((v >> 9) & 0xFF) * 25 / 4);
    else if (fam == 0x10 || fam == 0x11 || fam == 0x15) mv = 1550 - (int)(((v >> 9) & 0x7F) * 25 / 2);
    else return 0;
    return mv >= 300 ? (uint32_t)mv : 0;
}

/* K8, MSR 0xC0010042 (FIDVID_STATUS): FID 0 = 4x 200 MHz, cada unidad suma
 * medio multiplicador (100 MHz). VID de mesa: 1,55 V - 25 mV por paso. */
uint32_t sconv_k8_khz(uint64_t v)
{
    return 800000u + (uint32_t)(v & 0x3F) * 100000u;
}

uint32_t sconv_k8_mult_max_x10(uint64_t v)
{
    return 40 + (uint32_t)((v >> 16) & 0x3F) * 5;
}

uint32_t sconv_k8_vid_mv(uint64_t v)
{
    uint32_t vid = (uint32_t)((v >> 32) & 0x3F);
    return vid <= 0x1E ? 1550 - vid * 25 : 0;
}

/* ------------------------------------------------------------ APERF/MPERF */

uint32_t sconv_freq_real_khz(uint32_t tsc_khz, uint64_t da, uint64_t dm)
{
    if (!dm || !tsc_khz) return 0;
    /* Reduce los dos hasta que el MPERF quepa en 32 bits, sin perder la razón. */
    while (dm >> 32 || da >> 32) { dm >>= 1; da >>= 1; }
    if (!dm) return 0;
    return (uint32_t)sconv_div64(da * tsc_khz, (uint32_t)dm);
}

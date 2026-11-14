// SPDX-License-Identifier: GPL-2.0-or-later
/* eist_antiguo.c - Enhanced SpeedStep de los Intel antiguos (D177, fases A y B).
 *
 * Pentium M, Core Solo/Duo y Core 2 llevan en IA32_PERF_CTL (0x199) el FID
 * (bits 12:8, más el medio multiplicador en el bit 14 de los Core 2) y el
 * VID (bits 7:0): la TENSIÓN. Los pares válidos solo están en la tabla
 * _PSS de la BIOS; por eso frequencia.c (D106) excluye estas CPU y aquí, de
 * momento, solo se lee:
 *   - fase A: CPUID 01H:ECX[7] (EIST), IA32_MISC_ENABLE (0x1A0) bit 16
 *     (activado por la BIOS) y bit 20 (bloqueo), IA32_PERF_STATUS (0x198) y
 *     IA32_PERF_CTL (0x199). Los bits 63:32 de PERF_STATUS (máximo y mínimo)
 *     no están en el SDM: se muestran como informativos y solo los Core 2
 *     los usan para validar (bits 44:40, como sensores.c);
 *   - fase B: el _PSS, _PSD y _PCT de cada procesador (acpi_pss.c), leídos
 *     antes de la paginación, y su validación contra los MSR.
 * Ningún MSR se escribe aquí. La fase C (escribir solo valores del _PSS
 * fiable, por dominio _PSD) irá en frequencia.c sobre la D175. */
#include "eist_antiguo.h"
#include "acpi_pss.h"
#include "bootinfo.h"
#include "console.h"
#include "info_hardware.h"
#include "msr.h"
#include "texto.h"

#define MSR_PERF_STATUS 0x198
#define MSR_PERF_CTL    0x199
#define MSR_MISC_ENABLE 0x1A0

static acpi_pss_t pss;
static bool pss_lido;
static uint8_t presentes[32];
static uint32_t qtd_presentes;
static eist_antiguo_t ea;
static bool iniciado;

/* ------------------------------------------------------------ antes de la paginación */

/* Solo memoria que el firmware reservó (E820 2, 3 o 4) o el segmento F/E de
 * la BIOS: una dirección sacada del AML nunca debe llevar a leer un registro
 * de un dispositivo. Las tablas de la RSDT, como en acpi.c. */
static const uint8_t *mapear(uint32_t fis, uint32_t tam, bool raiz)
{
    if (fis < 0x1000 || !tam) return NULL;
    uint64_t fim = (uint64_t)fis + tam;
    if (fim > 0xFEC00000ull) return NULL;
    if (raiz) return (const uint8_t *)fis;
    if (fis >= 0xE0000 && fim <= 0x100000) return (const uint8_t *)fis;
    for (uint32_t i = 0; i < info_boot.mapa_qtd && i < MAPA_MAX; i++) {
        const regiao_e820_t *r = &mapa_memoria[i];
        if (r->tipo != 2 && r->tipo != 3 && r->tipo != 4) continue;
        if (r->base <= fis && fim <= r->base + r->tamanho) return (const uint8_t *)fis;
    }
    return NULL;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* Los ACPI Processor ID de los procesadores encendidos (MADT, entradas de tipo 0). */
static void capturar_madt(const info_acpi_t *a)
{
    for (uint32_t i = 0; i < a->qtd && i < ACPI_MAX_TABELAS; i++) {
        if (memcmp(a->tabela[i].assinatura, "APIC", 4) != 0) continue;
        const uint8_t *t = mapear(a->tabela[i].endereco, 44, true);
        if (!t || memcmp(t, "APIC", 4) != 0) return;
        uint32_t tam = le32(t + 4);
        if (tam < 44 || tam > 65536 || !mapear(a->tabela[i].endereco, tam, true)) return;
        for (uint32_t off = 44; off + 2 <= tam;) {
            uint8_t tipo = t[off], len = t[off + 1];
            if (len < 2 || off + len > tam) break;
            if (tipo == 0 && len >= 8 && (t[off + 4] & 1) && qtd_presentes < sizeof presentes)
                presentes[qtd_presentes++] = t[off + 2];
            off += len;
        }
        return;
    }
}

static const char *txt_psd(uint8_t m)
{
    switch (m) {
    case PSD_M_SW_ALL: return "SW_ALL";
    case PSD_M_SW_ANY: return "SW_ANY";
    case PSD_M_HW_ALL: return "HW_ALL";
    case PSD_M_SW_ALL | PSD_M_HW_ALL: return "SW_ALL o HW_ALL (segun _PDC)";
    default: return m ? "varios" : "?";
    }
}

static const char *txt_pct(uint8_t m)
{
    switch (m) {
    case 0: return "sin _PCT legible";
    case PCT_M_FFH: return "FFixedHW (MSR)";
    case PCT_M_IO: return "SystemIO (SMI)";
    case PCT_M_FFH | PCT_M_IO: return "FFixedHW o SystemIO (segun _PDC)";
    default: return "otro espacio";
    }
}

static void log_cpu(const pss_cpu_t *c)
{
    if (c->pss_origem == PSS_SEM) {
        klog("[PSS] %s (id %u): sin _PSS\n", c->caminho, c->id);
        return;
    }
    if (c->pss_origem == PSS_ILEGIVEL) {
        klog("[PSS] %s (id %u): _PSS ilegible (calculado o con referencias)\n", c->caminho, c->id);
        return;
    }
    char lista[80];
    int k = 0;
    lista[0] = 0;
    for (uint32_t i = 0; i < c->qtd && k < (int)sizeof lista - 8; i++)
        k += snformatar(lista + k, sizeof lista - (size_t)k, "%s%u", i ? "/" : "", c->est[i].mhz);
    klog("[PSS] %s (id %u): _PSS de %s (%s \"%s\"%s), %u estados: %s MHz%s\n", c->caminho, c->id, c->pss_de,
         c->pss_dinamica ? "SSDT" : "tabla", c->pss_tabela, c->pss_dinamica ? ", cargada con Load" : "", c->qtd, lista,
         c->pss_variantes > 1 ? " (el metodo tiene variantes: se toma la nativa)" : "");
    for (uint32_t i = 0; i < c->qtd; i++)
        klog("[PSS]   P%u: %u MHz, %u mW, latencia %u/%u us, Control 0x%x, Status 0x%x\n", i, c->est[i].mhz, c->est[i].mw,
             c->est[i].lat_us, c->est[i].lat_bm_us, c->est[i].controle, c->est[i].status);
    if (c->psd_variantes)
        klog("[PSS]   _PSD: dominio %u, %u procesadores, %s%s\n", c->psd_dominio, c->psd_num, txt_psd(c->psd_tipos),
             c->psd_dominios_iguais ? "" : " (dominios distintos segun la variante)");
    else
        klog("[PSS]   sin _PSD legible\n");
    klog("[PSS]   _PCT: %s\n", txt_pct(c->pct_espacos));
}

void eist_pss_capturar(void)
{
    if (info_boot.opcoes & OPC_BOOT_SEM_ACPI) {
        klog("[PSS] no se leen las tablas (--sem-acpi)\n");
        return;
    }
    const info_acpi_t *a = hw_acpi();
    if (!a || !a->achou) {
        klog("[PSS] sin tablas ACPI\n");
        return;
    }
    uint32_t tabs[PSS_MAX_TABELAS], n = 0;
    for (int dsdt = 1; dsdt >= 0; dsdt--)   /* la DSDT primero */
        for (uint32_t i = 0; i < a->qtd && i < ACPI_MAX_TABELAS && n < PSS_MAX_TABELAS; i++)
            if (memcmp(a->tabela[i].assinatura, dsdt ? "DSDT" : "SSDT", 4) == 0) tabs[n++] = a->tabela[i].endereco;
    acpi_pss_ler(&pss, tabs, n, mapear);
    capturar_madt(a);
    pss_lido = true;
    for (uint32_t i = 0; i < pss.tabelas; i++)
        klog("[PSS] tabla %s \"%s\" en 0x%x (%u bytes)%s\n", pss.tabela[i].assinatura, pss.tabela[i].oem, pss.tabela[i].fis,
             pss.tabela[i].tam, pss.tabela[i].dinamica ? ", cargada con Load" : "");
    uint32_t com = 0;
    for (uint32_t i = 0; i < pss.qtd_cpus; i++) {
        if (pss.cpu[i].pss_origem != PSS_SEM) com++;
        log_cpu(&pss.cpu[i]);
    }
    klog("[PSS] %u tablas (%u cargadas con Load, %u rechazadas), %u procesadores, %u en la MADT; %s\n", pss.tabelas,
         pss.dinamicas, pss.rejeitadas, pss.cpus_total, qtd_presentes,
         com ? "con _PSS" : "sin _PSS en ningun procesador");
    if (pss.aml_incompleto || pss.objetos_perdidos || pss.pss_bytes > pss.pss_lidos)
        klog("[PSS] AML leido a medias: %u listas sin terminar, %u objetos sin sitio, %u _PSS en bytes / %u leidos\n",
             pss.aml_incompleto, pss.objetos_perdidos, pss.pss_bytes, pss.pss_lidos);
}

/* ------------------------------------------------------------ después */

static inline void cpuid(uint32_t f, uint32_t s, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(s));
}

static const char *nome_modelo(uint32_t m)
{
    switch (m) {
    case 0x09: return "Pentium M (Banias)";
    case 0x0D: return "Pentium M (Dothan)";
    case 0x0E: return "Core Solo/Duo (Yonah)";
    case 0x0F: return "Core 2 (Merom)";
    case 0x16: return "Core 2 (Merom-L)";
    case 0x17: return "Core 2 (Penryn)";
    case 0x1D: return "Xeon 7400 (Dunnington)";
    default: return NULL;
    }
}

static void nao_aplica(const char *m)
{
    ea.estado = EIST_ANT_NAO_APLICA;
    txt_copia_n(ea.motivo, m, sizeof ea.motivo);
}

void eist_antiguo_iniciar(void)
{
    uint32_t r[4];
    memset(&ea, 0, sizeof ea);
    cpuid(0, 0, r);
    uint32_t max = r[0];
    bool intel = r[1] == 0x756E6547 && r[3] == 0x49656E69 && r[2] == 0x6C65746E;
    bool hv = false, tem_msr = false;
    if (max >= 1) {
        cpuid(1, 0, r);
        uint32_t bf = (r[0] >> 8) & 0xF, bm = (r[0] >> 4) & 0xF;
        ea.familia = bf == 0xF ? bf + ((r[0] >> 20) & 0xFF) : bf;
        ea.modelo = (bf == 6 || bf == 0xF) ? (((r[0] >> 16) & 0xF) << 4) + bm : bm;
        ea.eist_cpuid = (r[2] >> 7) & 1;
        hv = (r[2] >> 31) & 1;
        tem_msr = (r[3] >> 5) & 1;
    }
    const char *nome = intel && ea.familia == 6 ? nome_modelo(ea.modelo) : NULL;
    if (nome) txt_copia_n(ea.nome, nome, sizeof ea.nome);

    if (!intel) nao_aplica("no es Intel");
    else if (!nome) nao_aplica("no es un Intel con FID/VID en PERF_CTL (Pentium M a Core 2)");
    else if (hv) nao_aplica("máquina virtual");
    else if (!tem_msr) nao_aplica("la CPU no tiene MSR");
    else {
        ea.estado = EIST_ANT_DETECTADO;
        uint64_t v;
        if (msr_ler_seguro(MSR_MISC_ENABLE, &v)) {
            ea.misc_enable = v;
            ea.eist_bios = (v >> 16) & 1;
            ea.bloqueado = (v >> 20) & 1;
        }
        if (msr_ler_seguro(MSR_PERF_STATUS, &ea.perf_status) && msr_ler_seguro(MSR_PERF_CTL, &ea.perf_ctl)) {
            ea.tem_msr = 1;
            uint32_t st = (uint32_t)ea.perf_status, ct = (uint32_t)ea.perf_ctl, hi = (uint32_t)(ea.perf_status >> 32);
            ea.fid_x2 = (uint8_t)pss_fid_x2(st, ea.modelo);
            ea.vid = (uint8_t)pss_vid(st);
            ea.ctl_fid_x2 = (uint8_t)pss_fid_x2(ct, ea.modelo);
            ea.ctl_vid = (uint8_t)pss_vid(ct);
            if (ea.modelo >= 0x0E) {   /* bits 47:32 máximo, 63:48 mínimo (no documentados) */
                ea.max_fid_x2 = (uint8_t)pss_fid_x2(hi & 0xFFFF, ea.modelo);
                ea.max_vid = (uint8_t)pss_vid(hi & 0xFFFF);
                ea.min_fid_x2 = (uint8_t)pss_fid_x2(hi >> 16, ea.modelo);
                ea.min_vid = (uint8_t)pss_vid(hi >> 16);
            }
        }
    }

    /* Validación del _PSS (aunque la CPU no sea de estas: así el log dice por qué). */
    pss_cpu_info_t in;
    memset(&in, 0, sizeof in);
    in.familia = ea.familia;
    in.modelo = ea.modelo;
    in.intel_fidvid = ea.estado == EIST_ANT_DETECTADO;
    in.tem_status = ea.tem_msr;
    in.perf_status = ea.perf_status;
    in.tsc_khz = ea.modelo >= 0x0E ? hw_tsc_khz() : 0;   /* el TSC de los Pentium M cambia con la frecuencia */
    in.qtd_presentes = qtd_presentes;
    memcpy(in.presentes, presentes, sizeof in.presentes);
    if (pss_lido) {
        ea.pss_fiaveis = acpi_pss_validar(&pss, &in);
        for (uint32_t i = 0; i < pss.qtd_cpus; i++)
            if (pss.cpu[i].pss_origem != PSS_SEM) ea.pss_cpus++;
    }
    iniciado = true;

    if (ea.estado != EIST_ANT_DETECTADO) {
        klog("[EIST] sin EIST antiguo: %s\n", ea.motivo);
        return;
    }
    klog("[EIST] %s (familia 6, modelo %xh): EIST en CPUID %s, %s por la BIOS%s; MISC_ENABLE 0x%x:%x\n", ea.nome,
         ea.modelo, ea.eist_cpuid ? "si" : "no", ea.eist_bios ? "activado" : "DESACTIVADO",
         ea.bloqueado ? ", bloqueado" : "", (uint32_t)(ea.misc_enable >> 32), (uint32_t)ea.misc_enable);
    if (ea.tem_msr) {
        klog("[EIST] PERF_STATUS 0x%x:%x (actual FID %u%s VID 0x%x), PERF_CTL 0x%x (FID %u%s VID 0x%x)\n",
             (uint32_t)(ea.perf_status >> 32), (uint32_t)ea.perf_status, ea.fid_x2 / 2, ea.fid_x2 & 1 ? ",5" : "", ea.vid,
             (uint32_t)ea.perf_ctl, ea.ctl_fid_x2 / 2, ea.ctl_fid_x2 & 1 ? ",5" : "", ea.ctl_vid);
        if (ea.max_fid_x2 || ea.min_fid_x2)
            klog("[EIST] PERF_STATUS 63:32 (informativo, no documentado): maximo FID %u VID 0x%x, minimo FID %u VID 0x%x\n",
                 ea.max_fid_x2 / 2, ea.max_vid, ea.min_fid_x2 / 2, ea.min_vid);
    } else {
        klog("[EIST] PERF_STATUS/PERF_CTL no responden\n");
    }
    for (uint32_t i = 0; pss_lido && i < pss.qtd_cpus; i++) {
        const pss_cpu_t *c = &pss.cpu[i];
        if (c->pss_origem == PSS_SEM && c->ausente) continue;
        klog("[EIST] %s: %s%s%s\n", c->caminho, c->fiavel ? "_PSS fiable" : "no fiable",
             c->fiavel ? "" : ": ", c->motivo);
    }
    const char *estado_pss;
    if (!pss_lido) estado_pss = "sin leer las tablas ACPI";
    else if (!ea.pss_cpus) estado_pss = "sin tabla _PSS";
    else if (ea.pss_fiaveis) estado_pss = "_PSS fiable";
    else estado_pss = "_PSS no fiable";
    snformatar(ea.motivo, sizeof ea.motivo, "%s, %u de %u procesadores fiables", estado_pss, ea.pss_fiaveis, ea.pss_cpus);
    klog("[EIST] EIST antiguo (%s): FID %u%s, VID 0x%x, %s -> no se cambia (solo lectura, D177)\n", ea.nome, ea.fid_x2 / 2,
         ea.fid_x2 & 1 ? ",5" : "", ea.vid, estado_pss);
}

bool eist_antiguo_info(eist_antiguo_t *out)
{
    if (!iniciado) return false;
    *out = ea;
    return true;
}

const acpi_pss_t *eist_pss(void) { return pss_lido ? &pss : NULL; }

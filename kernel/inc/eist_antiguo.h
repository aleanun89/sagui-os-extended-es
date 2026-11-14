// SPDX-License-Identifier: GPL-2.0-or-later
/* eist_antiguo.h - Enhanced SpeedStep de los Intel antiguos (D177).
 *
 * Fases A y B, solo lectura: qué CPU es (Pentium M, Core Solo/Duo, Core 2),
 * si la BIOS dejó el EIST activado, el FID/VID actual y lo que declaran las
 * tablas ACPI (_PSS, _PSD, _PCT, ver acpi_pss.h). No escribe ningún MSR:
 * el driver que cambia la frecuencia sigue siendo frequencia.c (D106/D175). */
#ifndef SAGUI_EIST_ANTIGUO_H
#define SAGUI_EIST_ANTIGUO_H
#include "tipos.h"
#include "acpi_pss.h"

#define EIST_ANT_NAO_APLICA  0   /* otra CPU, máquina virtual, sin MSR... */
#define EIST_ANT_DETECTADO   1   /* Intel con FID/VID en IA32_PERF_CTL */

typedef struct {
    uint8_t  estado;            /* EIST_ANT_* */
    uint8_t  eist_cpuid;        /* CPUID 01H:ECX[7] */
    uint8_t  eist_bios;         /* IA32_MISC_ENABLE[16]: la BIOS lo dejó activado */
    uint8_t  bloqueado;         /* IA32_MISC_ENABLE[20]: bloqueo del EIST (no se puede cambiar el bit 16) */
    uint8_t  tem_msr;           /* se leyeron PERF_STATUS y PERF_CTL */
    uint8_t  fid_x2, vid;       /* actuales (PERF_STATUS 15:0), FID en medios */
    uint8_t  ctl_fid_x2, ctl_vid;   /* pedidos (PERF_CTL 15:0) */
    uint8_t  max_fid_x2, max_vid, min_fid_x2, min_vid;   /* PERF_STATUS 63:32: informativos, 0 = no expuestos */
    uint32_t familia, modelo;
    uint64_t perf_status, perf_ctl, misc_enable;
    uint32_t pss_cpus, pss_fiaveis;   /* procesadores con _PSS leído / fiables */
    char     nome[32];          /* "Core Duo (Yonah)" */
    char     motivo[96];        /* por qué no aplica, o el estado del _PSS */
} eist_antiguo_t;

/* Antes de la paginación (hw_capturar_boot): lee _PSS/_PSD/_PCT de la DSDT,
 * de las SSDT y de las SSDT que se cargan con Load(). Solo lee. */
void eist_pss_capturar(void);

/* Con el tratador de #GP de los MSR listo: detecta la CPU, lee los MSR,
 * valida el _PSS contra ellos y lo deja en el log. Solo lee. */
void eist_antiguo_iniciar(void);

/* false = todavía sin iniciar. */
bool eist_antiguo_info(eist_antiguo_t *out);
const acpi_pss_t *eist_pss(void);   /* NULL = no se leyeron las tablas */

#endif

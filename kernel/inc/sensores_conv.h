// SPDX-License-Identifier: GPL-2.0-or-later
/* sensores_conv.h - las fórmulas del monitor de hardware (D092).
 *
 * Solo aritmética, sin tocar el hardware: el kernel lee los registros y
 * llama a estas funciones, y las pruebas compilan el mismo archivo en el anfitrión
 * (tests/h71_sensores.py) para comprobar cada familia con valores simulados.
 * Temperaturas en milésimas de grado; frecuencias en kHz; tensiones en mV.
 * Las tablas por modelo siguen las de Linux (coretemp, k10temp, k8temp),
 * consultadas como referencia (D073, docs/terceros.md). */
#ifndef SAGUI_SENSORES_CONV_H
#define SAGUI_SENSORES_CONV_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

uint64_t sconv_div64(uint64_t n, uint32_t d);   /* sin la libgcc */

/* Intel */
bool     sconv_intel_temp(uint64_t therm_status, int tjmax_mc, int *mc);   /* false: lectura no válida (bit 31) */
uint32_t sconv_intel_throttling(uint64_t therm_status, bool tem_pln);      /* SENS_THR_* */
int      sconv_intel_tjmax_msr(uint64_t temperature_target);              /* 0 = valor fuera de lo razonable */
int      sconv_intel_tjmax_tabela(uint32_t modelo, uint32_t stepping, bool movel, bool ee_85);
bool     sconv_intel_core2(uint32_t modelo);                               /* Core / Core 2 (sin MSR 0x1A2) */
uint32_t sconv_intel_mult_x10(uint32_t modelo, uint64_t perf_status);
uint32_t sconv_intel_mult_max_core2_x10(uint64_t perf_status);
uint32_t sconv_intel_vid_mv(uint32_t modelo, uint64_t perf_status);

/* AMD */
int      sconv_amd_k8(uint32_t f3xe4);
int      sconv_amd_k10(uint32_t f3xa4);
int      sconv_amd_zen(uint32_t tctl);
int      sconv_amd_zen_desvio(const char *marca);   /* Tctl - Tdie de los modelos que lo tienen */
int      sconv_amd_htc_limite(uint32_t f3x64);      /* 0 = HTC desactivado */
uint32_t sconv_amd_pstate_khz(uint32_t familia, uint64_t pstate_def);   /* 0 = familia sin fórmula */
uint32_t sconv_amd_vid_mv(uint32_t familia, uint64_t pstate_def);
uint32_t sconv_k8_khz(uint64_t fidvid_status);
uint32_t sconv_k8_mult_max_x10(uint64_t fidvid_status);
uint32_t sconv_k8_vid_mv(uint64_t fidvid_status);

/* APERF/MPERF: frecuencia efectiva = TSC * dA / dM. */
uint32_t sconv_freq_real_khz(uint32_t tsc_khz, uint64_t d_aperf, uint64_t d_mperf);

#endif

// SPDX-License-Identifier: GPL-2.0-or-later
/* fpu.h - FPU x87, MMX y SSE para los programas (estado guardado por proceso). */
#ifndef SAGUI_FPU_H
#define SAGUI_FPU_H
#include "tipos.h"

/* Recursos detectados por CPUID (bits de fpu_recursos()). */
#define CPU_FPU    0x001
#define CPU_MMX    0x002
#define CPU_FXSR   0x004
#define CPU_SSE    0x008
#define CPU_SSE2   0x010
#define CPU_SSE3   0x020
#define CPU_SSSE3  0x040
#define CPU_SSE41  0x080
#define CPU_SSE42  0x100
/* D381: AVX solo si el kernel lo activó (CR4.OSXSAVE y XCR0 con x87|SSE|AVX): entonces el estado
 * se guarda con XSAVE (los YMM enteros) en vez de FXSAVE. AVX2 y FMA solo se anotan si CPU_AVX. */
#define CPU_AVX    0x200
#define CPU_AVX2   0x400
#define CPU_FMA    0x800

/* D381: área por proceso. FXSAVE usa 512 bytes (el FNSAVE solo 108); XSAVE con x87|SSE|AVX en
 * el formato estándar, 512 + 64 de cabecera + 256 de las mitades altas de los YMM = 832. El
 * tamaño real lo da CPUID 0xD al arrancar: si no cabe aquí, el kernel no enciende AVX. XSAVE
 * exige el área alineada a 64 bytes. */
#define FPU_ESTADO        832
#define FPU_ESTADO_ALINHA 64

struct processo;

void        fpu_iniciar(void);                 /* CPUID, CR0/CR4, #NM; antes de los procesos */
bool        fpu_iniciar_secundaria(void);      /* D164: CR0/CR4 y FPU limpia en un secundario; false = no puede (D381: sin AVX) */
uint32_t    fpu_recursos(void);                /* CPU_* */
const char *cpu_fabricante(void);              /* "GenuineIntel", "AuthenticAMD"... */
int         fpu_descrever(char *s, int max);   /* "FPU MMX SSE SSE2 ... AVX AVX2 FMA" */
const char *fpu_avx_motivo(void);              /* D381: por qué AVX está o no está encendido (para el registro) */
void        fpu_trocar(struct processo *prox); /* planificador: activa CR0.TS si el estado no es del siguiente */
void        fpu_esquecer(struct processo *p);  /* el proceso terminó: su estado ya no vale */
void        fpu_guardar(struct processo *p);   /* D401: los registros de "p" a su área; el próximo #NM la carga */
uint32_t    fpu_formato(void);                 /* D401: SINAL_FPU_* (0 sin FPU, 1 FNSAVE, 2 FXSAVE, 3 XSAVE) */
void        fpu_area_sanear(uint8_t *area);    /* D401: área que viene del programa: sin #GP al cargarla */

/* SSE2 dentro del kernel (D042), como kernel_fpu_begin/end de Linux: guarda
 * los registros del proceso dueño, desactiva CR0.TS y las interrupciones; el fin
 * vuelve a activar el TS (el próximo #NM devuelve el estado al proceso). Pueden anidarse.
 * inicio devuelve false sin SSE2: entonces no llame al fin. Solo el código de
 * kernel/lib/sse2.c (compilado con -msse2) usa XMM, y solo aquí dentro. */
bool        fpu_kernel_inicio(void);
void        fpu_kernel_fim(void);
bool        fpu_kernel_ativo(void);            /* dentro de un inicio/fin */

#endif

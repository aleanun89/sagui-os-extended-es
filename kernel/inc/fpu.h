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

#define FPU_ESTADO 512   /* tamaño del área del FXSAVE (el FNSAVE usa solo 108) */

struct processo;

void        fpu_iniciar(void);                 /* CPUID, CR0/CR4, #NM; antes de los procesos */
void        fpu_iniciar_secundaria(void);      /* D164: CR0/CR4 y FPU limpia en un procesador secundario */
uint32_t    fpu_recursos(void);                /* CPU_* */
const char *cpu_fabricante(void);              /* "GenuineIntel", "AuthenticAMD"... */
int         fpu_descrever(char *s, int max);   /* "FPU MMX SSE SSE2 ..." */
void        fpu_trocar(struct processo *prox); /* planificador: activa CR0.TS si el estado no es del siguiente */
void        fpu_esquecer(struct processo *p);  /* el proceso terminó: su estado ya no vale */

/* SSE2 dentro del kernel (D042), como kernel_fpu_begin/end de Linux: guarda
 * los registros del proceso dueño, desactiva CR0.TS y las interrupciones; el fin
 * vuelve a activar el TS (el próximo #NM devuelve el estado al proceso). Pueden anidarse.
 * inicio devuelve false sin SSE2: entonces no llame al fin. Solo el código de
 * kernel/lib/sse2.c (compilado con -msse2) usa XMM, y solo aquí dentro. */
bool        fpu_kernel_inicio(void);
void        fpu_kernel_fim(void);
bool        fpu_kernel_ativo(void);            /* dentro de un inicio/fin */

#endif

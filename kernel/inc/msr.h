// SPDX-License-Identifier: GPL-2.0-or-later
/* msr.h - lectura protegida de MSR (D092). */
#ifndef SAGUI_MSR_H
#define SAGUI_MSR_H
#include "tipos.h"

void msr_iniciar(void);   /* registra el tratador del #GP (vector 13); justo después de la IDT */

/* Lee un MSR sin riesgo de tumbar el kernel: si el rdmsr genera #GP (MSR que
 * no existe en esta CPU o que el hipervisor no emula), el tratador salta la
 * instrucción y la función devuelve false. El MSR queda marcado como "no
 * soportado" y las próximas llamadas ni lo intentan. Nunca hay escritura. */
bool msr_ler_seguro(uint32_t msr, uint64_t *valor);
bool msr_marcado_ruim(uint32_t msr);      /* ya falló antes */
uint32_t msr_falhas(void);                /* cuántos #GP ya absorbió el tratador */

/* Escritura protegida (D106): solo para el driver de frecuencia. Un #GP en
 * el wrmsr se absorbe y devuelve false (el MSR no se marca: quien escribe
 * decide; el driver se desactiva). */
bool msr_escrever_seguro(uint32_t msr, uint64_t valor);

#endif

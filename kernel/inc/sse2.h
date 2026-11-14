// SPDX-License-Identifier: GPL-2.0-or-later
/* sse2.h - copias y rellenos con SSE2 (D042).
 *
 * El mismo código (kernel/lib/sse2.c, el único archivo compilado con -msse2)
 * sirve al kernel y a la libsagui. Solo puede llamarse:
 *   - en el kernel: entre fpu_kernel_inicio() y fpu_kernel_fim();
 *   - en los programas: después de comprobar que la CPU tiene SSE2 (cpu_tem_sse2()).
 * Nada aquí depende de otras cabeceras: solo tipos básicos de C. */
#ifndef SAGUI_SSE2_H
#define SAGUI_SSE2_H

/* Cualquier alineación; las escrituras quedan alineadas a 16 bytes. */
void sse2_copiar(void *d, const void *o, unsigned n);
/* Igual, con escrituras no temporales (movntdq + sfence): para la tarjeta de
 * vídeo o bloques grandes que no van a leerse pronto. */
void sse2_copiar_nt(void *d, const void *o, unsigned n);
/* De atrás hacia delante (memmove con el destino después del origen). */
void sse2_copiar_tras(void *d, const void *o, unsigned n);
/* n bytes con el byte v (memset). */
void sse2_memset(void *d, int v, unsigned n);
/* n píxeles de 32 bits con el color v; d alineado a 4 bytes. */
void sse2_preencher32(unsigned *d, unsigned v, unsigned n);
/* Amplía una línea 2x en horizontal: d[2i] = d[2i+1] = o[i], n píxeles de o;
 * si d2 no es NULL, escribe la misma línea en él (ampliar 2x también en vertical). */
void sse2_ampliar2x(unsigned *d, unsigned *d2, const unsigned *o, unsigned n);
/* d[i] = (d[i] >> desloc) & mascara: oscurecer píxeles (el Sagui 3D usa >> 2). */
void sse2_escurecer(unsigned *d, unsigned n, unsigned desloc, unsigned mascara);
/* n píxeles 0x00RRGGBB -> 16 bits RGB565 (r555 = 0) o RGB555 (r555 = 1), para
 * la tarjeta de vídeo en 16 o 15 bits (D078). */
void sse2_converter16(unsigned short *d, const unsigned *o, unsigned n, int r555);

/* Intercambia rojo y azul: 0xAARRGGBB <-> 0xAABBGGRR, n píxeles (D208). o y d pueden ser
 * el mismo bloque. La versión SSSE3 usa pshufb. */
void sse2_trocar_rb32(unsigned *d, const unsigned *o, unsigned n);
void ssse3_trocar_rb32(unsigned *d, const unsigned *o, unsigned n);
/* Mezcla constante de n píxeles de o sobre d: cada canal d = (o*a + d*(256-a)) >> 8, a de 0 a 256. */
void sse2_misturar32(unsigned *d, const unsigned *o, unsigned n, unsigned a);

/* Variantes SSE sin SSE2 (kernel/lib/sse1.c: movups/movaps/movntps y prefetch) y SSE3
 * (kernel/lib/sse3.c: lddqu para leer sin alinear). Mismas reglas que las SSE2 (D208). */
void sse1_copiar(void *d, const void *o, unsigned n);
void sse1_copiar_nt(void *d, const void *o, unsigned n);
void sse1_copiar_tras(void *d, const void *o, unsigned n);
void sse1_memset(void *d, int v, unsigned n);
void sse1_preencher32(unsigned *d, unsigned v, unsigned n);
void sse1_ampliar2x(unsigned *d, unsigned *d2, const unsigned *o, unsigned n);
void sse3_copiar(void *d, const void *o, unsigned n);
void sse3_copiar_nt(void *d, const void *o, unsigned n);

#endif

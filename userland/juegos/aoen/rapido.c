/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Rutas rápidas de píxeles. En SaguiOS usan las de la libsagui (rellenar, mezclar y oscurecer con SSE2, SSE3 o rep movs,
 * elegidas por CPUID y medidas en las primeras llamadas); en el PC o sin ellas, bucles en C. Siempre hay alternativa en C. */
#include "aoen.h"

#ifdef SAGUI_AOEN
#include "sagui.h"
int px_sse2(void) { return cpu_tem_sse2(); }
void px_fill(u32 *d, u32 col, int n) {
	if (n < 24) { while (n-- > 0) *d++ = col; return; }   /* un tramo corto no compensa la llamada */
	preencher32(d, col, (size_t)n);
}
void px_blend(u32 *d, const u32 *o, int n, int a) {
	if (a <= 0) return;
	if (n < 8) { px_blend_c(d, o, n, a); return; }
	misturar32(d, o, (size_t)n, (unsigned)(a > 256 ? 256 : a));
}
void px_dim(u32 *d, int n, int shift, u32 mask) {
	if (n < 8) { while (n-- > 0) { *d = ((*d >> shift) & mask) | 0xff000000u; ++d; } return; }
	escurecer32(d, (size_t)n, shift, mask);   /* el canal alfa se pierde: el búfer es XRGB (SDL_PIXELFORMAT_RGB888), no se usa */
}
#else
int px_sse2(void) { return 0; }
void px_fill(u32 *d, u32 col, int n) { while (n-- > 0) *d++ = col; }
void px_blend(u32 *d, const u32 *o, int n, int a) { px_blend_c(d, o, n, a); }
void px_dim(u32 *d, int n, int shift, u32 mask) { while (n-- > 0) { *d = ((*d >> shift) & mask) | 0xff000000u; ++d; } }
#endif

/* d = (o*a + d*(256-a)) >> 8 por canal, a de 0 a 256 */
void px_blend_c(u32 *d, const u32 *o, int n, int a) {
	int ia = 256 - a;
	for (int i = 0; i < n; ++i) {
		u32 x = o[i], y = d[i];
		u32 rb = (((x & 0xff00ffu) * (u32)a + (y & 0xff00ffu) * (u32)ia) >> 8) & 0xff00ffu;
		u32 g = (((x & 0x00ff00u) * (u32)a + (y & 0x00ff00u) * (u32)ia) >> 8) & 0x00ff00u;
		d[i] = 0xff000000u | rb | g;
	}
}

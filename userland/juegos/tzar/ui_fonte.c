/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_fonte.c - texto con las fuentes .FNT del original en el lienzo de la interfaz (D417, fase 4). Traducción a C de
 * font.rs de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later). La lectura del .FNT es la de dibujo.c. */
#include "ui.h"
#include <stdlib.h>
#include <string.h>

int tz_ui_fonte_medir(const TzUiFonte *f, const char *txt)
{
	int w = 0;
	if (!f->px) return 0;
	for (const unsigned char *p = (const unsigned char *)txt; *p; ++p) w += f->avanco[*p];
	return w;
}

void tz_ui_fonte_texto(TzTela *t, const TzUiFonte *f, int x, int y, const char *txt, uint32_t rgb)
{
	if (!f->px) return;
	for (const unsigned char *p = (const unsigned char *)txt; *p; ++p) {
		int g = f->glifo[*p];
		if (g >= f->nglifos) g = f->glifo['?'] < f->nglifos ? f->glifo['?'] : 0;
		for (int gy = 0; gy < f->alt; ++gy) {
			int py = y + gy;
			if (py < 0 || py >= t->h) continue;
			const uint8_t *s = f->px + ((size_t)g * f->alt + gy) * f->larg;
			for (int gx = 0; gx < f->larg; ++gx) {
				int px = x + gx;
				if (!s[gx] || px < 0 || px >= t->w) continue;
				uint32_t *d = &t->px[(size_t)py * t->w + px];
				unsigned a = (unsigned)s[gx] * 255 / (unsigned)f->niveis, ia = 255 - a;
				*d = 0xff000000u | ((((*d >> 16 & 255) * ia + (rgb >> 16 & 255) * a) / 255) << 16) |
				     ((((*d >> 8 & 255) * ia + (rgb >> 8 & 255) * a) / 255) << 8) | (((*d & 255) * ia + (rgb & 255) * a) / 255);
			}
		}
		x += f->avanco[*p];
	}
}

void tz_ui_fonte_caixa(TzTela *t, const TzUiFonte *f, TzRect r, const char *txt, uint32_t rgb, int centrado)
{
	char b[200];
	snprintf(b, sizeof b, "%s", txt);
	int w = tz_ui_fonte_medir(f, b);
	if (w > r.w) {   /* recorta con puntos suspensivos hasta que quepa */
		size_t l = strlen(b);
		while (l > 0) {
			b[--l] = 0;
			char c[210];
			snprintf(c, sizeof c, "%s...", b);
			if (tz_ui_fonte_medir(f, c) <= r.w || l == 0) { snprintf(b, sizeof b, "%s", c); break; }
		}
		w = tz_ui_fonte_medir(f, b);
	}
	int x = centrado ? r.x + (r.w - w) / 2 : r.x;
	int y = r.y + (r.h - f->alt) / 2;
	tz_ui_fonte_texto(t, f, x, y, b, rgb);
}

int tz_ui_fonte_quebrar(const TzUiFonte *f, const char *txt, int largura, char linha[][160], int max)
{
	int n = 0;
	char atual[160] = "";
	const char *p = txt;
	while (*p && n < max) {
		while (*p == ' ') ++p;
		const char *q = p;
		while (*q && *q != ' ' && *q != '\n') ++q;
		char palavra[100];
		size_t l = (size_t)(q - p);
		if (l >= sizeof palavra) l = sizeof palavra - 1;
		memcpy(palavra, p, l);
		palavra[l] = 0;
		char prova[260];
		snprintf(prova, sizeof prova, "%s%s%s", atual, atual[0] ? " " : "", palavra);
		if (atual[0] && tz_ui_fonte_medir(f, prova) > largura) { snprintf(linha[n++], 160, "%s", atual); atual[0] = 0; if (n >= max) break; snprintf(atual, sizeof atual, "%s", palavra); }
		else snprintf(atual, sizeof atual, "%s", prova);
		p = q;
		if (*p == '\n') { snprintf(linha[n++], 160, "%s", atual); atual[0] = 0; ++p; }
	}
	if (atual[0] && n < max) snprintf(linha[n++], 160, "%s", atual);
	return n;
}

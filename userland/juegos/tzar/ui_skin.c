/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ui_skin.c - recursos de la interfaz de Tzar: fuentes, textos (STRINGS.INI en el idioma de la instalación),
 * iconos y colores de los reinos (D417, fase 4). Carga el arte bajo demanda y lo guarda en una caché pequeña.
 * Los colores del minimapa y del jugador salen como en hud.rs de ReTzar (GPL-3.0-or-later). */
#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern const unsigned char fonte_sagui[256][16] __attribute__((weak));   /* la letra de 8x16 del sistema */

static int fonte_de(TzRecursos *r, const char *nome, TzUiFonte *f)
{
	uint32_t n;
	uint8_t *d = tz_recursos_ler(r, nome, &n);
	if (!d) return -1;
	int rc = tz_ui_fonte_ler(f, d, n);
	free(d);
	return rc;
}

/* fuente de respaldo con la letra del sistema (8x16) cuando no hay datos */
static int fonte_sistema(TzUiFonte *f)
{
	memset(f, 0, sizeof *f);
	if (!fonte_sagui) return tz_falha("sin fuente del sistema");
	f->alt = 16; f->larg = 8; f->nglifos = 256; f->niveis = 1;
	f->px = tz_malloc(256 * 16 * 8);
	if (!f->px) return -1;
	for (int g = 0; g < 256; ++g) {
		f->avanco[g] = 8; f->glifo[g] = (uint8_t)g;
		for (int r = 0; r < 16; ++r)
			for (int c = 0; c < 8; ++c) f->px[((size_t)g * 16 + r) * 8 + c] = (fonte_sagui[g][r] & (0x80 >> c)) ? 1 : 0;
	}
	return 0;
}

static const uint32_t cores_padrao[8] = { 0xc03030, 0x3050d0, 0x30b030, 0xd0c030, 0xc06020, 0x30c0c0, 0xa040c0, 0xb0b0b0 };

int tz_ui_skin_vazia(TzUiSkin *s)
{
	memset(s, 0, sizeof *s);
	for (int i = 0; i < 8; ++i) s->cores[i] = s->cores_mini[i] = cores_padrao[i];
	if (fonte_sistema(&s->f10)) return -1;
	s->f12b = s->f10; s->f14b = s->f10; s->f22 = s->f10; s->fpeq = s->f10;
	s->f12b.px = s->f14b.px = s->f22.px = s->fpeq.px = NULL;   /* una sola copia de los píxeles */
	s->f12b.px = s->f10.px; s->f14b.px = s->f10.px; s->f22.px = s->f10.px; s->fpeq.px = s->f10.px;
	s->ok = 0;
	return 0;
}

static void ler_cores(TzUiSkin *s)
{
	for (int i = 0; i < 8; ++i) s->cores[i] = s->cores_mini[i] = cores_padrao[i];
	uint32_t n;
	uint8_t *d = tz_recursos_ler(s->rec, "IMAGES/SCREENS/COMMON/COLOR BARS.BMP", &n);
	if (d) {
		TzImg im;
		if (!tz_bmp(d, n, 0, -1, &im)) {
			if (im.h >= 8)
				for (int i = 0; i < 8; ++i) s->cores[i] = im.px[(size_t)(im.h * (2 * i + 1) / 16) * im.w + im.w / 2] & 0xffffff;
			tz_img_liberar(&im);
		}
		free(d);
	}
	d = tz_recursos_ler(s->rec, "IMAGES/PALETTE.BMP", &n);
	if (d) {
		if (n > 60 && d[0] == 'B' && d[1] == 'M' && d[28] == 8) {
			uint32_t ini = 14 + tz_u32(d + 14);
			if ((uint64_t)ini + 1024 <= n)
				for (int slot = 0; slot < 8; ++slot) {
					const uint8_t *p = d + ini + (127 + slot * 16) * 4;
					unsigned r = p[2] >> 3, g = p[1] >> 3, b = p[0] >> 3;   /* RGB555 como la pantalla original */
					s->cores_mini[slot] = ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
				}
		}
		free(d);
	}
}

int tz_ui_skin_carregar(TzUiSkin *s, TzRecursos *r)
{
	memset(s, 0, sizeof *s);
	s->rec = r;
	if (fonte_de(r, "LOCAL/FONTS/ARIAL10_AA.FNT", &s->f10)) goto falha;
	if (fonte_de(r, "LOCAL/FONTS/ARIAL12B_AA.FNT", &s->f12b)) goto falha;
	if (fonte_de(r, "LOCAL/FONTS/ARIAL14B_AA.FNT", &s->f14b)) goto falha;
	if (fonte_de(r, "LOCAL/FONTS/CGB_22B.FNT", &s->f22)) goto falha;
	if (fonte_de(r, "LOCAL/FONTS/SMALL.FNT", &s->fpeq)) goto falha;
	static const char *const ini[] = { "LOCAL/STRINGS.INI", "LOCAL/STRINGS2.INI", "LOCAL/STRINGS3.INI" };
	for (int i = 0; i < 3; ++i) if (tz_ini_recurso(r, &s->textos, ini[i]) && i == 0) goto falha;
	if (tz_ini_recurso(r, &s->icones, "DATA/ICONS.INI")) goto falha;
	ler_cores(s);
	s->ok = 1;
	return 0;
falha:
	tz_ui_fonte_liberar(&s->f10); tz_ui_fonte_liberar(&s->f12b); tz_ui_fonte_liberar(&s->f14b); tz_ui_fonte_liberar(&s->f22); tz_ui_fonte_liberar(&s->fpeq);
	tz_ini_liberar(&s->textos); tz_ini_liberar(&s->icones);
	return -1;
}

void tz_ui_skin_liberar(TzUiSkin *s)
{
	for (int i = 0; i < s->ncache; ++i) tz_img_liberar(&s->cache[i].img);
	if (s->ok) {
		tz_ui_fonte_liberar(&s->f10); tz_ui_fonte_liberar(&s->f12b); tz_ui_fonte_liberar(&s->f14b); tz_ui_fonte_liberar(&s->f22); tz_ui_fonte_liberar(&s->fpeq);
	} else free(s->f10.px);   /* la skin vacía comparte los píxeles de una sola fuente */
	tz_ini_liberar(&s->textos); tz_ini_liberar(&s->icones);
	memset(s, 0, sizeof *s);
}

const TzImg *tz_ui_img(TzUiSkin *s, const char *caminho, int slot)
{
	if (!s->rec) return NULL;
	for (int i = 0; i < s->ncache; ++i)
		if (s->cache[i].slot == slot && !strcasecmp(s->cache[i].chave, caminho)) return s->cache[i].ok ? &s->cache[i].img : NULL;
	if (s->ncache == TZ_UI_CACHE) {   /* caché llena: se tira la más vieja */
		tz_img_liberar(&s->cache[0].img);
		memmove(&s->cache[0], &s->cache[1], (TZ_UI_CACHE - 1) * sizeof s->cache[0]);
		s->ncache--;
	}
	int k = s->ncache++;
	snprintf(s->cache[k].chave, sizeof s->cache[k].chave, "%s", caminho);
	s->cache[k].slot = slot;
	s->cache[k].ok = 0;
	char nome[160];
	snprintf(nome, sizeof nome, "IMAGES/%s.BMP", caminho);
	uint32_t n;
	uint8_t *d = tz_recursos_ler(s->rec, nome, &n);
	if (!d) return NULL;
	s->cache[k].ok = tz_bmp(d, n, 1, slot, &s->cache[k].img) == 0;
	free(d);
	return s->cache[k].ok ? &s->cache[k].img : NULL;
}

const char *tz_ui_txt(const TzUiSkin *s, const char *secao, const char *chave, const char *respaldo)
{
	const char *v = tz_ini(&s->textos, secao, chave);
	return v && *v ? v : respaldo;
}

int tz_ui_icone(TzUiSkin *s, const char *chave, int slot, const TzImg **img, TzRect *src)
{
	const char *im = tz_ini(&s->icones, chave, "image");
	if (!im) return 0;
	char c[160];
	snprintf(c, sizeof c, "%s", im);
	for (char *p = c; *p; ++p) if (*p == '\\') *p = '/';
	size_t l = strlen(c);
	if (l > 4 && !strcasecmp(c + l - 4, ".bmp")) c[l - 4] = 0;
	const TzImg *i = tz_ui_img(s, c, slot);
	if (!i && slot >= 0) i = tz_ui_img(s, c, -1);
	if (!i) return 0;
	const char *ix = tz_ini(&s->icones, chave, "index");
	int k = ix ? atoi(ix) : 0;
	if (k < 0 || (k + 1) * i->w > i->h) return 0;
	*img = i;
	*src = (TzRect){ 0, k * i->w, i->w, i->w };
	return 1;
}

int tz_ui_icone_classe(TzUiSkin *s, const TzBiblioteca *b, int classe, int civ, int slot, const TzImg **img, TzRect *src)
{
	if (!b || classe < 0 || classe >= TZ_NCLASSES) return 0;
	const TzIni *d = NULL;
	for (int i = 0; i < b->nobjdefs && !d; ++i) if (!strcmp(b->objclasse[i], tz_classes[classe])) d = &b->objdefs[i];
	if (!d) return 0;
	const char *sec = civ == 2 ? "arab" : civ == 3 ? "asia" : civ == 1 ? "euro" : "comm";
	const char *ic = tz_ini(d, sec, "icon");
	if (!ic || !*ic) ic = tz_ini(d, "euro", "icon");
	if (!ic || !*ic) ic = tz_ini(d, "comm", "icon");
	return ic && *ic ? tz_ui_icone(s, ic, slot, img, src) : 0;
}

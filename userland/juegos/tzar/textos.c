/* SPDX-License-Identifier: GPL-3.0-or-later */
/* textos.c - textos del juego (LOCAL/STRINGS*.INI) y fichas de objetos (D417, fase 3).
 *
 * Traducido de ReTzar, src/assets.rs (Ini::locale: STRINGS, STRINGS2 y STRINGS3 fusionados en ese orden) y
 * src/world.rs (class_label: «<clase> <e|r|a>», «<clase>», «<clase> e» en la sección [objects]; `\n` en los
 * valores son saltos de línea) (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 * La edición FX viene en castellano, en Windows-1252: SaguiOS usa Latin-1, igual salvo 0x80-0x9f, que se
 * sustituye por lo más parecido.
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>

int tz_textos_carregar(TzTextos *t, TzRecursos *r)
{
	memset(t, 0, sizeof *t);
	static const char *const nomes[] = { "LOCAL/STRINGS.INI", "LOCAL/STRINGS2.INI", "LOCAL/STRINGS3.INI" };
	int algum = 0;
	for (int i = 0; i < 3; ++i)
		if (tz_recursos_existe(r, nomes[i])) {
			if (tz_ini_recurso(r, &t->ini, nomes[i])) { tz_ini_liberar(&t->ini); memset(t, 0, sizeof *t); return -1; }
			algum = 1;
		}
	if (!algum) return tz_falha("la instalación no tiene LOCAL/STRINGS.INI");
	t->carregado = 1;
	return 0;
}

const char *tz_texto(const TzTextos *t, const char *secao, const char *chave)
{
	return t->carregado ? tz_ini(&t->ini, secao, chave) : NULL;
}

/* Windows-1252 0x80-0x9f: lo que Latin-1 no tiene se cambia por un signo parecido. */
static const char *const c1[32] = {
	"E", "", ",", "f", ",,", "...", "+", "+", "^", "%", "S", "<", "OE", "", "Z", "",
	"", "'", "'", "\"", "\"", "*", "-", "-", "~", "TM", "s", ">", "oe", "", "z", "Y",
};

void tz_cp1252_a_latin1(char *s)
{
	char *w = s;
	for (; *s; ++s) {
		unsigned char c = (unsigned char)*s;
		if (c >= 0x80 && c <= 0x9f) { for (const char *q = c1[c - 0x80]; *q; ++q) *w++ = *q; }
		else *w++ = (char)c;
	}
	*w = 0;
}

/* Los textos se guardan en Windows-1252; con los caracteres extra de c1 el resultado puede ser más largo: n manda. */
int tz_cp1252_a_utf8(const char *s, char *dst, size_t n)
{
	size_t o = 0;
	for (; *s; ++s) {
		unsigned c = (unsigned char)*s;
		if (c >= 0x80 && c <= 0x9f) {
			/* bloque C1 de la 1252: sus signos están fuera de Latin-1; aquí los de uso común */
			static const unsigned u[32] = { 0x20ac, '?', 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021, 0x2c6, 0x2030, 0x160, 0x2039, 0x152, '?', 0x17d, '?',
			                                '?', 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x2dc, 0x2122, 0x161, 0x203a, 0x153, '?', 0x17e, 0x178 };
			c = u[c - 0x80];
		}
		size_t k = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
		if (o + k + 1 > n) break;
		if (k == 1) dst[o++] = (char)c;
		else if (k == 2) { dst[o++] = (char)(0xc0 | c >> 6); dst[o++] = (char)(0x80 | (c & 63)); }
		else { dst[o++] = (char)(0xe0 | c >> 12); dst[o++] = (char)(0x80 | ((c >> 6) & 63)); dst[o++] = (char)(0x80 | (c & 63)); }
	}
	if (n) dst[o] = 0;
	return (int)o;
}

static void saltos(char *s)
{
	char *w = s;
	for (; *s; ++s) {
		if (s[0] == '\\' && s[1] == 'n') { *w++ = '\n'; ++s; }
		else if (s[0] == '\\' && s[1] == 'r' && s[2] == '\\' && s[3] == 'n') { *w++ = '\n'; s += 3; }
		else *w++ = *s;
	}
	*w = 0;
}

int tz_nome_objeto(const TzTextos *t, const TzObjeto *o, char *dst, size_t n)
{
	if (!n) return 0;
	dst[0] = 0;
	if (o->classe < 0 || o->classe >= 144) return 0;
	const char *classe = tz_classes[o->classe];
	const char *suf = o->civ == 2 ? "r" : o->civ == 3 ? "a" : "e";
	char k[3][100];
	snprintf(k[0], sizeof k[0], "%s %s", classe, suf);
	snprintf(k[1], sizeof k[1], "%s", classe);
	snprintf(k[2], sizeof k[2], "%s e", classe);
	const char *v = NULL;
	/* un objeto con dibujo propio puede tener su nombre en [images] */
	if (o->imagem[0]) v = tz_texto(t, "images", o->imagem);
	for (int i = 0; i < 3 && !v; ++i) v = tz_texto(t, "objects", k[i]);
	if (!v) v = classe;
	snprintf(dst, n, "%s", v);
	saltos(dst);
	tz_cp1252_a_latin1(dst);
	return (int)strlen(dst);
}

int tz_ficha_objeto(const TzTextos *t, const TzObjeto *o, char *dst, size_t n)
{
	char nome[160];
	tz_nome_objeto(t, o, nome, sizeof nome);
	if (o->vida_max > 0) snprintf(dst, n, "%s  %u/%u", nome, o->vida, o->vida_max);
	else snprintf(dst, n, "%s", nome);
	return (int)strlen(dst);
}

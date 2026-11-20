/* SPDX-License-Identifier: GPL-3.0-or-later */
/* guiao_expr.c - el lenguaje de expresiones de los guiones de misión de Tzar (D417, fase 7).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/script/expr.rs (0x4b7fc0 del
 * Tzar.exe). El texto se parte en «;», cada sentencia pasa a notación postfija y se evalúa desde el final. Todo son enteros
 * de 32 bits y valen las reglas del código nativo, rarezas incluidas: un operador solo saca a los de precedencia estrictamente
 * mayor (así «8-4-2» da 6), «&&» y «||» evalúan siempre los dos lados, dividir por cero da 0, un operando que falta lee 0 y
 * «&» y «|» ligan más que «+» y «-». */
#include "guiao_i.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

unsigned tz_expr_avisos;

static const char *const FUNCIONES[TZ_FN_N] = {
	"@gold", "@stone", "@food", "@wood", "@rnd", "@str", "@kingdomcanrescue", "@setai", "@select", "@unselect", "@clearselection",
	"@god", "@settime", "@gettime", "@min", "@max", "@getdiplomacy", "@isally", "@isneutral", "@isenemy", "@hassharedvision",
	"@hassharedunits", "@hassharedres", "@setpoplimit", "@build", "@setfood", "@setwood", "@setstone", "@setgold",
	"@setnoattacktime", "@count", "@inprogress",
};
static const char *const PROPRIEDADES[TZ_PR_N] = {
	"$health", "$maxhealth", "$attack", "$defence", "$level", "$manna", "$maxmanna", "$kingdom", "$race", "$x", "$y", "$cargo",
	"$isselected", "$inholder", "$program", "$flags", "$underfog",
};
static const char *const GLOBAIS[5] = { "$gold", "$stone", "$food", "$wood", "$difficulty" };

/* ------------------------------------------------------------------ variables */
static TzVar *tabela(TzVariaveis *v, const char *nome, int **n)
{
	if (nome[0] == '#') { *n = &v->nmantidas; return v->mantidas; }
	*n = &v->nmapa;
	return v->mapa;
}

static void chave63(char *dst, const char *nome)
{
	size_t l = strlen(nome);
	if (l > 63) l = 63;
	memcpy(dst, nome, l);
	dst[l] = 0;
}

int32_t tz_vars_get(TzVariaveis *v, const char *nome)
{
	char k[64];
	int *n;
	chave63(k, nome);
	TzVar *t = tabela(v, k, &n);
	for (int i = 0; i < *n; ++i) if (!strcmp(t[i].nome, k)) return t[i].valor;
	if (*n < 256) { memcpy(t[*n].nome, k, sizeof k); t[*n].valor = 0; (*n)++; }
	return 0;
}

void tz_vars_set(TzVariaveis *v, const char *nome, int32_t valor)
{
	char k[64];
	int *n;
	chave63(k, nome);
	TzVar *t = tabela(v, k, &n);
	for (int i = 0; i < *n; ++i) if (!strcmp(t[i].nome, k)) { t[i].valor = valor; return; }
	if (*n < 256) { memcpy(t[*n].nome, k, sizeof k); t[*n].valor = valor; (*n)++; }
}

/* ------------------------------------------------------------------ texto */
void tz_expr_sentencias(const char *src, char *dst, size_t n)
{
	size_t i = 0;
	for (; src[i] && i + 1 < n; ++i) dst[i] = (src[i] == '\r' || src[i] == '\n') ? ';' : src[i];
	dst[i] = 0;
}

void tz_expr_condicao(const char *src, char *dst, size_t n)
{
	size_t i = 0;
	for (; src[i] && i + 1 < n; ++i) dst[i] = (src[i] == '\r' || src[i] == '\n') ? ' ' : src[i];
	dst[i] = 0;
}

typedef struct {
	const TzExprOps *ops;
	void *ctx;
} Ctx;

typedef struct { uint8_t op; uint16_t code; int prec; const char *tok; int tlen; } Nodo;

static int32_t eval_texto(Ctx *c, const char *s, size_t n, int obj);

static int operando_car(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '$' || c == '#' || c == '_' || c == '@';
}

static int32_t atoi_nativo(const char *t)
{
	uint32_t v = 0;
	for (; *t >= '0' && *t <= '9'; ++t) v = v * 10u + (uint32_t)(*t - '0');
	return (int32_t)v;
}

/* los argumentos de primer nivel separados por comas (0x4b80e0); devuelve cuántos y deja (inicio, longitud) */
static int argumentos(const char *t, size_t n, const char *ini[8], size_t len[8])
{
	int cnt = 0, prof = 0;
	size_t start = 0;
	for (size_t i = 0; i < n; ++i) {
		char ch = t[i];
		if (ch == '(') prof++;
		else if (ch == ')' && prof > 0) prof--;
		else if (ch == ',' && prof == 0) {
			if (cnt < 8) { ini[cnt] = t + start; len[cnt] = i - start; cnt++; }
			start = i + 1;
		}
	}
	if (start < n && cnt < 8) { ini[cnt] = t + start; len[cnt] = n - start; cnt++; }
	return cnt;
}

static int32_t operando(Ctx *c, const char *tok, int obj)
{
	const TzExprOps *o = c->ops;
	size_t tl = strlen(tok);
	if (tok[0] >= '0' && tok[0] <= '9') return atoi_nativo(tok);
	if (tok[0] == '@') {
		const char *ap = strchr(tok, '(');
		size_t open = ap ? (size_t)(ap - tok) : tl;
		if (tl >= 10 && !strncasecmp(tok, "@playmovie", 10)) {
			char nome[128];
			size_t ini = open + 1, fim = tl ? tl - 1 : 0;
			size_t l = fim > ini ? fim - ini : 0;
			if (l > sizeof nome - 1) l = sizeof nome - 1;
			memcpy(nome, tok + (ini < tl ? ini : tl), l);
			nome[l] = 0;
			if (o->filme) o->filme(c->ctx, nome);
			return 0;
		}
		int fn = -1;
		for (int i = 0; i < TZ_FN_N; ++i) {
			size_t fl = strlen(FUNCIONES[i]);
			if (fl >= open && !strncasecmp(FUNCIONES[i], tok, open)) { fn = i; break; }
		}
		if (fn < 0) return tz_vars_get(o->vars(c->ctx), tok);
		int32_t a[8] = { 0 };
		if (open < tl) {
			const char *inner = tok + open + 1;
			size_t il = tl - open - 1;
			if (il && inner[il - 1] == ')') il--;
			const char *ini[8]; size_t len[8];
			int n = argumentos(inner, il, ini, len);
			for (int k = 0; k < n; ++k) a[k] = eval_texto(c, ini[k], len[k], obj);
		}
		return o->chamar(c->ctx, fn, a, obj);
	}
	if (obj >= 0) {
		for (int i = 0; i < TZ_PR_N; ++i)
			if (!strcasecmp(PROPRIEDADES[i], tok)) return o->propriedade(c->ctx, obj, i);
	}
	for (int i = 0; i < 5; ++i)
		if (!strcasecmp(GLOBAIS[i], tok)) return o->global(c->ctx, i);
	int v;
	if (o->evento && o->evento(c->ctx, tok, &v)) return v;
	return tz_vars_get(o->vars(c->ctx), tok);
}

/* 0x4b8f50: propiedades y globales con efecto; todo nombre se guarda además en su tabla de variables */
static void atribuir(Ctx *c, const char *nome, int32_t valor, int obj)
{
	const TzExprOps *o = c->ops;
	int feito = 0;
	if (obj >= 0) {
		/* escribibles: salud, salud máxima, ataque, defensa, nivel, maná, maná máximo */
		static const int ESCRIBIBLES[7] = { TZ_PR_HEALTH, TZ_PR_MAXHEALTH, TZ_PR_ATTACK, TZ_PR_DEFENCE, TZ_PR_LEVEL, TZ_PR_MANNA, TZ_PR_MAXMANNA };
		for (int k = 0; k < 7 && !feito; ++k)
			if (!strcasecmp(PROPRIEDADES[ESCRIBIBLES[k]], nome)) { o->definir_propriedade(c->ctx, obj, ESCRIBIBLES[k], valor); feito = 1; }
	}
	if (!feito) {
		for (int i = 0; i < 4 && !feito; ++i)
			if (!strcasecmp(GLOBAIS[i], nome)) { o->definir_global(c->ctx, i, valor); feito = 1; }
		if (!feito && !strcasecmp(nome, "$candelobjects")) o->definir_global(c->ctx, TZ_GL_CAN_DELETE, valor);
		else if (!feito && !strcasecmp(nome, "$canusediplomacy")) o->definir_global(c->ctx, TZ_GL_CAN_DIPLOMACY, valor);
	}
	tz_vars_set(o->vars(c->ctx), nome, valor);
}

static int32_t avaliar(Ctx *c, Nodo *v, int *n, int obj)
{
	for (;;) {
		if (*n <= 0) return 0;
		Nodo nd = v[--*n];
		if (!nd.op) {
			char buf[2100];
			int l = nd.tlen < (int)sizeof buf - 1 ? nd.tlen : (int)sizeof buf - 1;
			memcpy(buf, nd.tok, (size_t)l);
			buf[l] = 0;
			return operando(c, buf, obj);
		}
		unsigned code = nd.code;
		int32_t dir = avaliar(c, v, n, obj);
		if (code == '=') {
			if (*n > 0) {
				Nodo nm = v[--*n];
				if (!nm.op) {
					char buf[2100];
					int l = nm.tlen < (int)sizeof buf - 1 ? nm.tlen : (int)sizeof buf - 1;
					memcpy(buf, nm.tok, (size_t)l);
					buf[l] = 0;
					atribuir(c, buf, dir, obj);
				}
			}
			return dir;
		}
		if (code == ',') continue;   /* el lado derecho se tira y la evaluación sigue */
		if (code == '!') return dir == 0;
		if (code == '~') return ~dir;
		int32_t esq = avaliar(c, v, n, obj);
		switch (code) {
		case '%': return dir == 0 || dir == -1 ? 0 : esq % dir;
		case '/': return dir == 0 ? 0 : dir == -1 ? (int32_t)(0u - (uint32_t)esq) : esq / dir;
		case '*': return (int32_t)((uint32_t)esq * (uint32_t)dir);
		case '+': return (int32_t)((uint32_t)esq + (uint32_t)dir);
		case '-': return (int32_t)((uint32_t)esq - (uint32_t)dir);
		case '&': return esq & dir;
		case '|': return esq | dir;
		case '<': return esq < dir;
		case '>': return esq > dir;
		case 0x3d3c: return esq <= dir;
		case 0x3d3e: return esq >= dir;
		case 0x3d3d: return esq == dir;
		case 0x3d21: return esq != dir;
		case 0x2626: return esq != 0 && dir != 0;
		case 0x7c7c: return esq != 0 || dir != 0;
		default: tz_expr_avisos++; return 0;   /* «Expression: Unknown operator» */
		}
	}
}

/* 0x4b7ba0: de infija a postfija (patio de maniobras) y a evaluar */
static int32_t sentencia(Ctx *c, const char *s, size_t n, int obj)
{
	Nodo *out = malloc((n + 2) * sizeof *out * 2);
	if (!out) return 0;
	Nodo *pila = out + (n + 2);
	int nout = 0, npila = 0, nivel = 0;
	size_t at = 0;
	while (1) {
		while (at < n && s[at] == ' ') at++;
		if (at >= n) break;
		unsigned char ch = (unsigned char)s[at];
		unsigned par = at + 1 < n ? (unsigned)ch | ((unsigned)(unsigned char)s[at + 1] << 8) : 0;
		unsigned code = 0; int prec = 0, ancho = 0;
		if (at + 1 < n && (par == 0x3d3d || par == 0x3d21 || par == 0x3d3c || par == 0x3d3e)) { code = par; prec = 50; ancho = 2; }
		else if (at + 1 < n && par == 0x2626) { code = par; prec = 20; ancho = 2; }
		else if (at + 1 < n && par == 0x7c7c) { code = par; prec = 10; ancho = 2; }
		else switch (ch) {
		case '(': nivel += 100; at++; continue;
		case ')': nivel -= 100; at++; continue;
		case '!': case '~': code = ch; prec = 90; ancho = 1; break;
		case '%': case '*': case '/': code = ch; prec = 70; ancho = 1; break;
		case '&': case '|': code = ch; prec = 65; ancho = 1; break;
		case '+': case '-': code = ch; prec = 60; ancho = 1; break;
		case '<': case '>': code = ch; prec = 50; ancho = 1; break;
		case '=': code = ch; prec = 5; ancho = 1; break;
		case ',': code = ch; prec = 3; ancho = 1; break;
		default: break;
		}
		if (!ancho) {
			/* un operando: un nombre o un número; «@nombre(» se lleva los argumentos entre paréntesis */
			size_t ini = at;
			while (at < n && operando_car((unsigned char)s[at])) at++;
			if (ch == '@' && at < n && s[at] == '(') {
				int prof = 1;
				at++;
				while (prof > 0 && at < n) {
					if (s[at] == '(') prof++;
					else if (s[at] == ')') prof--;
					at++;
				}
			}
			if (at == ini) { at++; continue; }   /* el original no avanzaría con un carácter suelto */
			out[nout++] = (Nodo){ 0, 0, 0, s + ini, (int)(at - ini) };
			continue;
		}
		at += (size_t)ancho;
		prec += nivel;
		while (npila > 0 && pila[npila - 1].prec > prec) out[nout++] = pila[--npila];
		pila[npila++] = (Nodo){ 1, (uint16_t)code, prec, NULL, 0 };
	}
	int32_t r = 0;
	if (nivel != 0) { tz_expr_avisos++; free(out); return 0; }   /* «Brackets didn't match» */
	while (npila > 0) out[nout++] = pila[--npila];
	int idx = nout;
	r = avaliar(c, out, &idx, obj);
	free(out);
	return r;
}

static int32_t eval_texto(Ctx *c, const char *s, size_t n, int obj)
{
	if (n > 0x7ff) n = 0x7ff;   /* la copia nativa es de 0x800 bytes */
	int32_t valor = 0;
	const char *rest = s;
	size_t restn = n;
	for (;;) {
		const char *pc = memchr(rest, ';', restn);
		size_t sn = pc ? (size_t)(pc - rest) : restn;
		valor = sentencia(c, rest, sn, obj);
		if (!pc) break;
		size_t resto = restn - sn - 1;
		if (!resto) break;
		rest = pc + 1;
		restn = resto;
	}
	return valor;
}

int32_t tz_expr_eval(const TzExprOps *ops, void *ctx, const char *texto, int obj)
{
	Ctx c = { ops, ctx };
	return eval_texto(&c, texto, strlen(texto), obj);
}

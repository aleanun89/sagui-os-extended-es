/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ia_guiao.c - el lenguaje de los guiones de IA de Tzar (DATA\AI\*.ais y los *.ai que incluyen): la biblioteca de
 * ficheros y tipos y el compilador (D417, fase 6).
 *
 * Traducción a C de ReTzar (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later), src/gameplay/ai/script.rs: el
 * compilador del Tzar.exe (0x418b80; sentencias 0x418c60, símbolos 0x4196c0, condiciones 0x419ea0). El programa
 * conserva la forma del original (las condiciones, gatillos y reglas olvidadas de una regla son tramos de una lista
 * compartida) para que sus rarezas sigan igual: una cláusula repetida en una regla empieza otro tramo pero sigue
 * contando (0x419e40, 0x41a730, 0x41a850).
 */
#include "ia.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ utilidades */
static void *cresce(void *p, int *cap, int n, size_t tam)
{
	if (n < *cap) return p;
	int c = *cap ? *cap * 2 : 16;
	while (c <= n) c *= 2;
	void *q = realloc(p, (size_t)c * tam);
	if (!q) { free(p); *cap = 0; return NULL; }
	*cap = c;
	return q;
}

static int igual_sem_caixa(const char *a, const uint8_t *b, size_t nb)
{
	size_t na = strlen(a);
	if (na != nb) return 0;
	for (size_t i = 0; i < na; ++i)
		if (toupper((unsigned char)a[i]) != toupper(b[i])) return 0;
	return 1;
}

static char *copia_n(const uint8_t *s, size_t n)
{
	char *r = malloc(n + 1);
	if (!r) return NULL;
	memcpy(r, s, n);
	r[n] = 0;
	return r;
}

/* ------------------------------------------------------------------ biblioteca (script.rs Library) */
static void bib_tipo(TzIaBiblioteca *b, const char *nome)
{
	b->tipo = cresce(b->tipo, &b->captipos, b->ntipos, sizeof *b->tipo);
	if (b->tipo) b->tipo[b->ntipos++] = tz_strdup(nome);
}

void tz_ia_bib_ficheiro(TzIaBiblioteca *b, const char *nome, const uint8_t *dados, uint32_t n)
{
	char chave[256];
	tz_chave(chave, nome, sizeof chave);
	for (int i = 0; i < b->nfich; ++i)
		if (!strcmp(b->nome[i], chave)) {   /* el último gana */
			free(b->dados[i]);
			b->dados[i] = malloc(n ? n : 1);
			if (b->dados[i]) memcpy(b->dados[i], dados, n);
			b->tam[i] = n;
			return;
		}
	int cap = b->capfich;
	b->nome = cresce(b->nome, &cap, b->nfich, sizeof *b->nome);
	cap = b->capfich;
	b->dados = cresce(b->dados, &cap, b->nfich, sizeof *b->dados);
	cap = b->capfich;
	b->tam = cresce(b->tam, &cap, b->nfich, sizeof *b->tam);
	b->capfich = cap;
	if (!b->nome || !b->dados || !b->tam) return;
	b->nome[b->nfich] = tz_strdup(chave);
	b->dados[b->nfich] = malloc(n ? n : 1);
	if (b->dados[b->nfich]) memcpy(b->dados[b->nfich], dados, n);
	b->tam[b->nfich] = n;
	b->nfich++;
}

void tz_ia_bib_iniciar(TzIaBiblioteca *b, const char *const *extra, int nextra)
{
	memset(b, 0, sizeof *b);
	for (int i = 0; i < TZ_NCLASSES; ++i) bib_tipo(b, tz_classes[i]);
	for (int i = 0; i < nextra; ++i) bib_tipo(b, extra[i]);
	/* Library::new: las investigaciones son los tipos tras las clases que empiezan por «te_» */
	b->tecnologias = b->ntipos;
	for (int i = TZ_NCLASSES; i < b->ntipos; ++i)
		if (!strncmp(b->tipo[i], "te_", 3)) { b->tecnologias = i; break; }
}

int tz_ia_bib_carregar(TzIaBiblioteca *b, TzRecursos *r)
{
	tz_ia_bib_iniciar(b, NULL, 0);
	for (int i = 0; i < r->nnomes; ++i) {
		if (strncmp(r->nomes[i], "DATA/AI/", 8)) continue;
		uint32_t n = 0;
		uint8_t *d = tz_recursos_ler(r, r->nomes[i], &n);
		if (!d) { tz_ia_bib_liberar(b); return -1; }
		tz_ia_bib_ficheiro(b, r->nomes[i], d, n);
		free(d);
	}
	/* Library::load: las secciones de UPGRADES.INI y luego las de TECHNOLOGIES.INI (0x4975ad, 0x42b2d0) */
	static const char *const inis[2] = { "DATA/UPGRADES.INI", "DATA/TECHNOLOGIES.INI" };
	for (int j = 0; j < 2; ++j) {
		TzIni ini;
		memset(&ini, 0, sizeof ini);
		b->tecnologias = b->ntipos;
		if (tz_ini_recurso(r, &ini, inis[j])) { tz_ia_bib_liberar(b); return -1; }
		for (int s = 0; s < ini.ns; ++s) bib_tipo(b, ini.s[s].nome);
		tz_ini_liberar(&ini);
	}
	return 0;
}

void tz_ia_bib_liberar(TzIaBiblioteca *b)
{
	for (int i = 0; i < b->nfich; ++i) { free(b->nome[i]); free(b->dados[i]); }
	for (int i = 0; i < b->ntipos; ++i) free(b->tipo[i]);
	free(b->nome); free(b->dados); free(b->tam); free(b->tipo);
	memset(b, 0, sizeof *b);
}

/* 0x42b250: el tipo de un nombre sin distinguir mayúsculas */
int tz_ia_bib_tipo(const TzIaBiblioteca *b, const char *nome)
{
	for (int i = 0; i < b->ntipos; ++i)
		if (igual_sem_caixa(b->tipo[i], (const uint8_t *)nome, strlen(nome))) return i;
	return -1;
}

int tz_ia_bib_categoria(const TzIaBiblioteca *b, int k)
{
	if (k < 0) return 0;
	if (k < TZ_NCLASSES) {
		if (tz_classe_e_unidade(k)) return 1;
		if ((k >= 39 && k <= 73) || k == 133) return 2;   /* class::DUMMY */
		return 0;
	}
	return k >= b->tecnologias && k < b->ntipos ? 3 : 0;
}

static int bib_achar(const TzIaBiblioteca *b, const char *chave)
{
	for (int i = 0; i < b->nfich; ++i)
		if (!strcmp(b->nome[i], chave)) return i;
	return -1;
}

/* ------------------------------------------------------------------ símbolos (0x4196c0) */
static const char *const PALAVRAS[24] = {
	"RULE", "CRITICAL", "WHEN", "HAVE", "TRIGGER", "FORGET", "THIS", "END", "TIME", "RANDOM", "EXTERN", "INCLUDE",
	"NOT", "CIV", "EUROPEAN", "ARABIAN", "ASIAN", "DIFFICULTY", "EASY", "NORMAL", "HARD", "SAY", "ENEMY", "ALLY",
};
enum {
	K_RULE = 1, K_CRITICAL, K_WHEN, K_HAVE, K_TRIGGER, K_FORGET, K_THIS, K_END, K_TIME, K_RANDOM, K_EXTERN,
	K_INCLUDE, K_NOT, K_CIV, K_EUROPEAN, K_ARABIAN, K_ASIAN, K_DIFFICULTY, K_EASY, K_NORMAL, K_HARD, K_SAY,
	K_ENEMY, K_ALLY,
};
static const char *const METAS[7] = { "convert", "gold", "stone", "food", "wood", "needs_locked", "resign" };
static const char *const RECURSOS[4] = { "gold", "stone", "food", "wood" };   /* 0x58105c */
#define GRUPOS 32           /* anidamiento de grupos que guarda la pila del original (+0x846) */
#define INCLUSOES 32        /* de ReTzar: un fichero que se incluye a sí mismo */

enum { S_FIM = 0, S_SIMBOLO, S_NUMERO, S_PALAVRA };
typedef struct {
	int tipo;
	uint8_t simbolo;
	uint32_t n;
	int chave;                    /* palabra clave (0 = ninguna) */
	int ini, fim;
} Simb;

typedef struct {
	const char *ruta;
	const uint8_t *cru; int ncru;  /* el texto tal cual, para los mensajes */
	uint8_t *txt; int ntxt;        /* en mayúsculas (0x519de0) hasta el primer NUL */
	int pos, marca;
} Fonte;

static uint8_t em(const Fonte *s, int i) { return i >= 0 && i < s->ntxt ? s->txt[i] : 0; }

/* 0x419850: blancos y comentarios con «;» */
static void brancos(Fonte *s)
{
	for (;;) {
		uint8_t c = em(s, s->pos);
		if (c == '\t' || c == '\n' || c == '\r' || c == ' ') s->pos++;
		else if (c == ';') { while (em(s, s->pos) && em(s, s->pos) != '\n' && em(s, s->pos) != '\r') s->pos++; }
		else break;
	}
	s->marca = s->pos;
}

static const char *simbolo(Fonte *s, Simb *t)
{
	brancos(s);
	memset(t, 0, sizeof *t);
	uint8_t c = em(s, s->pos);
	if (!c) { t->tipo = S_FIM; return NULL; }
	if (c == ',' || c == '{' || c == '}') { s->pos++; t->tipo = S_SIMBOLO; t->simbolo = c; return NULL; }
	if (c >= '0' && c <= '9') {
		uint32_t n = 0;
		while (em(s, s->pos) >= '0' && em(s, s->pos) <= '9') n = n * 10u + (uint32_t)(em(s, s->pos++) - '0');
		if (em(s, s->pos) == '%') { s->pos++; n = (n < 0x7fff ? n : 0x7fff) | TZ_IA_PCT; }
		t->tipo = S_NUMERO; t->n = n;
		return NULL;
	}
	if ((c >= 'A' && c <= 'Z') || c == '_') {
		int ini = s->pos;
		for (;;) {
			uint8_t d = em(s, s->pos);
			if (!((d >= 'A' && d <= 'Z') || d == '_' || (d >= '0' && d <= '9'))) break;
			s->pos++;
		}
		t->tipo = S_PALAVRA; t->ini = ini; t->fim = s->pos;
		for (int k = 0; k < 24; ++k)
			if ((int)strlen(PALAVRAS[k]) == s->pos - ini && !memcmp(PALAVRAS[k], s->txt + ini, (size_t)(s->pos - ini))) {
				t->chave = k + 1;
				break;
			}
		return NULL;
	}
	return "Unrecognized token";
}

/* un texto entre comillas en una línea (0x419a00, 0x419f7d, 0x41a970) */
static const char *aspas(Fonte *s, int *ini, int *fim)
{
	brancos(s);
	if (em(s, s->pos) != '"') return "Quote (\") expected";
	s->pos++;
	*ini = s->pos;
	while (em(s, s->pos) && em(s, s->pos) != '"' && em(s, s->pos) != '\r' && em(s, s->pos) != '\n') s->pos++;
	if (em(s, s->pos) != '"') return "Quote (\") expected";
	*fim = s->pos;
	s->pos++;
	return NULL;
}

static void erro_fonte(const Fonte *s, const char *msg, TzIaErro *e)
{
	int at = s->marca < s->ncru ? s->marca : s->ncru;
	int ini = at;
	while (ini > 0 && s->cru[ini - 1] != '\n' && s->cru[ini - 1] != '\r') ini--;
	int fim = at;
	while (fim < s->ncru && s->cru[fim] != '\n' && s->cru[fim] != '\r') fim++;
	int nl = 0, cr = 0;
	for (int i = 0; i < ini; ++i) { if (s->cru[i] == '\n') nl++; if (s->cru[i] == '\r') cr++; }
	memset(e, 0, sizeof *e);
	snprintf(e->ficheiro, sizeof e->ficheiro, "%s", s->ruta);
	e->linha = 1 + (nl > cr ? nl : cr);
	e->coluna = at - ini + 1;
	e->mensagem = msg;
	/* la línea sin blancos a los lados */
	while (ini < fim && (s->cru[ini] == ' ' || s->cru[ini] == '\t')) ini++;
	while (fim > ini && (s->cru[fim - 1] == ' ' || s->cru[fim - 1] == '\t')) fim--;
	int n = fim - ini;
	if (n > (int)sizeof e->texto - 1) n = (int)sizeof e->texto - 1;
	memcpy(e->texto, s->cru + ini, (size_t)(n > 0 ? n : 0));
}

/* ------------------------------------------------------------------ compilador (0x418c60) */
typedef struct { char *nome; int gatilho; int cond; int lista; } Pendente;   /* 14 bytes en +0x8ca */

typedef struct {
	const TzIaBiblioteca *b;
	TzIaPrograma *p;
	char **nomes; int nnomes, capnomes;   /* nombres de regla (+0x8d2), "" sin nombre, uno por regla o marca */
	Pendente *pend; int npend, cappend;
	uint32_t grupos[GRUPOS]; int ngrupos;
	int prof;
	int sem_memoria;
} Comp;

static void push_lista(Comp *c, uint32_t v)
{
	TzIaPrograma *p = c->p;
	p->lista = cresce(p->lista, &p->caplista, p->nlista, sizeof *p->lista);
	if (!p->lista) { c->sem_memoria = 1; p->nlista = 0; return; }
	p->lista[p->nlista++] = v;
}

static void push_nome(Comp *c, const uint8_t *s, int n)
{
	c->nomes = cresce(c->nomes, &c->capnomes, c->nnomes, sizeof *c->nomes);
	if (!c->nomes) { c->sem_memoria = 1; c->nnomes = 0; return; }
	c->nomes[c->nnomes++] = copia_n(s, (size_t)n);
}

static void push_regra(Comp *c, const TzIaRegra *r)
{
	TzIaPrograma *p = c->p;
	p->regra = cresce(p->regra, &p->capregras, p->nregras, sizeof *p->regra);
	if (!p->regra) { c->sem_memoria = 1; p->nregras = 0; return; }
	p->regra[p->nregras++] = *r;
}

static int indice_regra(const Comp *c, const uint8_t *s, int n)
{
	for (int i = 0; i < c->nnomes; ++i)
		if (c->nomes[i] && (int)strlen(c->nomes[i]) == n && !memcmp(c->nomes[i], s, (size_t)n)) return i;
	return -1;
}

static int indice_gatilho(const Comp *c, const uint8_t *s, int n)
{
	for (int i = 0; i < c->p->ngat; ++i)
		if ((int)strlen(c->p->gat[i].nome) == n && !memcmp(c->p->gat[i].nome, s, (size_t)n)) return i;
	return -1;
}

/* 0x419540 y 0x419600: un nombre ya conocido, o anotado para el final */
static uint32_t referencia(Comp *c, const uint8_t *s, int n, int gatilho, int cond, int lista)
{
	int i = gatilho ? indice_gatilho(c, s, n) : indice_regra(c, s, n);
	if (i >= 0) return (uint32_t)i;
	c->pend = cresce(c->pend, &c->cappend, c->npend, sizeof *c->pend);
	if (!c->pend) { c->sem_memoria = 1; c->npend = 0; return 0xffffffffu; }
	c->pend[c->npend].nome = copia_n(s, (size_t)n);
	c->pend[c->npend].gatilho = gatilho;
	c->pend[c->npend].cond = cond;
	c->pend[c->npend].lista = lista;
	c->npend++;
	return 0xffffffffu;
}

/* el tipo de un nombre, o del nombre sin la S final */
static int tipo_de(const Comp *c, const uint8_t *w, int n)
{
	for (int i = 0; i < c->b->ntipos; ++i)
		if (igual_sem_caixa(c->b->tipo[i], w, (size_t)n)) return i;
	if (n > 0 && w[n - 1] == 'S')
		for (int i = 0; i < c->b->ntipos; ++i)
			if (igual_sem_caixa(c->b->tipo[i], w, (size_t)(n - 1))) return i;
	return -1;
}

static int achar_tabela(const char *const *t, int nt, const uint8_t *w, int n)
{
	for (int i = 0; i < nt; ++i)
		if (igual_sem_caixa(t[i], w, (size_t)n)) return i;
	return -1;
}

static void add_cond(Comp *c, TzIaRegra *r, TzIaCond cd)
{
	TzIaPrograma *p = c->p;
	uint32_t i = (uint32_t)p->nconds;
	p->cond = cresce(p->cond, &p->capconds, p->nconds, sizeof *p->cond);
	if (!p->cond) { c->sem_memoria = 1; p->nconds = 0; return; }
	p->cond[p->nconds++] = cd;
	push_lista(c, i);
	r->quando.n++;
}

/* 0x4198a0 */
static const char *abrir(Comp *c)
{
	if (c->ngrupos >= GRUPOS) return "Too deeep group nesting";
	push_nome(c, (const uint8_t *)"", 0);
	c->grupos[c->ngrupos++] = (uint32_t)c->p->nregras;
	TzIaRegra r;
	memset(&r, 0, sizeof r);
	r.flags = TZ_IA_R_GRUPO_INI;
	push_regra(c, &r);
	return NULL;
}

/* 0x419940: un grupo vacío desaparece (ReTzar quita también su nombre, que el original deja) */
static const char *fechar(Comp *c)
{
	if (!c->ngrupos) return "Unexpected \"}\"";
	uint32_t ini = c->grupos[--c->ngrupos];
	if ((int)ini + 1 == c->p->nregras) {
		c->p->nregras--;
		if (c->nnomes) free(c->nomes[--c->nnomes]);
		return NULL;
	}
	push_nome(c, (const uint8_t *)"", 0);
	uint32_t fim = (uint32_t)c->p->nregras;
	TzIaRegra r;
	memset(&r, 0, sizeof r);
	r.flags = TZ_IA_R_GRUPO_FIM;
	push_regra(c, &r);
	if (!c->sem_memoria) c->p->regra[ini].fim = fim;
	return NULL;
}

/* 0x419ea0: una condición; deja en *t el símbolo que la sigue */
static const char *condicao(Comp *c, Fonte *s, TzIaRegra *r, Simb *t)
{
	Simb w;
	const char *err = simbolo(s, &w);
	if (err) return err;
	if (w.tipo != S_PALAVRA) return "Identifier expected";
	enum { A_NUMERO, A_PALAVRA } arg;
	int opcional = 0;
	TzIaCond cd;
	memset(&cd, 0, sizeof cd);
	const uint8_t *pal = s->txt + w.ini;
	int npal = w.fim - w.ini;
	switch (w.chave) {
	case 0:
		if (igual_sem_caixa("units", pal, (size_t)npal)) { cd.tipo = TZ_IA_C_CONTA; cd.classe = 0; arg = A_NUMERO; opcional = 1; }
		else {
			int rr = achar_tabela(RECURSOS, 4, pal, npal);
			if (rr >= 0) { cd.tipo = TZ_IA_C_RECURSO; cd.recurso = (uint8_t)rr; arg = A_NUMERO; }
			else {
				int k = tipo_de(c, pal, npal);
				if (k < 0) return "Bad condition";
				cd.tipo = TZ_IA_C_CONTA; cd.classe = (uint16_t)k; arg = A_NUMERO; opcional = 1;
			}
		}
		break;
	case K_RULE: cd.tipo = TZ_IA_C_REGRA; arg = A_PALAVRA; break;
	case K_TRIGGER: cd.tipo = TZ_IA_C_GATILHO; arg = A_PALAVRA; break;
	case K_TIME: cd.tipo = TZ_IA_C_TEMPO; arg = A_NUMERO; break;
	case K_RANDOM: cd.tipo = TZ_IA_C_ACASO; arg = A_NUMERO; break;
	case K_EXTERN: {
		int a, b;
		if ((err = aspas(s, &a, &b)) != NULL) return err;
		TzIaPrograma *p = c->p;
		p->texto = cresce(p->texto, &p->captextos, p->ntextos, sizeof *p->texto);
		if (!p->texto) { c->sem_memoria = 1; p->ntextos = 0; return "Out of memory"; }
		cd.tipo = TZ_IA_C_EXTERNO;
		cd.n = (uint32_t)p->ntextos;
		p->texto[p->ntextos++] = copia_n(s->txt + a, (size_t)(b - a));
		add_cond(c, r, cd);
		return simbolo(s, t);
	}
	case K_NOT: {
		if ((err = condicao(c, s, r, t)) != NULL) return err;
		TzIaCond nao;
		memset(&nao, 0, sizeof nao);
		nao.tipo = TZ_IA_C_NAO;
		nao.n = (uint32_t)c->p->nconds - 1;
		uint32_t at = (uint32_t)c->p->nconds;
		c->p->cond = cresce(c->p->cond, &c->p->capconds, c->p->nconds, sizeof *c->p->cond);
		if (!c->p->cond) { c->sem_memoria = 1; c->p->nconds = 0; return "Out of memory"; }
		c->p->cond[c->p->nconds++] = nao;
		/* la última entrada del tramo pasa a ser la negación */
		uint32_t slot = r->quando.ini + r->quando.n - 1;
		if (slot < (uint32_t)c->p->nlista) c->p->lista[slot] = at;
		return NULL;
	}
	case K_CIV: cd.tipo = TZ_IA_C_CIV; arg = A_PALAVRA; break;
	case K_DIFFICULTY: cd.tipo = TZ_IA_C_DIFICULDADE; arg = A_PALAVRA; break;
	case K_ENEMY: case K_ALLY: {
		Simb u;
		if ((err = simbolo(s, &u)) != NULL) return err;
		if (u.tipo != S_PALAVRA) return "Identifier expected";
		if (u.chave) return "Unexpected keyword";
		int k;
		if (igual_sem_caixa("units", s->txt + u.ini, (size_t)(u.fim - u.ini))) k = 0;
		else if ((k = tipo_de(c, s->txt + u.ini, u.fim - u.ini)) < 0) return "Bad condition";
		cd.tipo = w.chave == K_ENEMY ? TZ_IA_C_INIMIGO : TZ_IA_C_ALIADO;
		cd.classe = (uint16_t)k;
		arg = A_NUMERO; opcional = 1;
		break;
	}
	default:
		return "Bad condition";
	}
	int volta = s->pos;
	Simb a;
	if ((err = simbolo(s, &a)) != NULL) return err;
	int cabe = arg == A_NUMERO ? a.tipo == S_NUMERO : a.tipo == S_PALAVRA;
	if (!cabe) {
		if (!opcional) return arg == A_PALAVRA ? "Identifier expected" : "Number expected";
		s->pos = volta;
		memset(&a, 0, sizeof a);
		a.tipo = S_NUMERO; a.n = 1;
	}
	int at = c->p->nconds;
	if (a.tipo == S_NUMERO) {
		uint32_t n = a.n;
		if (n & TZ_IA_PCT) {
			if (cd.tipo == TZ_IA_C_RECURSO) return "Number expected";
			if (cd.tipo == TZ_IA_C_ACASO) n &= ~TZ_IA_PCT;
		}
		if (cd.tipo == TZ_IA_C_RECURSO || cd.tipo == TZ_IA_C_CONTA || cd.tipo == TZ_IA_C_INIMIGO ||
		    cd.tipo == TZ_IA_C_ALIADO || cd.tipo == TZ_IA_C_TEMPO || cd.tipo == TZ_IA_C_ACASO)
			cd.n = n;
	} else if (cd.tipo == TZ_IA_C_REGRA) {
		cd.n = referencia(c, s->txt + a.ini, a.fim - a.ini, 0, at, -1);
	} else if (cd.tipo == TZ_IA_C_GATILHO) {
		cd.n = referencia(c, s->txt + a.ini, a.fim - a.ini, 1, at, -1);
	} else if (cd.tipo == TZ_IA_C_CIV) {
		if (a.chave < K_EUROPEAN || a.chave > K_ASIAN) return "Invalid civilization";
		cd.recurso = (uint8_t)(a.chave - K_EUROPEAN + 1);
	} else if (cd.tipo == TZ_IA_C_DIFICULDADE) {
		if (a.chave < K_EASY || a.chave > K_HARD) return "Invalid difficulty";
		cd.recurso = (uint8_t)(a.chave - K_EASY);
	}
	if (cd.tipo == TZ_IA_C_REGRA && cd.n == (uint32_t)c->p->nregras) return "Recursed rule";
	add_cond(c, r, cd);
	return simbolo(s, t);
}

/* 0x419e40: condiciones hasta una palabra clave; lo demás que sigue a una se toma como separador */
static const char *quando(Comp *c, Fonte *s, TzIaRegra *r, Simb *t)
{
	r->quando.ini = (uint32_t)c->p->nlista;
	for (;;) {
		const char *err = condicao(c, s, r, t);
		if (err) return err;
		if (t->tipo == S_PALAVRA && t->chave) return NULL;
	}
}

/* 0x41a5c0 */
static const char *ter(Comp *c, Fonte *s, TzIaRegra *r, Simb *t)
{
	Simb w;
	const char *err = simbolo(s, &w);
	if (err) return err;
	uint16_t n = 1;
	if (w.tipo == S_NUMERO) {
		n = (uint16_t)w.n;
		if ((err = simbolo(s, &w)) != NULL) return err;
	}
	if (w.tipo != S_PALAVRA) return "Identifier expected";
	if (w.chave) return "Unexpected keyword";
	const uint8_t *pal = s->txt + w.ini;
	int npal = w.fim - w.ini;
	int g = achar_tabela(METAS, 7, pal, npal);
	int k;
	if (g >= 0) k = TZ_IA_M_CONVERTER - g;
	else {
		/* el tipo 0 («none») no es meta */
		k = -1;
		for (int i = 1; i < c->b->ntipos; ++i)
			if (igual_sem_caixa(c->b->tipo[i], pal, (size_t)npal)) { k = i; break; }
		if (k < 0 && npal > 0 && pal[npal - 1] == 'S')
			for (int i = 1; i < c->b->ntipos; ++i)
				if (igual_sem_caixa(c->b->tipo[i], pal, (size_t)(npal - 1))) { k = i; break; }
		if (k < 0) return "Invalid type";
	}
	r->tem_meta = 1;
	r->meta_tipo = (uint16_t)k;
	r->meta_n = n;
	return simbolo(s, t);
}

/* 0x41a730 */
static const char *gatilhos(Comp *c, Fonte *s, TzIaRegra *r, Simb *t)
{
	r->gatilhos.ini = (uint32_t)c->p->nlista;
	for (;;) {
		brancos(s);
		int apaga = em(s, s->pos) == '!';
		if (apaga) s->pos++;
		Simb w;
		const char *err = simbolo(s, &w);
		if (err) return err;
		if (w.tipo != S_PALAVRA) return "Identifier expected";
		if (w.chave) return "Unexpected keyword";
		int i = indice_gatilho(c, s->txt + w.ini, w.fim - w.ini);
		if (i < 0) {
			TzIaPrograma *p = c->p;
			p->gat = cresce(p->gat, &p->capgat, p->ngat, sizeof *p->gat);
			if (!p->gat) { c->sem_memoria = 1; p->ngat = 0; return "Out of memory"; }
			p->gat[p->ngat].nome = copia_n(s->txt + w.ini, (size_t)(w.fim - w.ini));
			p->gat[p->ngat].ligado = 0;
			i = p->ngat++;
		}
		push_lista(c, (uint32_t)i | (apaga ? 0x80000000u : 0));
		r->gatilhos.n++;
		if ((err = simbolo(s, t)) != NULL) return err;
		if (!(t->tipo == S_SIMBOLO && t->simbolo == ',')) return NULL;
	}
}

/* 0x41a850 */
static const char *esquece(Comp *c, Fonte *s, TzIaRegra *r, Simb *t)
{
	r->esquece.ini = (uint32_t)c->p->nlista;
	const char *err = simbolo(s, t);
	if (err) return err;
	for (;;) {
		if (t->tipo != S_PALAVRA) return "Identifier expected";
		uint32_t i;
		if (t->chave == K_THIS) i = (uint32_t)c->p->nregras;
		else if (t->chave) return "Unexpected keyword";
		else i = referencia(c, s->txt + t->ini, t->fim - t->ini, 0, -1, c->p->nlista);
		push_lista(c, i);
		r->esquece.n++;
		if ((err = simbolo(s, t)) != NULL) return err;
		if (!(t->tipo == S_SIMBOLO && t->simbolo == ',')) return NULL;
		if ((err = simbolo(s, t)) != NULL) return err;
	}
}

static void regra_liberar(TzIaRegra *r) { free(r->nome); free(r->diz); r->nome = r->diz = NULL; }

/* 0x419b70 */
static const char *regra(Comp *c, Fonte *s)
{
	TzIaRegra r;
	memset(&r, 0, sizeof r);
	r.flags = TZ_IA_R_SUJA;
	Simb t;
	const char *err = simbolo(s, &t);
	if (err) return err;
	if (t.tipo == S_PALAVRA && !t.chave) {
		const uint8_t *pal = s->txt + t.ini;
		int n = t.fim - t.ini;
		if (indice_regra(c, pal, n) >= 0) return "Rule name already used";
		r.nome = copia_n(pal, (size_t)n);
		push_nome(c, pal, n);
		if ((err = simbolo(s, &t)) != NULL) { regra_liberar(&r); return err; }
		if (!(t.tipo == S_PALAVRA && t.chave)) { regra_liberar(&r); return "Keyword expected"; }
	} else if (t.tipo == S_PALAVRA) {
		push_nome(c, (const uint8_t *)"", 0);
	} else {
		return "Identifier expected";
	}
	for (;;) {
		switch (t.chave) {
		case K_END:
			push_regra(c, &r);
			return NULL;
		case K_WHEN: err = quando(c, s, &r, &t); break;
		case K_HAVE: err = ter(c, s, &r, &t); break;
		case K_TRIGGER: err = gatilhos(c, s, &r, &t); break;
		case K_FORGET: err = esquece(c, s, &r, &t); break;
		case K_SAY: {
			int a, b;
			err = aspas(s, &a, &b);
			if (!err) {
				free(r.diz);
				r.diz = copia_n(s->txt + a, (size_t)(b - a));
				err = simbolo(s, &t);
			}
			break;
		}
		default:
			err = "Statement expected";
		}
		if (err) { regra_liberar(&r); return err; }
		if (!(t.tipo == S_PALAVRA && t.chave)) { regra_liberar(&r); return "Keyword expected"; }
	}
}

/* 0x419af0 */
static const char *critica(Comp *c, Fonte *s)
{
	Simb t;
	const char *err = simbolo(s, &t);
	if (err) return err;
	if (!(t.tipo == S_PALAVRA && t.chave == K_RULE)) return "RULE expected";
	if ((err = regra(c, s)) != NULL) return err;
	if (c->p->nregras) c->p->regra[c->p->nregras - 1].flags |= TZ_IA_R_CRITICA;
	return NULL;
}

static int ficheiro(Comp *c, const char *ruta, TzIaErro *e);

/* 0x419a00: el fichero junto al actual; su error nombra también esta línea */
static int incluir(Comp *c, Fonte *s, TzIaErro *e)
{
	int a, b;
	const char *err = aspas(s, &a, &b);
	if (err) { erro_fonte(s, err, e); return -1; }
	if (c->prof >= INCLUSOES) { erro_fonte(s, "Include nesting too deep", e); return -1; }
	char ruta[256];
	int pasta = 0;
	for (int i = 0; s->ruta[i]; ++i) if (s->ruta[i] == '\\' || s->ruta[i] == '/') pasta = i + 1;
	int n = b - a;
	if (pasta + n >= (int)sizeof ruta) { erro_fonte(s, "File not found", e); return -1; }
	memcpy(ruta, s->ruta, (size_t)pasta);
	memcpy(ruta + pasta, s->txt + a, (size_t)n);
	ruta[pasta + n] = 0;
	c->prof++;
	int r = ficheiro(c, ruta, e);
	c->prof--;
	if (r < 0) {
		TzIaErro aqui;
		erro_fonte(s, "", &aqui);
		size_t l = strlen(e->incluido);
		snprintf(e->incluido + l, sizeof e->incluido - l, " (incluido desde %s:%d)", s->ruta, aqui.linha);
		return -1;
	}
	return 0;
}

/* 0x418c60 para un fichero; 0 si está vacío, 1 si se leyó, -1 si hay error */
static int ficheiro(Comp *c, const char *ruta, TzIaErro *e)
{
	char chave[256];
	tz_chave(chave, ruta, sizeof chave);
	int i = bib_achar(c->b, chave);
	if (i < 0) {
		memset(e, 0, sizeof *e);
		snprintf(e->ficheiro, sizeof e->ficheiro, "%s", ruta);
		e->mensagem = "File not found";
		return -1;
	}
	if (!c->b->tam[i]) return 0;
	Fonte s;
	memset(&s, 0, sizeof s);
	s.ruta = ruta;
	s.cru = c->b->dados[i];
	s.ncru = (int)c->b->tam[i];
	s.txt = malloc((size_t)s.ncru + 1);
	if (!s.txt) { memset(e, 0, sizeof *e); e->mensagem = "Out of memory"; return -1; }
	for (int j = 0; j < s.ncru; ++j) {
		if (!s.cru[j]) break;
		s.txt[j] = (uint8_t)toupper(s.cru[j]);
		s.ntxt = j + 1;
	}
	int r = 1;
	while (s.pos < s.ntxt) {
		Simb t;
		const char *err = simbolo(&s, &t);
		if (!err) {
			if (t.tipo == S_FIM) break;
			if (t.tipo == S_PALAVRA && t.chave == K_RULE) err = regra(c, &s);
			else if (t.tipo == S_PALAVRA && t.chave == K_CRITICAL) err = critica(c, &s);
			else if (t.tipo == S_PALAVRA && t.chave == K_INCLUDE) { if (incluir(c, &s, e)) { r = -1; break; } }
			else if (t.tipo == S_SIMBOLO && t.simbolo == '{') err = abrir(c);
			else if (t.tipo == S_SIMBOLO && t.simbolo == '}') err = fechar(c);
			else err = "Statement expected";
		}
		if (!err && c->sem_memoria) err = "Out of memory";
		if (err) { erro_fonte(&s, err, e); r = -1; break; }
	}
	free(s.txt);
	return r;
}

void tz_ia_programa_liberar(TzIaPrograma *p)
{
	for (int i = 0; i < p->nregras; ++i) regra_liberar(&p->regra[i]);
	for (int i = 0; i < p->ngat; ++i) free(p->gat[i].nome);
	for (int i = 0; i < p->ntextos; ++i) free(p->texto[i]);
	free(p->regra); free(p->cond); free(p->lista); free(p->gat); free(p->texto);
	memset(p, 0, sizeof *p);
}

static void comp_liberar(Comp *c)
{
	for (int i = 0; i < c->nnomes; ++i) free(c->nomes[i]);
	for (int i = 0; i < c->npend; ++i) free(c->pend[i].nome);
	free(c->nomes); free(c->pend);
}

int tz_ia_compilar(const TzIaBiblioteca *b, const char *nome, TzIaPrograma *p, TzIaErro *e)
{
	TzIaErro lixo;
	if (!e) e = &lixo;
	memset(p, 0, sizeof *p);
	memset(e, 0, sizeof *e);
	char ruta[200];
	if (strchr(nome, '\\') || strchr(nome, '/')) snprintf(ruta, sizeof ruta, "%s", nome);
	else snprintf(ruta, sizeof ruta, "data\\ai\\%s", nome);
	Comp c;
	memset(&c, 0, sizeof c);
	c.b = b;
	c.p = p;
	int r = ficheiro(&c, ruta, e);
	if (r == 0) { comp_liberar(&c); return 0; }   /* 0x418dd2: un fichero vacío es un guion sin reglas */
	if (r < 0) { comp_liberar(&c); tz_ia_programa_liberar(p); return -1; }
	/* 0x41aa20: los nombres pendientes se resuelven en orden; el primero que falta es el error */
	for (int i = 0; i < c.npend; ++i) {
		Pendente *u = &c.pend[i];
		int n = (int)strlen(u->nome);
		int k = u->gatilho ? indice_gatilho(&c, (const uint8_t *)u->nome, n) : indice_regra(&c, (const uint8_t *)u->nome, n);
		if (k < 0) {
			memset(e, 0, sizeof *e);
			snprintf(e->ficheiro, sizeof e->ficheiro, "%s", ruta);
			e->mensagem = u->gatilho ? "Unresolved trigger" : "Unresolved rule";
			snprintf(e->texto, sizeof e->texto, "%s", u->nome);
			comp_liberar(&c);
			tz_ia_programa_liberar(p);
			return -1;
		}
		if (u->cond >= 0 && u->cond < p->nconds) p->cond[u->cond].n = (uint32_t)k;
		if (u->lista >= 0 && u->lista < p->nlista) p->lista[u->lista] = (uint32_t)k;
	}
	/* ReTzar: un grupo sin cerrar llega hasta el final del guion */
	for (int i = 0; i < c.ngrupos; ++i) p->regra[c.grupos[i]].fim = (uint32_t)p->nregras;
	/* los nombres de las reglas se guardan con la regla */
	comp_liberar(&c);
	return 0;
}

void tz_ia_erro_texto(const TzIaErro *e, char *out, int n)
{
	if (!e->linha) {
		if (e->texto[0]) snprintf(out, (size_t)n, "%s: %s: %s%s", e->ficheiro, e->mensagem ? e->mensagem : "?", e->texto, e->incluido);
		else snprintf(out, (size_t)n, "%s: %s%s", e->ficheiro, e->mensagem ? e->mensagem : "?", e->incluido);
	} else {
		snprintf(out, (size_t)n, "%s:%d:%d: %s: %s%s", e->ficheiro, e->linha, e->coluna, e->mensagem ? e->mensagem : "?",
		         e->texto, e->incluido);
	}
}

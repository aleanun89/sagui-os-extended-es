/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_campanas.c - arnés en el PC de los guiones, las campañas y el sonido de Tzar (D417, fase 7; tests/h417_tzar.py).
 *
 *   tzar_campanas sintetico            guiones hechos en memoria contra el mundo falso (tests/tzar_mundo_falso.c): expresiones,
 *                                      reglas, ventanas, temporizadores, conversaciones, final, héroes, guardar y cargar, y que
 *                                      los registros dañados se rechazan sin romper nada
 *   tzar_campanas privado <carpeta>    (a mano, nunca en la batería) con la instalación real: lee los guiones de todas las
 *                                      misiones de las campañas y los interpreta contra el mundo falso, contestando las ventanas
 * Escribe «OK ...» o «FALLO ...» por línea y sale con 1 si algo falló. */
#include "guiao_i.h"
#include "campanha.h"
#include "aleatorio.h"
#include "musica.h"
#ifdef TZ_REAL
#include "partida.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

struct FalsoRevelar { int n, k, cx, cy, raio; uint32_t tics; };
extern struct FalsoRevelar falso_rev;
TzMundo *falso_criar(const TzMapa *mapa, int jogador);
void falso_liberar(TzMundo *m);
void falso_tic(TzMundo *m);

static int fallos;
#define VER(c, ...) do { if (c) { printf("OK "); printf(__VA_ARGS__); printf("\n"); } else { fallos++; printf("FALLO "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- constructores de registros */
typedef struct { uint8_t *d; int n, cap; } Buf;
static void bb(Buf *b, const void *p, int n)
{
	if (n <= 0) return;
	if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 64; b->d = realloc(b->d, (size_t)b->cap); }
	memcpy(b->d + b->n, p, (size_t)n);
	b->n += n;
}
static void b8(Buf *b, int v) { uint8_t x = (uint8_t)v; bb(b, &x, 1); }
static void b16(Buf *b, int v) { b8(b, v & 255); b8(b, (v >> 8) & 255); }
static void b32(Buf *b, uint32_t v) { b16(b, (int)(v & 0xffff)); b16(b, (int)(v >> 16)); }
static void bstr(Buf *b, const char *s) { bb(b, s, (int)strlen(s) + 1); }
static void bsel_classe(Buf *b, int no_max, int classe, int reinos, const char *conta)
{
	b8(b, 1); b8(b, no_max); b16(b, classe); b16(b, reinos); b8(b, 0); b8(b, 0); b16(b, 0); bstr(b, conta);
}
static void bsel_ids(Buf *b, int todos, int n, const int *ids)
{
	b8(b, 0); b8(b, 0); b16(b, 0); b16(b, 0); b8(b, todos); b8(b, 0); b16(b, n);
	for (int i = 0; i < n; ++i) b16(b, ids[i]);
	bstr(b, "");
}
static void barea(Buf *b, int em_volta, int x1, int y1, int x2, int y2, int objeto, const char *raio)
{
	b8(b, em_volta); b32(b, (uint32_t)x1); b32(b, (uint32_t)y1); b32(b, (uint32_t)x2); b32(b, (uint32_t)y2); b16(b, objeto); bstr(b, raio);
}
static void breg(Buf *out, const char *nome, int tipo, const Buf *corpo)
{
	uint8_t h[36];
	memset(h, 0x5a, sizeof h);
	size_t l = strlen(nome);
	memcpy(h, nome, l);
	h[l] = 0;
	h[32] = (uint8_t)tipo; h[33] = (uint8_t)(tipo >> 8);
	h[34] = (uint8_t)corpo->n; h[35] = (uint8_t)(corpo->n >> 8);
	bb(out, h, 36);
	bb(out, corpo->d, corpo->n);
}
static void bfree(Buf *b) { free(b->d); memset(b, 0, sizeof *b); }

/* ---------------------------------------------------------------- un mundo de prueba */
enum { ID_HEROI = 10, ID_CASTELO = 11, ID_INIMIGO = 20, ID_SOLDADO = 21, ID_OCULTO = 30 };

static TzMapa mapa_prueba(void)
{
	static TzObjeto obj[8];
	memset(obj, 0, sizeof obj);
	static const struct { int id, classe, dono, x, y; uint32_t flags; int vida; } t[] = {
		{ ID_HEROI, 8, 1, 3, 3, TZ_F_CARREGADO | TZ_F_HEROI, 40 },
		{ ID_CASTELO, TZC_CASTLE, 1, 4, 4, 0, 500 },
		{ ID_INIMIGO, TZC_CASTLE, 2, 20, 20, 0, 500 },
		{ ID_SOLDADO, 3, 2, 21, 20, 0, 20 },
		{ ID_OCULTO, 3, 1, 10, 10, TZ_F_INATIVO, 20 },
		{ 22, 3, 2, 22, 20, 0, 20 },
	};
	TzMapa m;
	memset(&m, 0, sizeof m);
	m.lado = 32;
	m.nobj = (int)(sizeof t / sizeof *t);
	m.obj = obj;
	for (int i = 0; i < m.nobj; ++i) {
		obj[i].id = (uint16_t)t[i].id; obj[i].classe = (uint16_t)t[i].classe; obj[i].dono = (uint8_t)t[i].dono; obj[i].x = (uint16_t)t[i].x;
		obj[i].y = (uint16_t)t[i].y; obj[i].flags = t[i].flags; obj[i].vida = obj[i].vida_max = (uint16_t)t[i].vida; obj[i].civ = 1;
	}
	m.nreinos = 2;
	for (int i = 0; i < 2; ++i) { m.reinos[i].ativo = 1; snprintf(m.reinos[i].nome, sizeof m.reinos[i].nome, "Reino %d", i + 1); m.reinos[i].recursos[0] = 1000; }
	return m;
}

static TzObj *por_id(TzMundo *m, int id) { TzId i = tz_mundo_por_id_mapa(m, id); return i >= 0 ? &m->obj[i] : NULL; }
static int var(TzGuiao *g, const char *n) { return tz_vars_get(&g->vars, n); }
static void tics(TzMundo *m, int n) { for (int i = 0; i < n; ++i) falso_tic(m); }
static int tem_pedido(TzGuiao *g, int tipo)
{
	TzPedido p;
	int visto = 0;
	while (tz_guiao_pedido(g, &p)) if (p.tipo == tipo) visto = 1;
	return visto;
}

/* ---------------------------------------------------------------- expresiones (las pruebas de expr.rs) */
typedef struct { TzVariaveis v; int gold, health, nllamadas; int ultimo_fn; int32_t ultimos[8]; char filme[64]; } Prueba;
static int32_t p_chamar(void *vc, int fn, const int32_t a[8], int obj)
{
	Prueba *t = vc; (void)obj;
	t->nllamadas++; t->ultimo_fn = fn; memcpy(t->ultimos, a, sizeof t->ultimos);
	if (fn == TZ_FN_FOOD) return 100 * a[0];
	if (fn == TZ_FN_MAX) return a[0] > a[1] ? a[0] : a[1];
	return 0;
}
static void p_filme(void *vc, const char *n) { Prueba *t = vc; snprintf(t->filme, sizeof t->filme, "%s", n); }
static int32_t p_prop(void *vc, int i, int p) { Prueba *t = vc; (void)i; return p == TZ_PR_HEALTH ? t->health : 0; }
static void p_defprop(void *vc, int i, int p, int32_t v) { Prueba *t = vc; (void)i; if (p == TZ_PR_HEALTH) t->health = v; }
static int32_t p_global(void *vc, int g) { Prueba *t = vc; return g == TZ_GL_GOLD ? t->gold : 0; }
static void p_defglobal(void *vc, int g, int32_t v) { Prueba *t = vc; if (g == TZ_GL_GOLD) t->gold = v; }
static int p_evento(void *vc, const char *n, int *v) { (void)vc; if (!strcmp(n, "inicio")) { *v = 1; return 1; } return 0; }
static TzVariaveis *p_vars(void *vc) { return &((Prueba *)vc)->v; }
static const TzExprOps OPS_P = { p_chamar, p_filme, p_prop, p_defprop, p_global, p_defglobal, p_evento, p_vars };
static int32_t ev1(Prueba *t, const char *s, int obj) { return tz_expr_eval(&OPS_P, t, s, obj); }

static void prueba_expresiones(void)
{
	static Prueba t;
	memset(&t, 0, sizeof t);
	VER(ev1(&t, "1 + 2 * 3", -1) == 7, "expresiones: precedencia");
	VER(ev1(&t, "8 - 4 - 2", -1) == 6, "expresiones: a igual precedencia agrupa por la derecha (8-4-2 = 6)");
	VER(ev1(&t, "16 / 4 / 2", -1) == 8, "expresiones: 16/4/2 = 8");
	VER(ev1(&t, "1 + 2 & 3", -1) == 3, "expresiones: & liga más que +");
	VER(ev1(&t, "(1 + 2) * 3", -1) == 9, "expresiones: paréntesis");
	VER(ev1(&t, "5 / 0", -1) == 0 && ev1(&t, "5 % 0", -1) == 0, "expresiones: dividir por cero da 0");
	VER(ev1(&t, "-5", -1) == -5, "expresiones: menos unario (el operando que falta lee 0)");
	VER(ev1(&t, "!0 && 3 >= 2", -1) == 1 && ev1(&t, "~0", -1) == -1, "expresiones: lógicos y bit a bit");
	unsigned av = tz_expr_avisos;
	VER(ev1(&t, "(1 + 2", -1) == 0 && tz_expr_avisos == av + 1, "expresiones: paréntesis sin cerrar da 0 y se cuenta");
	VER(ev1(&t, "12abc + 1", -1) == 13, "expresiones: atoi lee los dígitos y se para");
	VER(ev1(&t, "1;2;3", -1) == 3, "expresiones: la última sentencia manda");
	VER(ev1(&t, "$a = $b = 4", -1) == 4 && tz_vars_get(&t.v, "$a") == 4 && tz_vars_get(&t.v, "$b") == 4, "expresiones: asignación encadenada");
	VER(ev1(&t, "$A", -1) == 0 && t.v.nmapa == 3, "expresiones: un nombre desconocido se crea con 0 y distingue mayúsculas");
	VER(ev1(&t, "inicio && $a==4", -1) == 1, "expresiones: un suceso por su nombre");
	VER(ev1(&t, "#kept = 2", -1) == 2 && t.v.nmantidas == 1, "expresiones: «#» va a la tabla que sobrevive al mapa");
	char buf[64];
	tz_expr_sentencias("$a==0\r\n$c=1", buf, sizeof buf);
	ev1(&t, buf, -1);
	VER(tz_vars_get(&t.v, "$a") == 4 && tz_vars_get(&t.v, "$c") == 1, "expresiones: una comparación como acción no hace nada");
	ev1(&t, "$gold = $gold + 500", -1);
	VER(t.gold == 500, "expresiones: $gold escribe el oro");
	t.health = 90;
	VER(ev1(&t, "$health<=100", 3) == 1, "expresiones: propiedad con objeto de contexto");
	ev1(&t, "$health=110", 3);
	VER(t.health == 110, "expresiones: se escribe la propiedad");
	VER(ev1(&t, "$health", -1) == 110, "expresiones: sin objeto es una variable que el escritor también guardó");
	VER(ev1(&t, "@food(1) >= 100", -1) == 1, "expresiones: funciones con argumentos");
	VER(ev1(&t, "@max(@food(1), 7*50)", -1) == 350, "expresiones: funciones anidadas");
	ev1(&t, "@SetFood(4, @food(4) - @food(4))", -1);
	VER(t.ultimo_fn == TZ_FN_SET_FOOD && t.ultimos[0] == 4 && t.ultimos[1] == 0, "expresiones: «@SetFood» sin distinguir mayúsculas");
	ev1(&t, "@go(2)", -1);
	VER(t.ultimo_fn == TZ_FN_GOLD, "expresiones: un prefijo del nombre casa con la primera función que empieza así");
	ev1(&t, "@playmovie(intro)", -1);
	VER(!strcmp(t.filme, "intro"), "expresiones: @playmovie");
	/* nombres de más de 63 caracteres y 256 variables como mucho */
	char largo[200];
	memset(largo, 'x', sizeof largo - 1); largo[0] = '$'; largo[sizeof largo - 1] = 0;
	tz_vars_set(&t.v, largo, 7);
	VER(tz_vars_get(&t.v, largo) == 7, "expresiones: nombre largo");
	for (int i = 0; i < 300; ++i) { char n[32]; snprintf(n, sizeof n, "$v%d", i); tz_vars_set(&t.v, n, i); }
	VER(t.v.nmapa == 256, "expresiones: la tabla guarda 256 nombres como mucho");
	/* basura: no debe romper nada */
	const char *mala[] = { "", ";;;", "((((", "))))", "@", "@(", "@food(", "1 +", "= =", "$a = ", ",,,", "!", "&&", "9999999999999 * 99999999999", "@gold(@gold(@gold(1)))" };
	for (unsigned i = 0; i < sizeof mala / sizeof *mala; ++i) ev1(&t, mala[i], -1);
	VER(1, "expresiones: cadenas rotas no hacen nada raro");
}

/* ---------------------------------------------------------------- el guion de prueba */
typedef struct { Buf ev, ac, rg; int nev, nac, nrg; } Guion;

static void g_evento(Guion *s, const char *n, int tipo, const Buf *c) { breg(&s->ev, n, tipo, c); s->nev++; }
static void g_accion(Guion *s, const char *n, int tipo, const Buf *c) { breg(&s->ac, n, tipo, c); s->nac++; }
static void g_regla(Guion *s, const char *n, const char *cond, int nacc, const char *const *acc)
{
	Buf c = { 0 };
	bstr(&c, cond);
	for (int i = 0; i < nacc; ++i) bstr(&c, acc[i]);
	breg(&s->rg, n, nacc, &c);
	bfree(&c);
	s->nrg++;
}
static void g_expr(Guion *s, const char *nome, const char *expr) { Buf c = { 0 }; bstr(&c, expr); g_accion(s, nome, TZ_ACC_EXPRESSAO, &c); bfree(&c); }

static void g_liberar(Guion *s) { bfree(&s->ev); bfree(&s->ac); bfree(&s->rg); }

static int g_ler(TzGuiao *g, Guion *s)
{
	uint16_t cont[4] = { (uint16_t)s->nrg, (uint16_t)s->nac, (uint16_t)s->nev, 0 };
	memset(g, 0, sizeof *g);
	return tz_guiao_de_memoria(g, cont, s->ev.d ? s->ev.d : (const uint8_t *)"", (uint32_t)s->ev.n, s->ac.d ? s->ac.d : (const uint8_t *)"", (uint32_t)s->ac.n,
	                           s->rg.d ? s->rg.d : (const uint8_t *)"", (uint32_t)s->rg.n);
}

static int silencio;
static void conv_prueba(TzGuiao *g)
{
	/* una conversación «Cv1»: frase 1 (inicio, habla el id 10) -> 2 o 3; la 3 solo si $lena==1; la 2 pone $presto=1 */
	Buf d = { 0 };
	b16(&d, 1);
	bstr(&d, "Cv1");
	b16(&d, 3);
	static const struct { int ind, quien, inicio; const char *cond, *acao, *texto; int ns, s[2]; } f[3] = {
		{ 1, ID_HEROI, 1, "", "", "Hola, señor.", 2, { 2, 3 } },
		{ 2, ID_HEROI, 0, "", "$presto=1", "Adelante.", 0, { 0, 0 } },
		{ 3, ID_CASTELO, 0, "$lena==1", "", "Solo si hay leña.", 0, { 0, 0 } },
	};
	for (int i = 0; i < 3; ++i) {
		b16(&d, f[i].ind); b16(&d, f[i].quien); b8(&d, f[i].inicio);
		bstr(&d, "Frase"); bstr(&d, f[i].cond); bstr(&d, f[i].acao); bstr(&d, ""); bstr(&d, "USER\\F.wav"); bstr(&d, f[i].texto);
		b16(&d, f[i].ns);
		for (int k = 0; k < f[i].ns; ++k) b16(&d, f[i].s[k]);
	}
	int rc = tz_guiao_conversas(g, d.d, (uint32_t)d.n);
	if (!silencio) VER(rc == 0 && g->nconversas == 1 && g->conversas[0].nfrases == 3 && g->conversas[0].frases[1].nseg == 0 &&
	    !strcmp(g->conversas[0].frases[0].texto, "Hola, se\xc3\xb1or.") && g->conversas[0].frases[2].fala_de == ID_CASTELO,
	    "conversaciones: se leen frases, enlaces, condición y acción");
	bfree(&d);
}

static void prueba_guion(void)
{
	Guion s = { { 0 } };
	/* ---- sucesos */
	{ Buf c = { 0 }; int ids[1] = { ID_INIMIGO }; bsel_ids(&c, 0, 1, ids); g_evento(&s, "castelo_inimigo_caiu", TZ_EVT_DESTRUIDO, &c); bfree(&c); }
	{ Buf c = { 0 }; bsel_classe(&c, 0, 3, 2, "2"); g_evento(&s, "dois_soldados", TZ_EVT_EXISTE, &c); bfree(&c); }
	{ Buf c = { 0 }; bsel_classe(&c, 0, 3, 2, "2"); g_evento(&s, "dois_soldados_mortos", TZ_EVT_DESTRUIDO, &c); bfree(&c); }
	{ Buf c = { 0 }; int ids[1] = { ID_HEROI }; bsel_ids(&c, 0, 1, ids); barea(&c, 0, 8, 8, 12, 12, 0, "0"); g_evento(&s, "heroi_na_area", TZ_EVT_NA_AREA, &c); bfree(&c); }
	{ Buf c = { 0 }; b16(&c, 2); g_evento(&s, "inimigo_derrotado", TZ_EVT_DERROTADO, &c); bfree(&c); }
	/* ---- acciones */
	{ Buf c = { 0 }; bstr(&c, "Construye 2 cuarteles"); bstr(&c, ""); g_accion(&s, "Quest1", TZ_ACC_MISSAO, &c); bfree(&c); }
	{ Buf c = { 0 }; bstr(&c, "Hola\r\nmundo"); bstr(&c, "USER\\retrato.bmp"); bstr(&c, "USER\\voz.wav"); bstr(&c, ""); bstr(&c, "0"); b8(&c, 0); g_accion(&s, "Historia", TZ_ACC_HISTORIA, &c); bfree(&c); }
	g_expr(&s, "Pon1", "$depois = 1");
	/* v8.5.5.2: «explorar» un rato (3 s sobre un área de 5x5) y @KingdomCanRescue */
	{ Buf c = { 0 }; barea(&c, 0, 8, 8, 12, 12, 0, "0"); bstr(&c, "3000"); g_accion(&s, "Explora", TZ_ACC_EXPLORAR, &c); bfree(&c); }
	g_expr(&s, "SemResgate", "@kingdomcanrescue(2, 0)");
	g_expr(&s, "PonKept", "#kept = 7\r\n$gold = $gold + 5");
	{ Buf c = { 0 }; bstr(&c, "4000"); b8(&c, 1); g_accion(&s, "Cuenta", TZ_ACC_TEMPORIZADOR, &c); bfree(&c); }
	{ Buf c = { 0 }; bstr(&c, "2000"); g_accion(&s, "Espera2s", TZ_ACC_ESPERAR, &c); bfree(&c); }
	g_expr(&s, "PonDepoisEspera", "$esperou = 1");
	{ Buf c = { 0 }; b16(&c, 3); bstr(&c, "3"); b8(&c, 2); barea(&c, 0, 5, 5, 8, 8, 0, "0"); b8(&c, 1); g_accion(&s, "Cria", TZ_ACC_CRIAR, &c); bfree(&c); }
	{ Buf c = { 0 }; int ids[1] = { ID_OCULTO }; bsel_ids(&c, 0, 1, ids); g_accion(&s, "Aparece", TZ_ACC_APARECER, &c); bfree(&c); }
	{ Buf c = { 0 }; bstr(&c, "Cv1"); g_accion(&s, "Conversa", TZ_ACC_CONVERSA, &c); bfree(&c); }
	{ Buf c = { 0 }; b16(&c, 1); bstr(&c, "Ganaste|Fin del juego"); g_accion(&s, "Ganha", TZ_ACC_GANHAR, &c); bfree(&c); }
	{ Buf c = { 0 }; bstr(&c, "MAPS\\Segundo.wmp"); g_accion(&s, "Proximo", TZ_ACC_MAPA, &c); bfree(&c); }
	{ Buf c = { 0 }; g_accion(&s, "Manter", TZ_ACC_MANTER, &c); bfree(&c); }
	{ Buf c = { 0 }; b8(&c, 1); b8(&c, 1); b8(&c, 1); g_accion(&s, "Cuidado", TZ_ACC_ESTADO, &c); bfree(&c); }
	/* ---- reglas */
	{ const char *a[] = { "Quest1", "Historia", "Pon1" }; g_regla(&s, "Inicio", "1", 3, a); }
	{ const char *a[] = { "PonKept", "Cuenta", "Espera2s", "PonDepoisEspera" }; g_regla(&s, "Contar", "$depois", 4, a); }
	{ const char *a[] = { "Cria", "Aparece" }; g_regla(&s, "Refuerzos", "$esperou==1", 2, a); }
	{ const char *a[] = { "Conversa" }; g_regla(&s, "Habla", "dois_soldados && $depois", 1, a); }
	{ const char *a[] = { "Ganha", "Proximo" }; g_regla(&s, "Gana", "castelo_inimigo_caiu", 2, a); }
	{ const char *a[] = { "Cuidado", "NoExiste", "Pon1" }; g_regla(&s, "Accion rota", "0", 3, a); }
	{ const char *a[] = { "Explora", "SemResgate" }; g_regla(&s, "Explorar", "$depois", 2, a); }
	TzGuiao g;
	int rc = g_ler(&g, &s);
	VER(rc == 0 && g.neventos == 5 && g.nacoes == 16 && g.nregras == 7, "guion: %d sucesos, %d acciones, %d reglas leídos", g.neventos, g.nacoes, g.nregras);
	VER(g.regras[0].nacoes == 3 && g.regras[5].acoes[1] == -1 && g.regras[5].acoes[0] >= 0, "guion: los nombres de acción se resuelven y los desconocidos quedan en -1");
	conv_prueba(&g);
	/* personajes (.objext) */
	g.pers = calloc(2, sizeof *g.pers);
	g.pers[0].id = ID_HEROI; snprintf(g.pers[0].nome, sizeof g.pers[0].nome, "Arturo"); snprintf(g.pers[0].subtitulo, sizeof g.pers[0].subtitulo, "Rey"); snprintf(g.pers[0].retrato_grande, sizeof g.pers[0].retrato_grande, "USER\\GA.bmp");
	g.npers = 1;

	TzMapa mapa = mapa_prueba();
	TzMundo *m = falso_criar(&mapa, 1);
	VER(tz_guiao_registrar(m, &g, 1) == 0 && m->regras.campanha && m->regras.sem_vitoria_auto && m->nganchos == 1, "guion: se registra como gancho «GUIA» y activa sem_vitoria_auto");
	tics(m, 2);
	/* regla Inicio: la misión, la historia y el aviso de la misión */
	VER(g.nmissoes == 1 && !strcmp(g.missoes[0].texto, "Construye 2 cuarteles"), "reglas: la acción de misión añade el objetivo");
	VER(tz_guiao_modal(&g) && !strcmp(tz_guiao_mensagem(&g)->texto, "Hola\r\nmundo") && !strcmp(tz_guiao_mensagem(&g)->retrato, "USER\\retrato.bmp"), "reglas: la historia abre su ventana con texto y retrato");
	{
		TzPedido p; int vista_historia = 0, vista_nota = 0;
		while (tz_guiao_pedido(&g, &p)) {
			if (p.tipo == TZ_PD_HISTORIA && !strcmp(p.som, "USER\\voz.wav")) vista_historia = 1;
			if (p.tipo == TZ_PD_NOTA && !strcmp(p.chave, "newquest")) vista_nota = 1;
		}
		VER(vista_historia && vista_nota, "reglas: pedidos de historia (con su voz) y de aviso de misión a la interfaz");
	}
	tics(m, 60);
	VER(var(&g, "$depois") == 0, "reglas: una regla que espera una ventana no sigue mientras está abierta");
	tz_guiao_fechar(m, &g);
	tics(m, 6);
	VER(!tz_guiao_modal(&g) && var(&g, "$depois") == 1, "reglas: al cerrar la ventana la regla sigue con la acción siguiente");
	/* Contar: temporizador, expresión con «#», espera de 2 s */
	tics(m, 25);
	VER(var(&g, "#kept") == 7 && m->reino[1].recursos[0] == 1005, "reglas: expresión con «#» y escritura de $gold (%d)", m->reino[1].recursos[0]);
	VER(g.ntemps == 1 && var(&g, "Cuenta") > 0 && var(&g, "Cuenta") <= 4000, "temporizadores: la cuenta atrás de 4000 baja de 1000 en 1000 (%d)", var(&g, "Cuenta"));
	VER(var(&g, "$esperou") == 0, "reglas: «esperar» detiene la regla");
	tics(m, 60);
	VER(var(&g, "$esperou") == 1, "reglas: pasada la espera, la regla sigue");
	VER(falso_rev.n == 1 && falso_rev.k == 1 && falso_rev.cx == 10 && falso_rev.cy == 10 && falso_rev.raio == 2 && falso_rev.tics == 60,
	    "explorar descubre un rato y no para siempre: %d llamada, área en (%d,%d) radio %d durante %u tics", falso_rev.n, falso_rev.cx, falso_rev.cy, falso_rev.raio, (unsigned)falso_rev.tics);
	VER((m->sem_resgate & (1u << 2)) && !(m->sem_resgate & (1u << 1)), "@kingdomcanrescue(2, 0) quita al reino 2 el rescate de prisioneros");
	tics(m, 20);
	VER(g.ntemps == 0, "temporizadores: al llegar a 0 se borra (%d)", var(&g, "Cuenta"));
	/* Refuerzos: crear y aparecer */
	tics(m, 40);
	int n3 = 0;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe == 3 && m->obj[i].dono == 2 && !(m->obj[i].flags & TZ_F_MORTO)) n3++;
	VER(n3 == 2 + 3, "acciones: «crear» pone 3 unidades más del reino 2 (%d)", n3);
	VER(!(por_id(m, ID_OCULTO)->flags & TZ_F_INATIVO), "acciones: «aparecer» saca al objeto oculto");
	/* Habla: dos soldados -> conversación; elige de las dos frases; la frase 3 solo con $lena==1 */
	tics(m, 40);
	VER(g.conv_activa && tz_guiao_modal(&g), "conversaciones: la acción abre la conversación y espera al jugador");
	{ const TzFrase *f = tz_guiao_fala(&g); VER(f && !strcmp(f->texto, "Hola, se\xc3\xb1or.") && !strcmp(tz_guiao_mensagem(&g)->titulo, "Arturo"), "conversaciones: la primera frase la dice Arturo"); }
	tem_pedido(&g, TZ_PD_FALA);
	tz_guiao_responder(m, &g, 0);
	{ const TzFrase *f = tz_guiao_fala(&g); VER(f && !strcmp(f->texto, "Adelante.") && g.nescolhas_conv == 0, "conversaciones: con $lena sin poner solo queda la frase 2 y se dice sola"); }
	tz_guiao_responder(m, &g, 0);
	VER(var(&g, "$presto") == 1 && !g.conv_activa && !tz_guiao_modal(&g) && tem_pedido(&g, TZ_PD_FECHAR), "conversaciones: la acción de la frase pone $presto y al no quedar frases se cierra");
	/* guardar y cargar a mitad: estado idéntico y el guion sigue igual */
	{
		FILE *f = tmpfile();
		TzGancho *gc = &m->gancho[0];
		VER(gc->gravar(m, gc->dados, f) == 0, "guardar: el bloque GUIA se escribe");
		long tam = ftell(f);
		rewind(f);
		silencio = 1;
		TzGuiao g2;
		Guion s2 = s;
		g_ler(&g2, &s2);
		conv_prueba(&g2);
		g2.pers = g.pers; g2.npers = g.npers;
		TzMapa mapa2 = mapa_prueba();
		TzMundo *m2 = falso_criar(&mapa2, 1);
		tz_guiao_registrar(m2, &g2, 1);
		int rc2 = m2->gancho[0].carregar(m2, m2->gancho[0].dados, f, (uint32_t)tam);
		VER(rc2 == 0 && g2.vars.nmapa == g.vars.nmapa && var(&g2, "$depois") == 1 && var(&g2, "#kept") == 7 && g2.nmissoes == 1 &&
		    g2.nrun == g.nrun && g2.seq == g.seq && !memcmp(g2.perdidos, g.perdidos, sizeof g.perdidos), "cargar: el estado del guion vuelve igual (%s)", rc2 ? tz_erro() : "");
		/* un bloque dañado se rechaza */
		rewind(f);
		uint8_t *buf = malloc((size_t)tam);
		size_t leidos = fread(buf, 1, (size_t)tam, f);
		int rechazados = 0, probados = 0;
		for (long i = 0; i < tam && leidos; i += 7) {
			uint8_t v = buf[i];
			buf[i] ^= 0xff;
			FILE *f3 = tmpfile();
			fwrite(buf, 1, (size_t)tam, f3);
			rewind(f3);
			TzGuiao g3;
			Guion s3 = s;
			g_ler(&g3, &s3);
			conv_prueba(&g3);
			TzMapa mapa3 = mapa_prueba();
			TzMundo *m3 = falso_criar(&mapa3, 1);
			tz_guiao_registrar(m3, &g3, 1);
			probados++;
			if (m3->gancho[0].carregar(m3, m3->gancho[0].dados, f3, (uint32_t)tam)) rechazados++;
			else tics(m3, 3);   /* si lo acepta, tiene que poder seguir sin romper nada */
			fclose(f3);
			falso_liberar(m3);
			tz_guiao_liberar(&g3);
			buf[i] = v;
		}
		silencio = 0;
		VER(probados > 10, "cargar: %d bloques dañados probados (%d rechazados, el resto sigue sin romper)", probados, rechazados);
		free(buf);
		falso_liberar(m2);
		g2.pers = NULL;
		tz_guiao_liberar(&g2);
		fclose(f);
	}
	/* Gana: matar el castillo enemigo -> «destruido» -> el reino 1 gana, el mapa siguiente */
	VER(!tz_guiao_acabou(&g, NULL), "final: la partida sigue");
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, ID_INIMIGO));
	tics(m, 45);
	int gan = 0;
	VER(tz_guiao_acabou(&g, &gan) && gan && m->acabou && m->ganhou && !strcmp(tz_guiao_proximo_mapa(&g), "MAPS\\Segundo.wmp") && !strcmp(g.texto_fim, "Fin del juego"),
	    "final: «ganar» da la victoria, el texto tras «|» y la ruta del mapa siguiente");
	/* héroes: el de CARREGADO vivo pasa al siguiente mapa con su vida y su nivel */
	por_id(m, ID_HEROI)->vida = 33; por_id(m, ID_HEROI)->nivel = 4;
	TzHeroi h[4];
	int nh = tz_guiao_herois(m, &g, h, 4);
	VER(nh == 1 && !strcmp(h[0].nome, "Arturo") && h[0].vida == 33 && h[0].nivel == 4 && h[0].promovido, "héroes: se llevan al siguiente mapa con su vida y nivel");
	{
		TzMapa mapa4 = mapa_prueba();
		TzMundo *m4 = falso_criar(&mapa4, 1);
		h[0].vida = 21; h[0].nivel = 5; h[0].vida_max = 60;
		tz_guiao_restaurar_herois(m4, &g, h, 1);
		TzObj *o = por_id(m4, ID_HEROI);
		VER(o->vida == 21 && o->nivel == 5 && o->vida_max == 60 && (o->flags & TZ_F_HEROI), "héroes: en el mapa nuevo recuperan vida, nivel y ascenso");
		falso_liberar(m4);
	}
	TzVar k[8];
	int nk = tz_guiao_mantidas(&g, k, 8);
	VER(nk == 1 && !strcmp(k[0].nome, "#kept") && k[0].valor == 7, "campaña: las variables «#» sobreviven al paso de misión");
	falso_liberar(m);
	tz_guiao_liberar(&g);
	g_liberar(&s);
}

/* destruido por clase, derrota, fin por derrota del jugador, victoria normal de campaña */
static void prueba_final(void)
{
	Guion s = { { 0 } };
	{ Buf c = { 0 }; bsel_classe(&c, 0, 3, 2, "2"); g_evento(&s, "dois_mortos", TZ_EVT_DESTRUIDO, &c); bfree(&c); }
	g_expr(&s, "Marca", "$marca = 1");
	{ const char *a[] = { "Marca" }; g_regla(&s, "R", "dois_mortos", 1, a); }
	TzGuiao g;
	g_ler(&g, &s);
	TzMapa mapa = mapa_prueba();
	TzMundo *m = falso_criar(&mapa, 1);
	tz_guiao_registrar(m, &g, 1);
	tics(m, 30);
	VER(var(&g, "$marca") == 0, "sucesos: «destruido» por clase espera a que mueran 2");
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, ID_SOLDADO));
	tics(m, 30);
	VER(var(&g, "$marca") == 0 && g.perdidos[2][3] == 1, "sucesos: una muerte cuenta (%u)", g.perdidos[2][3]);
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, 22));
	tics(m, 30);
	VER(var(&g, "$marca") == 1, "sucesos: a la segunda muerte se cumple");
	/* con el reino enemigo sin nada «grande», cae; con el del jugador sin nada, la partida se pierde */
	VER(!m->reino[2].derrotado && !tz_guiao_acabou(&g, NULL), "final: mientras al reino 2 le quede el castillo no cae");
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, ID_INIMIGO));
	tics(m, 5);
	VER(m->reino[2].derrotado, "final: sin castillo ni unidades grandes el reino enemigo cae");
	int gan = 0;
	VER(tz_guiao_acabou(&g, &gan) && gan, "final: sin rivales el jugador gana por la regla normal de campaña");
	falso_liberar(m);
	tz_guiao_liberar(&g);

	/* el jugador pierde todo -> derrota */
	g_ler(&g, &s);
	TzMapa mapa2 = mapa_prueba();
	m = falso_criar(&mapa2, 1);
	tz_guiao_registrar(m, &g, 1);
	tics(m, 3);
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, ID_HEROI));
	tz_mundo_guiao_matar(m, tz_mundo_por_id_mapa(m, ID_CASTELO));
	tics(m, 5);
	gan = 1;
	VER(m->reino[1].derrotado && tz_guiao_acabou(&g, &gan) && !gan && m->acabou && !m->ganhou, "final: si el jugador se queda sin nada pierde");
	falso_liberar(m);
	tz_guiao_liberar(&g);
	/* un reino «no puede perder» (acción de estado) no cae aunque no le quede nada */
	g_liberar(&s);
}

/* registros dañados: se rechazan o se leen, pero nunca rompen */
static void prueba_danados(void)
{
	Guion s = { { 0 } };
	{ Buf c = { 0 }; bsel_classe(&c, 0, 3, 2, "2"); g_evento(&s, "e1", TZ_EVT_EXISTE, &c); bfree(&c); }
	{ Buf c = { 0 }; bstr(&c, "texto"); bstr(&c, "ret"); g_accion(&s, "a1", TZ_ACC_MISSAO, &c); bfree(&c); }
	{ Buf c = { 0 }; b16(&c, 3); bstr(&c, "1"); b8(&c, 1); barea(&c, 0, 1, 1, 2, 2, 0, "0"); b8(&c, 1); g_accion(&s, "a2", TZ_ACC_CRIAR, &c); bfree(&c); }
	{ const char *a[] = { "a1", "a2" }; g_regla(&s, "r1", "e1", 2, a); }
	TzGuiao g;
	VER(g_ler(&g, &s) == 0, "dañados: el guion bueno se lee");
	tz_guiao_liberar(&g);
	uint16_t cont[4] = { 1, 2, 1, 0 };
	/* truncados en cada byte */
	int malos = 0, total = 0;
	for (int n = 0; n < s.ev.n; ++n) {
		memset(&g, 0, sizeof g);
		total++;
		if (tz_guiao_de_memoria(&g, cont, s.ev.d, (uint32_t)n, s.ac.d, (uint32_t)s.ac.n, s.rg.d, (uint32_t)s.rg.n)) malos++;
		tz_guiao_liberar(&g);
	}
	for (int n = 0; n < s.ac.n; ++n) {
		memset(&g, 0, sizeof g);
		total++;
		if (tz_guiao_de_memoria(&g, cont, s.ev.d, (uint32_t)s.ev.n, s.ac.d, (uint32_t)n, s.rg.d, (uint32_t)s.rg.n)) malos++;
		tz_guiao_liberar(&g);
	}
	for (int n = 0; n < s.rg.n; ++n) {
		memset(&g, 0, sizeof g);
		total++;
		if (tz_guiao_de_memoria(&g, cont, s.ev.d, (uint32_t)s.ev.n, s.ac.d, (uint32_t)s.ac.n, s.rg.d, (uint32_t)n)) malos++;
		tz_guiao_liberar(&g);
	}
	VER(malos == total, "dañados: los %d registros truncados se rechazan sin fugas (%d)", total, malos);
	/* un byte cambiado en cada posición: no puede romper (ASan/UBSan lo vigila); si lee, el guion tiene que poder correr */
	unsigned seed = 1;
	for (int r = 0; r < 3; ++r) {
		uint8_t *bufs[3] = { s.ev.d, s.ac.d, s.rg.d };
		int lens[3] = { s.ev.n, s.ac.n, s.rg.n };
		for (int q = 0; q < 3; ++q)
			for (int i = 0; i < lens[q]; ++i) {
				uint8_t v = bufs[q][i];
				seed = seed * 1103515245u + 12345u;
				bufs[q][i] = (uint8_t)(seed >> 16);
				memset(&g, 0, sizeof g);
				if (tz_guiao_de_memoria(&g, cont, s.ev.d, (uint32_t)s.ev.n, s.ac.d, (uint32_t)s.ac.n, s.rg.d, (uint32_t)s.rg.n) == 0) {
					TzMapa mapa = mapa_prueba();
					TzMundo *m = falso_criar(&mapa, 1);
					tz_guiao_registrar(m, &g, 0);
					tics(m, 45);
					falso_liberar(m);
				}
				tz_guiao_liberar(&g);
				bufs[q][i] = v;
			}
	}
	VER(1, "dañados: %d bytes cambiados al azar, leídos y corridos sin romper nada", 3 * (s.ev.n + s.ac.n + s.rg.n));
	/* un tipo de acción que no existe */
	{
		Guion t = { { 0 } };
		Buf c = { 0 };
		g_accion(&t, "x", 99, &c);
		uint16_t ct[4] = { 0, 1, 0, 0 };
		memset(&g, 0, sizeof g);
		VER(tz_guiao_de_memoria(&g, ct, (const uint8_t *)"", 0, t.ac.d, (uint32_t)t.ac.n, (const uint8_t *)"", 0) != 0, "dañados: un tipo de acción desconocido se rechaza");
		tz_guiao_liberar(&g);
		g_liberar(&t);
	}
	g_liberar(&s);
}

/* ---------------------------------------------------------------- campañas y mundo: lo de campanha.c */
static const char *inst_campanhas;   /* carpeta de la instalación sintética de campañas (tools/tzar_sintetico.py --campanhas) */

static void prueba_campanha(void)
{
	if (!inst_campanhas) { printf("OK campañas: sin carpeta de campañas, se salta\n"); return; }
	TzMapas c;
	int falhas = -1;
	VER(tz_mapas_ler(&c, inst_campanhas, &falhas) == 0 && falhas == 0, "campañas: el catálogo se lee sin fallos (%d)", falhas);
	VER(c.nsoltas == 1 && c.ncamp == 1 && c.camp[0].nmissoes == 2, "campañas: 1 mapa suelto, 1 campaña de 2 misiones");
	if (c.ncamp != 1 || c.camp[0].nmissoes != 2) { tz_mapas_liberar(&c); return; }
	TzCampanha *cp = &c.camp[0];
	VER(!strcmp(cp->titulo, "La historia de prueba") && strstr(cp->descricao, "sint"), "campañas: título y descripción de la campaña");
	VER(!strcmp(cp->missoes[0].titulo, "Inicio") && !strcmp(cp->missoes[1].titulo, "Final") && cp->missoes[0].inicial && !cp->missoes[1].inicial,
	    "campañas: las misiones salen en el orden de la historia, la primera disponible");
	VER(strstr(cp->missoes[0].descricao, "descripci") != NULL && cp->missoes[0].lado == 32, "campañas: descripción y tamaño de la misión");
	VER(tz_campanha_achar(cp, "MAPS\\02 Final.wmp") == 1 && tz_campanha_achar(cp, "maps/02 final.wmp") == 1 && tz_campanha_achar(cp, "MAPS\\No.wmp") == -1,
	    "campañas: «cargar mapa» encuentra la misión sin distinguir barras ni mayúsculas");
	{
		TzCampanha f;
		TzMissao mm[1];
		memset(&f, 0, sizeof f); memset(mm, 0, sizeof mm);
		snprintf(mm[0].dentro, sizeof mm[0].dentro, "MAPS/Reino de terror_en.wmp");
		f.missoes = mm; f.nmissoes = 1;
		VER(tz_campanha_achar(&f, "MAPS\\Reino de terror.wmp") == 0 && tz_campanha_achar(&f, "MAPS\\Reino de terror_en2.wmp") == -1,
		    "campañas: la misión que solo añade el sufijo de idioma («_en») también casa");
	}
	/* progreso */
	char ruta[300];
	snprintf(ruta, sizeof ruta, "%s/progreso.txt", inst_campanhas);
	remove(ruta);
	TzProgresso pr;
	tz_progresso_ler(&pr, ruta);
	VER(pr.n == 0 && tz_missao_disponivel(&pr, &cp->missoes[0]) && !tz_missao_disponivel(&pr, &cp->missoes[1]), "progreso: sin fichero solo está la inicial");
	VER(tz_progresso_marcar(&pr, cp->missoes[1].id, ruta) == 0 && tz_missao_disponivel(&pr, &cp->missoes[1]), "progreso: alcanzar una misión la deja disponible y se guarda");
	tz_progresso_liberar(&pr);
	tz_progresso_ler(&pr, ruta);
	VER(pr.n == 1 && tz_progresso_tem(&pr, cp->missoes[1].id), "progreso: se recupera al volver a leer el fichero");
	tz_progresso_liberar(&pr);
	remove(ruta);

	/* la misión 1: abrir, correr y llevar al héroe y las variables a la 2 */
	TzMissaoAberta ab;
	VER(tz_missao_abrir(&c, &cp->missoes[0], &ab) == 0 && ab.guiao_ok, "misión 1: se abre con su guion (%s)", ab.motivo);
	TzGuiao *g = &ab.guiao;
	VER(g->nregras == 2 && g->nacoes == 6 && g->nconversas == 1 && g->npers == 1 && g->derrota[1] && !strcmp(g->pers[0].nome, "Arturo") && g->tem_diplomacia && g->jogador == 1,
	    "misión 1: reglas, acciones, conversación, personajes, derrota y diplomacia leídos");
	TzMundo *m = falso_criar(&ab.mapa, g->jogador);
	tz_guiao_registrar(m, g, 1);
	VER(m->reino[1].diplomacia[2] == 0x10 && m->reino[2].diplomacia[1] == 0x10, "misión 1: el .diplomacy del mapa se aplica");
	tics(m, 3);
	VER(g->nmissoes == 1 && var(g, "#kept") == 9 && var(g, "$iniciado") == 1 && tz_guiao_modal(g), "misión 1: la regla de inicio pone el objetivo, las variables y abre la historia");
	tz_guiao_fechar(m, g);
	tics(m, 6);
	VER(g->conv_activa && tz_guiao_fala(g) && strstr(tz_guiao_fala(g)->texto, "Mi se"), "misión 1: tras la historia empieza la conversación de la regla");
	tz_guiao_responder(m, g, 0);
	VER(!g->conv_activa && !tz_guiao_modal(g), "misión 1: la conversación termina al no haber más frases");
	tics(m, 60);
	VER(!tz_guiao_acabou(g, NULL), "misión 1: sin el oro no se gana");
	tz_mundo_guiao_recursos(m, 1, TZ_OURO, 500);
	tics(m, 45);
	int gan = 0;
	VER(tz_guiao_acabou(g, &gan) && gan && !strcmp(tz_guiao_proximo_mapa(g), "MAPS\\02 Final.wmp"), "misión 1: con 1500 de oro la regla gana y apunta a la misión 2");
	int sig = tz_campanha_achar(cp, tz_guiao_proximo_mapa(g));
	VER(sig == 1, "misión 1: el mapa siguiente es la misión 2 de la campaña");
	TzObj *heroi = NULL;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].id_mapa == 1) heroi = &m->obj[i];
	heroi->vida = 33; heroi->nivel = 6;
	static TzLevar lev;
	tz_levar_de(m, g, &lev);
	VER(lev.nherois == 1 && !strcmp(lev.herois[0].nome, "Arturo") && lev.herois[0].vida == 33 && lev.nmantidas == 1, "misión 1: se llevan el héroe y la variable «#»");
	falso_liberar(m);
	tz_missao_fechar(&ab);

	TzMissaoAberta ab2;
	VER(tz_missao_abrir(&c, &cp->missoes[sig], &ab2) == 0 && ab2.guiao_ok, "misión 2: se abre (%s)", ab2.motivo);
	m = falso_criar(&ab2.mapa, ab2.guiao.jogador);
	tz_guiao_registrar(m, &ab2.guiao, 1);
	tz_levar_para(m, &ab2.guiao, &lev);
	tics(m, 3);
	TzObj *h2 = NULL;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].id_mapa == 1) h2 = &m->obj[i];
	VER(h2 && h2->vida == 33 && h2->nivel == 6 && (h2->flags & TZ_F_HEROI), "misión 2: el héroe llega con su vida, nivel y ascenso");
	VER(var(&ab2.guiao, "$herdou") == 9, "misión 2: la variable «#» de la misión 1 está (%d)", var(&ab2.guiao, "$herdou"));
	falso_liberar(m);
	tz_missao_fechar(&ab2);
	tz_mapas_liberar(&c);
}


/* ---------------------------------------------------------------- sonido */
extern void tz_mundo_evento(TzMundo *m, const TzEvento *e);

static double energia(const int16_t *b, int n, int canal)
{
	double s = 0;
	for (int i = 0; i < n; ++i) s += (double)b[i * 2 + canal] * b[i * 2 + canal];
	return s / (n ? n : 1);
}

static void prueba_som(const char *raiz)
{
	/* WAV: 16 bits mono, 8 bits estéreo, cortado y basura */
	{
		uint8_t w[44 + 200];
		memset(w, 0, sizeof w);
		memcpy(w, "RIFF", 4); memcpy(w + 8, "WAVEfmt ", 8);
		w[16] = 16; w[20] = 1; w[22] = 1; w[24] = 0x22; w[25] = 0x56; w[34] = 16;
		memcpy(w + 36, "data", 4); w[40] = 200;
		for (int i = 0; i < 100; ++i) { w[44 + i * 2] = (uint8_t)(i * 3); w[45 + i * 2] = 0x10; }
		int16_t *pcm; int n, taxa;
		VER(tz_som_wav(w, sizeof w, &pcm, &n, &taxa) && n == 100 && taxa == 22050 && pcm[1] == 0x1003, "sonido: WAV de 16 bits mono");
		free(pcm);
		VER(tz_som_wav(w, 44 + 101, &pcm, &n, &taxa) && n == 50, "sonido: un WAV cortado vale hasta donde llega");
		free(pcm);
		w[22] = 2; w[34] = 8; memset(w + 44, 0, 200);
		w[44] = 255; w[45] = 1;
		VER(tz_som_wav(w, sizeof w, &pcm, &n, &taxa) && n == 100 && pcm[0] == ((255 - 128) * 256 + (1 - 128) * 256) / 2, "sonido: WAV de 8 bits estéreo pasa a mono");
		free(pcm);
		w[20] = 2;
		VER(!tz_som_wav(w, sizeof w, &pcm, &n, &taxa) && !tz_som_wav((const uint8_t *)"RIFFbasura", 10, &pcm, &n, &taxa), "sonido: ADPCM y basura se rechazan");
	}
	/* atenuación y elección por pesos */
	{
		int vol, pan;
		tz_som_atenuacao(0, 0, 1024, 1, &vol, &pan);
		VER(vol == 0 && pan == 0, "sonido: sin distancia ni volumen perdido ni panorama");
		tz_som_atenuacao(32, 0, 1024, 1, &vol, &pan);
		VER(vol == -833 && pan == 1600, "sonido: a 32 casillas, -8,3 dB y panorama a la derecha (%d, %d)", vol, pan);
		tz_som_atenuacao(-300, 0, 1024, 1, &vol, &pan);
		VER(vol == -10000 && pan == -10000, "sonido: más allá de -2500 un efecto calla y el panorama se recorta (%d, %d)", vol, pan);
		tz_som_atenuacao(50, 0, 1024, 0, &vol, &pan);
		VER(vol == -312 && pan == 2500, "sonido: las voces atenúan menos (%d)", vol);
		const int pesos[3] = { 100, 200, 300 };
		VER(tz_som_escolher(pesos, 3, 50) == 0 && tz_som_escolher(pesos, 3, 150) == 1 && tz_som_escolher(pesos, 3, 600) == 2 && tz_som_escolher(pesos, 3, 601) == -1,
		    "sonido: la tirada de 0 a 1000 elige el clip por los pesos acumulados");
	}
	if (!raiz) return;
	TzRecursos r;
	if (tz_recursos_abrir(&r, raiz)) { VER(0, "sonido: abrir recursos: %s", tz_erro()); return; }
	TzSom *som = tz_som_criar(&r, NULL, 0);
	VER(som && !tz_som_tem_dispositivo(som), "sonido: se crea sin tarjeta (las pruebas piden el PCM)");
	static int16_t b[2048 * 2];
	tz_som_camara(som, 50, 50, 1024);
	VER(tz_som_tocar(som, "@hand_attack", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 50, 50) == 1 && tz_som_ativas(som) == 1, "sonido: un grupo de SOUNDS.INI suena (los clips con pesos y el «;» del valor)");
	tz_som_misturar(som, b, 2048);
	double el = energia(b, 2048, 0), er = energia(b, 2048, 1);
	VER(el > 1e5 && el == er, "sonido: en el centro suena por igual en los dos altavoces (%.0f)", el);
	tz_som_parar_todos(som);
	tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 50, 50);
	tz_som_misturar(som, b, 2048);
	double cerca = energia(b, 2048, 0);
	tz_som_parar_todos(som);
	tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 70, 50);
	tz_som_misturar(som, b, 2048);
	double lejos = energia(b, 2048, 0);
	VER(lejos < cerca * 0.5 && lejos > 0, "sonido: veinte casillas más allá suena más bajo (%.0f frente a %.0f)", lejos, cerca);
	tz_som_parar_todos(som);
	tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 35, 50);
	tz_som_misturar(som, b, 2048);
	VER(energia(b, 2048, 0) > energia(b, 2048, 1) * 1.5, "sonido: lo de la izquierda suena más por el altavoz izquierdo");
	tz_som_parar_todos(som);
	VER(tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 400, 50) == 0 && tz_som_ativas(som) == 0, "sonido: lo que está muy lejos ni empieza");
	for (int i = 0; i < 9; ++i) tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 50 + i, 50);
	VER(tz_som_ativas(som) == 5, "sonido: una categoría con límite 5 no pasa de 5 voces a la vez (%d)", tz_som_ativas(som));
	VER(tz_som_tocar(som, "SOUND/NO/EXISTE.WAV", 0, TZ_CANAL_EFEITO, -1, -1) == 0, "sonido: un clip que falta no rompe nada");
	/* el habla calla los efectos */
	VER(tz_som_fala(som, "SOUND/NOTIFY/CHAT.WAV") == 0 && tz_som_ativas(som) == 1, "sonido: al hablar callan los efectos");
	tz_som_misturar(som, b, 2048);
	VER(energia(b, 2048, 0) > 1e5, "sonido: la voz suena");
	tz_som_fala(som, NULL);
	VER(tz_som_ativas(som) == 0, "sonido: la voz se calla con NULL");
	/* los clips acaban solos */
	tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 50, 50);
	for (int i = 0; i < 8; ++i) tz_som_misturar(som, b, 2048);
	VER(tz_som_ativas(som) == 0, "sonido: un clip termina y libera su hueco");
	/* película: calla todo y suena su PCM tal cual */
	{
		static int16_t filme[1000 * 2];
		for (int i = 0; i < 2000; ++i) filme[i] = (int16_t)(i % 2 ? -1000 : 1000);
		tz_som_tocar(som, "SOUND/ATTACKS/HAND1.WAV", TZ_CAT_GOLPE, TZ_CANAL_EFEITO, 50, 50);
		tz_som_filme_empurrar(som, filme, 1000);
		tz_som_misturar(som, b, 600);
		int ok = tz_som_filme_pendente(som) == 400;
		for (int i = 0; i < 600; ++i) if (b[i * 2] != 1000 || b[i * 2 + 1] != -1000) ok = 0;
		VER(ok, "sonido: la película suena tal cual y calla el resto");
	}
	/* eventos del mundo: un golpe suena si lo ve el jugador; una orden de otro reino, no */
	{
		TzIni ini;
		memset(&ini, 0, sizeof ini);
		{ const char *t = "[euro]\nsound_attack=hand_attack\nsound_command=hand_attack\n"; tz_ini_ler(&ini, (const uint8_t *)t, (uint32_t)strlen(t)); }
		TzBiblioteca bib;
		memset(&bib, 0, sizeof bib);
		bib.objdefs = &ini;
		char *nomes[1] = { (char *)"footman" };
		bib.objclasse = nomes; bib.nobjdefs = 1;
		TzSom *s2 = tz_som_criar(&r, &bib, 0);
		TzMapa mapa = mapa_prueba();
		TzMundo *m = falso_criar(&mapa, 1);
		m->obj[3].classe = 3;   /* un soldado del reino 2 en (21,20) */
		tz_som_camara(s2, 21, 20, 1024);
		TzEvento e;
		memset(&e, 0, sizeof e);
		e.tipo = TZ_EV_ATACOU; e.reino = 2; e.obj = 3; e.alvo = 0;
		tz_mundo_evento(m, &e);
		tz_som_eventos(s2, m);
		VER(tz_som_ativas(s2) == 1, "sonido: un evento de ataque hace sonar «sound_attack» de la clase");
		memset(&e, 0, sizeof e);
		e.tipo = TZ_EV_ORDEM; e.reino = 2; e.obj = 3;
		tz_mundo_evento(m, &e);
		tz_som_eventos(s2, m);
		VER(tz_som_ativas(s2) == 1, "sonido: la orden de otro reino no suena");
		e.reino = 1;
		tz_mundo_evento(m, &e);
		tz_som_eventos(s2, m);
		VER(tz_som_ativas(s2) == 2, "sonido: la orden del propio reino sí (voz de la unidad)");
		tz_som_eventos(s2, m);
		VER(tz_som_ativas(s2) == 2, "sonido: cada evento suena una sola vez (cursor propio)");
		/* la voz sigue al objeto: si anda hacia la derecha de la vista, suena más por la derecha */
		{
			int16_t mb[2048 * 2];
			tz_som_parar_todos(s2);
			m->obj[3].cx = 21; m->obj[3].cy = 20;
			e.tipo = TZ_EV_ORDEM; e.reino = 1; e.obj = 3;
			tz_mundo_evento(m, &e);
			tz_som_eventos(s2, m);
			tz_som_misturar(s2, mb, 512);
			double l0 = energia(mb, 512, 0), r0 = energia(mb, 512, 1);
			m->obj[3].cx = 21 + 40;
			tz_som_eventos(s2, m);
			tz_som_misturar(s2, mb, 512);
			double l1 = energia(mb, 512, 0), r1 = energia(mb, 512, 1);
			VER(l0 > 0 && r0 > 0 && l0 < r0 * 1.5 && r1 > l1 * 1.5, "sonido: la voz sigue al objeto que anda (centro %.0f/%.0f, a la derecha %.0f/%.0f)", l0, r0, l1, r1);
			m->obj[3].cx = 21;
			tz_som_parar_todos(s2);
		}
		/* sonidos puestos en el mapa (<mapa>.sounds, registros de 112 bytes): uno en bucle en (21,20) y uno de un objeto que no existe */
		{
			uint8_t reg[224];
			memset(reg, 0, sizeof reg);
			strcpy((char *)reg + 0x20, "SOUND/ATTACKS/HAND1.WAV");
			reg[0x60] = 1; reg[0x6a] = 21; reg[0x6c] = 20;
			strcpy((char *)reg + 112 + 0x20, "SOUND/ATTACKS/HAND2.WAV");
			reg[112 + 0x68] = 1; reg[112 + 0x6e] = 0xee; reg[112 + 0x6f] = 0xee;
			VER(tz_som_fontes_mapa(s2, reg, sizeof reg, &mapa) == 1, "sonido: <mapa>.sounds: un sonido en una casilla; el de un objeto que no existe se salta");
			tz_som_eventos(s2, m);
			VER(tz_som_ativas(s2) == 1, "sonido: el sonido del mapa en bucle empieza donde se ve");
			int16_t mb[2048 * 2];
			for (int i = 0; i < 30; ++i) tz_som_misturar(s2, mb, 2048);
			tz_som_eventos(s2, m);
			VER(tz_som_ativas(s2) == 1, "sonido: y sigue en bucle (una sola voz) %d", tz_som_ativas(s2));
			tz_som_fontes_mapa(s2, NULL, 0, NULL);
			tz_som_parar_todos(s2);
		}
		/* la lluvia: empieza, sigue y acaba con sus clips (sin los WAV de lluvia en los datos sintéticos, solo los estados) */
		tz_som_chuva(s2, 1, 0);
		VER(tz_som_chovendo(s2), "sonido: empieza a llover");
		tz_som_chuva(s2, 1, 5000);
		tz_som_chuva(s2, 0, 6000);
		VER(!tz_som_chovendo(s2), "sonido: deja de llover (el clip de acabar)");
		falso_liberar(m);
		tz_som_fechar(s2);
		tz_ini_liberar(&ini);
	}
	/* música MIDI con el sintetizador del sistema */
	{
		TzMusica *mu = tz_som_musica(som);
		VER(tz_musica_sintetizador(), "música: esta compilación trae el sintetizador MIDI");
		VER(tz_musica_tocar(mu, "MUSIC/INTRO.MID", 0) == 0 && tz_musica_a_tocar(mu), "música: se abre y suena MUSIC/INTRO.MID del paquete de sonidos");
		double suma = 0;
		for (int i = 0; i < 40; ++i) { tz_som_misturar(som, b, 2048); suma += energia(b, 2048, 0); }
		VER(suma > 1e5, "música: el sintetizador da sonido (%.0f)", suma);
		for (int i = 0; i < 200 && tz_musica_a_tocar(mu); ++i) tz_som_misturar(som, b, 2048);
		VER(!tz_musica_a_tocar(mu), "música: una canción sin bucle termina");
		tz_musica_menu(mu);
		for (int i = 0; i < 400; ++i) tz_som_misturar(som, b, 2048);
		VER(tz_musica_a_tocar(mu) && !strcmp(tz_musica_atual(mu), "MUSIC/INTRO.MID"), "música: la del menú sigue en bucle pasados 18 segundos");
		tz_musica_jogo(mu);
		VER(tz_musica_a_tocar(mu) && strncmp(tz_musica_atual(mu), "MUSIC/INTRO", 11), "música: en la partida suena una de la lista (%s)", tz_musica_atual(mu));
		for (int i = 0; i < 300 && tz_musica_a_tocar(mu); ++i) tz_som_misturar(som, b, 2048);
		tz_musica_tic(mu, 1000);
		VER(!tz_musica_a_tocar(mu), "música: tras acabar la canción espera la pausa");
		tz_musica_tic(mu, 1000 + 10002);
		VER(tz_musica_a_tocar(mu), "música: pasada la pausa empieza la siguiente");
		tz_musica_resultado(mu, 1);
		VER(!strncmp(tz_musica_atual(mu), "MUSIC/WIN", 9) && tz_musica_a_tocar(mu), "música: al ganar suena la de victoria");
		VER(tz_musica_tocar(mu, "MUSIC/NO.MID", 0) != 0 && tz_musica_tocar_dados(mu, (const uint8_t *)"basura", 6, 0) != 0, "música: lo que falta o no es MIDI se rechaza");
		tz_musica_parar(mu);
		VER(!tz_musica_a_tocar(mu), "música: parar la calla");
	}
	tz_som_fechar(som);
	tz_recursos_fechar(&r);
}

/* ---------------------------------------------------------------- mapas aleatorios */
static uint32_t suma_mapa(const TzMapa *m)
{
	uint32_t h = 2166136261u;
	for (int i = 0; i < m->lado * m->lado; ++i) h = (h ^ m->casillas[i]) * 16777619u;
	for (int i = 0; i < m->nobj; ++i) {
		const TzObjeto *o = &m->obj[i];
		h = (h ^ o->classe) * 16777619u; h = (h ^ o->x) * 16777619u; h = (h ^ o->y) * 16777619u; h = (h ^ o->dono) * 16777619u; h = (h ^ o->id) * 16777619u;
	}
	return h;
}

/* comprueba un mapa generado; devuelve los fallos */
static int checar_mapa(const TzMapa *m, const TzAleatorio *op, const char *rotulo)
{
	int mal = 0, castillos[9] = { 0 }, minas[9] = { 0 };
	int cx[9], cy[9];
	for (int i = 0; i < m->nobj; ++i) {
		const TzObjeto *o = &m->obj[i];
		if (o->x >= m->lado || o->y >= m->lado) { mal++; printf("FALLO %s: objeto %d fuera del mapa\n", rotulo, i); }
		for (int j = 0; j < i; ++j) if (m->obj[j].id == o->id) { mal++; printf("FALLO %s: id repetido %d\n", rotulo, o->id); break; }
		if (o->classe == TZC_CASTLE && o->dono >= 1 && o->dono <= 8) { castillos[o->dono]++; cx[o->dono] = o->x; cy[o->dono] = o->y; }
		if ((o->classe == TZC_RES_GOLD || o->classe == TZC_RES_STONE) && o->ndados >= 62) { uint32_t c = tz_u32(o->dados + 58); if (c < 1000) { mal++; printf("FALLO %s: mina con %u\n", rotulo, c); } }
		if (o->classe == TZC_RES_GOLD) minas[0]++;
	}
	for (int k = 1; k <= op->jogadores; ++k) if (castillos[k] != 1) { mal++; printf("FALLO %s: el reino %d tiene %d castillos\n", rotulo, k, castillos[k]); }
	for (int a = 1; a <= op->jogadores; ++a)
		for (int b = a + 1; b <= op->jogadores; ++b)
			if (abs(cx[a] - cx[b]) < 8 && abs(cy[a] - cy[b]) < 8) { mal++; printf("FALLO %s: los castillos %d y %d se pisan\n", rotulo, a, b); }
	if (minas[0] < op->jogadores) { mal++; printf("FALLO %s: faltan minas de oro (%d)\n", rotulo, minas[0]); }
	for (int k = 0; k < op->jogadores; ++k) if (!m->reinos[k].ativo) { mal++; printf("FALLO %s: reino %d inactivo\n", rotulo, k); }
	return mal;
}

static void prueba_aleatorio(const char *raiz, int verboso)
{
	uint64_t st = 0;
	VER(tz_aleatorio_proximo(&st) == 0xe220a8397b1dcdafull, "aleatorio: SplitMix64 da el valor conocido para la semilla 0");
	static uint8_t a[64 * 64], b[64 * 64], c[64 * 64];
	tz_aleatorio_plasma(64, 5, 5, 2, a); tz_aleatorio_plasma(64, 5, 5, 2, b); tz_aleatorio_plasma(64, 6, 5, 2, c);
	int min = 255, max = 0;
	for (int i = 0; i < 64 * 64; ++i) { if (a[i] < min) min = a[i]; if (a[i] > max) max = a[i]; }
	VER(!memcmp(a, b, sizeof a) && memcmp(a, c, sizeof a) && max - min > 100, "aleatorio: el plasma es determinista, cambia con la semilla y reparte alturas (%d..%d)", min, max);
	if (!raiz) return;
	TzRecursos r;
	if (tz_recursos_abrir(&r, raiz)) { VER(0, "aleatorio: abrir recursos: %s", tz_erro()); return; }
	TzAleatorio op;
	int mal = 0, hechos = 0;
	static const int lados[] = { 64, 128, 192, 256 };
	for (unsigned li = 0; li < sizeof lados / sizeof *lados; ++li)
		for (int jog = 2; jog <= 8; ++jog) {
			if (lados[li] == 64 && jog > 4) continue;
			tz_aleatorio_padrao(&op);
			op.lado = lados[li]; op.jogadores = jog; op.humano = jog - 1; op.semente = 100u + (unsigned)jog;
			op.tipo = jog % 6; op.recursos_mapa = jog % 3;
			TzMapa m1, m2;
			char rot[80];
			snprintf(rot, sizeof rot, "lado %d, %d reinos", op.lado, jog);
			if (tz_mapa_aleatorio(&m1, &r, NULL, &op)) { mal++; printf("FALLO %s: %s\n", rot, tz_erro()); continue; }
			hechos++;
			mal += checar_mapa(&m1, &op, rot);
			if (tz_mapa_aleatorio(&m2, &r, NULL, &op) || suma_mapa(&m1) != suma_mapa(&m2)) { mal++; printf("FALLO %s: no es determinista\n", rot); }
			else tz_mapa_liberar(&m2);
			if (verboso) printf("   %s: %d objetos, suma %08x\n", rot, m1.nobj, suma_mapa(&m1));
			if (jog == 2) {
				op.semente++;
				if (!tz_mapa_aleatorio(&m2, &r, NULL, &op)) { if (suma_mapa(&m1) == suma_mapa(&m2)) { mal++; printf("FALLO %s: otra semilla da el mismo mapa\n", rot); } tz_mapa_liberar(&m2); }
			}
			tz_mapa_liberar(&m1);
		}
	VER(mal == 0 && hechos >= 20, "aleatorio: %d mapas de 64 a 256 casillas y de 2 a 8 reinos (castillo único y separado, minas, ids, determinismo)", hechos);
	/* guardar y cargar: los parámetros van a una línea de texto y regeneran el mismo mapa (v8.5.5.2) */
	{
		TzAleatorio a1, a2;
		tz_aleatorio_padrao(&a1);
		a1.lado = 96; a1.jogadores = 5; a1.humano = 3; a1.tipo = 2; a1.recursos_mapa = 2; a1.nivel_recursos = 0; a1.guardado = 0; a1.animais = 0;
		a1.semente = 0x123456789abcdefull; snprintf(a1.bioma, sizeof a1.bioma, "grass");
		for (int i = 0; i < 8; ++i) { a1.equipe[i] = (uint8_t)(i % 3); a1.civ[i] = (uint8_t)(i % 4); }
		char t[320];
		int ok = !tz_aleatorio_texto(&a1, t, sizeof t) && !tz_aleatorio_de_texto(t, &a2) && !memcmp(&a1, &a2, sizeof a1);
		VER(ok, "aleatorio: los parámetros se guardan en texto y vuelven iguales («%.60s...»)", t);
		TzMapa m1, m2;
		int e1 = tz_mapa_aleatorio(&m1, &r, NULL, &a1); if (e1) printf("   (%s)\n", tz_erro());
		ok = !e1 && !tz_mapa_aleatorio(&m2, &r, NULL, &a2) && suma_mapa(&m1) == suma_mapa(&m2) && m1.nobj == m2.nobj;
		VER(ok, "aleatorio: el mapa regenerado desde el texto es el mismo (%d objetos)", ok ? m1.nobj : -1);
		if (ok) { tz_mapa_liberar(&m1); tz_mapa_liberar(&m2); }
		/* puertas mágicas y objetos al azar (v8.5.5.2) */
		{
			TzAleatorio a3;
			tz_aleatorio_padrao(&a3);
			a3.lado = 128; a3.jogadores = 4; a3.semente = 77;
			TzMapa mm;
			int puertas = 0, libros = 0, pociones = 0, equipo = 0, mal3 = 0, aguilas = 0, banderas = 0;
			if (!tz_mapa_aleatorio(&mm, &r, NULL, &a3)) {
				for (int i = 0; i < mm.nobj; ++i) {
					const TzObjeto *o = &mm.obj[i];
					if (o->classe == 72) puertas++;
					if (o->classe == 119) aguilas++;
					if (o->classe == 135) banderas++;
					if (o->classe == TZC_LIVRO || o->classe == TZC_POCAO || o->classe == TZC_ITEM_MAIS) {
						int ok = o->ndados == 106 && o->dados[56] == 'i' && o->vida == 0 && o->vida_max == 0;
						if (o->classe == TZC_LIVRO) { libros++; ok = ok && o->dados[96] < 21 && o->dados[99] == 1 && tz_u16(o->dados + 0x66) >= 1 && tz_u16(o->dados + 0x66) <= 3; }
						else if (o->classe == TZC_POCAO) { pociones++; ok = ok && (tz_u16(o->dados + 0x12) + tz_u16(o->dados + 0x24) + tz_u16(o->dados + 0x1c) + o->dados[0x16] + tz_u16(o->dados + 0x18)) >= 2; }
						else { equipo++; ok = ok && (o->dados[0x16] + o->dados[0x17] + tz_u16(o->dados + 0x18)) >= 5; }
						if (!ok) mal3++;
					}
				}
				tz_mapa_liberar(&mm);
			}
			VER(puertas == 2 && libros >= 5 && pociones >= 5 && equipo >= 5 && !mal3 && aguilas >= 1 && !banderas,
			    "aleatorio: 4 reinos dan 2 puertas mágicas, objetos al azar con sus cifras (%d libros, %d pociones, %d equipos, %d mal), águilas %d y ninguna bandera de objeto %d",
			    libros, pociones, equipo, mal3, aguilas, banderas);
			a3.portas = 0; a3.itens = 0;
			int sin = 0;
			if (!tz_mapa_aleatorio(&mm, &r, NULL, &a3)) { for (int i = 0; i < mm.nobj; ++i) sin += mm.obj[i].classe == 72 || mm.obj[i].classe == TZC_LIVRO || mm.obj[i].classe == TZC_POCAO || mm.obj[i].classe == TZC_ITEM_MAIS; tz_mapa_liberar(&mm); }
			VER(sin == 0, "aleatorio: sin las opciones no hay puertas ni objetos al azar");
		}
		const char *malos[] = { "", "ALEAT", "ALEAT 96 5 3 2 2 0 0 0 grass 5", "ALEAT 100 5 3 2 2 0 0 0 grass 5 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
		                        "ALEAT 96 1 0 2 2 0 0 0 grass 5 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", "ALEAT 96 5 7 2 2 0 0 0 snow 5 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
		                        "ALEAT 96 5 3 2 2 0 0 0 grass 5 0 0 0 0 0 0 0 0 9 0 0 0 0 0 0 0", "otra cosa 1 2 3" };
		int rechazados = 0;
		for (unsigned i = 0; i < sizeof malos / sizeof *malos; ++i) rechazados += tz_aleatorio_de_texto(malos[i], &a2) != 0;
		VER(rechazados == (int)(sizeof malos / sizeof *malos), "aleatorio: el texto dañado, incompleto o fuera de rango se rechaza (%d de %d)", rechazados, (int)(sizeof malos / sizeof *malos));
	}
	tz_aleatorio_padrao(&op);
	op.lado = 100;
	TzMapa mx;
	VER(tz_mapa_aleatorio(&mx, &r, NULL, &op) != 0, "aleatorio: un lado que no es de la lista se rechaza");
	op.lado = 64; op.jogadores = 1;
	VER(tz_mapa_aleatorio(&mx, &r, NULL, &op) != 0, "aleatorio: un solo reino se rechaza");
	tz_aleatorio_padrao(&op);
	snprintf(op.bioma, sizeof op.bioma, "lava");
	VER(tz_mapa_aleatorio(&mx, &r, NULL, &op) != 0, "aleatorio: un bioma que no existe se rechaza");
	/* v8.5.5.2: cada opción de la pantalla del mapa aleatorio cambia el mapa como debe */
	{
		TzAleatorio o;
		TzMapa mm;
		int ok;
		/* tamaño */
		static const int lados2[] = { 64, 96, 128, 192, 256 };
		ok = 1;
		for (unsigned i = 0; i < sizeof lados2 / sizeof *lados2; ++i) {
			tz_aleatorio_padrao(&o); o.lado = lados2[i]; o.semente = 9;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &o)) { ok = 0; continue; }
			ok = ok && mm.lado == lados2[i] && mm.nobj > 10;
			tz_mapa_liberar(&mm);
		}
		VER(ok, "opciones aleatorias: el tamaño elegido es el del mapa (64, 96, 128, 192 y 256)");
		/* número de reinos colocados */
		ok = 1;
		for (int jog = 2; jog <= 8; ++jog) {
			tz_aleatorio_padrao(&o); o.lado = 192; o.jogadores = jog; o.semente = 31;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &o)) { ok = 0; continue; }
			int cast[10] = { 0 }, total = 0, act = 0;
			for (int i = 0; i < mm.nobj; ++i) if (mm.obj[i].classe == TZC_CASTLE && mm.obj[i].dono >= 1 && mm.obj[i].dono <= 9) { cast[mm.obj[i].dono]++; total++; }
			for (int k = 0; k < 8; ++k) act += mm.reinos[k].ativo;
			for (int k = 1; k <= jog; ++k) ok = ok && cast[k] == 1;
			ok = ok && total == jog && act == jog;
			tz_mapa_liberar(&mm);
		}
		VER(ok, "opciones aleatorias: de 2 a 8 reinos, cada uno con su castillo y los demás inactivos");
		/* recursos del mapa: más minas (cantidad total) de «pocos» a «muchos» */
		long suma_min[3] = { 0, 0, 0 };
		for (int rm = 0; rm < 3; ++rm) {
			tz_aleatorio_padrao(&o); o.lado = 192; o.jogadores = 4; o.semente = 55; o.recursos_mapa = rm;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &o)) continue;
			for (int i = 0; i < mm.nobj; ++i) if ((mm.obj[i].classe == TZC_RES_GOLD || mm.obj[i].classe == TZC_RES_STONE) && mm.obj[i].ndados >= 62) suma_min[rm] += (long)tz_u32(mm.obj[i].dados + 58);
			tz_mapa_liberar(&mm);
		}
		VER(suma_min[0] > 0 && suma_min[0] < suma_min[1] && suma_min[1] < suma_min[2], "opciones aleatorias: recursos del mapa pocos < normales < muchos (%ld, %ld, %ld)", suma_min[0], suma_min[1], suma_min[2]);
		/* recursos iniciales: bajo < medio < alto en el reino */
		long ini[3] = { 0, 0, 0 };
		for (int nv = 0; nv < 3; ++nv) {
			tz_aleatorio_padrao(&o); o.lado = 128; o.semente = 56; o.nivel_recursos = nv;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &o)) continue;
			for (int j = 0; j < 4; ++j) ini[nv] += (long)mm.reinos[0].recursos[j];
			tz_mapa_liberar(&mm);
		}
		VER(ini[0] > 0 && ini[0] < ini[1] && ini[1] < ini[2], "opciones aleatorias: recursos iniciales bajo < medio < alto (%ld, %ld, %ld)", ini[0], ini[1], ini[2]);
		/* agua: archipiélagos tienen mucha, «tierra» ninguna */
		int agua[6] = { 0 };
		for (int tp = 1; tp <= 5; ++tp) {
			tz_aleatorio_padrao(&o); o.lado = 192; o.jogadores = 2; o.semente = 77; o.tipo = tp;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &o)) continue;
			for (int i = 0; i < mm.lado * mm.lado; ++i) agua[tp] += (mm.casillas[i] >> 7) == 0;
			tz_mapa_liberar(&mm);
		}
		VER(agua[1] > 192 * 192 / 10 && agua[5] == 0 && agua[1] > agua[3], "opciones aleatorias: agua según el tipo (archipiélagos %d, islas %d, continente %d, lagos %d, tierra %d)", agua[1], agua[2], agua[3], agua[4], agua[5]);
		/* torres de guardia, animales, puertas y objetos: con la opción apagada hay menos objetos */
		int nobj_[2][4] = { { 0 } };
		for (int enc = 0; enc < 2; ++enc) {
			TzAleatorio b;
			tz_aleatorio_padrao(&b); b.lado = 192; b.jogadores = 4; b.semente = 91;
			b.guardado = b.animais = b.portas = b.itens = enc;
			if (tz_mapa_aleatorio(&mm, &r, NULL, &b)) continue;
			nobj_[enc][0] = mm.nobj;
			tz_mapa_liberar(&mm);
		}
		VER(nobj_[0][0] > 0 && nobj_[1][0] > nobj_[0][0], "opciones aleatorias: guardias, animales, puertas y objetos añaden objetos (%d sin, %d con)", nobj_[0][0], nobj_[1][0]);
		for (int opc = 0; opc < 4; ++opc) {   /* cada una por separado */
			TzAleatorio b, c0;
			tz_aleatorio_padrao(&b); b.lado = 192; b.jogadores = 4; b.semente = 91;
			b.guardado = b.animais = b.portas = b.itens = 0;
			c0 = b;
			if (opc == 0) b.guardado = 1; else if (opc == 1) b.animais = 1; else if (opc == 2) b.portas = 1; else b.itens = 1;
			TzMapa m0;
			int n0 = -1, n1 = -1;
			if (!tz_mapa_aleatorio(&m0, &r, NULL, &c0)) { n0 = m0.nobj; tz_mapa_liberar(&m0); }
			if (!tz_mapa_aleatorio(&m0, &r, NULL, &b)) { n1 = m0.nobj; tz_mapa_liberar(&m0); }
			VER(n0 > 0 && n1 > n0, "opciones aleatorias: sola la opción «%s» añade objetos (%d -> %d)", opc == 0 ? "guardias" : opc == 1 ? "animales" : opc == 2 ? "puertas" : "objetos", n0, n1);
		}
		/* equipos: los del mismo equipo son aliados, los solos no */
		tz_aleatorio_padrao(&o); o.lado = 192; o.jogadores = 4; o.semente = 33;
		o.equipe[0] = 1; o.equipe[1] = 1; o.equipe[2] = 2; o.equipe[3] = 0;
		if (!tz_mapa_aleatorio(&mm, &r, NULL, &o)) {
			VER(mm.reinos[0].equipe == mm.reinos[1].equipe && mm.reinos[0].equipe != mm.reinos[2].equipe && mm.reinos[3].equipe != mm.reinos[0].equipe &&
			    mm.reinos[3].equipe != mm.reinos[2].equipe && mm.reinos[2].equipe != mm.reinos[1].equipe,
			    "opciones aleatorias: equipos (1 y 2 aliados, 3 y 4 solos): %d %d %d %d", mm.reinos[0].equipe, mm.reinos[1].equipe, mm.reinos[2].equipe, mm.reinos[3].equipe);
			tz_mapa_liberar(&mm);
		}
		/* civilización de cada reino */
		tz_aleatorio_padrao(&o); o.lado = 128; o.jogadores = 3; o.semente = 34; o.civ[0] = 3; o.civ[1] = 2; o.civ[2] = 1;
		if (!tz_mapa_aleatorio(&mm, &r, NULL, &o)) { VER(mm.reinos[0].civ == 3 && mm.reinos[1].civ == 2 && mm.reinos[2].civ == 1, "opciones aleatorias: la civilización de cada reino"); tz_mapa_liberar(&mm); }
		/* la misma semilla y las mismas opciones dan el mismo mapa; otra semilla u otra opción, otro */
		TzMapa ma, mb;
		tz_aleatorio_padrao(&o); o.lado = 192; o.jogadores = 5; o.tipo = 3; o.recursos_mapa = 2; o.semente = 4242; o.equipe[1] = 1; o.civ[2] = 2;
		ok = !tz_mapa_aleatorio(&ma, &r, NULL, &o) && !tz_mapa_aleatorio(&mb, &r, NULL, &o) && suma_mapa(&ma) == suma_mapa(&mb);
		VER(ok, "opciones aleatorias: la misma semilla con las mismas opciones da el mismo mapa");
		if (ok) {
			uint32_t base = suma_mapa(&ma);
			tz_mapa_liberar(&mb);
			int distintos = 0, total = 0;
			for (int v = 0; v < 6; ++v) {
				TzAleatorio o2 = o;
				if (v == 0) o2.semente++; else if (v == 1) o2.tipo = 1; else if (v == 2) o2.jogadores = 6; else if (v == 3) o2.recursos_mapa = 0; else if (v == 4) o2.lado = 256; else o2.animais = 0;
				if (tz_mapa_aleatorio(&mb, &r, NULL, &o2)) continue;
				total++; distintos += suma_mapa(&mb) != base;
				tz_mapa_liberar(&mb);
			}
			VER(total == 6 && distintos == 6, "opciones aleatorias: cambiar la semilla o cualquier opción cambia el mapa (%d de %d)", distintos, total);
			tz_mapa_liberar(&ma);
		}
		/* guardar y cargar: una línea con todas las opciones y el mapa regenerado es el mismo */
		tz_aleatorio_padrao(&o); o.lado = 256; o.jogadores = 6; o.humano = 0; o.tipo = 4; o.recursos_mapa = 0; o.nivel_recursos = 2; o.guardado = 0; o.animais = 1; o.portas = 0; o.itens = 1;
		o.semente = 987654321987ull; snprintf(o.bioma, sizeof o.bioma, "grass");   /* los datos sintéticos solo traen hierba */
		for (int i = 0; i < 6; ++i) { o.equipe[i] = (uint8_t)(i / 2 + (i & 1)); o.civ[i] = (uint8_t)(i % 4); }
		char tx[320]; TzAleatorio o3;
		int cargado = !tz_aleatorio_texto(&o, tx, sizeof tx) && !tz_aleatorio_de_texto(tx, &o3) && !memcmp(&o, &o3, sizeof o);
		VER(cargado, "opciones aleatorias: guardar y cargar conserva tamaño, tipo, recursos, equipos, civilizaciones y las casillas");
		int g1 = cargado ? tz_mapa_aleatorio(&ma, &r, NULL, &o) : -1;
		if (g1) printf("   (%s)\n", tz_erro());
		if (!g1 && !tz_mapa_aleatorio(&mb, &r, NULL, &o3)) {
			VER(suma_mapa(&ma) == suma_mapa(&mb) && ma.reinos[2].equipe == mb.reinos[2].equipe && ma.reinos[3].civ == mb.reinos[3].civ,
			    "opciones aleatorias: el mapa cargado es el mismo (equipos y civilizaciones incluidos)");
			tz_mapa_liberar(&ma); tz_mapa_liberar(&mb);
		}
	}
	tz_recursos_fechar(&r);
}

/* ---------------------------------------------------------------- privado: la instalación real */
static int privado(const char *raiz);

#ifdef TZ_REAL
static int real(const char *raiz);
static int real_aleatorios(TzRecursos *rec);
#endif

int main(int argc, char **argv)
{
#ifdef TZ_REAL
	if (argc >= 3 && !strcmp(argv[1], "real")) return real(argv[2]) ? 1 : 0;
#endif
	if (argc >= 3 && !strcmp(argv[1], "privado")) return privado(argv[2]) ? 1 : 0;
	if (argc >= 2 && !strcmp(argv[1], "sintetico")) {
		if (argc >= 3) inst_campanhas = argv[2];
		prueba_expresiones();
		prueba_guion();
		prueba_final();
		prueba_danados();
		prueba_campanha();
		prueba_aleatorio(inst_campanhas, 0);
		prueba_som(inst_campanhas);
		printf("%s sintético: %d fallos\n", fallos ? "FALLO" : "OK", fallos);
		return fallos ? 1 : 0;
	}
	fprintf(stderr, "uso: tzar_campanas sintetico | privado <carpeta>\n");
	return 2;
}

/* ---------------------------------------------------------------- privado */


typedef struct { int misiones, fallos_lectura, guion_malo, acciones_malas, convs, frases, reglas, acoes, disparadas, ejecutadas, acabaron, ganaron, ventanas, conversas_corridas, avisos; double ms; } Totales;

/* corre una misión contra el mundo falso: inactiva y luego con los rivales muertos y mucho oro, contestando las ventanas */
static void correr(TzMissaoAberta *ab, Totales *t, const char *nome, int verbose)
{
	TzGuiao *g = &ab->guiao;
	for (int pasada = 0; pasada < 2; ++pasada) {
		TzMapa *mapa = &ab->mapa;
		TzMundo *m = falso_criar(mapa, g->jogador);
		TzGuiao copia;
		/* la pasada 2 vuelve a leer el guion para empezar de cero */
		TzGuiao *gg = g;
		if (pasada) {
			if (tz_guiao_ler(&copia, ab->tem_sub ? &ab->sub : &ab->c)) { falso_liberar(m); continue; }
			gg = &copia;
		}
		tz_guiao_registrar(m, gg, 1);
		int tics_n = pasada ? 1500 : 6000;
		clock_t t0 = clock();
		for (int i = 0; i < tics_n; ++i) {
			if (pasada && i == 100) {
				for (int k = 0; k < m->nobj; ++k)
					if (m->obj[k].dono != m->jogador && m->obj[k].dono >= 1 && m->obj[k].dono <= 8) tz_mundo_guiao_matar(m, k);
				for (int r = 0; r < 4; ++r) tz_mundo_guiao_recursos(m, m->jogador, r, 100000);
			}
			falso_tic(m);
			TzPedido p;
			while (tz_guiao_pedido(gg, &p)) {
				if (p.tipo == TZ_PD_HISTORIA || p.tipo == TZ_PD_AVISO_JANELA) t->ventanas++;
				if (p.tipo == TZ_PD_FALA) t->conversas_corridas++;
			}
			if (tz_guiao_modal(gg)) {
				if (gg->conv_activa) tz_guiao_responder(m, gg, i % 3 == 0 ? 0 : gg->nescolhas_conv - 1);
				else tz_guiao_fechar(m, gg);
			}
		}
		t->ms += (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
		t->disparadas += (int)gg->n_regras_disparadas;
		t->ejecutadas += (int)gg->n_acoes;
		int gan = 0;
		if (tz_guiao_acabou(gg, &gan)) { t->acabaron++; t->ganaron += gan; }
		if (verbose) printf("   %-40s pasada %d: reglas disparadas %u, acciones %u, final %d, ventanas, modal %d\n", nome, pasada, gg->n_regras_disparadas, gg->n_acoes, gg->acabou, gg->modal);
		falso_liberar(m);
		if (pasada) tz_guiao_liberar(&copia);
	}
}

static void privado_missao(TzMapas *c, const TzMissao *mi, Totales *t, int verbose)
{
	TzMissaoAberta ab;
	t->misiones++;
	if (tz_missao_abrir(c, mi, &ab)) { t->fallos_lectura++; printf("FALLO abrir %s %s: %s\n", mi->fich, mi->dentro, tz_erro()); return; }
	if (!ab.guiao_ok) { t->guion_malo++; printf("FALLO guion de %s %s: %s\n", mi->fich, mi->dentro, ab.motivo); }
	TzGuiao *g = &ab.guiao;
	t->reglas += g->nregras; t->acoes += g->nacoes;
	for (int i = 0; i < g->nconversas; ++i) { t->convs++; t->frases += g->conversas[i].nfrases; }
	/* cada regla debe nombrar acciones que existen */
	int malas = 0;
	for (int i = 0; i < g->nregras; ++i) for (int k = 0; k < g->regras[i].nacoes; ++k) if (g->regras[i].acoes[k] < 0) malas++;
	t->acciones_malas += malas;
	if (malas) printf("AVISO %s %s: %d nombres de acción sin acción\n", mi->fich, mi->dentro, malas);
	/* cada frase enlaza con frases que existen */
	for (int i = 0; i < g->nconversas; ++i)
		for (int j = 0; j < g->conversas[i].nfrases; ++j)
			for (int k = 0; k < g->conversas[i].frases[j].nseg; ++k) {
				int ok = 0;
				for (int q = 0; q < g->conversas[i].nfrases; ++q) if (g->conversas[i].frases[q].indice == g->conversas[i].frases[j].seg[k]) ok = 1;
				if (!ok) { t->avisos++; printf("AVISO %s %s: la frase %d de «%s» enlaza con una que no existe\n", mi->fich, mi->dentro, g->conversas[i].frases[j].indice, g->conversas[i].nome); }
			}
	char nome[300];
	snprintf(nome, sizeof nome, "%s|%s", mi->fich, mi->dentro[0] ? mi->dentro : "-");
	unsigned av0 = tz_expr_avisos;
	correr(&ab, t, nome, verbose);
	if (tz_expr_avisos != av0) { t->avisos += (int)(tz_expr_avisos - av0); printf("AVISO %s: %u expresiones con paréntesis o operadores raros\n", nome, tz_expr_avisos - av0); }
	tz_missao_fechar(&ab);
}


/* privado: todos los sonidos y músicas de la instalación real */
static int privado_som(const char *raiz)
{
	TzRecursos r;
	if (tz_recursos_abrir(&r, raiz)) { printf("FALLO sonido: recursos: %s\n", tz_erro()); return 1; }
	int mal = 0, wavs = 0, midis = 0, grupos = 0, clips = 0, sin_clip = 0;
	double seg_total = 0;
	TzSom *som = tz_som_criar(&r, NULL, 0);
	TzMusica *mu = tz_som_musica(som);
	static int16_t b[2048 * 2];
	for (int i = 0; i < r.nnomes; ++i) {
		const char *n = r.nomes[i];
		size_t l = strlen(n);
		if (!strncmp(n, "SOUND/", 6) && l > 4 && !strcmp(n + l - 4, ".WAV")) {
			uint32_t nb;
			uint8_t *d = tz_recursos_ler(&r, n, &nb);
			int16_t *pcm; int q, taxa;
			if (!d || !tz_som_wav(d, nb, &pcm, &q, &taxa)) { mal++; printf("FALLO WAV %s\n", n); }
			else { wavs++; seg_total += (double)q / taxa; free(pcm); }
			free(d);
		} else if (!strncmp(n, "MUSIC/", 6) && l > 4 && !strcmp(n + l - 4, ".MID")) {
			int pico = 0, frames = 0;
			if (tz_musica_tocar(mu, n, 0)) { mal++; printf("FALLO MIDI %s: %s\n", n, tz_erro()); continue; }
			while (tz_musica_a_tocar(mu) && frames < 44100 * 600) {
				tz_som_misturar(som, b, 2048);
				frames += 2048;
				for (int k = 0; k < 4096; ++k) if (abs(b[k]) > pico) pico = abs(b[k]);
			}
			if (pico < 500 || frames < 44100) { mal++; printf("FALLO MIDI %s: pico %d, %d cuadros\n", n, pico, frames); }
			else { midis++; printf("   %s: %.0f s, pico %d\n", n, (double)frames / 44100, pico); }
		}
	}
	/* los grupos de SOUNDS.INI: cada clip existe */
	TzIni ini;
	memset(&ini, 0, sizeof ini);
	if (tz_ini_recurso(&r, &ini, "DATA/SOUNDS.INI")) { mal++; printf("FALLO sonido: DATA/SOUNDS.INI\n"); }
	else {
		for (int i = 0; i < ini.ns; ++i) {
			if (!strcmp(ini.s[i].nome, "priority")) continue;
			grupos++;
			for (int k = 0; k < ini.s[i].n; ++k) {
				char n[200];
				snprintf(n, sizeof n, "SOUND/%s", ini.s[i].p[k].chave);
				if (atoi(ini.s[i].p[k].valor) <= 0) continue;
				clips++;
				if (!tz_recursos_existe(&r, n)) { sin_clip++; if (sin_clip < 12) printf("AVISO el grupo [%s] nombra un clip que no está: %s\n", ini.s[i].nome, n); }
			}
		}
		tz_ini_liberar(&ini);
	}
	tz_som_fechar(som);
	tz_recursos_fechar(&r);
	printf("%s sonido real: %d WAV (%.0f s en total), %d MIDI, %d grupos con %d clips (%d sin fichero)\n", mal ? "FALLO" : "OK", wavs, seg_total, midis, grupos, clips, sin_clip);
	return mal;
}

static int privado(const char *raiz)
{
	int mal_som = privado_som(raiz);
	TzMapas c;
	int falhas = 0;
	if (tz_mapas_ler(&c, raiz, &falhas)) { printf("FALLO no hay carpeta %s\n", raiz); return 1; }
	Totales t;
	memset(&t, 0, sizeof t);
	int verbose = getenv("TZAR_VERBOSO") != NULL;
	printf("%s catálogo: %d mapas sueltos, %d campañas, %d fallos de lectura\n", falhas ? "FALLO" : "OK", c.nsoltas, c.ncamp, falhas);
	for (int i = 0; i < c.nsoltas; ++i) privado_missao(&c, &c.soltas[i], &t, verbose);
	for (int k = 0; k < c.ncamp; ++k) {
		printf("campaña «%s» (%s): %d misiones\n", c.camp[k].titulo, c.camp[k].ficheiro, c.camp[k].nmissoes);
		for (int i = 0; i < c.camp[k].nmissoes; ++i) {
			if (verbose) printf("  %d. %s [%s]\n", i + 1, c.camp[k].missoes[i].titulo, c.camp[k].missoes[i].dentro);
			privado_missao(&c, &c.camp[k].missoes[i], &t, verbose);
		}
	}
	int mal = falhas || t.fallos_lectura || t.guion_malo || mal_som;
	printf("%s privado: %d misiones, %d reglas, %d acciones, %d conversaciones (%d frases); %d reglas disparadas y %d acciones ejecutadas; "
	       "%d finales (%d victorias), %d ventanas y %d frases dichas; %d nombres de acción sin acción, %d avisos; %.0f ms de guion\n",
	       mal ? "FALLO" : "OK", t.misiones, t.reglas, t.acoes, t.convs, t.frases, t.disparadas, t.ejecutadas, t.acabaron, t.ganaron, t.ventanas,
	       t.conversas_corridas, t.acciones_malas, t.avisos, t.ms);
	tz_mapas_liberar(&c);
	return mal;
}

#ifdef TZ_REAL
/* ---------------------------------------------------------------- real: las misiones contra la simulación de verdad y el sonido */

/* mapas aleatorios con los datos reales: se generan, se abren en la simulación real y corren unos tics con la partida de prueba */
static int real_aleatorios(TzRecursos *rec)
{
	int mal = 0, hechos = 0, tics_n = 0;
	static const char *const biomas[] = { "grass", "summer", "snow", "desert" };
	for (int b = 0; b < 4; ++b)
		for (int jog = 2; jog <= 8; jog += (jog == 2 ? 2 : 3)) {
			TzAleatorio op;
			tz_aleatorio_padrao(&op);
			op.lado = jog == 2 ? 128 : 256; op.jogadores = jog; op.humano = 0; op.tipo = (b + jog) % 6; op.semente = 1000u + (unsigned)(b * 10 + jog);
			snprintf(op.bioma, sizeof op.bioma, "%s", biomas[b]);
			TzBiblioteca bib;
			TzCatalogo cat;
			if (tz_biblioteca_carregar(&bib, rec, op.bioma)) { mal++; printf("FALLO aleatorio %s: biblioteca: %s\n", op.bioma, tz_erro()); continue; }
			if (tz_catalogo_carregar(&cat, rec, &bib)) { mal++; printf("FALLO aleatorio %s: catálogo: %s\n", op.bioma, tz_erro()); tz_biblioteca_liberar(&bib); continue; }
			TzMapa m;
			if (tz_mapa_aleatorio(&m, rec, &cat, &op)) { mal++; printf("FALLO aleatorio %s %d reinos: %s\n", op.bioma, jog, tz_erro()); tz_catalogo_liberar(&cat); tz_biblioteca_liberar(&bib); continue; }
			char rot[80];
			snprintf(rot, sizeof rot, "real %s lado %d, %d reinos", op.bioma, op.lado, jog);
			mal += checar_mapa(&m, &op, rot);
			TzMundo *w = calloc(1, sizeof *w);
			TzRegras rg;
			memset(&rg, 0, sizeof rg);
			rg.populacao = 200;
			if (tz_mundo_criar(w, &cat, &m, &rg, 417)) { mal++; printf("FALLO %s: tz_mundo_criar: %s\n", rot, tz_erro()); }
			else {
				for (int t = 0; t < 300; ++t) { tz_mundo_tic(w); tics_n++; }
				hechos++;
				tz_mundo_liberar(w);
			}
			free(w);
			tz_mapa_liberar(&m);
			tz_catalogo_liberar(&cat);
			tz_biblioteca_liberar(&bib);
		}
	printf("%s aleatorio real: %d mapas de 4 biomas abiertos en la simulación real, %d tics\n", mal || !hechos ? "FALLO" : "OK", hechos, tics_n);
	return mal;
}

static int real(const char *raiz)
{
	TzMapas c;
	int falhas = 0;
	TzRecursos rec;
	if (tz_recursos_abrir(&rec, raiz)) { printf("FALLO recursos: %s\n", tz_erro()); return 1; }
	prueba_aleatorio(raiz, getenv("TZAR_VERBOSO") != NULL);
	if (!getenv("TZAR_SOLO") && real_aleatorios(&rec)) { printf("FALLO aleatorios\n"); }
	if (tz_mapas_ler(&c, raiz, &falhas)) return 1;
	int mal = falhas, misiones = 0, ganadas = 0, acabadas = 0, ventanas = 0, tics_tot = 0, sonidos = 0, picos = 0;
	int solo = getenv("TZAR_SOLO") ? atoi(getenv("TZAR_SOLO")) : -1;
	double ms_guion = 0;
	int n_tot = 0;
	for (int k = 0; k < c.ncamp; ++k)
		for (int i = 0; i < c.camp[k].nmissoes; ++i) {
			if (solo >= 0 && n_tot++ != solo) continue;
			const TzMissao *mi = &c.camp[k].missoes[i];
			TzMissaoAberta ab;
			if (tz_missao_abrir(&c, mi, &ab)) { printf("FALLO abrir %s: %s\n", mi->dentro, tz_erro()); mal++; continue; }
			TzBiblioteca bib;
			TzPartida pt;
			if (tz_biblioteca_carregar(&bib, &rec, ab.mapa.bioma)) { printf("FALLO biblioteca %s: %s\n", mi->dentro, tz_erro()); mal++; tz_missao_fechar(&ab); continue; }
			if (tz_partida_iniciar(&pt, &rec, &bib, &ab.mapa, 417)) { printf("FALLO partida %s: %s\n", mi->dentro, tz_erro()); mal++; tz_biblioteca_liberar(&bib); tz_missao_fechar(&ab); continue; }
			misiones++;
			TzGuiao *g = &ab.guiao;
			TzMundo *m = &pt.m;
			tz_guiao_registrar(m, g, 1);
			TzSom *som = tz_som_criar(&rec, &bib, 0);
			tz_som_fonte_missao(som, ab.tem_sub ? &ab.sub : &ab.c);
			int cx = 0, cy = 0;
			for (int o = 0; o < m->nobj; ++o) if (m->obj[o].dono == m->jogador && tz_obj_edificio(&m->obj[o])) { cx = m->obj[o].cx; cy = m->obj[o].cy; break; }
			tz_som_camara(som, cx, cy, 1024);
			static int16_t buf[1024 * 2];
			clock_t t0 = clock();
			for (int t = 0; t < 3000; ++t) {
				tz_mundo_tic(m);
				tz_som_eventos(som, m);
				TzPedido p;
				while (tz_guiao_pedido(g, &p)) { if (p.tipo == TZ_PD_HISTORIA || p.tipo == TZ_PD_AVISO_JANELA) ventanas++; if (p.tipo == TZ_PD_HISTORIA && p.som[0]) tz_som_fala(som, p.som); }
				if (tz_guiao_modal(g)) { if (g->conv_activa) tz_guiao_responder(m, g, 0); else tz_guiao_fechar(m, g); }
				if (t % 20 == 0) {
					tz_som_misturar(som, buf, 1024);
					for (int q = 0; q < 2048; ++q) if (abs(buf[q]) > picos) picos = abs(buf[q]);
				}
				tics_tot++;
				if (m->acabou) break;
			}
			ms_guion += (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
			sonidos += tz_som_ativas(som);
			int gan = 0;
			if (tz_guiao_acabou(g, &gan)) { acabadas++; ganadas += gan; }
			printf("%s %s: %d reglas, %d acciones ejecutadas, final %d, %d objetos\n", mi->dentro[0] ? mi->dentro : mi->fich, "ok", g->nregras, (int)g->n_acoes, g->acabou, m->nobj);
			tz_som_fechar(som);
			tz_partida_liberar(&pt);
			tz_biblioteca_liberar(&bib);
			tz_missao_fechar(&ab);
		}
	printf("%s real: %d misiones con la simulación real, %d tics, %d finales (%d victorias), %d ventanas, pico de sonido %d, %d voces vivas al acabar; %.0f ms\n",
	       mal ? "FALLO" : "OK", misiones, tics_tot, acabadas, ganadas, ventanas, picos, sonidos, ms_guion);
	tz_mapas_liberar(&c);
	tz_recursos_fechar(&rec);
	return mal;
}
#endif

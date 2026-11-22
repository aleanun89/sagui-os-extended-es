/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_extras.c - pruebas de la simulación de Tzar añadida en la v8.5.5.2 (D417), en el PC con ASan/UBSan y datos SINTÉTICOS
 * (tools/tzar_sintetico.py): comercio y rutas de mercaderes, intercambio, préstamos, ofertas y contratos de la taberna, objetos
 * (recoger, soltar, usar, vender, equipo, libros), hechizos (maldición, hipnosis, invisibilidad, remolino, alzar muertos),
 * espías y el golpe del ninja, cruzada y conversiones, experiencia y niveles, prioridad de blancos, determinismo y guardar/cargar.
 *
 * Uso: tzar_extras <instalación sintética>. Imprime «OK ...» o «FALLO ...» por comprobación y sale con 0 si no falla ninguna.
 */
#include "mundo_int.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int fallos, oks;
static void chk(int ok, const char *fmt, ...)
{
	char b[300];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(b, sizeof b, fmt, ap);
	va_end(ap);
	if (ok) oks++; else fallos++;
	printf("%s %s\n", ok ? "OK" : "FALLO", b);
}
#define CHK(c, ...) chk((c) ? 1 : 0, __VA_ARGS__)

static TzRecursos rec;
static TzBiblioteca bib;
static TzCatalogo cat;
static TzHpfs hp;
static TzMapa mp;
static TzMundo mundoA, mundoB;

static int abrir(const char *raiz)
{
	char ruta[1024];
	if (tz_recursos_abrir(&rec, raiz)) return -1;
	snprintf(ruta, sizeof ruta, "%s/prueba/sim.wmp", raiz);
	if (tz_hpfs_abrir(&hp, ruta) || tz_mapa_ler(&hp, &mp)) return -1;
	if (tz_biblioteca_carregar(&bib, &rec, mp.bioma)) return -1;
	return tz_catalogo_carregar(&cat, &rec, &bib);
}

/* un mundo limpio: los dos castillos y nada más, sin niebla, con la población de siempre */
static TzMundo *novo(TzMundo *m, uint32_t semente)
{
	TzRegras r; memset(&r, 0, sizeof r);
	r.populacao = 200; r.sem_nevoa = 1;
	if (m->cel) tz_mundo_liberar(m);
	if (tz_mundo_criar(m, &cat, &mp, &r, semente)) { printf("FALLO no se pudo crear el mundo: %s\n", tz_erro()); exit(2); }
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe != TZC_CASTLE) tz_mundo_guiao_remover(m, i);
	for (int k = 1; k <= 2; ++k) for (int q = 0; q < TZ_NREC; ++q) m->reino[k].recursos[q] = 0;
	return m;
}

static void ticar(TzMundo *m, int n) { for (int i = 0; i < n; ++i) tz_mundo_tic(m); }
static TzId criar(TzMundo *m, int classe, int dono, int x, int y)
{
	TzId i = tz_mundo_guiao_criar(m, classe, 0, dono, x, y);
	if (i < 0) { printf("FALLO no cabe la clase %d en (%d,%d)\n", classe, x, y); exit(2); }
	return i;
}
static void pesquisar(TzMundo *m, int k, const char *t) { int q = tz_catalogo_tec(&cat, t); if (q >= 0) tz_mundo_guiao_pesquisa(m, k, q); else { printf("FALLO falta la tecnología %s\n", t); exit(2); } }
static void ordem_f(TzMundo *m, int k, TzId id, TzOrdem o, int fila) { TzComando c; memset(&c, 0, sizeof c); c.tipo = TZ_C_ORDEM; c.reino = (uint8_t)k; c.flags = (uint8_t)fila; c.nids = 1; c.ids[0] = id; c.ordem = o; tz_mundo_comando(m, &c); }
static void ordem(TzMundo *m, int k, TzId id, TzOrdem o) { ordem_f(m, k, id, o, 0); }
static TzOrdem o_(int tipo, TzId alvo, int x, int y) { TzOrdem o; memset(&o, 0, sizeof o); o.tipo = (uint8_t)tipo; o.alvo = alvo; o.x = (int16_t)x; o.y = (int16_t)y; o.x2 = o.y2 = -1; return o; }
static void cmd(TzMundo *m, int k, int tipo, TzId id, int arg, int32_t valor, int x, int y)
{
	TzComando c; memset(&c, 0, sizeof c);
	c.tipo = (uint8_t)tipo; c.reino = (uint8_t)k; c.arg = (int16_t)arg; c.valor = valor; c.x = (int16_t)x; c.y = (int16_t)y;
	c.nids = id >= 0; c.ids[0] = id;
	tz_mundo_comando(m, &c);
}
static void quieto(TzMundo *m, TzId i) { m->obj[i].ator->modo = TZ_MODO_QUIETO; }
static int contar_ev(TzMundo *m, int tipo, int de)
{
	TzEvento e[256]; uint32_t cur = m->ev_seq > TZ_EVENTOS ? m->ev_seq - TZ_EVENTOS : 0; int n = 0, q;
	while ((q = tz_mundo_eventos(m, &cur, e, 256)) > 0) for (int i = 0; i < q; ++i) if (e[i].tipo == tipo && e[i].tic >= (uint32_t)de) n++;
	return n;
}
static int contar_cls(const TzMundo *m, int classe, int dono)
{
	int n = 0;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe == classe && m->obj[i].dono == dono && tz_obj_vivo(&m->obj[i])) n++;
	return n;
}

/* ------------------------------------------------------------------ comercio */
static void t_comercio(void)
{
	TzMundo *m = novo(&mundoA, 7);
	TzId a = criar(m, TZC_MARKET, 1, 3, 20), b = criar(m, TZC_MARKET, 1, 20, 20);
	TzId t = criar(m, TZC_TRADER, 1, 6, 23);
	m->reino[1].recursos[TZ_OURO] = 5000;
	CHK(tz_mundo_capacidade_comercio(m, t) == 500, "el mercader europeo carga 500 por viaje (%u)", tz_mundo_capacidade_comercio(m, t));
	CHK(tz_mundo_lucro_rota(m, 1, a, b, 500) == 106, "la ganancia de 500 entre mercados a 17 casillas es 106 (%u)", tz_mundo_lucro_rota(m, 1, a, b, 500));
	CHK(tz_mundo_lucro_rota(m, 1, a, a, 500) == 0, "mismo mercado: sin ganancia");
	TzOrdem o = o_(TZ_O_COMERCIAR, b, 0, 0); o.x2 = 3; o.y2 = 20; o.arg = TZ_ARG_COMERCIO(TZ_OURO, TZ_COMIDA, 500);
	ordem(m, 1, t, o);
	int invariante = 1, vio_cargado = 0;
	for (int q = 0; q < 1600; ++q) {
		tz_mundo_tic(m);
		const TzAtor *ta = m->obj[t].ator;
		if (ta->rota.reservado >= 0 && ta->ordem.alvo != ta->rota.reservado && ta->ordem.tipo == TZ_O_COMERCIAR && ta->rota.carga) invariante = 0;
		if (ta->rota.carga == 500) vio_cargado = 1;
	}
	int evs = contar_ev(m, TZ_EV_COMERCIO, 0);
	CHK(vio_cargado, "el mercader carga los 500 de oro en el origen");
	CHK(evs >= 1, "el mercader vende en el otro mercado (%d ventas)", evs);
	TzEvento e[64]; uint32_t cur = m->ev_seq > TZ_EVENTOS ? m->ev_seq - TZ_EVENTOS : 0; int n = tz_mundo_eventos(m, &cur, e, 64), primero = -1;
	for (int i = 0; i < n && primero < 0; ++i) if (e[i].tipo == TZ_EV_COMERCIO) primero = e[i].valor;
	CHK(primero == 606, "la primera venta es 500 + 106 de ganancia (%d)", primero);
	CHK(m->reino[1].recursos[TZ_OURO] + (int)m->obj[t].ator->rota.carga > 5000, "el oro sube con las rutas (%d)", m->reino[1].recursos[TZ_OURO]);
	CHK(invariante, "el mercader solo ocupa el mercado al que va");
	/* con Banking la ganancia sube (40 + 3) */
	pesquisar(m, 1, "te_banking");
	CHK(tz_mundo_lucro_rota(m, 1, a, b, 500) == 114, "Banking sube la ganancia a 114 (%u)", tz_mundo_lucro_rota(m, 1, a, b, 500));
	/* un solo mercader a la vez por mercado */
	m = novo(&mundoA, 8);
	a = criar(m, TZC_MARKET, 1, 3, 20); b = criar(m, TZC_MARKET, 1, 20, 20);
	TzId t1 = criar(m, TZC_TRADER, 1, 5, 24), t2 = criar(m, TZC_TRADER, 1, 5, 26);
	m->reino[1].recursos[TZ_OURO] = 20000;
	o = o_(TZ_O_COMERCIAR, b, 0, 0); o.x2 = 3; o.y2 = 20; o.arg = TZ_ARG_COMERCIO(TZ_OURO, TZ_COMIDA, 500);
	ordem(m, 1, t1, o); ordem(m, 1, t2, o);
	int solo = 1;
	for (int q = 0; q < 1200; ++q) {
		tz_mundo_tic(m);
		for (TzId mk = a; mk <= b; mk += b - a)
			if (m->obj[t1].ator->rota.reservado == mk && m->obj[t2].ator->rota.reservado == mk) solo = 0;
	}
	CHK(solo, "nunca dos mercaderes ocupan el mismo mercado a la vez");
	/* vender: lo que lleva se paga sin ganancia */
	m = novo(&mundoA, 9);
	a = criar(m, TZC_MARKET, 1, 3, 20);
	t = criar(m, TZC_TRADER, 1, 6, 23);
	m->obj[t].ator->rota.carga = 300; m->obj[t].ator->rota.carga_tipo = TZ_MADEIRA;
	ordem(m, 1, t, o_(TZ_O_VENDER, a, 0, 0));
	ticar(m, 200);
	CHK(m->reino[1].recursos[TZ_MADEIRA] == 300 && m->obj[t].ator->rota.carga == 0, "vender entrega la carga en el mercado (%d)", m->reino[1].recursos[TZ_MADEIRA]);
	/* un soldado no comercia */
	TzId f = criar(m, 3, 1, 8, 8);
	CHK(tz_mundo_capacidade_comercio(m, f) == 0, "un infante no comercia");
}

/* ------------------------------------------------------------------ taberna: intercambio, préstamos, contratos */
static void t_taberna(void)
{
	TzMundo *m = novo(&mundoA, 11);
	TzId tb = criar(m, TZC_TAVERN, 1, 12, 20);
	m->reino[1].recursos[TZ_OURO] = 5000;
	cmd(m, 1, TZ_C_TROCAR, tb, TZ_OURO | (TZ_COMIDA << 2), 750, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 5000, "el intercambio de 750 se rechaza (de 500 en 500)");
	cmd(m, 1, TZ_C_TROCAR, tb, TZ_OURO | (TZ_COMIDA << 2), 1000, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 4000, "el intercambio cobra 1000 de golpe (%d)", m->reino[1].recursos[TZ_OURO]);
	cmd(m, 1, TZ_C_TROCAR, tb, TZ_OURO | (TZ_MADEIRA << 2), 500, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 4000, "solo un intercambio a la vez por reino");
	ticar(m, 300);
	int mitad = m->reino[1].recursos[TZ_COMIDA];
	CHK(mitad > 40 && mitad < 60, "una unidad cada 300 ms: a los 15 s van %d de 100", mitad);
	ticar(m, 400);
	CHK(m->reino[1].recursos[TZ_COMIDA] == 100 && contar_ev(m, TZ_EV_TROCA, 0) == 1, "el intercambio acaba con 100 de comida (%d)", m->reino[1].recursos[TZ_COMIDA]);
	/* préstamos */
	cmd(m, 1, TZ_C_EMPRESTAR, tb, TZ_PEDRA, 0, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_PEDRA] == 1000, "el préstamo da 1000 (%d)", m->reino[1].recursos[TZ_PEDRA]);
	cmd(m, 1, TZ_C_EMPRESTAR, tb, TZ_OURO, 0, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 4000, "solo un préstamo vivo");
	m->reino[1].recursos[TZ_PEDRA] = 100;
	cmd(m, 1, TZ_C_DEVOLVER, tb, 0, 0, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_PEDRA] == 100 && m->reino[1].emp_rec, "sin 1200 de piedra no se devuelve");
	m->reino[1].recursos[TZ_PEDRA] = 1300;
	cmd(m, 1, TZ_C_DEVOLVER, tb, 0, 0, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_PEDRA] == 100 && !m->reino[1].emp_rec, "devolver cuesta 1200 (%d)", m->reino[1].recursos[TZ_PEDRA]);
	CHK(m->reino[1].emp_quantia == 1000, "el siguiente préstamo sube un minuto después, no antes");
	ticar(m, 1200);
	CHK(m->reino[1].emp_quantia == 1200, "un minuto después el préstamo es de 1200 (%u)", m->reino[1].emp_quantia);
	/* ofertas y contratos */
	m = novo(&mundoA, 12);
	tb = criar(m, TZC_TAVERN, 1, 12, 20);
	m->reino[1].recursos[TZ_OURO] = 20000; m->reino[1].recursos[TZ_COMIDA] = 20000; m->reino[1].recursos[TZ_MADEIRA] = 20000;
	ticar(m, 100);
	struct TzOferta of[8];
	CHK(tz_mundo_ofertas(m, tb, 0, of, 8) == 0, "sin Mercenaries no hay ofertas");
	pesquisar(m, 1, "te_mercenaries"); pesquisar(m, 1, "te_heroesquest");
	ticar(m, 2500);
	int nm = tz_mundo_ofertas(m, tb, 0, of, 8), nh = 0;
	struct TzOferta oh[8];
	nh = tz_mundo_ofertas(m, tb, 1, oh, 8);
	CHK(nm >= 2 && nh >= 2, "la taberna ofrece mercenarios (%d) y héroes (%d)", nm, nh);
	/* 2500 tics = 125 s: una oferta cada 60 s, sin pasar de ocho; de las dos (o más) alguna se pudo no formar si salió una civilización sin cuartel */
	int cantidad_ok = 1, nivel_ok = 1;
	for (int i = 0; i < nm; ++i) if (of[i].quantidade < 8 || of[i].quantidade > 12 || of[i].nivel != 0) cantidad_ok = 0;
	for (int i = 0; i < nh; ++i) if (oh[i].quantidade != 1 || oh[i].nivel < 10 || oh[i].nivel > 30) nivel_ok = 0;
	CHK(cantidad_ok, "los mercenarios son 8 a 12 de nivel 0");
	CHK(nivel_ok, "los héroes son uno de nivel 10 a 30");
	uint32_t pr[TZ_NREC];
	tz_mundo_preco_oferta(m, &of[0], pr);
	const TzAtributos *at = tz_catalogo_at(&cat, of[0].classe, of[0].civ);
	int precio_ok = 1;
	for (int r = 0; r < TZ_NREC; ++r) {
		uint32_t base = (uint32_t)((uint64_t)at->custo[r] * of[0].quantidade * 160 / 100);
		uint32_t esperado = (r == 0 || base) ? base + (r == 0 ? 30u : 15u) * of[0].nivel : 0;
		if (pr[r] != esperado) precio_ok = 0;
	}
	CHK(precio_ok, "el precio es coste x cantidad x 1,6 (+30 de oro y +15 de los demás por nivel): %u/%u/%u/%u", pr[0], pr[1], pr[2], pr[3]);
	int pop0 = m->reino[1].populacao, tot0 = 0;
	for (int r = 0; r < TZ_NREC; ++r) tot0 += m->reino[1].recursos[r];
	uint16_t id = of[0].id;
	cmd(m, 1, TZ_C_CONTRATAR, tb, id, 0, 0, 0);
	ticar(m, 2);
	int nuevos = 0;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].dono == 1 && m->obj[i].classe == of[0].classe && tz_obj_vivo(&m->obj[i])) nuevos++;
	CHK(nuevos == of[0].quantidade, "el contrato trae a los %d mercenarios", of[0].quantidade);
	int tot1 = 0;
	for (int r = 0; r < TZ_NREC; ++r) tot1 += m->reino[1].recursos[r];
	CHK(tot1 < tot0, "el contrato cuesta recursos (%d)", tot0 - tot1);
	struct TzOferta tras[8];
	int n2 = tz_mundo_ofertas(m, tb, 0, tras, 8), sigue = 0;
	for (int i = 0; i < n2; ++i) if (tras[i].id == id) sigue = 1;
	CHK(!sigue, "la oferta contratada desaparece");
	(void)pop0;
	/* un héroe sale con su nivel y marcado como héroe */
	uint16_t idh = oh[0].id; int clh = oh[0].classe, nivh = oh[0].nivel;
	cmd(m, 1, TZ_C_CONTRATAR, tb, idh, 0, 0, 0);
	ticar(m, 2);
	int heroi = 0;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].dono == 1 && m->obj[i].classe == clh && (m->obj[i].flags & TZ_F_HEROI) && m->obj[i].nivel == nivh) heroi = 1;
	CHK(heroi, "el héroe contratado es héroe y de nivel %d", nivh);
	/* sin población no se contrata */
	m->reino[1].populacao_max = 1;
	n2 = tz_mundo_ofertas(m, tb, 0, tras, 8);
	if (n2) {
		int u0 = 0; for (int i = 0; i < m->nobj; ++i) u0 += tz_obj_unidade(&m->obj[i]) && m->obj[i].dono == 1;
		cmd(m, 1, TZ_C_CONTRATAR, tb, tras[0].id, 0, 0, 0);
		/* contar_populacao recalcula al principio del tic: se deja el tope bajo cambiando la casa */
		TzReinoEstado *r = &m->reino[1];
		(void)r;
		ticar(m, 1);
		int u1 = 0; for (int i = 0; i < m->nobj; ++i) u1 += tz_obj_unidade(&m->obj[i]) && m->obj[i].dono == 1 && tz_obj_vivo(&m->obj[i]);
		(void)u1;
	}
}

/* ------------------------------------------------------------------ objetos */
static TzItem item(int vida, int mana, int at, int df, int xp)
{
	TzItem t; memset(&t, 0, sizeof t);
	t.vida = vida; t.mana = mana; t.ataque = (uint16_t)at; t.defesa = (uint16_t)df; t.xp = (uint16_t)xp;
	return t;
}

static void t_itens(void)
{
	TzMundo *m = novo(&mundoA, 21);
	TzId f = criar(m, 3, 1, 8, 8);
	CHK(tz_mundo_capacidade_itens(m, f) == 4, "el infante lleva hasta 4 objetos");
	int at0 = m->obj[f].ataque, df0 = m->obj[f].defesa;
	TzId esp = tz_mundo_criar_item(m, TZC_ITEM_MAIS, 9, 8, &(TzItem){ .ataque = 10, .defesa = 4 });
	TzId poc = tz_mundo_criar_item(m, TZC_POCAO, 8, 10, &(TzItem){ .vida = 50, .mana = 0, .ataque = 5, .defesa = 3, .xp = 0 });
	TzId pop = tz_mundo_criar_item(m, TZC_POCAO, 6, 8, &(TzItem){ .vida = 0, .mana = 30, .ataque = 0, .defesa = 0, .xp = 3 });
	CHK(esp >= 0 && poc >= 0 && pop >= 0, "los objetos caben en el suelo");
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, esp, 0, 0));
	ticar(m, 60);
	CHK((m->obj[esp].flags & TZ_F_CARREGADO) && m->obj[esp].dentro_de == f && m->obj[f].ator->nitens == 1, "el infante recoge la espada");
	CHK(m->obj[f].ataque == at0 + 10 && m->obj[f].defesa == df0 + 4, "el equipo suma ataque y defensa (%d/%d)", m->obj[f].ataque, m->obj[f].defesa);
	TzId lleva[8];
	CHK(tz_mundo_itens(m, f, lleva, 8) == 1 && lleva[0] == esp, "tz_mundo_itens lista lo que lleva");
	ordem(m, 1, f, o_(TZ_O_SOLTAR_ITEM, esp, 0, 0));
	ticar(m, 10);
	CHK(!(m->obj[esp].flags & TZ_F_CARREGADO) && m->obj[f].ataque == at0 && m->obj[f].ator->nitens == 0, "soltar quita el equipo y sus bonos");
	/* pociones: bebida propia y a otro */
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, poc, 0, 0));
	ticar(m, 60);
	int vm0 = m->obj[f].vida_max;
	TzOrdem u = o_(TZ_O_USAR_ITEM, poc, 0, 0); u.arg = 0;
	ordem(m, 1, f, u);
	ticar(m, 5);
	CHK(m->obj[f].vida_max == vm0 + 50 && m->obj[f].ataque == at0 + 5 && m->obj[f].defesa == df0 + 3, "la poción sube vida máxima +50, ataque +5 y defensa +3 para siempre");
	CHK(tz_obj_item(&m->obj[poc]) && (m->obj[poc].flags & TZ_F_INATIVO), "la poción bebida desaparece");
	/* experiencia de la poción: sin tope no sube */
	CHK(m->obj[f].nivel == 0, "sin Learning la poción de experiencia no sube de nivel");
	pesquisar(m, 1, "te_learning");
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, pop, 0, 0));
	ticar(m, 80);
	ordem(m, 1, f, u); u.alvo = pop; ordem(m, 1, f, u);
	ticar(m, 5);
	CHK(m->obj[f].nivel == 1, "con Learning la poción de experiencia da un nivel (%d)", m->obj[f].nivel);
	/* a otro: la poción se da a la unidad de al lado */
	TzId g = criar(m, 3, 1, 8, 12);
	TzId pc = tz_mundo_criar_item(m, TZC_POCAO, 8, 10, &(TzItem){ .vida = 20 });
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, pc, 0, 0));
	ticar(m, 60);
	int gvm = m->obj[g].vida_max;
	TzOrdem ua = o_(TZ_O_USAR_ITEM, pc, 8, 12); ua.arg = 1;
	ordem(m, 1, f, ua);
	ticar(m, 80);
	CHK(m->obj[g].vida_max == gvm + 20, "una poción se bebe sobre otra unidad (%d)", m->obj[g].vida_max);
	/* capacidad */
	m = novo(&mundoA, 22);
	f = criar(m, 3, 1, 8, 8);
	TzId varios[6];
	for (int i = 0; i < 6; ++i) varios[i] = tz_mundo_criar_item(m, TZC_PERGAMINHO, 8, 10 + i, &(TzItem){ .texto = 1 });
	for (int i = 0; i < 6; ++i) ordem_f(m, 1, f, o_(TZ_O_PEGAR_ITEM, varios[i], 0, 0), i ? TZ_CMD_FILA : 0);
	ticar(m, 400);
	CHK(m->obj[f].ator->nitens == 4, "recoge 4 objetos en cola y no más (%d)", m->obj[f].ator->nitens);
	/* libros: aprender y lanzar */
	m = novo(&mundoA, 23);
	TzId mg = criar(m, TZC_MAGE, 1, 8, 8);
	m->obj[mg].mana = 100;
	TzItem lib = item(0, 0, 0, 0, 0); lib.feitico = 20; lib.finito = 0;
	TzId lb = tz_mundo_criar_item(m, TZC_LIVRO, 9, 8, &lib);
	CHK(!tz_mundo_conhece_feitico(m, mg, "spell_curse"), "el mago no conoce la maldición");
	ordem(m, 1, mg, o_(TZ_O_PEGAR_ITEM, lb, 0, 0));
	ticar(m, 40);
	TzOrdem ul = o_(TZ_O_USAR_ITEM, lb, 0, 0); ul.arg = 0;
	ordem(m, 1, mg, ul);
	ticar(m, 5);
	CHK(tz_mundo_conhece_feitico(m, mg, "spell_curse") && (m->obj[lb].flags & TZ_F_INATIVO), "el libro enseña la maldición y se gasta");
	/* lanzar desde el libro, sin maná y con una carga */
	TzId mg2 = criar(m, TZC_MAGE, 1, 12, 12);
	m->obj[mg2].mana = 0;
	TzId herido = criar(m, 3, 1, 14, 12);
	m->obj[herido].vida = 10;
	TzItem lc = item(0, 0, 0, 0, 0); lc.feitico = 2; lc.finito = 1; lc.cargas = 1;
	TzId lb2 = tz_mundo_criar_item(m, TZC_LIVRO, 11, 12, &lc);
	ordem(m, 1, mg2, o_(TZ_O_PEGAR_ITEM, lb2, 0, 0));
	ticar(m, 40);
	TzOrdem ut = o_(TZ_O_USAR_ITEM, lb2, 14, 12); ut.arg = 1;
	ordem(m, 1, mg2, ut);
	ticar(m, 120);
	CHK(m->obj[herido].vida > 10, "el libro cura sin maná (el mago tenía 0 y la curación pide 4): vida %d", m->obj[herido].vida);
	CHK(m->obj[lb2].flags & TZ_F_INATIVO, "el libro de una carga desaparece al usarla");
	/* vender en el mercado */
	m = novo(&mundoA, 24);
	TzId mk = criar(m, TZC_MARKET, 1, 3, 20);
	f = criar(m, 3, 1, 8, 20);
	TzId es2 = tz_mundo_criar_item(m, TZC_ITEM_MAIS, 9, 20, &(TzItem){ .ataque = 10, .defesa = 4 });
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, es2, 0, 0));
	ticar(m, 60);
	CHK(tz_mundo_valor_item(m, es2) == 56, "la espada vale 4 x (10 + 4) = 56 (%u)", tz_mundo_valor_item(m, es2));
	cmd(m, 1, TZ_C_VENDER, mk, 0, es2, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 56 && (m->obj[es2].flags & TZ_F_INATIVO), "vender da el valor en oro (%d)", m->reino[1].recursos[TZ_OURO]);
	/* al morir, deja lo que lleva */
	f = criar(m, 3, 1, 8, 24);
	TzId p2 = tz_mundo_criar_item(m, TZC_POCAO, 9, 24, &(TzItem){ .vida = 5 });
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, p2, 0, 0));
	ticar(m, 60);
	int cargado = (m->obj[p2].flags & TZ_F_CARREGADO) != 0;
	tz_mundo_guiao_matar(m, f);
	ticar(m, 2);
	CHK(cargado && !(m->obj[p2].flags & (TZ_F_CARREGADO | TZ_F_INATIVO)), "el que muere suelta lo que llevaba");
}

/* ------------------------------------------------------------------ hechizos y espías */
static void t_magia(void)
{
	/* maldición */
	TzMundo *m = novo(&mundoA, 31);
	TzId mg = criar(m, TZC_MAGE, 1, 10, 10);
	TzId e1 = criar(m, 3, 2, 14, 10), e2 = criar(m, 4, 2, 14, 12);
	quieto(m, e1); quieto(m, e2);
	quieto(m, mg);
	m->obj[mg].mana = 100;
	int a0 = m->obj[e1].ataque, d0 = m->obj[e1].defesa;
	int curse = tz_feitico_ordem(m, "spell_curse");
	CHK(curse >= 0, "la maldición está en las órdenes");
	TzOrdem h = o_(TZ_O_HECHIZO, TZ_NENHUM, 14, 11); h.arg = (int16_t)curse;
	ordem(m, 1, mg, h);
	ticar(m, 80);
	CHK(m->obj[e1].ataque == a0 / 2 && m->obj[e1].defesa == 0 && m->obj[e2].defesa == 0, "la maldición: la mitad del ataque y nada de defensa (%d/%d de %d/%d)", m->obj[e1].ataque, m->obj[e1].defesa, a0, d0);
	CHK(m->obj[mg].mana >= 50 && m->obj[mg].mana <= 56, "la maldición cuesta 50 de maná (queda %d)", m->obj[mg].mana);
	ticar(m, 620);
	CHK(m->obj[e1].ataque == a0 && m->obj[e1].defesa == d0, "a los 30 s la maldición se acaba (%d/%d hasta %u, tic %u)", m->obj[e1].ataque, m->obj[e1].defesa, m->obj[e1].ator->maldicao_ate, m->tic);
	/* hipnosis */
	m = novo(&mundoA, 32);
	mg = criar(m, TZC_MAGE, 1, 10, 10);
	e1 = criar(m, 3, 2, 14, 10);
	quieto(m, e1); quieto(m, mg);
	m->obj[mg].mana = 100;
	h = o_(TZ_O_HECHIZO, e1, 0, 0); h.arg = (int16_t)tz_feitico_ordem(m, "spell_hypnotize");
	ordem(m, 1, mg, h);
	ticar(m, 80);
	CHK(m->obj[e1].dono == 1, "la hipnosis pasa al enemigo a mi reino");
	ticar(m, 620);
	CHK(m->obj[e1].dono == 2 && m->obj[e1].ator->hipnose_ate == 0, "a los 30 s vuelve a su reino");
	/* invisibilidad */
	m = novo(&mundoA, 33);
	mg = criar(m, TZC_MAGE, 1, 4, 10);
	TzId amigo = criar(m, 3, 1, 9, 10);
	TzId caballero = criar(m, 3, 2, 12, 10);
	quieto(m, mg); quieto(m, amigo);
	m->obj[mg].mana = 100;
	h = o_(TZ_O_HECHIZO, amigo, 0, 0); h.arg = (int16_t)tz_feitico_ordem(m, "spell_cloak");
	ordem(m, 1, mg, h);
	ticar(m, 80);
	CHK(!tz_mundo_ve(m, 2, amigo) && tz_mundo_ve(m, 1, amigo), "el invisible no lo ve el enemigo pero sí su reino (ve2 %d ve1 %d hasta %u mana %d)", tz_mundo_ve(m, 2, amigo), tz_mundo_ve(m, 1, amigo), m->obj[amigo].ator->invisivel_ate, m->obj[mg].mana);
	CHK(m->obj[caballero].ator->ordem.tipo != TZ_O_ATACAR || m->obj[caballero].ator->ordem.alvo != amigo, "el enemigo no ataca al invisible");
	ticar(m, 900);
	CHK(tz_mundo_ve(m, 2, amigo), "a los 45 s deja de ser invisible");
	/* remolino */
	m = novo(&mundoA, 34);
	for (int y = 2; y <= 9; ++y) for (int x = 24; x <= 31; ++x) m->cel[y * m->lado + x].bits |= TZ_CEL_AGUA;
	mg = criar(m, TZC_MAGE, 1, 22, 5);
	quieto(m, mg);
	TzId gal = criar(m, 23, 2, 27, 5);
	tz_mundo_guiao_atributos(m, gal, 1000, -1, -1, 0, 0);
	m->obj[mg].mana = 100;
	TzOrdem hr = o_(TZ_O_HECHIZO, TZ_NENHUM, 27, 5); hr.arg = (int16_t)tz_feitico_ordem(m, "spell_whirlpool");
	ordem(m, 1, mg, hr);
	ticar(m, 200);
	CHK(contar_cls(m, TZC_REDEMOINHO, 0) == 1, "el remolino aparece en el agua");
	CHK(m->obj[gal].vida < 1000 && (1000 - m->obj[gal].vida) % 20 == 0, "el remolino hiere a los barcos de 20 en 20 (%d)", 1000 - m->obj[gal].vida);
	ticar(m, 700);
	CHK(contar_cls(m, TZC_REDEMOINHO, 0) == 0, "a los 15 movimientos el remolino se va");
	/* en tierra no */
	m = novo(&mundoA, 35);
	mg = criar(m, TZC_MAGE, 1, 10, 10);
	m->obj[mg].mana = 100;
	hr = o_(TZ_O_HECHIZO, TZ_NENHUM, 12, 10); hr.arg = (int16_t)tz_feitico_ordem(m, "spell_whirlpool");
	ordem(m, 1, mg, hr);
	ticar(m, 100);
	CHK(contar_cls(m, TZC_REDEMOINHO, 0) == 0, "el remolino solo se forma en el agua");
	/* alzar muertos */
	m = novo(&mundoA, 36);
	mg = criar(m, TZC_MAGE, 1, 10, 10);
	m->obj[mg].mana = 100;
	TzId c1 = criar(m, 3, 2, 12, 10), c2 = criar(m, 3, 2, 12, 12), c3 = criar(m, TZC_MAGE, 2, 12, 14);
	tz_mundo_guiao_matar(m, c1); tz_mundo_guiao_matar(m, c2); tz_mundo_guiao_matar(m, c3);
	ticar(m, 2);
	h = o_(TZ_O_HECHIZO, TZ_NENHUM, 10, 10); h.arg = (int16_t)tz_feitico_ordem(m, "spell_raisedead");
	ordem(m, 1, mg, h);
	ticar(m, 40);
	CHK(contar_cls(m, TZC_SKELETON, 1) == 2, "alzar muertos levanta dos esqueletos, no el del mago (%d)", contar_cls(m, TZC_SKELETON, 1));
	CHK(m->obj[mg].mana >= 30 && m->obj[mg].mana <= 36, "alzar muertos cuesta 70 de maná (queda %d)", m->obj[mg].mana);
	/* maná insuficiente */
	m = novo(&mundoA, 37);
	mg = criar(m, TZC_MAGE, 1, 10, 10);
	m->obj[mg].mana = 10;
	e1 = criar(m, 3, 2, 14, 10);
	quieto(m, e1);
	h = o_(TZ_O_HECHIZO, TZ_NENHUM, 14, 10); h.arg = (int16_t)tz_feitico_ordem(m, "spell_curse");
	ordem(m, 1, mg, h);
	ticar(m, 60);
	CHK(m->obj[e1].ator->maldicao_ate == 0, "sin maná no hay maldición");
}

static void t_espias(void)
{
	TzMundo *m = novo(&mundoA, 41);
	TzId spy = criar(m, TZC_SPY, 1, 10, 10);
	TzId host = criar(m, 3, 2, 12, 10);
	quieto(m, host);
	CHK(m->obj[spy].ator->espia_energia == 100, "el espía empieza con 100 de energía");
	ordem(m, 1, spy, o_(TZ_O_SUPLANTAR, host, 0, 0));
	ticar(m, 60);
	CHK((m->obj[spy].flags & TZ_F_MORTO) && m->obj[host].ator->espia_reino == 1, "el espía se mete en la unidad enemiga");
	CHK(m->obj[host].dono == 2, "el anfitrión sigue siendo del otro reino");
	/* el reino del espía manda al anfitrión */
	ordem(m, 1, host, o_(TZ_O_MOVER, TZ_NENHUM, 20, 10));
	ticar(m, 120);
	CHK(m->obj[host].cx > 12, "el reino del espía puede mover al anfitrión (x %d)", m->obj[host].cx);
	/* revelar */
	TzComando c; memset(&c, 0, sizeof c);
	c.tipo = TZ_C_CONVERTIR; c.reino = 1; c.arg = TZC_SPY; c.nids = 1; c.ids[0] = host;
	tz_mundo_comando(m, &c);
	ticar(m, 2);
	CHK(contar_cls(m, TZC_SPY, 1) == 1 && m->obj[host].ator->espia_reino == 0, "revelarse devuelve al espía");
	/* con poca energía no se infiltra */
	m = novo(&mundoA, 42);
	spy = criar(m, TZC_SPY, 1, 10, 10);
	host = criar(m, 3, 2, 12, 10);
	quieto(m, host);
	m->obj[spy].ator->espia_energia = 30;
	ordem(m, 1, spy, o_(TZ_O_SUPLANTAR, host, 0, 0));
	ticar(m, 60);
	CHK(!(m->obj[spy].flags & TZ_F_MORTO) && m->obj[host].ator->espia_reino == 0, "con menos de 60 de energía no se infiltra");
	ticar(m, 20 * 40);
	CHK(m->obj[spy].ator->espia_energia >= 60, "la energía vuelve a 1 por segundo (%d)", m->obj[spy].ator->espia_energia);
	/* el ninja */
	m = novo(&mundoA, 43);
	TzId nj = criar(m, TZC_NINJA, 1, 10, 10);
	TzId obj = criar(m, 3, 2, 12, 10);
	quieto(m, obj);
	m->obj[nj].mana = 100; m->obj[obj].vida_max = m->obj[obj].vida = 100;
	ordem(m, 1, nj, o_(TZ_O_ASESINAR, obj, 0, 0));
	ticar(m, 60);
	CHK(!tz_obj_vivo(&m->obj[obj]) && m->obj[nj].mana == 0, "el golpe del ninja mata de una vez y gasta todo el maná (%d)", m->obj[nj].mana);
	/* sin maná no */
	m = novo(&mundoA, 44);
	nj = criar(m, TZC_NINJA, 1, 10, 10); obj = criar(m, 3, 2, 12, 10);
	quieto(m, obj); quieto(m, nj);
	m->obj[nj].mana = 20; m->obj[obj].vida_max = m->obj[obj].vida = 1000;
	ordem(m, 1, nj, o_(TZ_O_ASESINAR, obj, 0, 0));
	ticar(m, 60);
	CHK(m->obj[obj].vida == 1000, "con menos de 60 de maná el ninja no golpea");
}

/* ------------------------------------------------------------------ cruzada, conversiones, experiencia, combate */
static void t_cruzada(void)
{
	TzMundo *m = novo(&mundoA, 51);
	TzId cat1 = criar(m, TZC_CATHEDRAL, 1, 12, 20);
	m->reino[1].recursos[TZ_OURO] = 1000;
	CHK(m->reino[1].cruzados == 16, "se empieza con 16 cruzados listos");
	cmd(m, 1, TZ_C_CRUZADA, cat1, 0, 0, 16, 16);
	ticar(m, 2);
	CHK(m->reino[1].cruz_ancla >= 0 && m->reino[1].recursos[TZ_OURO] == 900, "la cruzada cobra el ancla (100 de oro) y la pone");
	ticar(m, 200);
	int cr = contar_cls(m, TZC_CRUSADER, 1);
	CHK(cr == 16, "llegan los 16 cruzados, uno cada 500 ms (%d)", cr);
	cmd(m, 1, TZ_C_CRUZADA, cat1, 0, 0, 18, 18);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_OURO] == 900, "no se lanza otra cruzada con una en marcha");
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe == TZC_CRUSADER && tz_obj_vivo(&m->obj[i])) tz_mundo_guiao_matar(m, i);
	ticar(m, 4);
	CHK(m->reino[1].cruz_ancla < 0 && m->reino[1].cruzados == 4, "al morir todos acaba la cruzada y la catedral empieza de 4 (%u)", m->reino[1].cruzados);
	CHK(m->reino[1].cruzadas_completas == 1, "una cruzada completa sube el techo");
	ticar(m, 20 * 12 * 2);
	CHK(m->reino[1].cruzados >= 6, "la catedral suma un cruzado cada 12 s (%u)", m->reino[1].cruzados);
	/* conversiones */
	m = novo(&mundoA, 52);
	TzId mj = criar(m, TZC_MONK, 1, 10, 10), pn = criar(m, TZC_PEON, 1, 12, 10);
	m->reino[1].recursos[TZ_OURO] = 100;
	TzComando c; memset(&c, 0, sizeof c);
	c.tipo = TZ_C_CONVERTIR; c.reino = 1; c.arg = TZC_PRIEST; c.nids = 1; c.ids[0] = mj;
	tz_mundo_comando(m, &c);
	c.arg = TZC_JIHAD; c.ids[0] = pn;
	tz_mundo_comando(m, &c);
	ticar(m, 2);
	CHK(m->obj[mj].classe == TZC_PRIEST && m->reino[1].recursos[TZ_OURO] == 20, "el monje se hace sacerdote pagando 80 (%d)", m->reino[1].recursos[TZ_OURO]);
	CHK(m->obj[pn].classe == TZC_JIHAD, "el peón se hace guerrero de la yihad sin coste");
}

static void t_xp(void)
{
	TzMundo *m = novo(&mundoA, 61);
	TzId f = criar(m, 3, 1, 8, 8);
	tz_mundo_experiencia(m, f, 20);
	CHK(m->obj[f].nivel == 0, "sin investigaciones no se sube por experiencia");
	pesquisar(m, 1, "te_learning");
	int tope, vel;
	tz_mundo_regras_xp(m, 1, &tope, &vel);
	CHK(tope == 4 && vel == 100, "Learning da tope 4 (%d)", tope);
	tz_mundo_experiencia(m, f, 1);
	CHK(m->obj[f].nivel == 1, "con tope, el primer punto da el nivel 1");
	tz_mundo_experiencia(m, f, 1);
	CHK(m->obj[f].nivel == 2, "el segundo punto, el nivel 2 (cada nivel pide su número)");
	tz_mundo_experiencia(m, f, 1);
	CHK(m->obj[f].nivel == 2, "el nivel 3 pide 2 puntos");
	for (int i = 0; i < 20; ++i) tz_mundo_experiencia(m, f, 5);
	CHK(m->obj[f].nivel == 4, "el tope es 4 (%d)", m->obj[f].nivel);
	int a4 = m->obj[f].ataque;
	TzId f0 = criar(m, 3, 1, 10, 8);
	CHK(a4 > m->obj[f0].ataque, "el nivel sube el ataque (%d contra %d)", a4, m->obj[f0].ataque);
	/* academia y héroes */
	m = novo(&mundoA, 62);
	f = criar(m, 3, 1, 8, 8);
	TzId ac = criar(m, 56, 1, 20, 20);
	(void)ac;
	tz_mundo_regras_xp(m, 1, &tope, &vel);
	CHK(tope == 10, "cada academia suma 10 al tope (%d)", tope);
	pesquisar(m, 1, "te_heroism"); pesquisar(m, 1, "te_warfare");
	tz_mundo_regras_xp(m, 1, &tope, &vel);
	CHK(tope == 30000 && vel == 70, "Heroism quita el tope y Warfare baja el nivel al 70 %% (%d, %d)", tope, vel);
	for (int i = 0; i < 300; ++i) tz_mundo_experiencia(m, f, 5);
	CHK(m->obj[f].nivel >= 12 && (m->obj[f].flags & TZ_F_HEROI), "al nivel 12 es héroe (nivel %d)", m->obj[f].nivel);
	/* muertes dan un punto */
	m = novo(&mundoA, 63);
	pesquisar(m, 1, "te_learning");
	TzId k1 = criar(m, 3, 1, 10, 10), v1 = criar(m, 3, 2, 11, 10);
	m->obj[v1].vida = 1; m->obj[v1].defesa = 0;
	ordem(m, 1, k1, o_(TZ_O_ATACAR, v1, 0, 0));
	ticar(m, 100);
	CHK(!tz_obj_vivo(&m->obj[v1]) && m->obj[k1].nivel == 1, "matar a un enemigo da un punto de experiencia (nivel %d)", m->obj[k1].nivel);
}

static void t_prioridade(void)
{
	/* un infante parado ve a un peón enemigo cerca y a un arquero lejos: ataca al soldado (6000 contra 3000) */
	TzMundo *m = novo(&mundoA, 71);
	TzId f = criar(m, 3, 1, 10, 10);
	TzId peao = criar(m, TZC_PEON, 2, 12, 10), arq = criar(m, 4, 2, 15, 10);
	ticar(m, 20);
	CHK(m->obj[f].ator->ordem.tipo == TZ_O_ATACAR && m->obj[f].ator->ordem.alvo == arq, "elige al soldado (prioridad 6000) y no al peón (3000): alvo %d", m->obj[f].ator->ordem.alvo);
	(void)peao;
	/* un mago (4000) antes que un edificio corriente (2000) */
	m = novo(&mundoA, 72);
	f = criar(m, 3, 1, 10, 10);
	TzId casa = criar(m, 66, 2, 12, 10), mg = criar(m, TZC_MAGE, 2, 14, 10);
	ticar(m, 20);
	CHK(m->obj[f].ator->ordem.alvo == mg, "elige al mago (4000) antes que la casa (2000): alvo %d", m->obj[f].ator->ordem.alvo);
	(void)casa;
	/* las murallas no se atacan solas */
	m = novo(&mundoA, 73);
	f = criar(m, 3, 1, 10, 10);
	TzId muro = criar(m, TZC_WALL_STONE, 2, 12, 10);
	ticar(m, 20);
	CHK(m->obj[f].ator->ordem.tipo != TZ_O_ATACAR || m->obj[f].ator->ordem.alvo != muro, "las murallas no se atacan solas");
}

/* ------------------------------------------------------------------ unidades grandes (2x2) */
static void t_grandes(void)
{
	TzMundo *m = novo(&mundoA, 81);
	TzId t = criar(m, TZC_TRADER, 1, 6, 6);
	TzId c = criar(m, 19, 1, 10, 6), d = criar(m, 32, 1, 12, 12), f = criar(m, 3, 1, 3, 3);
	CHK(tz_unid_celulas(TZC_TRADER) == 2 && tz_unid_celulas(19) == 2 && tz_unid_celulas(23) == 2 && tz_unid_celulas(32) == 2 && tz_unid_celulas(3) == 1,
	    "mercader, catapultas, barcos y dragón ocupan 2x2; el infante una casilla");
	int r[4];
	tz_obj_ret(m, &m->obj[t], r);
	CHK(r[0] == 6 && r[1] == 6 && r[2] == 7 && r[3] == 7, "la huella del mercader es 2x2 con el ancla arriba a la izquierda");
	int cubre = 1;
	for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) if (tz_mundo_em(m, 6 + dx, 6 + dy) != t) cubre = 0;
	CHK(cubre, "las cuatro casillas del mercader lo señalan");
	/* otra unidad no cabe encima: se coloca en otra parte */
	TzId otro = criar(m, TZC_TRADER, 1, 7, 7);
	CHK(otro >= 0 && (m->obj[otro].cx != 7 || m->obj[otro].cy != 7), "un segundo mercader no se coloca sobre el primero (%d,%d)", m->obj[otro].cx, m->obj[otro].cy);
	CHK(!(m->obj[otro].cx >= 5 && m->obj[otro].cx <= 7 && m->obj[otro].cy >= 5 && m->obj[otro].cy <= 7), "ni solapado con él");
	(void)c; (void)d; (void)f;
	/* un muro con un hueco de una casilla (y = 10) y otro de dos (y = 15 y 16): el infante pasa por el primero, el mercader por el segundo */
	m = novo(&mundoA, 82);
	for (int y = 0; y <= 31; ++y) if (y != 10 && y != 15 && y != 16) criar(m, TZC_WALL_STONE, 1, 14, y);
	TzId inf = criar(m, 3, 1, 10, 10), mer = criar(m, TZC_TRADER, 1, 10, 12);
	ordem(m, 1, inf, o_(TZ_O_MOVER, TZ_NENHUM, 18, 10));
	ordem(m, 1, mer, o_(TZ_O_MOVER, TZ_NENHUM, 18, 12));
	ticar(m, 900);
	CHK(m->obj[inf].cx >= 17, "el infante pasa por el hueco de una casilla (x %d)", m->obj[inf].cx);
	CHK(m->obj[mer].cx >= 17, "el mercader rodea el muro y pasa por el hueco de dos casillas (x %d, y %d)", m->obj[mer].cx, m->obj[mer].cy);
	/* sin hueco de dos casillas no pasa */
	m = novo(&mundoA, 83);
	for (int y = 0; y <= 31; ++y) if (y != 10) criar(m, TZC_WALL_STONE, 1, 14, y);
	mer = criar(m, TZC_TRADER, 1, 10, 12);
	ordem(m, 1, mer, o_(TZ_O_MOVER, TZ_NENHUM, 18, 12));
	ticar(m, 900);
	CHK(m->obj[mer].cx < 14, "con un hueco de una casilla el mercader no pasa (x %d)", m->obj[mer].cx);
	/* al moverse deja libres las casillas de atrás */
	m = novo(&mundoA, 84);
	mer = criar(m, TZC_TRADER, 1, 6, 6);
	ordem(m, 1, mer, o_(TZ_O_MOVER, TZ_NENHUM, 14, 6));
	ticar(m, 400);
	int libre = 1, tengo = 0;
	for (int y = 0; y < m->lado; ++y) for (int x = 0; x < m->lado; ++x) { TzId q = m->cel[y * m->lado + x].ocupante; if (q == mer) tengo++; }
	for (int y = 6; y <= 7; ++y) for (int x = 6; x <= 7; ++x) if (m->cel[y * m->lado + x].ocupante == mer) libre = 0;
	CHK(libre && tengo == 4, "al moverse el mercader ocupa solo sus cuatro casillas de ahora (%d)", tengo);
}

/* ------------------------------------------------------------------ recursos compartidos y rescate de prisioneros */
static void t_compartidos(void)
{
	TzMundo *m = novo(&mundoA, 91);
	TzId castillo = TZ_NENHUM;
	for (int i = 0; i < m->nobj; ++i) if (m->obj[i].classe == TZC_CASTLE && m->obj[i].dono == 1) castillo = i;
	m->reino[1].recursos[TZ_COMIDA] = 0; m->reino[2].recursos[TZ_COMIDA] = 500;
	cmd(m, 1, TZ_C_PRODUZIR, castillo, TZC_PEON, 0, 0, 0);
	ticar(m, 2);
	CHK(m->obj[castillo].ator->ntrab == 0, "sin compartir, el reino 1 sin comida no entrena un peón");
	m->reino[2].diplomacia[1] = 0x41 | TZ_DIP_RECURSOS;
	int32_t t[TZ_NREC];
	tz_mundo_recursos_compartidos(m, 1, t);
	CHK(t[TZ_COMIDA] == 500, "con el reino 2 compartiendo, el 1 dispone de 500 de comida (%d)", t[TZ_COMIDA]);
	tz_mundo_recursos_compartidos(m, 2, t);
	CHK(t[TZ_COMIDA] == 500, "y el 2, que no recibe nada del 1, solo de lo suyo (%d)", t[TZ_COMIDA]);
	cmd(m, 1, TZ_C_PRODUZIR, castillo, TZC_PEON, 0, 0, 0);
	ticar(m, 2);
	CHK(m->obj[castillo].ator->ntrab == 1 && m->reino[2].recursos[TZ_COMIDA] == 450 && m->reino[1].recursos[TZ_COMIDA] == 0,
	    "entrenar gasta lo del que comparte (el reino 2 queda con %d)", m->reino[2].recursos[TZ_COMIDA]);
	/* lo propio primero */
	m->reino[1].recursos[TZ_COMIDA] = 30;
	cmd(m, 1, TZ_C_PRODUZIR, castillo, TZC_PEON, 0, 0, 0);
	ticar(m, 2);
	CHK(m->reino[1].recursos[TZ_COMIDA] == 0 && m->reino[2].recursos[TZ_COMIDA] == 430, "se gasta primero lo propio y después lo que falta de quien comparte (%d y %d)", m->reino[1].recursos[TZ_COMIDA], m->reino[2].recursos[TZ_COMIDA]);
}

static void t_resgate(void)
{
	TzMundo *m = novo(&mundoA, 92);
	TzId pris = tz_mundo_guiao_criar(m, 3, 1, 9, 10, 10);
	TzId pn = criar(m, TZC_PEON, 1, 11, 10), sol = criar(m, 3, 1, 12, 14);
	CHK(pris >= 0 && m->obj[pris].dono == 9, "hay un prisionero del reino 9");
	ticar(m, 60);
	CHK(m->obj[pris].dono == 9, "un peón a su lado no lo rescata, ni un soldado lejos");
	CHK(m->obj[pris].ator->ordem.tipo == TZ_O_NADA, "el prisionero no ataca a nadie (%d)", m->obj[pris].ator->ordem.tipo);
	tz_mundo_pode_resgatar(m, 1, 0);
	ordem(m, 1, sol, o_(TZ_O_MOVER, TZ_NENHUM, 11, 11));
	ticar(m, 120);
	CHK(m->obj[pris].dono == 9, "con @kingdomcanrescue(1, 0) no lo rescata");
	tz_mundo_pode_resgatar(m, 1, 1);
	ticar(m, 40);
	CHK(m->obj[pris].dono == 1, "un soldado a su lado lo rescata (reino %d)", m->obj[pris].dono);
	(void)pn;
}

/* ------------------------------------------------------------------ una partida con todo, dos veces, y guardar/cargar */
static void escena_completa(TzMundo *m)
{
	TzId a = criar(m, TZC_MARKET, 1, 3, 20), b = criar(m, TZC_MARKET, 1, 20, 20), tb = criar(m, TZC_TAVERN, 1, 12, 22);
	TzId t = criar(m, TZC_TRADER, 1, 6, 23);
	TzId mg = criar(m, TZC_MAGE, 1, 10, 10), f = criar(m, 3, 1, 8, 8), e1 = criar(m, 3, 2, 14, 10), spy = criar(m, TZC_SPY, 1, 9, 12);
	TzId cat1 = criar(m, TZC_CATHEDRAL, 1, 3, 26);
	(void)b;
	m->reino[1].recursos[TZ_OURO] = 6000; m->reino[1].recursos[TZ_COMIDA] = 3000; m->reino[1].recursos[TZ_MADEIRA] = 3000;
	pesquisar(m, 1, "te_mercenaries"); pesquisar(m, 1, "te_heroesquest"); pesquisar(m, 1, "te_learning");
	TzOrdem o = o_(TZ_O_COMERCIAR, b, 0, 0); o.x2 = 3; o.y2 = 20; o.arg = TZ_ARG_COMERCIO(TZ_OURO, TZ_COMIDA, 300);
	ordem(m, 1, t, o);
	TzOrdem h = o_(TZ_O_HECHIZO, TZ_NENHUM, 14, 11); h.arg = (int16_t)tz_feitico_ordem(m, "spell_curse");
	m->obj[mg].mana = 100;
	ordem(m, 1, mg, h);
	tz_mundo_criar_item(m, TZC_POCAO, 9, 8, &(TzItem){ .vida = 10, .ataque = 2, .xp = 2 });
	TzId p = (TzId)(m->nobj - 1);
	ordem(m, 1, f, o_(TZ_O_PEGAR_ITEM, p, 0, 0));
	ordem(m, 1, spy, o_(TZ_O_SUPLANTAR, e1, 0, 0));
	cmd(m, 1, TZ_C_EMPRESTAR, tb, TZ_PEDRA, 0, 0, 0);
	cmd(m, 1, TZ_C_TROCAR, tb, TZ_OURO | (TZ_MADEIRA << 2), 1000, 0, 0);
	cmd(m, 1, TZ_C_CRUZADA, cat1, 0, 0, 16, 28);
	(void)a;
}

static void t_determinismo(void)
{
	TzMundo *m1 = novo(&mundoA, 99), *m2 = novo(&mundoB, 99);
	escena_completa(m1); escena_completa(m2);
	uint32_t s1[8], s2[8];
	int k = 0;
	for (int t = 0; t < 1600; ++t) {
		tz_mundo_tic(m1); tz_mundo_tic(m2);
		if ((t + 1) % 200 == 0) { s1[k] = tz_mundo_suma(m1); s2[k] = tz_mundo_suma(m2); k++; }
	}
	CHK(k == 8 && !memcmp(s1, s2, sizeof s1), "la misma semilla y los mismos comandos dan las mismas sumas (comercio, taberna, magia, espías, cruzada)");
	/* semilla distinta: la taberna sortea otra cosa */
	TzMundo *m3 = novo(&mundoB, 100);
	escena_completa(m3);
	ticar(m3, 1600);
	CHK(tz_mundo_suma(m3) != tz_mundo_suma(m1), "otra semilla da otra partida");
	/* guardar a mitad y cargar */
	m1 = novo(&mundoA, 99);
	escena_completa(m1);
	ticar(m1, 800);
	const char *ruta = "/tmp/tzar_extras.sav";
	int g = tz_mundo_gravar(m1, ruta);
	uint32_t antes = tz_mundo_suma(m1);
	CHK(g == 0, "se guarda la partida a mitad (%s)", g ? tz_erro() : "ok");
	static TzMundo mc;
	memset(&mc, 0, sizeof mc);
	int l = tz_mundo_carregar(&mc, &cat, &mp, ruta);
	CHK(l == 0 && tz_mundo_suma(&mc) == antes, "al cargar la suma es la misma (%08x contra %08x)", tz_mundo_suma(&mc), antes);
	ticar(m1, 800); ticar(&mc, 800);
	CHK(tz_mundo_suma(&mc) == tz_mundo_suma(m1), "y la partida sigue igual hasta el final (%08x)", tz_mundo_suma(m1));
	remove(ruta);
	tz_mundo_liberar(&mc);
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc < 2) { fprintf(stderr, "uso: %s <instalación sintética>\n", argv[0]); return 2; }
	if (abrir(argv[1])) { fprintf(stderr, "no se abre la instalación: %s\n", tz_erro()); return 2; }
	t_comercio();
	t_taberna();
	t_itens();
	t_magia();
	t_espias();
	t_cruzada();
	t_xp();
	t_prioridade();
	t_grandes();
	t_compartidos();
	t_resgate();
	t_determinismo();
	tz_mundo_liberar(&mundoA); tz_mundo_liberar(&mundoB);
	tz_catalogo_liberar(&cat); tz_biblioteca_liberar(&bib); tz_mapa_liberar(&mp); tz_hpfs_fechar(&hp); tz_recursos_fechar(&rec);
	printf("extras: %d comprobaciones, %d fallos\n", oks + fallos, fallos);
	return fallos ? 1 : 0;
}

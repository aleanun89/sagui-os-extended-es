/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ia_casos.c - casos sintéticos de lo que la IA de Tzar hace con el agua, la magia, los muros y las metas propias
 * (D417, fase 6; D470), en el PC con la simulación real.
 *
 * Uso: tzar_ia_casos <instalación sintética de tools/tzar_ia_sintetico.py>
 * Sobre el mapa Maps/IA2.wmp (dos islas separadas por un canal) pone a mano barcos, magos, muros y unidades enemigas y
 * comprueba, con las funciones internas del jugador (ia/ia_int.h): por dónde se llega (tierra, mar, no), el transporte
 * de una unidad a la otra isla, el barco que pide la flota, el sitio de un puerto, los hechizos antes de atacar (lluvia
 * de fuego sobre un grupo, rayo), el sacerdote que cura, la brecha en un muro enemigo, la yihad, el cambio de la
 * taberna y la rendición (permitida o no). Escribe «tzar_ia_casos: N comprobaciones, M fallos».
 */
#include "mundo.h"
#include "ia/ia_int.h"
#include <stdlib.h>
#include <string.h>

static int nchk, nfalhas;
#define CHK(c, ...) do { nchk++; if (!(c)) { nfalhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static TzCatalogo cat;
static TzMapa mp;
static TzIaBiblioteca bib;
static TzMundo m;

static TzIa *mundo_novo(int rendicao, int k, const char *guiao, int modo)
{
	TzRegras r;
	memset(&r, 0, sizeof r);
	r.populacao = 200;
	r.dificuldade = 1;
	r.sem_nevoa = 1;
	r.ia_rendicao = (uint8_t)rendicao;
	if (m.cel) tz_mundo_liberar(&m);
	memset(&m, 0, sizeof m);
	if (tz_mundo_criar(&m, &cat, &mp, &r, 777)) { printf("mundo: %s\n", tz_erro()); exit(1); }
	TzIaErro e;
	TzIa *ia = tz_ia_criar(&m, &bib, k, modo, guiao, 0, 1, &e);
	if (!ia) { char t[600]; tz_ia_erro_texto(&e, t, sizeof t); printf("ia: %s\n", t); exit(1); }
	if (!modo) tz_ia_ordem(ia, "none");
	return ia;
}

static TzId criar(int classe, int dono, int x, int y)
{
	TzId i = tz_mundo_guiao_criar(&m, classe, m.reino[dono].civ, dono, x, y);
	if (i < 0) printf("no se pudo crear %s en %d,%d: %s\n", tz_classes[classe], x, y, tz_erro());
	return i;
}

static void tics(int n) { for (int t = 0; t < n; ++t) tz_mundo_tic(&m); }

static int ordem(TzId i) { return m.obj[i].ator ? m.obj[i].ator->ordem.tipo : -1; }

static void caso_alcance(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	ia_preparar(ia);
	CHK(ia->nagua == 1, "una región de agua en el canal (%d)", ia->nagua);
	int dir[4] = { 45, 20, 45, 20 }, esq[4] = { 5, 30, 5, 30 };
	CHK(ia_alcanca(ia, 10, 6, esq) && ia_via_ponto(ia, 10, 6, esq) == IA_VIA_TERRA, "la misma isla se alcanza por tierra");
	CHK(!ia_alcanca(ia, 10, 6, dir) && ia_via_ponto(ia, 10, 6, dir) == IA_VIA_NAO, "sin barco la otra isla no se alcanza");
	CHK(ia->quer_navios[1] >= 1, "el canal (más de 60 casillas) quiere un barco (%d)", ia->quer_navios[1]);
	criar(TZC_GALLEON, 1, 30, 20);
	ia_preparar(ia);
	CHK(ia->navios.n == 1 && ia_via_ponto(ia, 10, 6, dir) == IA_VIA_MAR, "con un galeón la otra isla se alcanza por mar");
	tz_ia_liberar(ia);
}

static void caso_transporte(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	TzId s = criar(TZC_GALLEON, 1, 30, 20);
	TzId u = criar(3, 1, 20, 20);   /* infante */
	ia_preparar(ia);
	CHK(s >= 0 && u >= 0 && ia_transportar(ia, u, 45, 22, 1) == 1, "la unidad sube al barco para cruzar");
	int t, subiu = 0;
	for (t = 0; t < 6000; t += 20) {
		tics(20);
		if (m.obj[u].flags & TZ_F_DENTRO) subiu = 1;
		ia_preparar(ia);
		ia_transportes(ia);
		if (!(m.obj[u].flags & TZ_F_DENTRO) && m.obj[u].cx >= 38 && !ia->ntr) break;
	}
	CHK(subiu, "la unidad entró en el barco");
	CHK(m.obj[u].cx >= 38 && tz_obj_vivo(&m.obj[u]), "la unidad desembarcó en la otra isla (%d,%d, tic %u)", m.obj[u].cx, m.obj[u].cy, m.tic);
	CHK(ia->contas_d470.desembarques == 1 && ia->ntr == 0, "el transporte terminó (%u)", ia->contas_d470.desembarques);
	tz_ia_liberar(ia);
}

static void caso_frota(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	ia_preparar(ia);
	int x, y;
	ia->agua_porto = 1;
	CHK(ia_sitio_porto(ia, ia->casa[0], ia->casa[1], &x, &y) && x + 2 >= 26 && x <= 26, "el puerto va en la costa de la isla (%d,%d)", x, y);
	TzId h = criar(TZC_HARBOR, 1, x, y);
	ia_preparar(ia);
	CHK(h >= 0 && ia->portos.n == 1 && ia_agua_de(ia, h) == 1, "el puerto está en el canal");
	ia->quer_navios[1] = 1;
	ia_gastavel(ia, ia->disponivel);
	ia_frota(ia);
	CHK(ia->contas_d470.navios == 1, "el puerto hace el barco que quiere el agua");
	tics(20 * 25);
	int n = 0;
	for (TzId i = 0; i < m.nobj; ++i) if (m.obj[i].classe == TZC_GALLEON && m.obj[i].dono == 1 && tz_obj_vivo(&m.obj[i])) n++;
	CHK(n == 1, "el galeón sale del puerto (%d)", n);
	tz_ia_liberar(ia);
}

static void caso_magia(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	TzId mago = criar(TZC_MAGE, 1, 12, 30);
	TzId v[6];
	for (int j = 0; j < 6; ++j) v[j] = criar(3, 2, 17 + j % 3, 30 + j / 3);
	CHK(tz_mundo_relacao(&m, 1, 2) < 0, "los reinos 1 y 2 están en guerra");
	TzOrdem o;
	memset(&o, 0, sizeof o);
	o.tipo = TZ_O_ATACAR; o.alvo = v[0]; o.x = m.obj[v[0]].cx; o.y = m.obj[v[0]].cy;
	tz_mundo_guiao_ordem(&m, mago, &o);
	ia_preparar(ia);
	ia_magias(ia);
	tics(1);
	int lluvia = tz_feitico_ordem(&m, "spell_firerain");
	CHK(ia->contas_d470.hechizos == 1 && ordem(mago) == TZ_O_HECHIZO && m.obj[mago].ator->ordem.arg == lluvia,
	    "el mago lanza la lluvia de fuego sobre el grupo (orden %d)", ordem(mago));
	CHK(m.obj[mago].ator->nfila == 1 && m.obj[mago].ator->fila[0].tipo == TZ_O_ATACAR, "tras el hechizo vuelve a atacar");
	/* uno solo y fuerte (un galeón enemigo, sin remolino): el rayo */
	TzIa *ia2 = mundo_novo(0, 1, NULL, 0);
	(void)ia;
	mago = criar(TZC_MAGE, 1, 22, 40);
	TzId c = criar(TZC_GALLEON, 2, 29, 40);
	memset(&o, 0, sizeof o);
	o.tipo = TZ_O_ATACAR; o.alvo = c; o.x = 29; o.y = 40;
	tz_mundo_guiao_ordem(&m, mago, &o);
	ia_preparar(ia2);
	ia_magias(ia2);
	tics(1);
	CHK(ordem(mago) == TZ_O_HECHIZO && m.obj[mago].ator->ordem.arg == tz_feitico_ordem(&m, "spell_lightning"), "contra una sola unidad fuerte, el rayo");
	tz_ia_liberar(ia2);
	tz_ia_liberar(ia);
	/* el sacerdote cura al herido */
	ia = mundo_novo(0, 1, NULL, 0);
	TzId p = criar(TZC_PRIEST, 1, 12, 30), f = criar(3, 1, 14, 30);
	m.obj[f].vida = 20;
	ia_preparar(ia);
	for (int j = 0; j < 40 && !ia->contas_d470.curas; ++j) ia_ocioso_sacerdote(ia, p);
	tics(1);
	CHK(ia->contas_d470.curas == 1 && ordem(p) == TZ_O_HECHIZO, "el sacerdote ocioso cura al herido");
	tz_ia_liberar(ia);
}

static void caso_brecha(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	/* un muro de piedra del reino 2 alrededor de su castillo (50..54, 8..12) */
	int muros = 0;
	for (int y = 5; y <= 15; ++y)
		for (int x = 47; x <= 57; ++x)
			if (x == 47 || x == 57 || y == 5 || y == 15) muros += criar(TZC_WALL_STONE, 2, x, y) >= 0;
	TzId u = criar(3, 1, 45, 30);
	tics(41);   /* la simulación rehace sus islas cada 40 tics */
	ia_preparar(ia);
	int alvo[4] = { 48, 7, 48, 7 };   /* dentro del muro, junto al castillo */
	CHK(muros == 40 && !ia_alcanca(ia, 45, 30, alvo) && ia_via_ponto(ia, 45, 30, alvo) == IA_VIA_NAO, "el castillo amurallado no se alcanza por tierra (%d muros)", muros);
	CHK(ia_brecha_alcanca(ia, 45, 30, alvo), "quitando los muros enemigos sí");
	CHK(ia_brecha_mandar(ia, u, 48, 7) && ia->nbrechas >= 1, "la unidad va contra el muro (%d marcados)", ia->nbrechas);
	tics(1);
	TzId w = m.obj[u].ator->ordem.alvo;
	CHK(ordem(u) == TZ_O_ATACAR && w >= 0 && m.obj[w].classe == TZC_WALL_STONE, "su orden es atacar un muro");
	CHK(ia_brecha_de(ia, u), "atacando la brecha no es candidata para otros lugares");
	tz_mundo_guiao_matar(&m, w);
	tics(40);
	ia_preparar(ia);
	CHK(ia->nbrechas == 0, "al caer el muro marcado se quitan las marcas (%d)", ia->nbrechas);
	tz_ia_liberar(ia);
}

static void caso_metas(void)
{
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	m.reino[1].civ = TZ_CIV_ARAB;
	ia->civ = TZ_CIV_ARAB;
	ia_preparar(ia);
	CHK(ia_jihad(ia) && ia->contas_d470.jihad == 1, "un campesino árabe se hace guerrero de la yihad");
	tics(1);
	int n = 0;
	for (TzId i = 0; i < m.nobj; ++i) if (m.obj[i].classe == TZC_JIHAD && m.obj[i].dono == 1 && tz_obj_vivo(&m.obj[i])) n++;
	CHK(n == 1, "hay un guerrero de la yihad (%d)", n);
	tz_ia_liberar(ia);
	/* la taberna cambia comida por oro */
	ia = mundo_novo(0, 1, NULL, 0);
	TzId t = criar(TZC_TAVERN, 1, 14, 14);
	ia_preparar(ia);
	ia_gastavel(ia, ia->disponivel);
	ia_converter(ia, 60);
	tics(1);
	CHK(t >= 0 && m.obj[t].ator && m.obj[t].ator->troca.ativa && m.obj[t].ator->troca.de == TZ_COMIDA && m.obj[t].ator->troca.pago == 1000,
	    "la taberna cambia 1000 de comida (meta 60, en múltiplos de 500)");
	tz_ia_liberar(ia);
}

static void caso_captura(void)
{
	/* una torre de madera enemiga junto a los campesinos: la milicia la captura (war.rs militia con capturable) */
	TzIa *ia = mundo_novo(0, 1, NULL, 0);
	TzId t = criar(40, 2, 15, 9);
	ia_preparar(ia);
	CHK(t >= 0 && ia_capturavel(ia, t), "la torre enemiga se puede capturar");
	ia_milicia(ia, t);
	tics(1);
	int n = 0;
	for (TzId i = 0; i < m.nobj; ++i) if (m.obj[i].classe == TZC_PEON && m.obj[i].dono == 1 && ordem(i) == TZ_O_CAPTURAR) n++;
	CHK(ia->contas_d470.capturas == 1 && n >= 1, "los campesinos van a capturarla (%d)", n);
	tics(20 * 120);
	CHK(m.obj[t].dono == 1, "la torre pasa al reino 1 (dueño %d)", m.obj[t].dono);
	tz_ia_liberar(ia);
}

static void caso_rendicao(void)
{
	/* el reino 2 no tiene campesinos: con rendir.ais se rinde si la partida lo deja */
	for (int pode = 0; pode <= 1; ++pode) {
		TzIa *ia = mundo_novo(pode, 2, "rendir.ais", TZ_IA_COMPLETA);
		tics(20 * 60);
		CHK(m.reino[2].rendido == pode, "rendición %s: el reino 2 %s", pode ? "permitida" : "no permitida", m.reino[2].rendido ? "se rinde" : "sigue");
		tz_ia_liberar(ia);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "uso: %s <instalación sintética>\n", argv[0]); return 2; }
	static TzRecursos r;
	static TzBiblioteca b;
	static TzHpfs c;
	char ruta[1024];
	if (tz_recursos_abrir(&r, argv[1])) { printf("sin datos: %s\n", tz_erro()); return 1; }
	snprintf(ruta, sizeof ruta, "%s/Maps/IA2.wmp", argv[1]);
	if (tz_hpfs_abrir(&c, ruta) || tz_mapa_ler(&c, &mp)) { printf("mapa: %s\n", tz_erro()); return 1; }
	if (tz_biblioteca_carregar(&b, &r, mp.bioma)) { printf("biblioteca: %s\n", tz_erro()); return 1; }
	if (tz_catalogo_carregar(&cat, &r, &b)) { printf("catálogo: %s\n", tz_erro()); return 1; }
	if (tz_ia_bib_carregar(&bib, &r)) { printf("guiones: %s\n", tz_erro()); return 1; }
	caso_alcance();
	caso_transporte();
	caso_frota();
	caso_magia();
	caso_brecha();
	caso_metas();
	caso_captura();
	caso_rendicao();
	tz_mundo_liberar(&m);
	tz_ia_bib_liberar(&bib);
	tz_catalogo_liberar(&cat);
	tz_biblioteca_liberar(&b);
	tz_mapa_liberar(&mp);
	tz_hpfs_fechar(&c);
	tz_recursos_fechar(&r);
	printf("tzar_ia_casos: %d comprobaciones, %d fallos\n", nchk, nfalhas);
	return nfalhas ? 3 : 0;
}

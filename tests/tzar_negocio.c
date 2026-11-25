/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_negocio.c - pruebas en el PC (ASan/UBSan, datos SINTÉTICOS) de las ventanas de negocio de Tzar (ui_negocio.c, v8.5.6) con la
 * SIMULACIÓN REAL y la interfaz real: se abre cada ventana, se dibuja (800x600) y se pulsan sus botones como un jugador:
 * contratar en la taberna, dados, préstamo y devolución, intercambio, ruta de mercaderes, soltar y dar un objeto, cruzada,
 * tributo, rendición, fábrica mágica y mercado. Uso: tzar_negocio <instalación sintética> <carpeta-capturas>. Sale con 1 si falla algo. */
#include "mundo_int.h"
#include "ui.h"
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
static TzMundo mundo;
static TzUiSkin skin;
static TzUi ui;
static TzTela tela;
static uint32_t reloj = 1000;
static const char *capturas;

/* ui_mapa.c dibuja el mundo con tz_desenhar_mundo (partida.c): aquí no hace falta */
int tz_desenhar_mundo(TzMundo *m, TzEscena *e, TzLienzo *l, int camx, int camy, uint64_t ms) { (void)m; (void)e; (void)l; (void)camx; (void)camy; (void)ms; return 0; }

static void ticar(int n) { for (int i = 0; i < n; ++i) { tz_mundo_tic(&mundo); reloj += 50; tz_ui_tic(&ui, reloj); } }
static TzId criar(int classe, int dono, int x, int y)
{
	TzId i = tz_mundo_guiao_criar(&mundo, classe, 0, dono, x, y);
	if (i < 0) { printf("FALLO no cabe la clase %d en (%d,%d)\n", classe, x, y); exit(2); }
	return i;
}
static void novo(uint32_t semente)
{
	if (ui.skin) tz_ui_liberar(&ui);
	TzRegras r; memset(&r, 0, sizeof r);
	r.populacao = 200; r.sem_nevoa = 1;
	if (mundo.cel) tz_mundo_liberar(&mundo);
	if (tz_mundo_criar(&mundo, &cat, &mp, &r, semente)) { printf("FALLO no se pudo crear el mundo: %s\n", tz_erro()); exit(2); }
	for (int i = 0; i < mundo.nobj; ++i) if (mundo.obj[i].classe != TZC_CASTLE) tz_mundo_guiao_remover(&mundo, i);
	for (int k = 1; k <= 2; ++k) for (int q = 0; q < TZ_NREC; ++q) mundo.reino[k].recursos[q] = 0;
	mundo.reino[1].recursos[TZ_OURO] = 20000; mundo.reino[1].recursos[TZ_COMIDA] = 20000; mundo.reino[1].recursos[TZ_MADEIRA] = 20000; mundo.reino[1].recursos[TZ_PEDRA] = 20000;
	tz_ui_iniciar(&ui, &skin, &mundo, &bib, 800, 600);
	ui.pergaminho_pedido = TZ_NENHUM;
}
static void pesquisar(int k, const char *t) { int q = tz_catalogo_tec(&cat, t); if (q >= 0) tz_mundo_guiao_pesquisa(&mundo, k, q); }
static void selecionar(TzId id) { ui.sel.n = 1; ui.sel.id[0] = id; tz_ui_menu_atualizar(&ui); }

static void ev(int tipo, int x, int y, int botao, int tecla, int mods)
{
	TzUiEntrada e = { tipo, x, y, botao, tecla, mods, 0 };
	reloj += 50;
	tz_ui_entrada(&ui, &e, reloj);
}
/* dibuja la ventana abierta (anota sus botones) y pulsa el de este identificador; 0 si no está */
static int pulsar(int id, int botao, int mods)
{
	TzRect j = tz_ui_janela_rect(&ui);
	tz_tela_preencher(&tela, (TzRect){ 0, 0, tela.w, tela.h }, 0xff000000);
	tz_ui_janela_desenhar(&ui, &tela);
	(void)j;
	for (int i = 0; i < ui.njw; ++i)
		if (ui.jw[i].id == id) {
			int x = ui.jw[i].r.x + ui.jw[i].r.w / 2, y = ui.jw[i].r.y + ui.jw[i].r.h / 2;
			ev(TZ_IN_MOVER, x, y, 0, 0, 0);
			ev(TZ_IN_BAIXO, x, y, botao, 0, mods);
			ev(TZ_IN_CIMA, x, y, botao, 0, mods);
			return 1;
		}
	return 0;
}
static int tem_botao(int id)
{
	tz_ui_janela_desenhar(&ui, &tela);
	for (int i = 0; i < ui.njw; ++i) if (ui.jw[i].id == id) return 1;
	return 0;
}
/* cuántos píxeles de la ventana no son del fondo (la ventana se dibujó) y captura sintética */
static int desenhada(const char *nome)
{
	tz_tela_preencher(&tela, (TzRect){ 0, 0, tela.w, tela.h }, 0xff000000);
	tz_ui_janela_desenhar(&ui, &tela);
	TzRect j = tz_ui_janela_rect(&ui);
	int n = 0;
	for (int y = j.y; y < j.y + j.h && y < tela.h; ++y) for (int x = j.x; x < j.x + j.w && x < tela.w; ++x) if (tela.px[y * tela.w + x] != 0xff000000u) n++;
	if (capturas && nome) { char r[512]; snprintf(r, sizeof r, "%s/negocio-%s.bmp", capturas, nome); tz_tela_gravar_bmp(&tela, r); }
	return n;
}
static int contar(int classe, int dono)
{
	int n = 0;
	for (int i = 0; i < mundo.nobj; ++i) if (mundo.obj[i].classe == classe && mundo.obj[i].dono == dono && tz_obj_vivo(&mundo.obj[i])) n++;
	return n;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	if (argc < 2) { fprintf(stderr, "uso: %s <instalación sintética> [carpeta-capturas]\n", argv[0]); return 2; }
	capturas = argc > 2 ? argv[2] : NULL;
	char ruta[1024];
	if (tz_recursos_abrir(&rec, argv[1])) return 2;
	snprintf(ruta, sizeof ruta, "%s/prueba/sim.wmp", argv[1]);
	if (tz_hpfs_abrir(&hp, ruta) || tz_mapa_ler(&hp, &mp) || tz_biblioteca_carregar(&bib, &rec, mp.bioma) || tz_catalogo_carregar(&cat, &rec, &bib)) { fprintf(stderr, "no abre: %s\n", tz_erro()); return 2; }
	/* a mano (nunca en la batería): con la instalación real como tercer argumento, el arte, las fuentes y los textos del juego para las capturas */
	static TzRecursos rec_real;
	if (argc > 3 && !tz_recursos_abrir(&rec_real, argv[3]) && !tz_ui_skin_carregar(&skin, &rec_real)) printf("(arte real de %s)\n", argv[3]);
	else tz_ui_skin_vazia(&skin);
	tz_tela_criar(&tela, 800, 600);

	/* ---------------- taberna: contratar */
	novo(11);
	TzId tb = criar(TZC_TAVERN, 1, 12, 20);
	pesquisar(1, "te_mercenaries"); pesquisar(1, "te_heroesquest");
	selecionar(tb);
	ticar(2500);
	CHK(tz_ui_neg_acao(&ui, "", "tavern_merc", "tavern_merc") && ui.janela == TZ_UJ_TABERNA && ui.jan.obj == tb, "el botón «contratar mercenarios» abre la ventana de la taberna");
	CHK(desenhada("taberna") > 5000, "la taberna se dibuja");
	struct TzOferta of[8];
	int n0 = tz_mundo_ofertas(&mundo, tb, 0, of, 8);
	CHK(n0 >= 1 && tem_botao(TZ_W_LINHA), "la taberna lista %d ofertas de mercenarios con su botón", n0);
	int unid0 = 0; for (int i = 0; i < mundo.nobj; ++i) unid0 += tz_obj_unidade(&mundo.obj[i]) && mundo.obj[i].dono == 1;
	int32_t oro0 = mundo.reino[1].recursos[TZ_OURO] + mundo.reino[1].recursos[TZ_COMIDA] + mundo.reino[1].recursos[TZ_MADEIRA] + mundo.reino[1].recursos[TZ_PEDRA];
	pulsar(TZ_W_LINHA, 1, 0);
	ticar(3);
	int unid1 = 0; for (int i = 0; i < mundo.nobj; ++i) unid1 += tz_obj_unidade(&mundo.obj[i]) && mundo.obj[i].dono == 1;
	int32_t oro1 = mundo.reino[1].recursos[TZ_OURO] + mundo.reino[1].recursos[TZ_COMIDA] + mundo.reino[1].recursos[TZ_MADEIRA] + mundo.reino[1].recursos[TZ_PEDRA];
	CHK(unid1 >= unid0 + of[0].quantidade && oro1 < oro0, "contratar en la ventana trae a los %d mercenarios y cobra (%d -> %d)", of[0].quantidade, oro0, oro1);
	pulsar(TZ_W_ABA0 + 1, 1, 0);
	CHK(ui.janela == TZ_UJ_TABERNA && ui.jan.aba == 1, "la pestaña «Héroes» muestra la lista de héroes");
	int nh = tz_mundo_ofertas(&mundo, tb, 1, of, 8);
	if (nh) {
		pulsar(TZ_W_LINHA, 1, 0); ticar(3);
		int hero = 0; for (int i = 0; i < mundo.nobj; ++i) if ((mundo.obj[i].flags & TZ_F_HEROI) && mundo.obj[i].dono == 1 && tz_heroi_nome(&mundo, &mundo.obj[i])) hero = 1;
		CHK(hero, "el héroe contratado en la ventana tiene nombre de HEROES.INI");
	}

	/* ---------------- dados */
	pulsar(TZ_W_ABA0 + 2, 1, 0);
	CHK(ui.janela == TZ_UJ_DADOS && desenhada("dados") > 4000, "la pestaña «Dados» abre la ventana de dados y se dibuja");
	int32_t o2 = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_ACEITAR, 1, 0);
	ticar(2);
	CHK(ui.jan.dado_ms > reloj - 100 || ui.jan.dado_cara != 0, "al apostar los dados ruedan (o ya hay resultado)");
	CHK(!tem_botao(TZ_W_ACEITAR) || ui.jan.dado_cara != 0, "mientras ruedan los dados no se puede apostar otra vez");
	reloj += 2000; ticar(1);
	CHK(ui.jan.dado_cara == 1 || ui.jan.dado_cara == 2, "el resultado llega por el evento del mundo (cara %d, valor %d)", ui.jan.dado_cara, ui.jan.dado_valor);
	CHK(mundo.reino[1].recursos[TZ_OURO] != o2, "la tirada cambió el oro (%d -> %d)", o2, mundo.reino[1].recursos[TZ_OURO]);
	{
		int q0 = ui.jan.dado_quant;
		pulsar(TZ_W_Q_MAIS, 1, 0);
		CHK(ui.jan.dado_quant == q0 + 50, "«+» sube la puesta de 50 en 50");
		pulsar(TZ_W_Q_MAIS, 1, TZ_MOD_SHIFT);
		CHK(ui.jan.dado_quant == q0 + 250, "Mayús sube la puesta de 200 en 200");
		pulsar(TZ_W_PROP, 1, 0);
		CHK(ui.jan.prop == 1, "la probabilidad cambia al 33 %%");
	}

	/* ---------------- préstamos */
	pulsar(TZ_W_ABA0 + 3, 1, 0);
	CHK(ui.janela == TZ_UJ_PRESTAMOS && desenhada("prestamos") > 3000, "la pestaña «Préstamo» abre la ventana de préstamos");
	int32_t g0 = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_LINHA, 1, 0); ticar(2);
	CHK(mundo.reino[1].emp_rec == 1 && mundo.reino[1].recursos[TZ_OURO] >= g0 + 900, "pedir un préstamo de oro suma 1000 (%d -> %d)", g0, mundo.reino[1].recursos[TZ_OURO]);
	CHK(tem_botao(TZ_W_ACEITAR) && !tem_botao(TZ_W_LINHA), "con un préstamo vivo la ventana ofrece devolverlo");
	g0 = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_ACEITAR, 1, 0); ticar(2);
	CHK(mundo.reino[1].emp_rec == 0 && mundo.reino[1].recursos[TZ_OURO] <= g0 - 1150, "devolver cuesta 1200 (%d -> %d)", g0, mundo.reino[1].recursos[TZ_OURO]);

	/* ---------------- intercambio */
	pulsar(TZ_W_ABA0 + 4, 1, 0);
	CHK(ui.janela == TZ_UJ_INTERCAMBIO && desenhada("intercambio") > 3000, "la pestaña «Intercambio» abre la ventana");
	pulsar(TZ_W_REC_B, 1, 0); pulsar(TZ_W_REC_B, 1, 0);   /* de oro (0) a piedra (1): la madera (3) pasa por el oro */
	CHK(ui.jan.rec[0] == TZ_OURO && ui.jan.rec[1] == TZ_PEDRA, "el intercambio va de oro a piedra");
	int32_t p0 = mundo.reino[1].recursos[TZ_PEDRA], g1 = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_ACEITAR, 1, 0); ticar(1200);
	CHK(mundo.reino[1].recursos[TZ_OURO] <= g1 - 500 + 1 && mundo.reino[1].recursos[TZ_PEDRA] >= p0 + 49, "ofrecer 500 de oro da 50 de otro recurso (%d -> %d)", p0, mundo.reino[1].recursos[TZ_PEDRA]);
	CHK(ui.janela == TZ_UJ_INTERCAMBIO, "la ventana sigue abierta tras el intercambio");
	pulsar(TZ_W_FECHAR, 1, 0);
	CHK(ui.janela == 0, "«Cerrar» cierra la ventana");

	/* ---------------- comercio */
	novo(12);
	TzId ma = criar(TZC_MARKET, 1, 3, 20), mb = criar(TZC_MARKET, 1, 24, 20), mc = criar(TZC_TRADER, 1, 6, 24);
	selecionar(mc);
	CHK(tz_ui_neg_acao(&ui, "trade", "trade", "") && ui.janela == TZ_UJ_COMERCIO, "el botón «comerciar» abre la ventana de comercio");
	CHK(ui.jan.mkt[0] >= 0 && ui.jan.mkt[1] >= 0 && ui.jan.mkt[0] != ui.jan.mkt[1], "los dos mercados se proponen solos");
	CHK(desenhada("comercio") > 5000, "la ventana de comercio se dibuja");
	pulsar(TZ_W_Q_MAIS, 1, 0);
	pulsar(TZ_W_REC_A, 1, 0);
	pulsar(TZ_W_ACEITAR, 1, 0);
	ticar(2);
	CHK(mundo.obj[mc].ator->ordem.tipo == TZ_O_COMERCIAR && mundo.obj[mc].ator->rota.ativa && ui.janela == 0, "«Iniciar compra-venta» manda la ruta al mercader");
	CHK(mundo.obj[mc].ator->rota.quantidade == 100, "con la carga elegida (100)");
	(void)ma; (void)mb;
	/* elegir un mercado con el mapa */
	selecionar(mc);
	tz_ui_neg_acao(&ui, "trade", "trade", "");
	pulsar(TZ_W_ESCOLHER_B, 1, 0);
	CHK(ui.janela == 0 && ui.pick == TZ_PICK_MERCADO1 && ui.modo == TZ_UI_ALVO, "«Elegir en el mapa» espera un clic");
	int px = 3 * TZ_CASILLA + 16, py = 20 * TZ_CASILLA + 16;
	tz_ui_neg_pick(&ui, px, py);
	CHK(ui.janela == TZ_UJ_COMERCIO && ui.jan.mkt[1] == ma, "el clic sobre el mercado lo pone de destino y reabre la ventana");

	/* ---------------- objetos */
	novo(13);
	TzId f = criar(3, 1, 10, 12), f2 = criar(3, 1, 12, 12);
	TzId esp = tz_mundo_criar_item(&mundo, TZC_ITEM_MAIS, 10, 13, &(TzItem){ .ataque = 10, .defesa = 4 });
	TzOrdem po; memset(&po, 0, sizeof po); po.tipo = TZ_O_PEGAR_ITEM; po.alvo = esp; po.x2 = po.y2 = -1;
	TzComando cm; memset(&cm, 0, sizeof cm); cm.tipo = TZ_C_ORDEM; cm.reino = 1; cm.ordem = po; cm.nids = 1; cm.ids[0] = f; tz_mundo_comando(&mundo, &cm);
	ticar(80);
	selecionar(f);
	CHK(mundo.obj[esp].dentro_de == f, "el infante recogió la espada");
	ev(TZ_IN_TECLA, 0, 0, 0, 'i', TZ_MOD_CTRL);
	CHK(ui.janela == TZ_UJ_OBJETOS && ui.jan.obj == f && desenhada("objetos") > 6000, "Ctrl+I abre el inventario de la unidad");
	int at0 = mundo.obj[f].ataque;
	CHK(at0 >= 10, "el equipo que lleva suma ataque (%d)", at0);
	pulsar(TZ_W_LINHA3, 1, 0); ticar(80);   /* Dar */
	CHK(mundo.obj[esp].dentro_de == f2, "«Dar» pasa la espada al otro infante");
	selecionar(f2);
	tz_ui_janela_abrir(&ui, TZ_UJ_OBJETOS, f2);
	pulsar(TZ_W_LINHA2, 1, 0); ticar(20);   /* Soltar */
	CHK(!(mundo.obj[esp].flags & TZ_F_CARREGADO), "«Soltar» deja la espada en el suelo");
	tz_ui_janela_abrir(&ui, TZ_UJ_OBJETOS, f2);
	CHK(tem_botao(TZ_W_MUNDO + esp), "la espada del suelo se ofrece para recoger");
	pulsar(TZ_W_MUNDO + esp, 1, 0); ticar(80);
	CHK(mundo.obj[esp].dentro_de == f2 && (mundo.obj[esp].flags & TZ_F_CARREGADO), "«Recoger» la vuelve a coger");

	/* ---------------- cruzada */
	novo(14);
	TzId cat1 = criar(TZC_CATHEDRAL, 1, 12, 20);
	ticar(100);
	selecionar(cat1);
	CHK(tz_ui_neg_acao(&ui, "create crusadeanchor", "preach_crusade", "") && ui.janela == TZ_UJ_CRUZADA && desenhada("cruzada") > 3000, "«Convocar Cruzada» abre la ventana de la cruzada");
	int ready = mundo.reino[1].cruzados;
	CHK(ready >= 1, "hay cruzados reunidos (%d)", ready);
	if (tem_botao(TZ_W_ACEITAR)) {
		pulsar(TZ_W_ACEITAR, 1, 0);
		CHK(ui.janela == 0 && ui.pick == TZ_PICK_CRUZADA, "«Elegir destino» espera un clic en el mapa");
		tz_ui_neg_pick(&ui, 20 * TZ_CASILLA + 16, 10 * TZ_CASILLA + 16);
		ticar(5);
		CHK(mundo.reino[1].cruz_ancla >= 0, "el clic en el mapa lanza la cruzada con su estandarte");
		ticar(400);
		int n_cru = contar(TZC_CRUSADER, 1);
		CHK(n_cru >= 1, "empiezan a llegar cruzados (%d)", n_cru);
		tz_ui_neg_acao(&ui, "select_crusaders", "select_crusaders", "");
		CHK(ui.sel.n == n_cru, "«Seleccionar Cruzada» elige a todos los cruzados (%d)", ui.sel.n);
	} else CHK(0, "la ventana de la cruzada ofrece convocarla");

	/* ---------------- tributos y rendición */
	novo(15);
	mundo.reino[1].recursos[TZ_OURO] = 1000; mundo.reino[2].recursos[TZ_OURO] = 0;
	ui.janela = TZ_UJ_DIPLOMACIA;
	TzRect jd = tz_ui_janela_rect(&ui);
	tz_ui_janela_desenhar(&ui, &tela);
	ev(TZ_IN_MOVER, jd.x + 60, jd.y + jd.h - 26, 0, 0, 0);
	ev(TZ_IN_BAIXO, jd.x + 60, jd.y + jd.h - 26, 1, 0, 0); ev(TZ_IN_CIMA, jd.x + 60, jd.y + jd.h - 26, 1, 0, 0);
	CHK(ui.janela == TZ_UJ_TRIBUTO && desenhada("tributo") > 3000, "el botón «Tributos» de la diplomacia abre la ventana de tributos");
	pulsar(TZ_W_Q_MAIS, 1, TZ_MOD_SHIFT);   /* 100 + 1000 */
	pulsar(TZ_W_ACEITAR, 1, 0); ticar(2);
	CHK(mundo.reino[1].recursos[TZ_OURO] == 0 && mundo.reino[2].recursos[TZ_OURO] == 1000, "el tributo pasa el oro (recortado a lo que hay): %d y %d", mundo.reino[1].recursos[TZ_OURO], mundo.reino[2].recursos[TZ_OURO]);
	pulsar(TZ_W_FECHAR, 1, 0);
	ui.janela = TZ_UJ_DIPLOMACIA;
	jd = tz_ui_janela_rect(&ui);
	tz_ui_janela_desenhar(&ui, &tela);
	ev(TZ_IN_MOVER, jd.x + jd.w - 60, jd.y + jd.h - 26, 0, 0, 0);
	ev(TZ_IN_BAIXO, jd.x + jd.w - 60, jd.y + jd.h - 26, 1, 0, 0); ev(TZ_IN_CIMA, jd.x + jd.w - 60, jd.y + jd.h - 26, 1, 0, 0);
	CHK(ui.janela == TZ_UJ_RENDICION && desenhada("rendicion") > 2000, "el botón «Rendirse» pide confirmación");
	pulsar(TZ_W_FECHAR, 1, 0);
	ticar(100);
	CHK(!mundo.reino[1].rendido, "cancelar la rendición no hace nada");
	ui.janela = TZ_UJ_RENDICION;
	pulsar(TZ_W_SIM, 1, 0); ticar(100);
	CHK(mundo.reino[1].rendido && mundo.reino[1].derrotado && ui.janela == 0, "confirmar la rendición deja al reino derrotado");

	/* ---------------- fábrica mágica */
	novo(16);
	int cl_of = tz_catalogo_classe("magicworkshop");
	TzId of2 = criar(cl_of, 1, 12, 20);
	selecionar(of2);
	CHK(tz_ui_neg_acao(&ui, "build_item", "build_item", "") && ui.janela == TZ_UJ_FABRICA && desenhada("fabrica") > 3000, "«Construir objeto mágico» abre la fábrica");
	pulsar(TZ_W_CAT0 + 2, 1, 0);   /* objetos de poder */
	pulsar(TZ_W_Q_MAIS, 1, TZ_MOD_SHIFT);
	int32_t oro_f = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_ACEITAR, 1, 0); ticar(2);
	CHK(tz_mundo_fabrica_ocupada(&mundo, 1) && mundo.reino[1].recursos[TZ_OURO] < oro_f, "la fábrica empieza a hacer el objeto y cobra el oro");

	/* ---------------- mercado y joyas */
	novo(17);
	TzId mk = criar(TZC_MARKET, 1, 12, 20);
	pesquisar(1, "te_jewelry");
	ticar(1300);
	selecionar(mk);
	CHK(tz_ui_neg_acao(&ui, "", "market_items", "market_items") && ui.janela == TZ_UJ_MERCADO && desenhada("mercado") > 3000, "«Joyas» abre la ventana del mercado");
	CHK(tem_botao(TZ_W_LINHA), "el mercado asiático ofrece una joya");
	int32_t og = mundo.reino[1].recursos[TZ_OURO];
	pulsar(TZ_W_LINHA, 1, 0); ticar(3);
	CHK(mundo.reino[1].recursos[TZ_OURO] < og || mundo.reino[1].njoias == 0, "comprar la joya cuesta oro");

	tz_tela_liberar(&tela);
	tz_ui_liberar(&ui);
	tz_mundo_liberar(&mundo);
	tz_catalogo_liberar(&cat); tz_biblioteca_liberar(&bib); tz_mapa_liberar(&mp); tz_hpfs_fechar(&hp); tz_recursos_fechar(&rec);
	tz_ui_skin_liberar(&skin);
	printf("negocio: %d comprobaciones, %d fallos\n", oks + fallos, fallos);
	return fallos ? 1 : 0;
}

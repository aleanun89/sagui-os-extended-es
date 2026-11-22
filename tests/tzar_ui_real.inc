/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ui_real.inc - parte privada de tzar_ui.c: capturas con los datos REALES del mantenedor (nunca en la batería ni
 * en git). Abre la instalación, arma el mundo de prueba con los objetos de un mapa y guarda menús y HUD en BMP. */
#include <dirent.h>
#include <strings.h>

static void grabar(const TzTela *t, const char *saida, const char *nome)
{
	char r[600];
	snprintf(r, sizeof r, "%s/%s.bmp", saida, nome);
	VER(tz_tela_gravar_bmp(t, r) == 0, "captura %s (%dx%d)", nome, t->w, t->h);
}

static int cmp_item(const void *a, const void *b) { return strcasecmp(((const TzMnItem *)a)->titulo, ((const TzMnItem *)b)->titulo); }

static void listas_do_menu(TzMenu *m, const char *raiz)
{
	char dir[700];
	snprintf(dir, sizeof dir, "%s/Campaign", raiz);
	DIR *d = opendir(dir);
	struct dirent *de;
	m->campanhas = calloc(32, sizeof *m->campanhas);
	while (d && (de = readdir(d)) && m->ncampanhas < 32) {
		size_t l = strlen(de->d_name);
		if (l > 4 && !strcasecmp(de->d_name + l - 4, ".wcm")) {
			TzMnItem *it = &m->campanhas[m->ncampanhas++];
			snprintf(it->titulo, sizeof it->titulo, "%.*s", (int)(l - 4), de->d_name);
			snprintf(it->fich, sizeof it->fich, "Campaign/%s", de->d_name);
		}
	}
	if (d) closedir(d);
	snprintf(dir, sizeof dir, "%s/Maps", raiz);
	d = opendir(dir);
	m->mapas = calloc(64, sizeof *m->mapas);
	while (d && (de = readdir(d)) && m->nmapas < 64) {
		size_t l = strlen(de->d_name);
		if (l > 4 && !strcasecmp(de->d_name + l - 4, ".wmp")) {
			TzMnItem *it = &m->mapas[m->nmapas++];
			snprintf(it->titulo, sizeof it->titulo, "%.*s", (int)(l - 4), de->d_name);
			snprintf(it->fich, sizeof it->fich, "Maps/%s", de->d_name);
			it->jogadores_min = 2; it->jogadores_max = 4;
		}
	}
	if (d) closedir(d);
	qsort(m->mapas, (size_t)m->nmapas, sizeof *m->mapas, cmp_item);
	m->guardados = calloc(4, sizeof *m->guardados);
	m->nguardados = 3;
	snprintf(m->guardados[0].nome, 64, "Reconquista, misión 2"); snprintf(m->guardados[0].data, 40, "12/11/2026 21:40");
	snprintf(m->guardados[1].nome, 64, "MP1 escaramuza"); snprintf(m->guardados[1].data, 40, "10/11/2026 18:05");
	snprintf(m->guardados[2].nome, 64, "Partida rápida"); snprintf(m->guardados[2].data, 40, "02/11/2026 09:12");
}

static void capturas_menus(TzUiSkin *skin, const char *raiz, const char *saida, int w, int h, const char *suf)
{
	TzMenu m;
	tz_menu_iniciar(&m, skin, w, h);
	listas_do_menu(&m, raiz);
	TzTela t;
	tz_tela_criar(&t, w, h);
	char nome[80];
	static const struct { int tela; const char *n; } pantallas[] = {
		{ TZ_MN_PRINCIPAL, "menu-principal" }, { TZ_MN_UM, "menu-un-jugador" }, { TZ_MN_CAMPANHAS, "menu-campanas" }, { TZ_MN_MAPAS, "menu-misiones" },
		{ TZ_MN_CONFIG, "menu-escaramuza" }, { TZ_MN_OPCOES, "menu-opciones" }, { TZ_MN_CARREGAR, "menu-cargar" }, { TZ_MN_GUARDAR, "menu-guardar" },
		{ TZ_MN_SAIR, "menu-salir" }, { TZ_MN_PARTIDA, "menu-partida" }, { TZ_MN_ALEAT, "menu-aleatorio" },
	};
	m.campanha_sel = 2; m.mapa_sel = 0; m.guardado_sel = 0; m.cfg.jogadores = 3;
	m.aleat.c.jogadores = 5; m.aleat.c.equipe[1] = 1; m.aleat.c.equipe[2] = 1; m.aleat.c.equipe[3] = 2; m.aleat.c.raca[1] = 2; m.aleat.c.raca[2] = 3; m.aleat.tipo = 3; m.aleat.lado = 4;
	for (unsigned i = 0; i < sizeof pantallas / sizeof *pantallas; ++i) {
		m.tela = pantallas[i].tela;
		m.sobre = -1;
		tz_menu_desenhar(&m, &t);
		snprintf(nome, sizeof nome, "ui-%s%s", pantallas[i].n, suf);
		grabar(&t, saida, nome);
	}
	tz_tela_liberar(&t);
	tz_menu_liberar(&m);
}

static void real(const char *raiz, const char *saida)
{
	TzRecursos rec;
	if (tz_recursos_abrir(&rec, raiz)) { VER(0, "abrir la instalación %s", raiz); return; }
	TzUiSkin skin;
	VER(tz_ui_skin_carregar(&skin, &rec) == 0 && skin.ok, "skin real: fuentes FNT, textos de STRINGS e iconos");
	if (!skin.ok) return;
	VER(skin.f10.alt > 0 && skin.f12b.alt > 0 && skin.f14b.alt > 0 && skin.f22.alt > 0, "fuentes del juego: arial10 (%d), arial12b (%d), arial14b (%d), cgb_22b (%d)", skin.f10.alt, skin.f12b.alt, skin.f14b.alt, skin.f22.alt);
	VER(!strcmp(tz_ui_txt(&skin, "mainscr", "quit", ""), "SALIR| Abandonar el juego"), "STRINGS.INI en castellano: «%s»", tz_ui_txt(&skin, "mainscr", "quit", ""));
	VER(tz_fonte_medir(&skin.f10, "Oro") > 10, "medir texto con arial10");
	capturas_menus(&skin, raiz, saida, 800, 600, "-800x600");
	capturas_menus(&skin, raiz, saida, 1024, 768, "");
	capturas_menus(&skin, raiz, saida, 1920, 1080, "-1920x1080");

	/* partida: el mapa MP1 con sus objetos */
	char ruta[800];
	snprintf(ruta, sizeof ruta, "%s/Maps/MP1.wmp", raiz);
	TzHpfs hp;
	TzMapa mapa;
	if (tz_hpfs_abrir(&hp, ruta) || tz_mapa_ler(&hp, &mapa)) { VER(0, "abrir el mapa MP1"); return; }
	tz_hpfs_fechar(&hp);
	TzTerreno ter;
	TzBiblioteca bib;
	TzEscena esc;
	VER(tz_terreno_carregar(&ter, &rec, mapa.bioma) == 0 && tz_biblioteca_carregar(&bib, &rec, mapa.bioma) == 0 && tz_escena_montar(&esc, &bib, &rec, &ter, &mapa) == 0, "terreno, biblioteca y escena de MP1");
	TzCatalogo cat;
	stub_catalogo(&cat, &rec, &bib);
	TzMundo m;
	int jog = 1;
	for (int k = 0; k < mapa.nreinos; ++k) if (mapa.reinos[k].ativo) { jog = k + 1; break; }
	stub_mundo_de_mapa(&m, &cat, &mapa, jog);
	uint32_t *mini = malloc((size_t)mapa.lado * mapa.lado * 4);
	if (mini && tz_terreno_minimapa(&ter, &mapa, mini)) { free(mini); mini = NULL; }
	TzId castillo = TZ_NENHUM, peon = TZ_NENHUM, soldados[16];
	int ns = 0;
	for (int i = 0; i < m.nobj; ++i) {
		const TzObj *o = &m.obj[i];
		if (o->dono != jog) continue;
		if (o->classe == TZC_CASTLE && castillo == TZ_NENHUM) castillo = i;
		if (o->classe == TZC_PEON && peon == TZ_NENHUM) peon = i;
		if (tz_obj_unidade(o) && o->classe != TZC_PEON && ns < 16) soldados[ns++] = i;
	}
	printf("MP1: jugador %d, castillo %d, peón %d, %d soldados, %d objetos\n", jog, castillo, peon, ns, m.nobj);
	TzId centro = castillo != TZ_NENHUM ? castillo : (peon != TZ_NENHUM ? peon : 0);
	static const int tam[][2] = { { 800, 600 }, { 1024, 768 }, { 1920, 1080 } };
	static const char *const suf[] = { "-800x600", "", "-1920x1080" };
	for (int k = 0; k < 3; ++k) {
		TzUi u;
		tz_ui_iniciar(&u, &skin, &m, &bib, tam[k][0], tam[k][1]);
		u.minimapa = mini;
		TzUiMapa mp = { &ter, &mapa, &esc };
		tz_ui_centrar(&u, m.obj[centro].cx, m.obj[centro].cy);
		TzTela t;
		tz_tela_criar(&t, tam[k][0], tam[k][1]);
		char nome[80];
		TzUiMapa *pm = &mp;
		/* 1) un peón elegido, con el menú de órdenes */
		u.sel.n = 0;
		if (peon != TZ_NENHUM) { u.sel.id[0] = peon; u.sel.n = 1; }
		tz_ui_menu_atualizar(&u);
		tz_ui_mapa_desenhar(&u, &t, pm); tz_ui_desenhar_marcas(&u, &t); tz_ui_desenhar_hud(&u, &t);
		snprintf(nome, sizeof nome, "ui-hud-peon%s", suf[k]); grabar(&t, saida, nome);
		VER(u.nlista > 0, "HUD real: el peón tiene %d órdenes en su menú (%s)", u.nlista, u.botoes[0].rotulo);
		/* 2) submenú de construcción con costes y el cursor encima de un botón */
		for (int i = 0; i < u.nbotoes; ++i) if (u.botoes[i].submenu[0]) { tz_ui_botao_premir(&u, i, 0); break; }
		u.botao_sobre = 0; u.mx = 16 + 25; u.my = 332 + 25;
		tz_ui_mapa_desenhar(&u, &t, pm); tz_ui_desenhar_marcas(&u, &t); tz_ui_desenhar_hud(&u, &t);
		snprintf(nome, sizeof nome, "ui-hud-construir%s", suf[k]); grabar(&t, saida, nome);
		VER(u.npilha == 1 && u.nbotoes > 0, "HUD real: el submenú de construcción tiene %d botones (primero: «%s» %s)", u.nbotoes, u.botoes[0].rotulo, u.botoes[0].custo);
		/* 3) colocar el primer edificio junto al castillo */
		if (u.nbotoes > 0 && u.botoes[0].classe >= 0) {
			tz_ui_botao_premir(&u, 0, 0);
			u.mx = u.mapa_rect.x + u.mapa_rect.w / 2; u.my = u.mapa_rect.y + u.mapa_rect.h / 2;
			TzUiEntrada e = { TZ_IN_MOVER, u.mx, u.my, 0, 0, 0, 0 };
			tz_ui_entrada(&u, &e, 1000);
			tz_ui_mapa_desenhar(&u, &t, pm); tz_ui_desenhar_marcas(&u, &t); tz_ui_desenhar_hud(&u, &t);
			snprintf(nome, sizeof nome, "ui-hud-colocar%s", suf[k]); grabar(&t, saida, nome);
			VER(u.modo == TZ_UI_COLOCAR, "HUD real: modo de colocación de «%s» (%s)", tz_classes[u.classe_colocar], u.colocar_ok ? "válido" : "no válido");
			u.modo = TZ_UI_NORMAL;
		}
		/* 4) castillo elegido: producción */
		u.npilha = 0;
		u.sel.n = 0;
		if (castillo != TZ_NENHUM) { u.sel.id[0] = castillo; u.sel.n = 1; }
		tz_ui_menu_atualizar(&u);
		tz_ui_mapa_desenhar(&u, &t, pm); tz_ui_desenhar_marcas(&u, &t); tz_ui_desenhar_hud(&u, &t);
		snprintf(nome, sizeof nome, "ui-hud-castillo%s", suf[k]); grabar(&t, saida, nome);
		/* 5) varias unidades y recuadro */
		u.sel.n = 0;
		for (int i = 0; i < ns && i < 8; ++i) u.sel.id[u.sel.n++] = soldados[i];
		if (peon != TZ_NENHUM && ns == 0) u.sel.id[u.sel.n++] = peon;
		tz_ui_menu_atualizar(&u);
		u.arrasta = 1; u.ax = u.mapa_rect.x + 80; u.ay = u.mapa_rect.y + 80; u.mx = u.ax + 160; u.my = u.ay + 90;
		tz_ui_mapa_desenhar(&u, &t, pm); tz_ui_desenhar_marcas(&u, &t); tz_ui_desenhar_hud(&u, &t);
		snprintf(nome, sizeof nome, "ui-hud-grupo%s", suf[k]); grabar(&t, saida, nome);
		tz_tela_liberar(&t);
		tz_ui_liberar(&u);
		u.minimapa = NULL;
	}
	/* con el ratón sobre unidades reales: el clic elige y el derecho manda */
	{
		TzUi u;
		tz_ui_iniciar(&u, &skin, &m, &bib, 1024, 768);
		tz_ui_centrar(&u, m.obj[centro].cx, m.obj[centro].cy);
		int hit = 0;
		for (int i = 0; i < ns && !hit; ++i) {
			int x, y;
			tz_obj_pixel(&m.obj[soldados[i]], &x, &y);
			if (tz_rect_dentro((TzRect){ u.camx, u.camy, u.mapa_rect.w, u.mapa_rect.h }, x, y)) {
				TzUiEntrada e = { TZ_IN_BAIXO, u.mapa_rect.x + x - u.camx, u.mapa_rect.y + y - u.camy - 10, 1, 0, 0, 0 };
				tz_ui_entrada(&u, &e, 100); e.tipo = TZ_IN_CIMA; tz_ui_entrada(&u, &e, 150);
				hit = u.sel.n >= 1;
			}
		}
		VER(ns == 0 || hit, "unidad real: el clic sobre una unidad de MP1 la elige");
		tz_ui_liberar(&u);
	}
	free(mini);
	stub_liberar(&m);
	stub_catalogo_liberar(&cat);
	tz_escena_liberar(&esc);
	tz_biblioteca_liberar(&bib);
	tz_terreno_liberar(&ter);
	tz_mapa_liberar(&mapa);
	tz_ui_skin_liberar(&skin);
	tz_recursos_fechar(&rec);
}

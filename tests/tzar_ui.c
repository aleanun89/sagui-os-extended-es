/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_ui.c - arnés en el PC de la interfaz de Tzar (D417, fase 4; tests/h417_tzar.py, parte «ui»).
 *
 *   tzar_ui unidad <carpeta-capturas>   pruebas de unidad con datos SINTÉTICOS (mundo de prueba de tzar_ui_stub.c, skin
 *                                       vacía con la letra del sistema): fuentes FNT, selección, órdenes, colocación,
 *                                       menú de órdenes con costes, minimapa, avisos, menús principales y dibujo del HUD
 *                                       de 800x600 a 1920x1080 (con ASan/UBSan). Las capturas sintéticas van a la carpeta.
 *   tzar_ui real <instalación> <salida> (a mano, NUNCA en la batería) con los datos reales del mantenedor: arte, fuentes
 *                                       y textos del juego; deja capturas de los menús y el HUD en <salida>/ui-*.bmp.
 * Escribe «OK ...» o «FALLO ...» por línea y sale con 1 si algo falló.
 */
#include "tzar_ui_stub.h"
#include <stdlib.h>
#include <string.h>

static int fallos;
#define VER(c, ...) do { if (c) { printf("OK "); printf(__VA_ARGS__); printf("\n"); } else { fallos++; printf("FALLO "); printf(__VA_ARGS__); printf(" (%s)\n", tz_erro()); } } while (0)

extern const unsigned char fonte_sagui[256][16];

/* ---------------------------------------------------------------- ayudas de entrada */
static uint32_t reloj;
static void ev(TzUi *u, int tipo, int x, int y, int botao, int tecla, int mods)
{
	TzUiEntrada e = { tipo, x, y, botao, tecla, mods, 0 };
	reloj += 50;
	tz_ui_entrada(u, &e, reloj);
}
static void mover(TzUi *u, int x, int y) { ev(u, TZ_IN_MOVER, x, y, 0, 0, 0); }
static void clic(TzUi *u, int x, int y, int b, int mods) { mover(u, x, y); ev(u, TZ_IN_BAIXO, x, y, b, 0, mods); ev(u, TZ_IN_CIMA, x, y, b, 0, mods); }
static void tecla(TzUi *u, int k, int mods) { ev(u, TZ_IN_TECLA, 0, 0, 0, k, mods); }
/* píxel de la ventana del cuerpo de una unidad de la casilla (cx, cy) */
static void tela_de(const TzUi *u, int cx, int cy, int *sx, int *sy)
{
	*sx = u->mapa_rect.x + cx * 32 + 16 - u->camx;
	*sy = u->mapa_rect.y + cy * 32 + 16 - u->camy - 10;
}
static void clic_unidade(TzUi *u, int cx, int cy, int b, int mods) { int x, y; tela_de(u, cx, cy, &x, &y); clic(u, x, y, b, mods); }
static void arrastar(TzUi *u, int x0, int y0, int x1, int y1, int mods)
{
	mover(u, x0, y0); ev(u, TZ_IN_BAIXO, x0, y0, 1, 0, mods); mover(u, x1, y1); ev(u, TZ_IN_CIMA, x1, y1, 1, 0, mods);
}
static void limpar_log(void) { stub_nlog = 0; }

/* ---------------------------------------------------------------- FNT sintético */
static uint8_t *fnt_sintetica(uint32_t *n)
{
	/* altura 4, ancho de glifo 3, 2 glifos: el 0 vacío y el 1 con una barra; todos los códigos 'A'.. usan el glifo 1 */
	const int alt = 4, anc = 3, filas = 2 * alt, stride = 4;
	uint32_t tam = 562 + 54 + (uint32_t)(stride * filas);
	uint8_t *d = calloc(1, tam);
	d[32] = alt;
	for (int i = 0; i < 256; ++i) { d[50 + i] = 4; d[306 + i] = (i >= 'A' && i <= 'Z') ? 1 : 0; }
	uint8_t *b = d + 562;
	b[0] = 'B'; b[1] = 'M';
	uint32_t off = 54, hdr = 40; int32_t w = anc, h = filas;
	memcpy(b + 10, &off, 4); memcpy(b + 14, &hdr, 4); memcpy(b + 18, &w, 4); memcpy(b + 22, &h, 4);
	b[28] = 8;
	/* glifo 1 (filas 4..7 de arriba abajo): columna central a cobertura 7; el BMP va de abajo arriba */
	for (int y = alt; y < filas; ++y) b[54 + (size_t)(filas - 1 - y) * stride + 1] = 7;
	*n = tam;
	return d;
}

static void prueba_fuentes(void)
{
	uint32_t n;
	uint8_t *d = fnt_sintetica(&n);
	TzUiFonte f;
	VER(tz_ui_fonte_ler(&f, d, n) == 0 && f.alt == 4 && f.larg == 3 && f.nglifos == 2 && f.niveis == 7, "FNT: cabecera (altura 4, ancho 3, 2 glifos, 7 niveles)");
	VER(tz_ui_fonte_medir(&f, "ABC") == 12 && tz_ui_fonte_medir(&f, "") == 0, "FNT: medir texto por avances");
	TzTela t;
	tz_tela_criar(&t, 20, 10);
	tz_ui_fonte_texto(&t, &f, 1, 1, "AB", 0xffffff);
	VER(t.px[1 * 20 + 2] == 0xffffffff && t.px[4 * 20 + 2] == 0xffffffff && t.px[1 * 20 + 1] == 0 && t.px[1 * 20 + 6] == 0xffffffff, "FNT: la barra del glifo se dibuja a plena cobertura");
	tz_ui_fonte_texto(&t, &f, 100, 100, "AB", 0xffffff);   /* fuera del lienzo: no escribe */
	char l[8][160];
	int nl = tz_ui_fonte_quebrar(&f, "AAA BBB CCC DDD", 28, l, 8);
	VER(nl == 2 && !strcmp(l[0], "AAA BBB") && !strcmp(l[1], "CCC DDD"), "texto: se parte en líneas del ancho dado (%d líneas)", nl);
	tz_tela_liberar(&t);
	tz_ui_fonte_liberar(&f);
	/* dañados */
	uint8_t *e = fnt_sintetica(&n);
	e[562 + 28] = 24;
	VER(tz_ui_fonte_ler(&f, e, n) != 0, "FNT: un BMP de 24 bits se rechaza");
	e[562 + 28] = 8; e[562 + 22] = 0xff; e[562 + 23] = 0xff;
	VER(tz_ui_fonte_ler(&f, e, n) != 0, "FNT: dimensiones absurdas se rechazan");
	VER(tz_ui_fonte_ler(&f, d, 100) != 0, "FNT: truncado se rechaza");
	free(e); free(d);
}

/* ---------------------------------------------------------------- mundo de prueba */
typedef struct { TzCatalogo cat; TzMundo m; TzUiSkin skin; TzUi u; TzId peon, pelea[4], cuartel, castillo, enemigo, oro; } Cena;

static void cena_iniciar(Cena *c, int w, int h)
{
	memset(c, 0, sizeof *c);
	stub_catalogo(&c->cat, NULL, NULL);
	stub_mundo(&c->m, &c->cat, 64);
	c->peon = stub_obj(&c->m, TZC_PEON, 1, 1, 20, 20);
	for (int i = 0; i < 4; ++i) c->pelea[i] = stub_obj(&c->m, 3, 1, 1, 22 + i, 20);
	c->cuartel = stub_obj(&c->m, TZC_BARRACK, 1, 1, 30, 30);
	c->castillo = stub_obj(&c->m, TZC_CASTLE, 1, 1, 14, 28);
	c->enemigo = stub_obj(&c->m, 3, 1, 2, 28, 20);
	c->oro = stub_obj(&c->m, TZC_RES_GOLD, 0, 0, 18, 24);
	stub_nevoa(&c->m);
	tz_ui_skin_vazia(&c->skin);
	tz_ui_iniciar(&c->u, &c->skin, &c->m, NULL, w, h);
	tz_ui_centrar(&c->u, 24, 24);
	limpar_log();
	reloj = 1000;
}
static void cena_liberar(Cena *c)
{
	tz_ui_liberar(&c->u);
	tz_ui_skin_liberar(&c->skin);
	stub_liberar(&c->m);
	stub_catalogo_liberar(&c->cat);
}

static int checksum(const TzTela *t) { uint32_t h = 2166136261u; for (int i = 0; i < t->w * t->h; ++i) h = (h ^ t->px[i]) * 16777619u; return (int)h; }

static void prueba_seleccion(void)
{
	Cena c;
	cena_iniciar(&c, 1024, 768);
	TzUi *u = &c.u;
	clic_unidade(u, 20, 20, 1, 0);
	VER(u->sel.n == 1 && u->sel.id[0] == c.peon, "selección: el clic coge la unidad bajo el cursor");
	uint32_t cur = 0; TzEvento e[8];
	int ne = tz_mundo_eventos(&c.m, &cur, e, 8);
	VER(ne == 1 && e[0].tipo == TZ_EV_SELECIONADO && e[0].obj == c.peon, "selección: emite TZ_EV_SELECIONADO para el sonido");
	clic_unidade(u, 22, 20, 1, TZ_MOD_SHIFT);
	VER(u->sel.n == 2, "selección: Mayús añade");
	clic_unidade(u, 22, 20, 1, TZ_MOD_SHIFT);
	VER(u->sel.n == 1 && u->sel.id[0] == c.peon, "selección: Mayús sobre una ya elegida la quita");
	clic(u, 160, 40, 1, 0);
	VER(u->sel.n == 0, "selección: clic en el vacío deselecciona");
	/* doble clic: las del mismo tipo que se ven */
	clic_unidade(u, 22, 20, 1, 0);
	reloj -= 40;   /* segundo clic dentro de 400 ms */
	clic_unidade(u, 22, 20, 1, 0);
	VER(u->sel.n == 4, "selección: doble clic coge las 4 de la misma clase en pantalla (%d)", u->sel.n);
	/* recuadro: unidades antes que edificios */
	int x0, y0, x1, y1;
	tela_de(u, 19, 18, &x0, &y0); tela_de(u, 26, 22, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	VER(u->sel.n == 5, "selección: el recuadro coge las 5 unidades propias (peón y 4 soldados) (%d)", u->sel.n);
	tela_de(u, 29, 29, &x0, &y0); tela_de(u, 34, 34, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	VER(u->sel.n == 1 && u->sel.id[0] == c.cuartel, "selección: sin unidades, el recuadro coge el edificio");
	tela_de(u, 27, 19, &x0, &y0); tela_de(u, 29, 21, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	VER(u->sel.n == 1 && u->sel.id[0] == c.enemigo, "selección: un enemigo se puede elegir solo (para verlo)");
	/* grupos */
	tela_de(u, 19, 18, &x0, &y0); tela_de(u, 26, 22, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	tecla(u, '1', TZ_MOD_CTRL);
	clic(u, 160, 40, 1, 0);
	VER(u->sel.n == 0, "grupos: tras limpiar no queda nada");
	tecla(u, '1', 0);
	VER(u->sel.n == 5, "grupos: la tecla 1 recupera el grupo (%d)", u->sel.n);
	int cx0 = u->camx;
	tz_ui_mover_camara(u, 400, 300);
	tecla(u, '1', 0);
	VER(u->camx != cx0, "grupos: pulsar dos veces el número centra la cámara (%d -> %d)", cx0, u->camx);
	c.m.obj[c.pelea[0]].flags |= TZ_F_MORTO;
	tz_ui_tic(u, reloj);
	VER(u->sel.n == 4, "selección: los muertos salen de la selección");
	cena_liberar(&c);
}

static void prueba_ordenes(void)
{
	Cena c;
	cena_iniciar(&c, 1024, 768);
	TzUi *u = &c.u;
	int x0, y0, x1, y1;
	tela_de(u, 19, 18, &x0, &y0); tela_de(u, 26, 22, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	limpar_log();
	clic_unidade(u, 28, 20, 3, 0);
	VER(stub_nlog == 1 && stub_log[0].tipo == TZ_C_ORDEM && stub_log[0].ordem.tipo == TZ_O_ATACAR && stub_log[0].ordem.alvo == c.enemigo && stub_log[0].nids == 5 && stub_log[0].reino == 1,
	    "órdenes: clic derecho sobre un enemigo manda ATACAR a las 5 unidades (cmds %d)", stub_nlog);
	limpar_log();
	int sx = u->mapa_rect.x + 25 * 32 - u->camx + 5, sy = u->mapa_rect.y + 25 * 32 - u->camy + 5;
	clic(u, sx, sy, 3, TZ_MOD_SHIFT);
	VER(stub_nlog == 1 && stub_log[0].ordem.tipo == TZ_O_MOVER && stub_log[0].ordem.x == 25 && stub_log[0].ordem.y == 25 && (stub_log[0].flags & TZ_CMD_FILA),
	    "órdenes: con Mayús mover se encola (TZ_CMD_FILA) a la casilla (25, 25)");
	limpar_log();
	/* edificios: solo punto de reunión */
	clic_unidade(u, 31, 31, 1, 0);
	tz_ui_tic(u, reloj);
	limpar_log();
	clic(u, sx, sy, 3, 0);
	VER(stub_nlog == 1 && stub_log[0].tipo == TZ_C_REUNIAO && stub_log[0].x == 25 && stub_log[0].nids == 1 && stub_log[0].ids[0] == c.cuartel, "órdenes: con un edificio elegido, el clic derecho fija la reunión");
	/* un enemigo elegido no obedece */
	limpar_log();
	arrastar(u, 0, 0, 0, 0, 0);
	clic_unidade(u, 28, 20, 1, 0);
	clic(u, sx, sy, 3, 0);
	VER(stub_nlog == 0, "órdenes: una unidad ajena seleccionada no recibe órdenes");
	/* más de 64 unidades: varios comandos */
	cena_liberar(&c);
	cena_iniciar(&c, 1024, 768);
	u = &c.u;
	for (int i = 0; i < 70; ++i) stub_obj(&c.m, 3, 1, 1, 2 + i % 30, 40 + i / 30);
	stub_nevoa(&c.m);
	tz_ui_centrar(u, 12, 42);
	tela_de(u, 1, 39, &x0, &y0); tela_de(u, 33, 43, &x1, &y1);
	arrastar(u, x0, y0, x1, y1, 0);
	VER(u->sel.n == 70, "selección: 70 unidades entran en la selección (%d)", u->sel.n);
	limpar_log();
	clic(u, u->mapa_rect.x + 100, u->mapa_rect.y + 100, 3, 0);
	VER(stub_nlog == 2 && stub_log[0].nids == 64 && stub_log[1].nids == 6, "órdenes: 70 unidades salen en dos comandos de 64 + 6 (%d cmds)", stub_nlog);
	cena_liberar(&c);
}

static void prueba_menu_y_colocacion(void)
{
	Cena c;
	cena_iniciar(&c, 1024, 768);
	TzUi *u = &c.u;
	clic_unidade(u, 20, 20, 1, 0);
	tz_ui_menu_atualizar(u);
	VER(u->nbotoes == 5 && !strcmp(u->botoes[0].def->chave, "move") && u->botoes[0].hotkey == 'M', "menú: el peón muestra su menú de MENU.INI con atajos (%d botones)", u->nbotoes);
	int i_b = 3;
	VER(u->botoes[i_b].submenu[0] && !strcmp(u->botoes[i_b].submenu, "e_build"), "menú: «construir» abre un submenú");
	/* pulsar en la rejilla: el botón 3 está en la fila 1, columna 1 */
	int bx = 16 + 3 + 62 + 20, by = 332 + 3 + 57 + 20;
	clic(u, bx, by, 1, 0);
	VER(u->npilha == 1 && u->nbotoes == 2 && !strcmp(u->botoes[0].def->chave, "e_barrack"), "menú: el clic abre el submenú de construcción (%d botones)", u->nbotoes);
	VER(strstr(u->botoes[0].custo, "madera: 100") && strstr(u->botoes[0].custo, "piedra: 20"), "menú: el botón de construir muestra el coste («%s»)", u->botoes[0].custo);
	mover(u, 16 + 3 + 20, 332 + 3 + 20);
	char dica[200];
	tz_ui_dica(u, dica, sizeof dica);
	VER(strstr(dica, "barrack") && strstr(dica, "(C)") && strstr(dica, "madera: 100"), "ayuda: la línea muestra nombre, atajo y coste («%s»)", dica);
	clic(u, 16 + 3 + 20, 332 + 3 + 20, 1, 0);
	VER(u->modo == TZ_UI_COLOCAR && u->classe_colocar == TZC_BARRACK, "colocación: pulsar «cuartel» pasa al modo de colocar");
	/* sobre un sitio libre y explorado, luego sobre el cuartel existente */
	int sx = u->mapa_rect.x + 24 * 32 - u->camx + 16, sy = u->mapa_rect.y + 22 * 32 - u->camy + 16;
	mover(u, sx, sy);
	VER(u->colocar_ok == 1, "colocación: un hueco libre es válido (cabe == 0)");
	mover(u, u->mapa_rect.x + 30 * 32 - u->camx + 48, u->mapa_rect.y + 30 * 32 - u->camy + 48);
	VER(u->colocar_ok == 0, "colocación: encima de otro edificio no es válido");
	limpar_log();
	ev(u, TZ_IN_BAIXO, u->mx, u->my, 1, 0, 0); ev(u, TZ_IN_CIMA, u->mx, u->my, 1, 0, 0);
	VER(stub_nlog == 0 && u->modo == TZ_UI_COLOCAR && u->aviso[0], "colocación: un clic en sitio inválido no manda nada y avisa («%s»)", u->aviso);
	mover(u, sx, sy);
	ev(u, TZ_IN_BAIXO, sx, sy, 1, 0, 0); ev(u, TZ_IN_CIMA, sx, sy, 1, 0, 0);
	VER(stub_nlog == 1 && stub_log[0].tipo == TZ_C_CONSTRUIR && stub_log[0].arg == TZC_BARRACK && stub_log[0].nids == 1 && stub_log[0].ids[0] == c.peon && u->modo == TZ_UI_NORMAL,
	    "colocación: el clic manda TZ_C_CONSTRUIR de la clase en la casilla (%d, %d)", stub_log[0].x, stub_log[0].y);
	VER(stub_log[0].x == 23 && stub_log[0].y == 21, "colocación: la huella 3x3 queda centrada en el cursor");
	/* Esc cancela */
	tz_ui_menu_atualizar(u);
	u->modo = TZ_UI_COLOCAR; u->classe_colocar = TZC_HOUSE;
	tecla(u, TZ_TC_ESC, 0);
	VER(u->modo == TZ_UI_NORMAL, "colocación: Esc cancela");
	/* orden con objetivo: «atacar» */
	u->npilha = 0; tz_ui_menu_atualizar(u);
	clic(u, 16 + 3 + 62 + 20, 332 + 3 + 20, 1, 0);
	VER(u->modo == TZ_UI_ALVO && u->tipo_alvo == TZ_O_ATACAR, "órdenes: «atacar» espera objetivo");
	limpar_log();
	clic_unidade(u, 28, 20, 1, 0);
	VER(stub_nlog == 1 && stub_log[0].ordem.tipo == TZ_O_ATACAR && stub_log[0].ordem.alvo == c.enemigo && u->modo == TZ_UI_NORMAL, "órdenes: el clic izquierdo da la orden pendiente");
	/* atajo de teclado: S = parar */
	limpar_log();
	tecla(u, 's', 0);
	VER(stub_nlog == 1 && stub_log[0].ordem.tipo == TZ_O_PARAR, "menú: la letra de MENU.INI actúa como botón (S = parar)");
	/* cuartel: entrenar con coste y Mayús = 5 */
	clic_unidade(u, 31, 31, 1, 0);
	tz_ui_menu_atualizar(u);
	VER(u->nbotoes == 2 && u->botoes[0].classe == 4 && strstr(u->botoes[0].custo, "oro: 40"), "menú: el cuartel ofrece arquero con su coste («%s»)", u->botoes[0].custo);
	limpar_log();
	clic(u, 16 + 3 + 20, 332 + 3 + 20, 1, TZ_MOD_SHIFT);
	VER(stub_nlog == 5 && stub_log[0].tipo == TZ_C_PRODUZIR && stub_log[0].arg == 4 && stub_log[0].ids[0] == c.cuartel, "menú: Mayús entrena cinco (%d comandos PRODUZIR)", stub_nlog);
	/* cola en marcha: variante «_b» */
	c.m.obj[c.cuartel].ator->ntrab = 1;
	tz_ui_menu_atualizar(u);
	VER(u->nbotoes == 1, "menú: con cola en marcha se usa la variante «_b» (%d botón)", u->nbotoes);
	c.m.obj[c.cuartel].ator->ntrab = 0;
	/* castillo: tecnología no disponible (requisito) */
	clic_unidade(u, 15, 29, 1, 0);
	tz_ui_menu_atualizar(u);
	VER(u->nbotoes == 1 && u->botoes[0].tec == 0 && !u->botoes[0].ok && strstr(u->botoes[0].motivo, "castillo"), "menú: una tecnología sin requisito sale apagada con el motivo («%s»)", u->botoes[0].motivo);
	limpar_log();
	clic(u, 16 + 3 + 20, 332 + 3 + 20, 1, 0);
	VER(stub_nlog == 0 && strstr(u->aviso, "castillo"), "menú: pulsar un botón apagado muestra el motivo y no manda nada");
	cena_liberar(&c);
}

static void prueba_minimapa_y_avisos(void)
{
	Cena c;
	cena_iniciar(&c, 1024, 768);
	TzUi *u = &c.u;
	int antes = u->camx;
	clic(u, u->minimapa_rect.x + 100, u->minimapa_rect.y + 100, 1, 0);
	VER(u->camx != antes && u->camx >= 0, "minimapa: el clic centra la cámara (%d -> %d)", antes, u->camx);
	tz_ui_centrar(u, 20, 20);
	clic_unidade(u, 20, 20, 1, 0);
	/* tras elegir, el minimapa del casillero (10,10) (en el 128x128, la mitad de lado 64 = 2 por píxel) */
	limpar_log();
	clic(u, u->minimapa_rect.x + 10 * 128 / 64 + 1, u->minimapa_rect.y + 12 * 128 / 64 + 1, 3, 0);
	VER(stub_nlog == 1 && stub_log[0].ordem.tipo == TZ_O_MOVER && stub_log[0].ordem.x == 10 && stub_log[0].ordem.y == 12, "minimapa: el clic derecho manda mover a esa casilla (%d, %d)", stub_log[0].ordem.x, stub_log[0].ordem.y);
	/* arrastrando por el minimapa */
	ev(u, TZ_IN_BAIXO, u->minimapa_rect.x + 5, u->minimapa_rect.y + 5, 1, 0, 0);
	mover(u, u->minimapa_rect.x + 120, u->minimapa_rect.y + 120);
	int fin = u->camx;
	ev(u, TZ_IN_CIMA, u->minimapa_rect.x + 120, u->minimapa_rect.y + 120, 1, 0, 0);
	VER(fin == u->m->lado * 32 - u->mapa_rect.w, "minimapa: arrastrar lleva la cámara hasta el límite (%d)", fin);
	/* avisos desde los eventos */
	stub_evento(&c.m, TZ_EV_SEM_RECURSOS, 1, TZ_OURO);
	tz_ui_tic(u, reloj);
	VER(!strcmp(u->aviso, "No hay suficiente oro"), "avisos: TZ_EV_SEM_RECURSOS -> «%s»", u->aviso);
	stub_evento(&c.m, TZ_EV_SEM_POPULACAO, 1, 0);
	tz_ui_tic(u, reloj);
	VER(strstr(u->aviso, "casas") != NULL, "avisos: TZ_EV_SEM_POPULACAO -> «%s»", u->aviso);
	stub_evento(&c.m, TZ_EV_SEM_RECURSOS, 2, TZ_PEDRA);
	tz_ui_tic(u, reloj);
	VER(strstr(u->aviso, "casas") != NULL, "avisos: lo de otro reino no sale en mi pantalla");
	/* cámara con teclas y límites */
	tz_ui_centrar(u, 0, 0);
	VER(u->camx == 0 && u->camy == 0, "cámara: se queda en el límite del mapa");
	tecla(u, TZ_TC_DIR, 0);
	VER(u->camx == 96, "cámara: las flechas la mueven");
	cena_liberar(&c);
}

static void prueba_dibujo(void)
{
	static const int tam[][2] = { { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1366, 768 }, { 1920, 1080 } };
	for (unsigned k = 0; k < sizeof tam / sizeof *tam; ++k) {
		Cena c;
		cena_iniciar(&c, tam[k][0], tam[k][1]);
		TzTela t;
		tz_tela_criar(&t, tam[k][0], tam[k][1]);
		clic_unidade(&c.u, 20, 20, 1, 0);
		tz_ui_desenhar_marcas(&c.u, &t);
		tz_ui_desenhar_hud(&c.u, &t);
		int h1 = checksum(&t);
		tz_ui_desenhar_hud(&c.u, &t);
		VER(c.u.mapa_rect.w == tam[k][0] - 150 && c.u.mapa_rect.h == tam[k][1] - 30 && h1 == checksum(&t), "HUD %dx%d: se dibuja sin salirse y de forma estable (mapa %dx%d)", tam[k][0], tam[k][1], c.u.mapa_rect.w, c.u.mapa_rect.h);
		/* contadores: «500» de oro en la barra */
		int tinta = 0;
		for (int y = 0; y < 22; ++y) for (int x = 360; x < 408; ++x) tinta += (t.px[y * t.w + x] & 0xffffff) != 0x383830 && (t.px[y * t.w + x] & 0xffffff) != 0;
		VER(tinta > 10, "HUD %dx%d: el contador de oro tiene texto (%d píxeles)", tam[k][0], tam[k][1], tinta);
		/* el recuadro de selección y el fantasma del edificio */
		c.u.modo = TZ_UI_COLOCAR; c.u.classe_colocar = TZC_BARRACK;
		mover(&c.u, c.u.mapa_rect.x + 200, c.u.mapa_rect.y + 200);
		tz_ui_desenhar_marcas(&c.u, &t);
		if (k == 1) tz_tela_gravar_bmp(&t, "/tmp/tzar_ui_sint_1024.bmp");
		tz_tela_liberar(&t);
		cena_liberar(&c);
	}
}

/* ---------------------------------------------------------------- menús */
static void mclic(TzMenu *m, int x, int y) { TzUiEntrada e = { TZ_IN_MOVER, x, y, 0, 0, 0, 0 }; tz_menu_entrada(m, &e); e.tipo = TZ_IN_BAIXO; e.botao = 1; tz_menu_entrada(m, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(m, &e); }
static void mtecla(TzMenu *m, int k, int mods) { TzUiEntrada e = { TZ_IN_TECLA, 0, 0, 0, k, mods, 0 }; tz_menu_entrada(m, &e); }
static int botao_texto(TzMenu *m, int i, int *x, int *y, char *rot)
{
	TzRect r[16]; char t[16][64];
	int n = tz_menu_botoes(m, r, t, 16);
	if (i >= n) return -1;
	int ox = (m->w - 800) / 2, oy = (m->h - 600) / 2;
	if (m->tela == TZ_MN_PARTIDA) { ox = (m->w - 291) / 2; oy = (m->h - 413) / 2; }
	if (m->tela == TZ_MN_SAIR) { ox = (m->w - 500) / 2; oy = (m->h - 320) / 2; }
	if (m->tela == TZ_MN_CARREGAR || m->tela == TZ_MN_GUARDAR) { ox += 125; oy += 140; }
	*x = ox + r[i].x + r[i].w / 2; *y = oy + r[i].y + r[i].h / 2;
	if (rot) strcpy(rot, t[i]);
	return n;
}
static void mpremir(TzMenu *m, int i) { int x, y; if (botao_texto(m, i, &x, &y, NULL) >= 0) mclic(m, x, y); }

/* la pantalla del mapa aleatorio (S_SINGLERND, v8.5.5.2): opciones, límites por tamaño, otro mapa, jugar, recordar y ampliar */
static void prueba_menu_aleatorio(void)
{
	TzUiSkin skin;
	tz_ui_skin_vazia(&skin);
	TzMenu m;
	tz_menu_iniciar(&m, &skin, 1024, 768);
	int ox = (1024 - 800) / 2, oy = (768 - 600) / 2;
	mpremir(&m, 0);   /* un solo jugador */
	mpremir(&m, 2);   /* mapa aleatorio */
	VER(m.tela == TZ_MN_ALEAT, "aleatorio (pantalla): «mapa aleatorio» abre su pantalla");
	int lado0 = m.aleat.lado;
	mclic(&m, ox + 565 + 180 - 13, oy + 56 + 13);
	VER(m.aleat.lado == lado0 + 1 && m.aleat_mudou, "aleatorio (pantalla): la flecha derecha del tamaño lo sube (%d)", m.aleat.lado);
	for (int i = 0; i < 8; ++i) mclic(&m, ox + 565 + 13, oy + 56 + 13);
	VER(m.aleat.lado == (lado0 + 1 - 8 + 7 * 2) % 7, "aleatorio (pantalla): el tamaño da la vuelta (%d)", m.aleat.lado);
	m.aleat.lado = 0;
	m.aleat.c.jogadores = 2;
	for (int i = 0; i < 10; ++i) mclic(&m, ox + 66 + 180 - 13, oy + 369 + 13);
	VER(m.aleat.c.jogadores == 4, "aleatorio (pantalla): con 64 casillas los reinos no pasan de 4 (%d)", m.aleat.c.jogadores);
	m.aleat.lado = 2;
	for (int i = 0; i < 10; ++i) mclic(&m, ox + 66 + 180 - 13, oy + 369 + 13);
	VER(m.aleat.c.jogadores == 8, "aleatorio (pantalla): con 128 casillas llegan a 8 (%d)", m.aleat.c.jogadores);
	m.aleat.lado = 0;
	mclic(&m, ox + 565 + 180 - 13, oy + 56 + 13); mclic(&m, ox + 565 + 13, oy + 56 + 13);   /* sube y baja: normaliza */
	VER(m.aleat.lado == 0 && m.aleat.c.jogadores == 4, "aleatorio (pantalla): al achicar el mapa los reinos se recortan al límite (%d)", m.aleat.c.jogadores);
	/* geografías: en 64 solo valen «al azar», «continente» y «tierra» */
	m.aleat.tipo = 0;
	mclic(&m, ox + 565 + 180 - 13, oy + 111 + 13);
	VER(m.aleat.tipo == 3, "aleatorio (pantalla): con 64 casillas el tipo salta archipiélagos e islas (%d)", m.aleat.tipo);
	mclic(&m, ox + 565 + 180 - 13, oy + 111 + 13);
	VER(m.aleat.tipo == 5, "aleatorio (pantalla): ...y los lagos (%d)", m.aleat.tipo);
	m.aleat.lado = 4;
	m.aleat.tipo = 1;
	mclic(&m, ox + 565 + 180 - 13, oy + 56 + 13); mclic(&m, ox + 565 + 13, oy + 56 + 13);
	VER(m.aleat.tipo == 1, "aleatorio (pantalla): con 256 casillas valen todos los tipos");
	m.aleat.lado = 0; m.aleat.tipo = 1; tz_mn_aleat_normalizar(&m.aleat);
	VER(m.aleat.tipo == 0, "aleatorio (pantalla): un tipo que el tamaño no admite se normaliza a «al azar»");
	/* terreno y recursos del mapa */
	m.aleat.lado = 2;
	mclic(&m, ox + 565 + 180 - 13, oy + 169 + 13);
	mclic(&m, ox + 565 + 180 - 13, oy + 228 + 13);
	VER(m.aleat.terreno == 1 && m.aleat.recursos_mapa == 2, "aleatorio (pantalla): terreno y recursos del mapa (%d, %d)", m.aleat.terreno, m.aleat.recursos_mapa);
	/* casillas: animales y montañas (v8.5.6: ya hay generador) */
	int an = m.aleat.animais;
	mclic(&m, ox + 564 + 15, oy + 303 + 15);
	VER(m.aleat.animais == !an, "aleatorio (pantalla): la casilla de animales");
	mclic(&m, ox + 564 + 15, oy + 376 + 15);
	VER(m.aleat.penhascos == 1, "aleatorio (pantalla): la casilla de montañas se marca");
	{
		char tx[400]; TzMnAleat b;
		VER(tz_mn_aleat_texto(&m.aleat, tx, sizeof tx) == 0 && tz_mn_aleat_de_texto(tx, &b) == 0 && b.penhascos == 1, "aleatorio (pantalla): las montañas se recuerdan en tzar.cfg");
	}
	mclic(&m, ox + 564 + 15, oy + 376 + 15);
	VER(m.aleat.penhascos == 0, "aleatorio (pantalla): y se desmarca");
	mclic(&m, ox + 268 + 15, oy + 539 + 15);
	mclic(&m, ox + 268 + 15, oy + 569 + 15);
	VER(m.aleat.c.sem_nevoa == 1 && m.aleat.c.explorado == 1 && m.cfg.sem_nevoa == 0, "aleatorio (pantalla): sin niebla y explorar son de esta pantalla");
	/* reinos: equipo (clic en la casilla de la fila 2 = reino 1) y civilización */
	m.aleat.c.jogadores = 3;
	mclic(&m, ox + 511 + 6, oy + 31 + 35 + 6);
	mclic(&m, ox + 511 + 6, oy + 31 + 35 + 6);
	VER(m.aleat.c.equipe[1] == 2 && m.aleat.c.equipe[0] == 0, "aleatorio (pantalla): el equipo de un reino da la vuelta 0-1-2-3-4-0 (%d)", m.aleat.c.equipe[1]);
	mclic(&m, ox + 311 + 180 - 13, oy + 23 + 35 * 2 + 13);
	VER(m.aleat.c.raca[2] == 1 && m.aleat.c.raca[1] == 0, "aleatorio (pantalla): la civilización del reino 3 (%d)", m.aleat.c.raca[2]);
	/* reglas: tipo de partida con 2 valores, recursos iniciales con 3 */
	for (int i = 0; i < 3; ++i) mclic(&m, ox + 66 + 180 - 13, oy + 427 + 13);
	VER(m.aleat.c.tipo == 1, "aleatorio (pantalla): el tipo de partida tiene dos valores (%d)", m.aleat.c.tipo);
	for (int i = 0; i < 4; ++i) mclic(&m, ox + 270 + 180 - 13, oy + 487 + 13);
	VER(m.aleat.c.recursos >= 0 && m.aleat.c.recursos <= 2, "aleatorio (pantalla): los recursos iniciales van de 0 a 2 (%d)", m.aleat.c.recursos);
	/* D470: el comportamiento de la IA de un reino («COMPORTAMIENTO IA») y la rendición de la IA; los guiones, como si los diera
	 * DATA/AI/AI.INI */
	m.ia_lido = 1; m.nia = 3; m.rendicao_oferecida = 1;
	snprintf(m.ia_nome[1], sizeof m.ia_nome[1], "Agresivo"); snprintf(m.ia_fich[1], sizeof m.ia_fich[1], "War.ais");
	snprintf(m.ia_nome[2], sizeof m.ia_nome[2], "Constructor"); snprintf(m.ia_fich[2], sizeof m.ia_fich[2], "Builder.ais");
	mclic(&m, ox + 273 + 10, oy + 26 + 35 + 10);
	VER(m.escolher_ia == 1 && m.ia_sel == 0, "aleatorio (pantalla): el icono de la IA del reino 2 abre «COMPORTAMIENTO IA» (%d)", m.escolher_ia);
	mclic(&m, ox + 452 + 40, oy + 320 + 40);
	VER(m.ampliada == 0, "aleatorio (pantalla): con la ventana abierta lo de debajo no atiende");
	mclic(&m, ox + 150 + 46 + 20, oy + 140 + 88 + 16 * 1 + 5);
	VER(m.ia_sel == 1, "aleatorio (pantalla): se marca un comportamiento de la lista (%d)", m.ia_sel);
	{
		TzTela t;
		tz_tela_criar(&t, 1024, 768);
		tz_menu_desenhar(&m, &t);
		tz_tela_liberar(&t);
	}
	mclic(&m, ox + 150 + 47 + 20, oy + 140 + 254 + 10);
	VER(m.escolher_ia == 0 && !strcmp(m.aleat.ia[1], "War.ais") && !m.aleat.ia[2][0], "aleatorio (pantalla): «Aceptar» se lo da al reino (%s)", m.aleat.ia[1]);
	mclic(&m, ox + 273 + 10, oy + 26 + 35 * 2 + 10);
	mclic(&m, ox + 150 + 46 + 20, oy + 140 + 88 + 16 * 2 + 5);
	mclic(&m, ox + 150 + 261 + 20, oy + 140 + 254 + 10);
	VER(m.escolher_ia == 0 && !m.aleat.ia[2][0], "aleatorio (pantalla): «Cancelar» no cambia nada");
	mclic(&m, ox + 471 + 16, oy + 427 + 16);
	VER(m.aleat.rendicao == 1, "aleatorio (pantalla): la casilla de rendición de la IA");
	/* otro mapa: otra semilla */
	unsigned long long s0 = m.aleat.semente;
	m.aleat_mudou = 0;
	mpremir(&m, 0);
	VER(m.aleat.semente != s0 && m.aleat_mudou, "aleatorio (pantalla): «otro mapa» cambia la semilla");
	/* ampliar la vista previa */
	mclic(&m, ox + 452 + 40, oy + 320 + 40);
	VER(m.ampliada == 1, "aleatorio (pantalla): un clic en la vista previa la amplía");
	mclic(&m, ox + 10, oy + 10);
	VER(m.ampliada == 0 && m.tela == TZ_MN_ALEAT, "aleatorio (pantalla): otro clic la cierra sin hacer nada más");
	{
		TzTela t;
		tz_tela_criar(&t, 1024, 768);
		m.ampliada = 1; tz_menu_desenhar(&m, &t); m.ampliada = 0; tz_menu_desenhar(&m, &t);
		tz_tela_liberar(&t);
	}
	/* jugar devuelve lo elegido; la próxima partida tendrá otra semilla */
	TzMnAleat elegido = m.aleat;
	s0 = m.aleat.semente;
	mpremir(&m, 1);
	VER(m.acao == TZ_MR_JOGAR && m.res_indice == -1 && m.res_aleat.semente == s0 && m.res_aleat.c.jogadores == elegido.c.jogadores && m.res_aleat.terreno == elegido.terreno &&
	    m.res_aleat.recursos_mapa == elegido.recursos_mapa && m.res_aleat.c.equipe[1] == 2 && m.res_cfg.jogadores == 3 && m.aleat.semente != s0,
	    "aleatorio (pantalla): JUGAR devuelve el mapa elegido y deja otra semilla para la siguiente");
	m.acao = 0;
	/* recordar: todo a una línea y de vuelta; en tzar.cfg */
	char tx[400];
	TzMnAleat a2;
	elegido.c.equipe[7] = 4; elegido.c.raca[7] = 3; elegido.semente = 18446744073709551557ull;
	int ok = !tz_mn_aleat_texto(&elegido, tx, sizeof tx) && !tz_mn_aleat_de_texto(tx, &a2) && !memcmp(&elegido, &a2, sizeof a2);
	VER(ok, "aleatorio (pantalla): las opciones van a una línea y vuelven iguales («%.50s...»)", tx);
	TzOpcoes o1, o2;
	tz_opcoes_padrao(&o1); tz_opcoes_padrao(&o2);
	snprintf(o1.aleat, sizeof o1.aleat, "%s", tx);
	ok = !tz_opcoes_gravar(&o1, "/tmp/tzar_ui_aleat.cfg") && !tz_opcoes_ler(&o2, "/tmp/tzar_ui_aleat.cfg") && !strcmp(o1.aleat, o2.aleat) && o2.volume_som == o1.volume_som;
	TzMnAleat a3;
	ok = ok && !tz_mn_aleat_de_texto(o2.aleat, &a3) && !memcmp(&a3, &elegido, sizeof a3);
	VER(ok, "aleatorio (pantalla): tzar.cfg guarda y devuelve las opciones y sigue guardando las demás");
	remove("/tmp/tzar_ui_aleat.cfg");
	VER(!strcmp(elegido.ia[1], "War.ais") && elegido.rendicao == 1 && strstr(tx, " IA 1 - War.ais -"), "aleatorio (pantalla): la línea lleva la IA de cada reino y la rendición (D470)");
	ok = !tz_mn_aleat_de_texto("ALEAT2 2 0 0 1 1 1 1 1 5 2 0 0 200 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", &a3) && !a3.rendicao && !a3.ia[1][0];
	VER(ok, "aleatorio (pantalla): una línea de antes (sin «IA») sigue valiendo");
	/* v8.5.6: las montañas (D471) y la IA (D470) en la misma línea */
	{ TzMnAleat e2 = elegido, e3; e2.penhascos = 1;
	  ok = !tz_mn_aleat_texto(&e2, tx, sizeof tx) && strstr(tx, " 1 IA 1 ") && !tz_mn_aleat_de_texto(tx, &e3) && !memcmp(&e2, &e3, sizeof e3);
	  ok = ok && !tz_mn_aleat_de_texto("ALEAT2 2 0 0 1 1 1 1 1 5 2 0 0 200 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1", &e3) && e3.penhascos == 1 && !e3.rendicao;
	  VER(ok, "aleatorio (pantalla): montañas e IA juntas van y vuelven; montañas sin IA también"); }
	static const char *const malos[] = { "", "ALEAT 1 2", "ALEAT2 9 0 0 0 1 1 1 1 5 2 0 0 200 1 1 0 0", "ALEAT2 abc", "otra cosa" };
	int rech = 0;
	for (unsigned i = 0; i < sizeof malos / sizeof *malos; ++i) rech += tz_mn_aleat_de_texto(malos[i], &a3) != 0;
	VER(rech == 5, "aleatorio (pantalla): el texto incompleto o ajeno se rechaza (%d de 5)", rech);
	TzMnAleat fuera;
	tz_mn_aleat_padrao(&fuera);
	fuera.lado = 0; fuera.c.jogadores = 8; fuera.tipo = 2; fuera.c.recursos = 9; fuera.terreno = 7; fuera.c.equipe[0] = 12;
	tz_mn_aleat_normalizar(&fuera);
	VER(fuera.c.jogadores == 4 && fuera.tipo == 0 && fuera.c.recursos == 1 && fuera.terreno == 0 && fuera.c.equipe[0] == 0, "aleatorio (pantalla): lo fuera de rango se mete en rango");
	tz_menu_liberar(&m);
	tz_ui_skin_liberar(&skin);
}

static void prueba_menus(void)
{
	TzUiSkin skin;
	tz_ui_skin_vazia(&skin);
	TzMenu m;
	tz_menu_iniciar(&m, &skin, 1024, 768);
	m.ncampanhas = 6;
	m.campanhas = calloc(6, sizeof *m.campanhas);
	static const char *nc[6] = { "Tzar", "La Reconquista", "Excalibur y el rey Arturo", "El Imperio de Gengis Khan", "Los Dominios de la Magia", "Tutorial" };
	for (int i = 0; i < 6; ++i) snprintf(m.campanhas[i].titulo, sizeof m.campanhas[i].titulo, "%s", nc[i]);
	m.nmapas = 3;
	m.mapas = calloc(3, sizeof *m.mapas);
	for (int i = 0; i < 3; ++i) { snprintf(m.mapas[i].titulo, sizeof m.mapas[i].titulo, "Mapa %d", i + 1); m.mapas[i].jogadores_min = 2; m.mapas[i].jogadores_max = 4 + i; }
	char rot[64];
	int n = botao_texto(&m, 0, &(int){ 0 }, &(int){ 0 }, rot);
	VER(m.tela == TZ_MN_PRINCIPAL && n == 5 && !strcmp(rot, "UN SOLO JUGADOR"), "menú principal: 5 botones (un jugador, varios, opciones, créditos, salir) (%d)", n);
	mpremir(&m, 1);
	VER(m.tela == TZ_MN_PRINCIPAL, "menú principal: «varios jugadores» está apagado (sin partida en red)");
	mpremir(&m, 0);
	VER(m.tela == TZ_MN_UM, "menú: «un solo jugador» abre sus cinco opciones");
	mpremir(&m, 0);
	VER(m.tela == TZ_MN_CAMPANHAS && m.ncampanhas == 6, "menú: campañas muestra la lista de las 6");
	/* elegir la cuarta campaña con el ratón en su fila (80, 43 + 3*16) */
	int ox = (1024 - 800) / 2, oy = (768 - 600) / 2;
	mclic(&m, ox + 100, oy + 43 + 3 * 16 + 4);
	VER(m.campanha_sel == 3, "menú: elegir una campaña de la lista (%d)", m.campanha_sel);
	mtecla(&m, TZ_TC_BAIXO, 0);
	VER(m.campanha_sel == 4, "menú: las flechas cambian de campaña");
	mpremir(&m, 0);
	VER(m.acao == TZ_MR_CAMPANHA && m.res_indice == 4, "menú: JUGAR devuelve la campaña elegida (acción %d, índice %d)", m.acao, m.res_indice);
	m.acao = 0;
	mtecla(&m, TZ_TC_ESC, 0);
	VER(m.tela == TZ_MN_UM, "menú: Esc vuelve a la pantalla anterior");
	mpremir(&m, 1);
	VER(m.tela == TZ_MN_MAPAS, "menú: «misión» abre la lista de mapas");
	mclic(&m, ox + 100, oy + 41 + 2 * 16 + 4);
	VER(m.mapa_sel == 2, "menú: elegir mapa");
	mpremir(&m, 0);
	VER(m.tela == TZ_MN_CONFIG, "menú: JUGAR de la lista abre las opciones de la partida");
	/* los 6 spins: jugadores (66,369) -> flecha derecha en x+180-13 */
	int jog0 = m.cfg.jogadores;
	mclic(&m, ox + 66 + 180 - 13, oy + 369 + 13);
	VER(m.cfg.jogadores == jog0 + 1, "opciones: la flecha derecha sube el número de jugadores (%d)", m.cfg.jogadores);
	for (int i = 0; i < 10; ++i) mclic(&m, ox + 66 + 180 - 13, oy + 369 + 13);
	VER(m.cfg.jogadores == 6, "opciones: los jugadores no pasan del máximo del mapa (6)");
	mclic(&m, ox + 270 + 13, oy + 427 + 13);   /* dificultad: flecha izquierda */
	VER(m.cfg.dificuldade == 0, "opciones: dificultad con la flecha izquierda (%d)", m.cfg.dificuldade);
	mclic(&m, ox + 270 + 180 - 13, oy + 369 + 13);
	VER(m.cfg.populacao == 250, "opciones: población 200 -> 250 (%d)", m.cfg.populacao);
	mclic(&m, ox + 268 + 15, oy + 539 + 15);
	VER(m.cfg.sem_nevoa == 1, "opciones: casilla «sin niebla»");
	mpremir(&m, 0);
	VER(m.acao == TZ_MR_JOGAR && m.res_indice == 2 && m.res_cfg.jogadores == 6 && m.res_cfg.sem_nevoa == 1 && m.res_cfg.dificuldade == 0,
	    "menú: JUGAR devuelve mapa y opciones (mapa %d, %d jugadores)", m.res_indice, m.res_cfg.jogadores);
	m.acao = 0;
	/* guardar y cargar */
	tz_menu_ir(&m, TZ_MN_GUARDAR);
	m.nguardados = 2;
	m.guardados = calloc(2, sizeof *m.guardados);
	snprintf(m.guardados[0].nome, 64, "Partida 1"); snprintf(m.guardados[1].nome, 64, "Partida 2");
	m.nome_guardar[0] = 0;
	mclic(&m, (1024 - 800) / 2 + 125 + 121 + 5, (768 - 600) / 2 + 140 + 39 + 5);
	VER(m.editando, "guardar: el clic en el nombre lo pone en edición");
	mtecla(&m, 'm', TZ_MOD_SHIFT); mtecla(&m, 'i', 0); mtecla(&m, 'a', 0); mtecla(&m, TZ_TC_BORRAR, 0); mtecla(&m, 'x', 0);
	VER(!strcmp(m.nome_guardar, "Mix"), "guardar: se escribe el nombre con Mayús y Borrar («%s»)", m.nome_guardar);
	mtecla(&m, TZ_TC_ENTER, 0);
	mpremir(&m, 0);
	VER(m.acao == TZ_MR_GUARDAR && !strcmp(m.res_nome, "Mix"), "guardar: el botón devuelve la acción con el nombre");
	m.acao = 0;
	tz_menu_ir(&m, TZ_MN_CARREGAR);
	mclic(&m, (1024 - 800) / 2 + 125 + 100, (768 - 600) / 2 + 140 + 77 + 16 + 4);
	VER(m.guardado_sel == 1, "cargar: elegir la segunda partida de la lista (%d)", m.guardado_sel);
	mpremir(&m, 0);
	VER(m.acao == TZ_MR_CARREGAR && m.res_indice == 1, "cargar: el botón devuelve la partida elegida");
	m.acao = 0;
	/* salir pide confirmación */
	m.tela = TZ_MN_PRINCIPAL; m.npilha = 0;
	mpremir(&m, 4);
	VER(m.tela == TZ_MN_SAIR && m.acao == 0, "salir: pide confirmación");
	mpremir(&m, 1);
	VER(m.tela == TZ_MN_PRINCIPAL, "salir: «No» vuelve");
	mpremir(&m, 4); mpremir(&m, 0);
	VER(m.acao == TZ_MR_SAIR, "salir: «Sí» devuelve TZ_MR_SAIR");
	/* menú de dentro de la partida */
	tz_menu_liberar(&m);
	tz_menu_iniciar(&m, &skin, 1024, 768);
	m.tela = TZ_MN_PARTIDA;
	n = botao_texto(&m, 0, &(int){ 0 }, &(int){ 0 }, rot);
	VER(n == 8, "menú de partida: 8 botones, con «Rendirse» (%d)", n);
	mpremir(&m, 6);
	VER(m.acao == TZ_MR_RENDIR, "menú de partida: «rendirse» pide la ventana de rendición");
	m.acao = 0;
	mpremir(&m, 7);
	VER(m.acao == TZ_MR_CONTINUAR, "menú de partida: «volver al juego» continúa");
	m.acao = 0;
	mpremir(&m, 4);
	VER(m.acao == TZ_MR_REINICIAR, "menú de partida: reiniciar");
	m.acao = 0;
	/* dibujo de todas las pantallas a varios tamaños */
	static const int tam[][2] = { { 800, 600 }, { 1280, 720 }, { 1920, 1080 } };
	int dibujadas = 0;
	for (unsigned k = 0; k < 3; ++k) {
		TzMenu d;
		tz_menu_iniciar(&d, &skin, tam[k][0], tam[k][1]);
		d.ncampanhas = 6; d.campanhas = calloc(6, sizeof *d.campanhas);
		for (int i = 0; i < 6; ++i) snprintf(d.campanhas[i].titulo, 64, "%s", nc[i]);
		d.nmapas = 3; d.mapas = calloc(3, sizeof *d.mapas);
		for (int i = 0; i < 3; ++i) { snprintf(d.mapas[i].titulo, 64, "Mapa %d", i); d.mapas[i].jogadores_min = 2; d.mapas[i].jogadores_max = 4; }
		TzTela t;
		tz_tela_criar(&t, tam[k][0], tam[k][1]);
		for (int s = 0; s < TZ_MN_NTELAS; ++s) { d.tela = s; tz_menu_desenhar(&d, &t); dibujadas++; if (k == 0 && s == TZ_MN_PRINCIPAL) tz_tela_gravar_bmp(&t, "/tmp/tzar_ui_sint_menu.bmp"); }
		tz_tela_liberar(&t);
		tz_menu_liberar(&d);
	}
	VER(dibujadas == 3 * TZ_MN_NTELAS, "menús: las %d pantallas se dibujan a 800x600, 1280x720 y 1920x1080", TZ_MN_NTELAS);
	tz_menu_liberar(&m);
	tz_ui_skin_liberar(&skin);
}

/* ---------------------------------------------------------------- datos reales (privado) */
static void real(const char *raiz, const char *saida);

/* v8.5.5.2: modos de pantalla, panel derecho, diplomacia y chat, cursor y menús nuevos */
static void prueba_v8552(void)
{
	TzOpcoes o;
	TzModo m;
	tz_opcoes_padrao(&o);
	tz_modo_calcular(&o, 2560, 1440, &m);
	VER(m.cw == 1280 && m.ch == 720 && m.fator == 2 && m.dw == 2560 && m.dx == 0, "pantalla: 2560x1440 con interfaz automática dibuja a 1280x720 y amplía x2");
	tz_modo_calcular(&o, 1920, 1080, &m);
	VER(m.cw == 1920 && m.ch == 1080 && m.fator == 1, "pantalla: 1920x1080 a su tamaño (la mitad no llega a 800x600)");
	o.interface = 2;
	tz_modo_calcular(&o, 1920, 1080, &m);
	VER(m.cw == 1920 && m.ch == 1080 && m.fator == 1, "pantalla: interfaz x2 pedida en 1920x1080 baja a x1 (en 960x540 no cabe la interfaz) -> %dx%d", m.cw, m.ch);
	o.interface = 0; o.resolucao = 2;   /* 800x600 */
	tz_modo_calcular(&o, 1920, 1080, &m);
	VER(m.cw == 800 && m.fator == 1 && m.dw == 800 && m.dx == 560 && m.dy == 240, "pantalla: 800x600 entero en 1920x1080, centrado con bandas");
	o.escala = TZ_ESC_AJUSTAR;
	tz_modo_calcular(&o, 1920, 1080, &m);
	VER(m.dw == 1440 && m.dh == 1080 && m.dx == 240 && m.fator == 0, "pantalla: ajustado mantiene la proporción (1440x1080 y bandas a los lados)");
	int cx, cy;
	tz_modo_ponto(&m, 240 + 720, 540, &cx, &cy);
	VER(cx == 400 && cy == 300, "pantalla: el ratón vuelve al lienzo (%d,%d)", cx, cy);
	tz_modo_ponto(&m, 5, 5, &cx, &cy);
	VER(cx == 0 && cy == 2, "pantalla: en la banda de la izquierda se recorta al borde (%d,%d)", cx, cy);
	o.resolucao = 1; o.escala = TZ_ESC_INTEIRA;
	tz_modo_calcular(&o, 640, 480, &m);
	VER(m.cw == 800 && m.ch == 600 && m.dw == 640 && m.dh == 480, "pantalla: 640x480 dibuja a 800x600 y reduce");
	{
		uint32_t src[4] = { 0xff112233, 0xff445566, 0xff778899, 0xffaabbcc }, dst[6 * 4];
		TzModo q;
		TzOpcoes oo; tz_opcoes_padrao(&oo);
		tz_modo_destino(&oo, 2, 2, 6, 4, &q);
		tz_apresentar_escala(src, &q, dst, 6, 0);
		VER(q.fator == 2 && dst[0] == 0 && dst[1] == 0x112233 && dst[2] == 0x112233 && dst[3] == 0x445566 && dst[5] == 0 && dst[6 * 3 + 4] == 0xaabbcc,
		    "pantalla: x2 nítido copia cada píxel en 2x2 y pinta las bandas de negro");
		tz_modo_destino(&(TzOpcoes){ .escala = TZ_ESC_AJUSTAR }, 2, 2, 3, 3, &q);
		tz_apresentar_escala(src, &q, dst, 3, 1);
		VER(q.fator == 0 && dst[4] != 0x112233 && dst[4] != 0, "pantalla: el suave mezcla en el centro (%06x)", dst[4]);
	}
	/* opciones guardadas */
	{
		char ruta[] = "/tmp/tzar_ui_opcoes.cfg";
		TzOpcoes a, b;
		tz_opcoes_padrao(&a);
		a.tela_cheia = 1; a.resolucao = 3; a.escala = TZ_ESC_AJUSTAR; a.suave = 1; a.interface = 2; a.painel_direita = 1; a.volume_musica = 30;
		tz_opcoes_padrao(&b);
		VER(!tz_opcoes_gravar(&a, ruta) && !tz_opcoes_ler(&b, ruta) && !memcmp(&a, &b, sizeof a), "opciones: tzar.cfg se guarda y se lee igual");
		remove(ruta);
	}
	/* panel derecho */
	Cena c;
	cena_iniciar(&c, 1024, 768);
	TzRect mi = c.u.mapa_rect;
	tz_ui_lado(&c.u, 1);
	VER(c.u.mapa_rect.x == 0 && c.u.mapa_rect.w == 1024 - 149 && c.u.minimapa_rect.x == 668 + 224 && mi.x == 150,
	    "panel derecho: el mapa empieza en 0 y el minimapa sigue al borde derecho (%d)", c.u.minimapa_rect.x);
	TzRect rm = tz_ui_rect(&c.u, TZ_UR_MENU);
	VER(rm.x == 668 + 224 && tz_ui_botao_em(&c.u, 668 + 224 + 19 + 5, 335 + 5) < 0, "panel derecho: el botón del menú a la derecha");
	TzUiEntrada e = { TZ_IN_BAIXO, rm.x + 5, rm.y + 5, 1, 0, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	VER(c.u.pedir_menu == 1, "panel derecho: el clic en el menú lo pide");
	TzRect rd = tz_ui_rect(&c.u, TZ_UR_DIPLOMACIA);
	e = (TzUiEntrada){ TZ_IN_BAIXO, rd.x + 5, rd.y + 5, 1, 0, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	VER(c.u.janela == TZ_UJ_DIPLOMACIA, "diplomacia: el botón abre la ventana");
	TzTela t;
	tz_tela_criar(&t, 1024, 768);
	tz_ui_desenhar_hud(&c.u, &t);
	TzRect j = tz_ui_janela_rect(&c.u);
	/* el reino 2 en la primera fila: su actitud en (j.x + 250, j.y + 72) */
	int antes = tz_mundo_relacao(&c.m, 1, 2);
	limpar_log();
	e = (TzUiEntrada){ TZ_IN_BAIXO, j.x + 20 + 230 + 10, j.y + 70 + 10, 1, 0, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	VER(c.u.n_comandos > 0, "diplomacia: cambiar la actitud manda TZ_C_DIPLOMACIA (antes %d)", antes);
	e = (TzUiEntrada){ TZ_IN_TECLA, 0, 0, 0, TZ_TC_ESC, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	e = (TzUiEntrada){ TZ_IN_TECLA, 0, 0, 0, TZ_TC_ENTER, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	VER(c.u.janela == TZ_UJ_CHAT, "chat: Intro lo abre");
	const char *txt = "ai war";
	for (const char *p = txt; *p; ++p) { e = (TzUiEntrada){ TZ_IN_TECLA, 0, 0, 0, *p, 0, 0 }; tz_ui_entrada(&c.u, &e, reloj); }
	e = (TzUiEntrada){ TZ_IN_TECLA, 0, 0, 0, TZ_TC_ENTER, 0, 0 };
	tz_ui_entrada(&c.u, &e, reloj);
	VER(c.u.janela == 0 && !strcmp(c.u.chat_saida, "ai war") && c.u.nchat == 1, "chat: lo escrito sale para el programa y queda en el historial");
	tz_ui_chat_recebido(&c.u, "Reino 2: preparaos");
	tz_ui_desenhar_hud(&c.u, &t);
	VER(c.u.nchat == 2, "chat: las frases de la IA se ven");
	/* cursor propio sin datos: una flecha negra y blanca */
	memset(t.px, 0, (size_t)t.w * t.h * 4);
	tz_ui_cursor_desenhar(&c.skin, &t, "", 100, 100);
	VER(t.px[100 * t.w + 100] == 0xff000000u && t.px[105 * t.w + 102] == 0xffffffffu, "cursor: sin arte, la flecha propia en su punto");
	tz_tela_liberar(&t);
	cena_liberar(&c);
	tz_ui_cursor_liberar();
	/* menús nuevos: opciones de pantalla, misiones de la campaña, fin de partida, instrucciones y créditos */
	TzUiSkin s;
	tz_ui_skin_vazia(&s);
	TzMenu mn;
	tz_menu_iniciar(&mn, &s, 800, 600);
	TzTela tm;
	tz_tela_criar(&tm, 800, 600);
	tz_menu_ir(&mn, TZ_MN_OPCOES);
	TzRect bt[40]; char rot[40][64];
	tz_menu_desenhar(&mn, &tm);
	/* la flecha derecha del primer spin de pantalla: (578 + 186 - 27, 328) */
	e = (TzUiEntrada){ TZ_IN_BAIXO, 548 + 196 - 20, 322 + 10, 1, 0, 0, 0 };
	tz_menu_entrada(&mn, &e);
	e.tipo = TZ_IN_CIMA;
	tz_menu_entrada(&mn, &e);
	VER(mn.op_mudou && mn.op.tela_cheia == 1, "opciones: el spin de Modo pasa a pantalla completa");
	mn.ncampanhas = 1;
	mn.campanhas = calloc(1, sizeof *mn.campanhas);
	snprintf(mn.campanhas[0].titulo, sizeof mn.campanhas[0].titulo, "Prueba");
	mn.nmissoes = 3;
	mn.missoes = calloc(3, sizeof *mn.missoes);
	for (int i = 0; i < 3; ++i) { snprintf(mn.missoes[i].titulo, 64, "Mision %d", i + 1); mn.missoes[i].indice = i; mn.missoes[i].disponivel = i < 2; }
	tz_menu_ir(&mn, TZ_MN_CAMPANHAS);
	e = (TzUiEntrada){ TZ_IN_BAIXO, 80 + 10, 43 + 5, 1, 0, 0, 0 };
	tz_menu_entrada(&mn, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(&mn, &e);
	VER(mn.missao_sel == 1, "campañas: al elegir la campaña queda la última misión alcanzada (%d)", mn.missao_sel);
	e = (TzUiEntrada){ TZ_IN_BAIXO, 80 + 10, 352 + 2 * 16 + 5, 1, 0, 0, 0 };
	tz_menu_entrada(&mn, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(&mn, &e);
	VER(mn.missao_sel == 1, "campañas: una misión no alcanzada no se elige");
	e = (TzUiEntrada){ TZ_IN_BAIXO, 80 + 10, 352 + 5, 1, 0, 0, 0 };
	tz_menu_entrada(&mn, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(&mn, &e);
	int nb = tz_menu_botoes(&mn, bt, rot, 40);
	for (int i = 0; i < nb; ++i) if (bt[i].x == 78 && bt[i].y == 545) { e = (TzUiEntrada){ TZ_IN_BAIXO, 90, 550, 1, 0, 0, 0 }; tz_menu_entrada(&mn, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(&mn, &e); }
	VER(mn.acao == TZ_MR_CAMPANHA && mn.res_missao == 0, "campañas: jugar la misión elegida (%d)", mn.res_missao);
	tz_menu_desenhar(&mn, &tm);
	mn.acao = 0;
	mn.fim_ganhou = 1; mn.fim_seguinte = 1; mn.nfim_resumo = 1; snprintf(mn.fim_resumo[0], 160, "Tiempo de juego: 0:10:00");
	mn.tela = TZ_MN_PRINCIPAL; mn.npilha = 0;
	tz_menu_ir(&mn, TZ_MN_FIM);
	nb = tz_menu_botoes(&mn, bt, rot, 40);
	VER(nb == 4, "fin de partida: cuatro botones (repetir, siguiente, salir, ocultar)");
	tz_menu_desenhar(&mn, &tm);
	if (nb == 4) {
		e = (TzUiEntrada){ TZ_IN_BAIXO, bt[1].x + 150 + 5, bt[1].y + 140 + 5, 1, 0, 0, 0 };
		tz_menu_entrada(&mn, &e); e.tipo = TZ_IN_CIMA; tz_menu_entrada(&mn, &e);
		VER(mn.acao == TZ_MR_SEGUINTE, "fin de partida: «siguiente» pide la misión siguiente");
	}
	mn.acao = 0;
	tz_menu_ir(&mn, TZ_MN_AJUDA);
	tz_menu_desenhar(&mn, &tm);
	VER(mn.najuda > 3, "instrucciones: sin HELP/HK.TXT, el texto propio partido en %d líneas", mn.najuda);
	mn.tela = TZ_MN_PRINCIPAL;
	tz_menu_tic(&mn, 1000);
	tz_menu_ir(&mn, TZ_MN_CREDITOS);
	tz_menu_desenhar(&mn, &tm);
	int c1 = checksum(&tm);
	tz_menu_tic(&mn, 4000);
	tz_menu_desenhar(&mn, &tm);
	VER(mn.ncreditos > 3 && checksum(&tm) != c1, "créditos: el texto sube con el tiempo");
	tz_tela_liberar(&tm);
	tz_menu_liberar(&mn);
	tz_ui_skin_liberar(&s);
}

int main(int argc, char **argv)
{
	if (argc >= 3 && !strcmp(argv[1], "unidad")) {
		prueba_fuentes();
		prueba_seleccion();
		prueba_ordenes();
		prueba_menu_y_colocacion();
		prueba_minimapa_y_avisos();
		prueba_dibujo();
		prueba_menus();
		prueba_menu_aleatorio();
		prueba_v8552();
		printf("%s: %d fallos\n", fallos ? "FALLO" : "OK", fallos);
		return fallos != 0;
	}
	if (argc >= 4 && !strcmp(argv[1], "real")) { real(argv[2], argv[3]); return fallos != 0; }
	fprintf(stderr, "uso: tzar_ui unidad <capturas> | tzar_ui real <instalación> <salida>\n");
	return 2;
}

#include "tzar_ui_real.inc"

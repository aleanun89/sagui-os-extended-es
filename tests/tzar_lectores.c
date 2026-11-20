/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_lectores.c - arnés en el PC de los lectores de Tzar (D417, tests/h417_tzar.py).
 *
 *   tzar_lectores sintetico <carpeta>   los datos SINTÉTICOS de tools/tzar_sintetico.py: comprueba cada lector con
 *                                       los valores que el generador puso (y que los dañados se rechazan)
 *   tzar_lectores privado <carpeta>     (a mano, nunca en la batería) abre todo lo de una instalación real y
 *                                       cuenta fallos: entradas de los paquetes, BMP, RLE, mapas y campañas
 * Escribe «OK ...» o «FALLO ...» por línea y sale con 1 si algo falló.
 */
#include "tzar.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int fallos;
#define VER(c, ...) do { if (c) { printf("OK "); printf(__VA_ARGS__); printf("\n"); } else { fallos++; printf("FALLO "); printf(__VA_ARGS__); printf(" (%s)\n", tz_erro()); } } while (0)

static uint8_t *ler_fich(const char *ruta, uint32_t *n)
{
	FILE *f = fopen(ruta, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long l = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *d = malloc(l ? (size_t)l : 1);
	if (d && fread(d, 1, (size_t)l, f) != (size_t)l) { free(d); d = NULL; }
	fclose(f);
	*n = (uint32_t)l;
	return d;
}

static int mapa_e_escena(TzRecursos *r, TzHpfs *c, const char *nome, int verbose, TzMapa *out_m, TzEscena *out_e)
{
	TzMapa m;
	if (tz_mapa_ler(c, &m)) { printf("FALLO mapa %s: %s\n", nome, tz_erro()); return -1; }
	TzTerreno t;
	if (tz_terreno_carregar(&t, r, m.bioma)) { printf("FALLO terreno %s (%s): %s\n", nome, m.bioma, tz_erro()); tz_mapa_liberar(&m); return -1; }
	int malas = 0;
	for (int i = 0; i < m.lado * m.lado; ++i) {
		int paso;
		if (!tz_terreno_loseta(&t, m.casillas[i], i % m.lado, i / m.lado, &paso)) { if (!malas) printf("  loseta %d: %s\n", i, tz_erro()); malas++; }
	}
	uint32_t *mini = malloc((size_t)m.lado * m.lado * 4);
	int rm = mini ? tz_terreno_minimapa(&t, &m, mini) : -1;
	free(mini);
	TzBiblioteca b;
	TzEscena e;
	int r1 = tz_biblioteca_carregar(&b, r, m.bioma);
	int r2 = r1 ? -1 : tz_escena_montar(&e, &b, r, &t, &m);
	if (verbose || malas || rm || r2 || (!r2 && e.falhas))
		printf("%s mapa %s: lado %d bioma %s objetos %d losetas malas %d minimapa %d escena %d visuales %d imágenes %d sin dibujo %d fallos %d %s\n",
		       (malas || rm || r2 || (!r2 && e.falhas)) ? "FALLO" : "OK", nome, m.lado, m.bioma, m.nobj, malas, rm, r2,
		       r2 ? 0 : e.nv, r2 ? 0 : e.nimgs, r2 ? 0 : e.sem_imagem, r2 ? 0 : e.falhas, r2 ? tz_erro() : (e.falhas ? e.ultima_falha : ""));
	int res = (malas || rm || r2 || (!r2 && e.falhas)) ? -1 : 0;
	if (out_e && !r2) *out_e = e; else if (!r2) tz_escena_liberar(&e);
	if (!r1) tz_biblioteca_liberar(&b);
	tz_terreno_liberar(&t);
	if (out_m) *out_m = m; else tz_mapa_liberar(&m);
	return res;
}

/* ------------------------------------------------------------------ privado */

static int privado(const char *raiz)
{
	TzRecursos r;
	if (tz_recursos_abrir(&r, raiz)) { printf("FALLO recursos: %s\n", tz_erro()); return 1; }
	printf("recursos: %d paquetes, %d nombres\n", r.npac, r.nnomes);
	int ent = 0, fe = 0, nbmp = 0, fbmp = 0, nrle = 0, frle = 0, nini = 0;
	for (int p = 0; p < r.npac; ++p)
		for (int i = 0; i < r.pac[p].n; ++i) {
			uint32_t n;
			uint8_t *d = tz_pacote_ler(&r.pac[p], &r.pac[p].e[i], &n);
			ent++;
			if (!d) { if (fe++ < 5) printf("  entrada %s: %s\n", r.pac[p].e[i].nome, tz_erro()); continue; }
			const char *nm = r.pac[p].e[i].nome;
			size_t l = strlen(nm);
			if (l > 4 && !strcasecmp(nm + l - 4, ".bmp")) {
				TzImg im;
				nbmp++;
				if (tz_bmp(d, n, 1, -1, &im)) { if (fbmp++ < 5) printf("  bmp %s: %s\n", nm, tz_erro()); }
				else tz_img_liberar(&im);
			} else if (l > 4 && !strcasecmp(nm + l - 4, ".rle")) {
				nrle++;
				if (tz_rle_validar(d, n)) { if (frle++ < 5) printf("  rle %s: %s\n", nm, tz_erro()); }
			} else if (l > 4 && !strcasecmp(nm + l - 4, ".ini")) {
				TzIni ini = { 0 };
				tz_ini_ler(&ini, d, n);
				nini++;
				tz_ini_liberar(&ini);
			}
			free(d);
		}
	printf("%s entradas %d (fallos %d), BMP %d (fallos %d), RLE %d (fallos %d), INI %d\n", fe + fbmp + frle ? "FALLO" : "OK", ent, fe, nbmp, fbmp, nrle, frle, nini);
	fallos += fe + fbmp + frle;
	/* mapas sueltos y campañas */
	const char *pastas[2] = { "Maps", "Campaign" };
	int nm = 0, fm = 0;
	for (int k = 0; k < 2; ++k) {
		char ruta[1024];
		snprintf(ruta, sizeof ruta, "%s/%s", raiz, pastas[k]);
		DIR *d = opendir(ruta);
		struct dirent *de;
		while (d && (de = readdir(d))) {
			size_t l = strlen(de->d_name);
			if (l < 5 || (strcasecmp(de->d_name + l - 4, ".wmp") && strcasecmp(de->d_name + l - 4, ".wcm"))) continue;
			char f[1300];
			snprintf(f, sizeof f, "%s/%s", ruta, de->d_name);
			TzHpfs c;
			if (tz_hpfs_abrir(&c, f)) { printf("FALLO hpfs %s: %s\n", de->d_name, tz_erro()); fm++; continue; }
			if (!strcasecmp(de->d_name + l - 4, ".wmp")) {
				nm++;
				if (mapa_e_escena(&r, &c, de->d_name, 1, NULL, NULL)) fm++;
			} else {
				for (int i = 0; i < c.n; ++i) {
					size_t l2 = strlen(c.e[i].nome);
					if (l2 < 5 || strcasecmp(c.e[i].nome + l2 - 4, ".wmp")) continue;
					TzHpfs sub;
					nm++;
					if (tz_hpfs_anidado(&c, c.e[i].nome, &sub)) { printf("FALLO anidado %s/%s: %s\n", de->d_name, c.e[i].nome, tz_erro()); fm++; continue; }
					char nome[600];
					snprintf(nome, sizeof nome, "%s/%s", de->d_name, c.e[i].nome);
					if (mapa_e_escena(&r, &sub, nome, 1, NULL, NULL)) fm++;
					tz_hpfs_fechar(&sub);
				}
			}
			tz_hpfs_fechar(&c);
		}
		if (d) closedir(d);
	}
	printf("%s mapas %d (fallos %d)\n", fm ? "FALLO" : "OK", nm, fm);
	fallos += fm;
	/* fase 3: todas las hojas animadas de IMAGES.INI (el común y el de cada bioma) con sus cuadros y tiempos */
	TzTextos tx;
	int rt = tz_textos_carregar(&tx, &r);
	printf("%s textos: %s\n", rt ? "FALLO" : "OK", rt ? tz_erro() : "STRINGS cargados");
	fallos += rt != 0;
	if (!rt) tz_ini_liberar(&tx.ini);
	int nf = 0, ok_f = 0;
	for (int i = 0; i < r.nnomes; ++i) {
		const char *nm = r.nomes[i];
		size_t l = strlen(nm);
		if (strncmp(nm, "LOCAL/FONTS/", 12) || l < 16 || strcmp(nm + l - 4, ".FNT")) continue;
		uint32_t n;
		uint8_t *d = tz_recursos_ler(&r, nm, &n);
		TzFonte f;
		nf++;
		if (d && !tz_fonte_ler(&f, d, n)) { ok_f++; tz_fonte_liberar(&f); }
		else if (strstr(tz_erro(), "de color")) { ok_f++; printf("  fuente %s: de color, no de cobertura (ReTzar tampoco la usa para texto)\n", nm); }
		else printf("FALLO fuente %s: %s\n", nm, tz_erro());
		free(d);
	}
	printf("%s fuentes .FNT: %d de %d\n", ok_f == nf ? "OK" : "FALLO", ok_f, nf);
	fallos += ok_f != nf;
	for (int i = 0; i < r.nnomes; ++i) {
		const char *nm = r.nomes[i];
		size_t l = strlen(nm);
		if (strncmp(nm, "IMAGES/TERRAIN/", 15) || l < 28 || strcmp(nm + l - 12, ".TERRAIN.INI") || strchr(nm + 15, '/')) continue;
		char bioma[80];
		snprintf(bioma, sizeof bioma, "%.*s", (int)(l - 12 - 15), nm + 15);
		TzBiblioteca b;
		TzEscena e;
		TzMapa m;
		memset(&m, 0, sizeof m);
		int hojas, cuadros, fa, aus;
		char ult[300];
		if (tz_biblioteca_carregar(&b, &r, bioma) || tz_escena_preparar(&e, &b, &r, &m)) { printf("FALLO biblioteca %s: %s\n", bioma, tz_erro()); fallos++; continue; }
		int rv = tz_anim_verificar(&e, &hojas, &cuadros, &fa, &aus, ult, sizeof ult);
		printf("%s animaciones del bioma %s: %d hojas animadas, %d cuadros, %d fallos, %d secciones sin fichero %s\n", rv ? "FALLO" : "OK", bioma, hojas, cuadros, fa, aus, rv ? ult : "");
		fallos += rv != 0;
		tz_escena_liberar(&e);
		tz_biblioteca_liberar(&b);
	}
	tz_recursos_fechar(&r);
	printf("%s privado: %d fallos\n", fallos ? "FALLO" : "OK", fallos);
	return fallos != 0;
}


/* ------------------------------------------------------------------ fase 3: animación, selección y textos */

static TzIni ini_de(const char *t)
{
	TzIni i = { 0 };
	tz_ini_ler(&i, (const uint8_t *)t, (uint32_t)strlen(t));
	return i;
}

static uint32_t pix(const TzLienzo *l, int x, int y) { return l->px[y * l->passo + x] & 0xffffff; }

static void fase3(TzRecursos *r, const char *raiz)
{
	char ruta[1024];
	uint32_t n;
	/* tiempos: los casos de las pruebas de animation.rs de ReTzar */
	TzTempo t;
	TzIni ini = ini_de("[idle]\nrows=3\nanim_time=1\ndelay=8000,228,227\n");
	VER(!tz_tempo_direcional(&t, &ini, "idle", 3) && t.duracao == 8455 && tz_tempo_quadro(&t, 7999, 0) == 0 && tz_tempo_quadro(&t, 8000, 0) == 1 &&
	    tz_tempo_quadro(&t, 8228, 0) == 2 && tz_tempo_quadro(&t, 8455, 0) == 0 && tz_tempo_quadro(&t, 8455ull * 1000 + 8228, 0) == 2 && tz_tempo_quadro(&t, 0, 8000) == 1,
	    "tiempos: el reposo aguanta, anima y repite sin deriva (%llu ms)", (unsigned long long)t.duracao);
	VER(tz_tempo_atraso(&t, 1) == 228 && tz_tempo_inicio(&t, 2) == 8228, "atraso e inicio de cada cuadro");
	tz_tempo_liberar(&t); tz_ini_liberar(&ini);
	ini = ini_de("[ship]\nrows=7\n[d]\nanim_time=1000\ndelay=100,100,100,9000\n[s]\ndelay=10,10\n[cow]\ndelay=800,200,\n");
	VER(!tz_tempo_direcional(&t, &ini, "ship", 7) && t.duracao == 994, "sin claves: 1000 ms entre 7 filas, redondeado a 994");
	tz_tempo_liberar(&t);
	VER(!tz_tempo_direcional(&t, &ini, "s", 2) && t.duracao == 1000 && tz_tempo_quadro(&t, 499, 0) == 0 && tz_tempo_quadro(&t, 500, 0) == 1, "atrasos cortos crecen 1 ms por fila");
	tz_tempo_liberar(&t);
	VER(!tz_tempo_direcional(&t, &ini, "d", 4) && t.duracao == 9300, "atrasos largos se respetan");
	tz_tempo_liberar(&t);
	VER(!tz_tempo_direcional(&t, &ini, "cow", 2) && t.duracao == 1000, "una coma final no añade fila");
	tz_tempo_liberar(&t); tz_ini_liberar(&ini);
	ini = ini_de("[flag]\nfps=4\n[c]\nrows=6\n[still]\n[one]\nfps=5\n");
	VER(!tz_tempo_passos(&t, &ini, "flag", 7, 0) && t.duracao == 1750 && tz_tempo_quadro(&t, 250, 0) == 1, "una columna a su fps");
	tz_tempo_liberar(&t);
	VER(!tz_tempo_passos(&t, &ini, "c", 6, 10) && t.duracao == 600, "estandartes de edificio a 10 por segundo sin fps");
	tz_tempo_liberar(&t);
	VER(tz_tempo_passos(&t, &ini, "still", 6, 0) != 0 && tz_tempo_passos(&t, &ini, "one", 1, 0) != 0, "sin fps o con una fila: quieto");
	tz_ini_liberar(&ini);

	/* fuente .FNT, textos y fichas */
	uint8_t *d = tz_recursos_ler(r, "LOCAL/FONTS/SMALL.FNT", &n);
	TzFonte f;
	VER(d && !tz_fonte_ler(&f, d, n) && f.alt == 8 && f.nglifos == 3 && f.niveis == 7, "fuente .FNT: %dx%d, %d glifos", f.larg, f.alt, f.nglifos);
	free(d);
	uint32_t pixels[32 * 16] = { 0 };
	TzLienzo l = { pixels, 32, 16, 32 };
	tz_fonte_texto(&f, &l, 0, 0, "AB", 0xffffff);
	VER(tz_fonte_medir(&f, "AB") == 14 && pix(&l, 3, 3) == 0xffffff && pix(&l, 8 + 3, 3) == 0x6d6d6d && pix(&l, 20, 3) == 0, "texto con avances y cobertura: %06x %06x", pix(&l, 3, 3), pix(&l, 11, 3));
	tz_fonte_liberar(&f);
	TzTextos tx;
	VER(!tz_textos_carregar(&tx, r), "STRINGS.INI, STRINGS2 y STRINGS3 fusionados");
	TzObjeto o;
	memset(&o, 0, sizeof o);
	char nome[200], utf[200];
	o.classe = 8; o.civ = 1; o.vida = 40; o.vida_max = 100;
	VER(tz_nome_objeto(&tx, &o, nome, sizeof nome) && !strcmp(nome, "Caballero andante"), "nombre de la clase (el STRINGS2 gana): %s", nome);
	tz_ficha_objeto(&tx, &o, nome, sizeof nome);
	VER(!strcmp(nome, "Caballero andante  40/100"), "ficha con la vida: %s", nome);
	o.classe = 66;
	tz_nome_objeto(&tx, &o, nome, sizeof nome);
	VER(!strcmp(nome, "Casa - grande"), "0x96 de Windows-1252 pasado a Latin-1: %s", nome);
	o.civ = 3;
	tz_nome_objeto(&tx, &o, nome, sizeof nome);
	VER(!strcmp(nome, "Casa asi\xe1tica"), "variante por civilización (a): %s", nome);
	tz_cp1252_a_utf8(nome, utf, sizeof utf);
	VER(!strcmp(utf, "Casa asi\xc3\xa1tica"), "a UTF-8");
	o.classe = 1; o.civ = 1;
	tz_nome_objeto(&tx, &o, nome, sizeof nome);
	VER(!strcmp(nome, "Campesino\nde prueba"), "\\n del texto: salto de línea");
	o.classe = 3;
	tz_nome_objeto(&tx, &o, nome, sizeof nome);
	VER(!strcmp(nome, "footman"), "sin texto: el nombre de la clase");
	tz_ini_liberar(&tx.ini);

	/* terreno animado: el agua pasa de página con el tic y la transición compuesta la sigue */
	TzTerreno ter;
	VER(!tz_terreno_carregar(&ter, r, "grass"), "terreno grass");
	int paso;
	const uint32_t *a0 = tz_terreno_loseta_t(&ter, (uint16_t)(0), 0, 0, 0, &paso), *a1 = tz_terreno_loseta_t(&ter, (uint16_t)(0), 0, 0, 1, &paso);
	VER(a0 && a1 && (a0[5 * paso + 5] & 0xffffff) == 0x2040c0 && (a1[5 * paso + 5] & 0xffffff) == 0x2846c8 && tz_terreno_loseta_t(&ter, 0, 0, 0, 2, &paso) == a0,
	    "agua: dos páginas, la 2.ª al tic 1 y vuelve al 2");
	const uint16_t cel = (uint16_t)(1 << 7 | 0 << 4 | 2);
	const uint32_t *c0 = tz_terreno_loseta_t(&ter, cel, 3, 3, 0, &paso), *c1 = tz_terreno_loseta_t(&ter, cel, 3, 3, 1, &paso);
	VER(c0 && c1 && c0 != c1 && c0[5 * 32 + 30] == 0xff2040c0u && c1[5 * 32 + 30] == 0xff2846c8u && c0[5 * 32 + 2] == c1[5 * 32 + 2], "transición con agua: el lado del agua cambia con el tic y el de la hierba no");
	VER(tz_terreno_loseta_t(&ter, cel, 3, 3, 0, &paso) == c0, "cada cuadro de la transición queda en la caché");
	VER(ter.cor_selecao[0] == 255, "color de selección por defecto: blanco");
	tz_terreno_liberar(&ter);

	/* el mapa de animación: caballero, llama, peces, águila, puerta y casa */
	snprintf(ruta, sizeof ruta, "%s/prueba/anim.wmp", raiz);
	TzHpfs c;
	TzMapa m;
	VER(!tz_hpfs_abrir(&c, ruta) && !tz_mapa_ler(&c, &m) && m.nobj == 6, "mapa de animación");
	tz_hpfs_fechar(&c);
	tz_terreno_carregar(&ter, r, m.bioma);
	TzBiblioteca b;
	TzEscena e;
	VER(!tz_biblioteca_carregar(&b, r, m.bioma) && !tz_escena_montar(&e, &b, r, &ter, &m) && e.falhas == 0 && e.sem_imagem == 0, "escena con animaciones: %d visuales, %d animaciones, %d aves, %d fallos", e.nv, e.nanims, e.nvoos, e.falhas);
	/* caballero (índice 0): hoja de reposo knight_idle de 2 columnas x 3 filas, 333 ms por cuadro */
	const TzVisual *vk = NULL, *vf = NULL, *vp = NULL, *vh = NULL;
	int flags = 0, nm = 0;
	for (int i = 0; i < e.nv; ++i) {
		const TzVisual *v = &e.v[i];
		if (v->obj == 0 && v->capa == TZ_CAPA_CORPO) vk = v;
		if (v->obj == 1) vf = v;
		if (v->obj == 2) vp = v;
		if (v->obj == 5 && v->capa == TZ_CAPA_CORPO) vh = v;
		if (v->obj == 4 && v->capa == TZ_CAPA_ESTANDARTE) flags++;
		if (v->anim >= 0) nm++;
	}
	VER(vk && vk->anim >= 0 && vf && vf->anim >= 0 && vp && vp->anim >= 0 && vh && vh->anim < 0, "se animan el caballero, la llama y los peces; la casa no");
	int img0 = vf->img;
	tz_animar(&e, 0);
	VER(vf->img == img0, "la llama empieza en su primer cuadro");
	tz_animar(&e, 130);
	int img1 = vf->img;
	tz_animar(&e, 130 + 125 * 4);
	VER(img1 != img0 && vf->img == img1, "la llama avanza a 8 por segundo (125 ms) y da la vuelta a las 4 filas");
	tz_animar(&e, 0);
	VER(vf->img == img0, "el reloj puede volver atrás: la llama vuelve a su primer cuadro");
	int antes = vp->img;
	tz_animar(&e, 100);
	int d1 = vp->img != antes;
	tz_animar(&e, 200);
	VER(d1 || vp->img != antes, "los peces saltan a un cuadro cada 100 ms");
	VER(flags == 2, "la puerta lleva sus dos banderas del color del reino: %d", flags);
	/* el águila vuela: se mueve, siempre dentro de la franja del mapa y sin salirse de los cuadros */
	const TzVoo *vo = &e.voos[0];
	VER(e.nvoos == 1 && vo->corpo >= 0 && vo->sombra < 0, "un águila en vuelo, sin hoja de sombra");
	int x0 = e.v[vo->corpo].x, y0 = e.v[vo->corpo].y, dentro = 1, movido = 0;
	for (uint64_t ms = 40; ms < 120000; ms += 40) {
		tz_animar(&e, ms);
		const TzVisual *v = &e.v[vo->corpo];
		int cx = v->x - vo->desvio[0], cy = v->y - vo->desvio[1];
		if (cx < -2000 || cx > 40 * 32 || cy < -2000 || cy > 40 * 32) dentro = 0;
		if (v->x != x0 || v->y != y0) movido = 1;
		if (v->img < 0 || v->img >= e.nimgs) dentro = 0;
	}
	VER(movido && dentro, "el águila se mueve (%d,%d) -> (%d,%d) y no se sale", x0, y0, e.v[vo->corpo].x, e.v[vo->corpo].y);
	/* selección */
	int sx, sy, sw, sh;
	VER(tz_selecao_caixa(&e, 0, &sx, &sy, &sw, &sh) && sx == 4 * 32 && sy == 4 * 32 && sw == 32 && sh == 32, "caja de selección del caballero: %d,%d %dx%d", sx, sy, sw, sh);
	VER(tz_selecao_apontar(&e, &m, 4 * 32 + 8, 4 * 32 + 8) == 0, "clic sobre el caballero: lo elige");
	VER(tz_selecao_apontar(&e, &m, 4 * 32 + 30, 4 * 32 + 30) == 0, "clic cerca (alcance 30): lo elige");
	VER(tz_selecao_apontar(&e, &m, 20 * 32, 20 * 32) == -1, "clic en el suelo: nada");
	uint32_t lp[64 * 64] = { 0 };
	TzLienzo L = { lp, 64, 64, 64 };
	tz_marco_selecao(&L, 10, 10, 20, 20, 0xffffff);
	VER(pix(&L, 10, 10) == 0xffffff && pix(&L, 15, 10) == 0xffffff && pix(&L, 16, 10) == 0 && pix(&L, 29, 29) == 0xffffff && pix(&L, 20, 20) == 0, "marco de esquinas de 6 píxeles");
	memset(lp, 0, sizeof lp);
	tz_barra_vida(&L, 0, 30, 1, 50, 100, 0, 0);
	/* 20 de ancho (n = 19 + 1), 6 píxeles a la derecha de la esquina y 24 por encima: verde hasta 10, rojo el resto */
	VER(pix(&L, 6, 6) == 0x00ff00 && pix(&L, 6 + 9, 7) == 0x00ff00 && pix(&L, 6 + 10, 6) == 0xff0000 && pix(&L, 6 + 19, 6) == 0xff0000 && pix(&L, 6 + 20, 6) == 0 && pix(&L, 6, 8) == 0,
	    "barra de vida: verde %06x rojo %06x", pix(&L, 6 + 9, 6), pix(&L, 6 + 10, 6));

	int nv_antes = e.nv;
	/* dibujar objetos con pose y efectos (la simulación los usa así) */
	uint32_t pl[160 * 160];
	TzLienzo P = { pl, 160, 160, 160 };
	memset(pl, 0, sizeof pl);
	TzEstado es;
	memset(&es, 0, sizeof es);
	es.pose = TZ_POSE_ANDAR; es.direcao = 3; es.linha_ponte = -1;
	/* caballero andando mirando a la dirección 3: columna 3 de las 8; a los 250 ms (100 por cuadro) la fila 2 */
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[0], &es, 64, 64, 0, 0, 250) && pix(&P, 70, 70) == (uint32_t)((2 * 60 + 10) << 16 | (3 * 30 + 10) << 8 | 100), "pose andar, dirección 3, 250 ms: fila 2 (%06x)", pix(&P, 70, 70));
	memset(pl, 0, sizeof pl);
	es.desde_ms = 1000;
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[0], &es, 64, 64, 0, 0, 1000 + 400 + 150) && pix(&P, 70, 70) == (uint32_t)((1 * 60 + 10) << 16 | (3 * 30 + 10) << 8 | 100), "el ciclo se repite desde que empezó la pose (150 ms: fila 1)");
	memset(pl, 0, sizeof pl);
	es.pose = TZ_POSE_ATACAR; es.desde_ms = 0;
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[0], &es, 64, 64, 0, 0, 450) && pix(&P, 70, 70) == (uint32_t)(200 << 16 | (2 * 60 + 10) << 8 | (3 * 30 + 10)), "pose atacar (image_attack, 200 ms por cuadro): fila 2 (%06x)", pix(&P, 70, 70));
	memset(pl, 0, sizeof pl);
	es.pose = TZ_POSE_MORRER;
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[0], &es, 64, 64, 0, 0, 100000) && pix(&P, 70, 70) == (uint32_t)((2 * 90 + 10) << 16 | 10 << 8 | (3 * 30 + 10)), "morir: se queda en el último cuadro (%06x)", pix(&P, 70, 70));
	char buf[80];
	es.pose = TZ_POSE_TRABALHAR; es.tarefa = TZ_TAREFA_CORTAR;
	VER(tz_seccao_pose(&b, &m.obj[0], &es, buf, sizeof buf) && !strcmp(buf, "knight_idle"), "sin image_chop: vuelve al reposo (%s)", buf);
	es.pose = TZ_POSE_ATACAR; es.ataque = 1;
	VER(tz_seccao_pose(&b, &m.obj[0], &es, buf, sizeof buf) && !strcmp(buf, "knight_attack"), "sin image_attack2: vuelve a image_attack (%s)", buf);
	memset(pl, 0, sizeof pl);
	memset(&es, 0, sizeof es);
	es.em_obra = 1; es.vida = 50; es.vida_max = 100; es.linha_ponte = -1;
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[5], &es, 32, 32, 0, 0, 0) && pix(&P, 40, 40) == (uint32_t)((50 + 2 * 50) << 16 | 50 << 8 | 50), "obra al 50 %%: fila 2 de build3x3 (%06x)", pix(&P, 40, 40));
	memset(pl, 0, sizeof pl);
	memset(&es, 0, sizeof es);
	es.linha_ponte = 2;
	VER(!tz_dibujar_objeto(&e, &P, &m.obj[4], &es, 0, 0, 0, 0, 0), "puerta con el puente en la fila 2");
	memset(pl, 0, sizeof pl);
	VER(!tz_dibujar_efecto(&e, &P, "fire_small", 100, 100, 0, 0, 0, 250, -1) && pix(&P, 100 - 2 + 8, 100 - 2 + 8) == (uint32_t)(250 << 16 | 80 << 8), "efecto fire_small a 10 por segundo: a los 250 ms, fila 2 (%06x)", pix(&P, 106, 106));
	VER(tz_dibujar_efecto(&e, &P, "no_existe", 0, 0, 0, 0, 0, 0, -1) != 0, "efecto que no existe: error");
	VER(e.nv == nv_antes, "dibujar no deja visuales en la escena (%d)", e.nv);

	/* escena preparada sin montar el mapa (así la usa la simulación) */
	TzEscena e2;
	VER(!tz_escena_preparar(&e2, &b, r, &m) && e2.nv == 0, "escena preparada vacía");
	memset(pl, 0, sizeof pl);
	memset(&es, 0, sizeof es);
	es.linha_ponte = -1; es.direcao = 3;
	VER(!tz_dibujar_objeto(&e2, &P, &m.obj[0], &es, 64, 64, 0, 0, 0) && e2.nv == 0, "dibujar objetos sin escena montada");
	tz_escena_liberar(&e2);
	int hojas, cuadros, falhas, aus;
	char ult[200];
	TzEscena e3;
	VER(!tz_escena_preparar(&e3, &b, r, &m) && !tz_anim_verificar(&e3, &hojas, &cuadros, &falhas, &aus, ult, sizeof ult) && hojas >= 8 && falhas == 0, "todas las hojas animadas se leen: %d hojas, %d cuadros", hojas, cuadros);
	tz_escena_liberar(&e3);
	tz_escena_liberar(&e);
	tz_biblioteca_liberar(&b);
	tz_terreno_liberar(&ter);
	tz_mapa_liberar(&m);
}

/* ------------------------------------------------------------------ sintético */

static int sintetico(const char *raiz)
{
	char ruta[1024];
	uint32_t n;
	/* LZSS: el generador deja un fichero con su texto original al lado */
	snprintf(ruta, sizeof ruta, "%s/prueba/lzss.bin", raiz);
	uint8_t *comp = ler_fich(ruta, &n);
	snprintf(ruta, sizeof ruta, "%s/prueba/lzss.txt", raiz);
	uint32_t nt;
	uint8_t *txt = ler_fich(ruta, &nt);
	uint8_t *out = NULL;
	uint32_t no = 0;
	VER(comp && txt && !tz_lzss_desempacotar(comp, n, &out, &no) && no == nt && !memcmp(out, txt, nt), "LZSS por trozos (literales, referencias y ceros): %u bytes", nt);
	free(out);
	if (comp && n > 40) {
		/* un trozo truncado se rechaza sin salirse del búfer */
		out = NULL;
		VER(tz_lzss_desempacotar(comp, n - 7, &out, &no) != 0, "LZSS truncado rechazado");
		free(out);
	}
	free(comp); free(txt);

	TzRecursos r;
	VER(!tz_recursos_abrir(&r, raiz), "paquetes de la instalación sintética");
	if (fallos) return 1;
	VER(r.npac == 4, "cuatro paquetes (carpeta suelta, data, images, terrains): %d", r.npac);
	/* images.wdt sustituye a data.wdt en SOBRE.TXT; la carpeta suelta solo completa */
	uint8_t *d = tz_recursos_ler(&r, "data\\sobre.txt", &n);
	VER(d && n == 6 && !memcmp(d, "images", 6), "el paquete posterior gana: %.*s", d ? (int)n : 0, d ? (char *)d : "");
	free(d);
	d = tz_recursos_ler(&r, "DATA/SUELTO.TXT", &n);
	VER(d && n == 6 && !memcmp(d, "suelto", 6), "carpeta suelta DATA");
	free(d);
	VER(!tz_recursos_existe(&r, "DATA/NOEXISTE.INI"), "recurso que no existe");

	TzIni ini = { 0 };
	VER(!tz_ini_recurso(&r, &ini, "DATA/IMAGES.INI"), "IMAGES.INI");
	VER(tz_ini(&ini, "PEON_IDLE", "IMAGE") && !strcmp(tz_ini(&ini, "peon_idle", "image"), "units\\peon.bmp"), "INI sin mayúsculas y con rutas de Windows");
	int v[4];
	VER(tz_ini_numeros(&ini, "peon_idle", "object", v, 4) == 4 && v[0] == 8 && v[1] == 40 && v[2] == 32 && v[3] == 32, "números del INI");
	tz_ini_liberar(&ini);

	/* BMP de 16 bits (RGB555) e indexado con el color del reino */
	d = tz_recursos_ler(&r, "IMAGES/UNITS/PEON.BMP", &n);
	TzImg im;
	VER(d && !tz_bmp(d, n, 1, -1, &im) && im.w == 48 && im.h == 48, "BMP indexado de 48x48");
	if (d && im.px) {
		VER(im.px[0] == 0, "verde puro transparente");
		VER(im.px[20 * 48 + 20] == 0xffe0e0e0u, "gris 240 sin color de reino: %08x", im.px[20 * 48 + 20]);
		tz_img_liberar(&im);
		VER(!tz_bmp(d, n, 1, 1, &im) && im.px[20 * 48 + 20] == 0xff800000u, "gris 240 en el color 1 (rampa 128): %08x", im.px ? im.px[20 * 48 + 20] : 0);
		tz_img_liberar(&im);
		VER(!tz_bmp_recorte(d, n, 16, 16, 16, 16, 1, -1, &im) && im.w == 16 && im.px[4 * 16 + 4] == 0xffe0e0e0u, "recorte del BMP");
		tz_img_liberar(&im);
		VER(tz_bmp_recorte(d, n, 40, 40, 16, 16, 1, -1, &im) != 0, "recorte fuera del BMP rechazado");
		/* dañado: paleta más grande que el hueco */
		uint8_t *m2 = malloc(n);
		memcpy(m2, d, n);
		m2[46] = 0x00; m2[47] = 0x02;
		VER(tz_bmp(m2, n, 1, -1, &im) != 0, "paleta de BMP no válida rechazada");
		free(m2);
	}
	free(d);

	/* RLE: dos direcciones x tres cuadros, formato 3 (RGB555) */
	d = tz_recursos_ler(&r, "IMAGES/UNITS/KNIGHT.RLE", &n);
	TzRle h;
	VER(d && !tz_rle_cab(d, n, &h) && h.cols == 2 && h.rows == 3 && h.fh == 10 && h.w == 12, "cabecera RLE");
	VER(d && !tz_rle_validar(d, n), "RLE entero válido");
	if (d) {
		/* cada cuadro lleva un píxel en la columna 1 + 3*dirección + cuadro, rojo puro */
		int ok = 1;
		for (int c = 0; c < 2; ++c)
			for (int f = 0; f < 3; ++f) {
				if (tz_rle_quadro(d, n, c, f, -1, &im)) { ok = 0; continue; }
				int x = 1 + c * 3 + f;
				ok &= im.px[5 * 12 + x] == 0xffff0000u && im.px[5 * 12 + x + 1] == 0 && im.px[0] == 0;
				tz_img_liberar(&im);
			}
		VER(ok, "cuadros del RLE en su sitio (dirección antes que tiempo)");
		uint8_t *m2 = malloc(n);
		memcpy(m2, d, n);
		uint32_t base = 38 + 0;
		uint32_t rel = tz_u32(m2 + base);
		m2[base + rel] = 200;   /* salto que se sale de la fila */
		VER(tz_rle_quadro(m2, n, 0, 0, -1, &im) != 0, "tramo RLE fuera de la fila rechazado");
		free(m2);
	}
	free(d);
	d = tz_recursos_ler(&r, "IMAGES/UNITS/KNIGHT_S.RLE", &n);
	VER(d && !tz_rle_quadro(d, n, 0, 0, -1, &im) && im.px[2 * 12 + 3] == TZ_SOMBRA && im.px[0] == 0, "RLE de sombra (formato 7)");
	tz_img_liberar(&im);
	free(d);

	/* mapa HPFS (con bloques desordenados) y campaña con el mismo mapa anidado */
	snprintf(ruta, sizeof ruta, "%s/Maps/PRUEBA.wmp", raiz);
	TzHpfs c;
	VER(!tz_hpfs_abrir(&c, ruta), "contenedor HPFS");
	TzMapa m;
	TzEscena e;
	memset(&e, 0, sizeof e);
	memset(&m, 0, sizeof m);
	if (!fallos) {
		VER(c.n >= 4 && tz_hpfs_entrada(&c, "maps/prueba.HEADER"), "directorio HPFS (%d entradas)", c.n);
		VER(!mapa_e_escena(&r, &c, "PRUEBA.wmp", 1, &m, &e), "mapa, terreno y escena");
		VER(m.lado == 32 && !strcmp(m.bioma, "grass") && m.nobj == 7 && m.nreinos == 8, "cabecera: lado %d bioma %s objetos %d", m.lado, m.bioma, m.nobj);
		VER(m.nobj > 0 && m.obj[0].classe == 1 && m.obj[0].x == 4 && m.obj[0].y == 5 && m.obj[0].dono == 1, "primer objeto: peón del reino 1 en (4,5)");
		VER(tz_mapa_cor(&m, 1) == 2 && tz_mapa_cor(&m, 0) == -1, "color del reino 1 = 3 del registro");
		VER(e.nv > 0 && e.sem_imagem == 1 && e.falhas == 0, "escena: %d visuales, %d sin dibujo, %d fallos", e.nv, e.sem_imagem, e.falhas);
		/* el orden del pintor: dentro del grupo general, por fila */
		int ok = 1;
		for (int i = 1; i < e.nv; ++i) {
			const TzVisual *a = &e.v[i - 1], *b = &e.v[i];
			int ga = a->chao == 1 ? 0 : a->chao == 2 ? 1 : 2, gb = b->chao == 1 ? 0 : b->chao == 2 ? 1 : 2;
			if (ga > gb || (ga == gb && a->prioridade > b->prioridade) || (ga == gb && a->prioridade == b->prioridade && a->profundidade > b->profundidade)) ok = 0;
		}
		VER(ok, "orden del pintor");
		TzTerreno t;
		VER(!tz_terreno_carregar(&t, &r, "grass"), "terreno grass");
		int paso;
		/* casilla de transición de hierba (1) a agua (0): la máscara mitad magenta mitad verde */
		const uint32_t *px = tz_terreno_loseta(&t, (uint16_t)(1 << 7 | 0 << 4 | 2), 3, 3, &paso);
		VER(px && paso == 32 && px[5 * 32 + 2] == 0xff20a020u && px[5 * 32 + 30] == 0xff2040c0u, "transición compuesta: %08x %08x",
		    px ? px[5 * 32 + 2] : 0, px ? px[5 * 32 + 30] : 0);
		const uint32_t *px2 = tz_terreno_loseta(&t, (uint16_t)(1 << 7 | 0 << 4 | 2), 3, 3, &paso);
		VER(px2 == px, "transición guardada en la caché");
		uint32_t mini[32 * 32];
		VER(!tz_terreno_minimapa(&t, &m, mini) && mini[0] == 0xff2040c0u && mini[10 * 32 + 10] == 0xff0b830bu, "minimapa: %08x %08x", mini[0], mini[10 * 32 + 10]);
		tz_terreno_liberar(&t);
		tz_escena_liberar(&e);
		tz_mapa_liberar(&m);
	}
	tz_hpfs_fechar(&c);
	snprintf(ruta, sizeof ruta, "%s/Campaign/PRUEBA.wcm", raiz);
	VER(!tz_hpfs_abrir(&c, ruta), "campaña HPFS");
	if (c.n) {
		TzHpfs sub;
		VER(!tz_hpfs_anidado(&c, "MAPS/Mision 1.wmp", &sub), "mapa anidado en la campaña");
		VER(!tz_mapa_ler(&sub, &m) && m.lado == 32 && m.nobj == 7, "mapa anidado leído sin copiar la campaña");
		tz_mapa_liberar(&m);
		tz_hpfs_fechar(&sub);
	}
	tz_hpfs_fechar(&c);
	/* HPFS dañado: un directorio que se contiene a sí mismo */
	snprintf(ruta, sizeof ruta, "%s/prueba/ciclo.wmp", raiz);
	VER(tz_hpfs_abrir(&c, ruta) != 0 && strstr(tz_erro(), "ciclo"), "ciclo en el directorio HPFS rechazado");
	snprintf(ruta, sizeof ruta, "%s/prueba/ruta_mala.wdt", raiz);
	TzPacote p;
	VER(tz_pacote_abrir(&p, ruta) != 0 && strstr(tz_erro(), "peligrosa"), "nombre de entrada con «..» rechazado");
	fase3(&r, raiz);
	tz_recursos_fechar(&r);
	printf("%s sintético: %d fallos\n", fallos ? "FALLO" : "OK", fallos);
	return fallos != 0;
}

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "privado")) return privado(argv[2]);
	if (argc == 3 && !strcmp(argv[1], "sintetico")) return sintetico(argv[2]);
	fprintf(stderr, "uso: %s sintetico|privado <carpeta>\n", argv[0]);
	return 2;
}

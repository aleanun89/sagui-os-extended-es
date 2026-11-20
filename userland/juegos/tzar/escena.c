/* SPDX-License-Identifier: GPL-3.0-or-later */
/* escena.c - qué dibujo lleva cada objeto del mapa, dónde va y en qué orden se pinta (D417).
 *
 * Traducido de ReTzar, src/world.rs: ObjectLibrary (final_image, property, runtime_image, first_row,
 * anchor, place), Builder (object, gate, sheet, frame), presentation_priority, painter_depths y depth,
 * add_trees y add_cliffs (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later).
 *
 * Fase 3 (animación): además del montaje en reposo, cada visual que se anima lleva su TzAnim (cuadros y tiempos de
 * ReTzar, world.rs: ObjectLibrary::timeline y Builder::object/gate/fish/flight, GATE_FLAGS, object_noise) y
 * tz_animar() pone el cuadro del instante; tz_dibujar_objeto() y tz_dibujar_efecto() dibujan un objeto en una pose
 * (andar, atacar, morir, trabajar, obras) o un efecto suelto, para que la simulación no dependa de cómo se dibuja.
 * Coordenadas con y hacia abajo (ReTzar usa y hacia arriba): la casilla (x, y) empieza en (32x, 32y).
 */
#include "tzar.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum {
	C_TROLL_BAT = 15, C_CARPET = 22, C_DRAGON = 32, C_WALL_STONE = 61, C_WALL_WOOD = 62, C_FIELD = 63, C_BRIDGE = 64,
	C_GATE_WOOD = 67, C_GATE_STONE = 68, C_RES_GOLD = 76, C_RES_STONE = 78, C_ITEM = 79, C_ITEM_PLUSPLUS = 81, C_ANIMATION = 82,
	C_DEBRIS = 87, C_ITEM_SCROLL = 103, C_ITEM_MAGIC = 104, C_ITEM_HEALTH = 106, C_GROUND_OBJECT = 107, C_GROUND_PATCH = 108,
	C_BIRD = 112, C_FISHPATCH = 115, C_EAGLE = 119, C_DUMMY = 133, C_ITEM_FLAG = 135, C_HARBOR = 54,
};
#define FLAG_INACTIVO 0x4000u
#define FLAG_MUERTO 0x2u

/* ------------------------------------------------------------------ biblioteca */

int tz_biblioteca_carregar(TzBiblioteca *b, TzRecursos *r, const char *bioma)
{
	memset(b, 0, sizeof *b);
	if (tz_ini_recurso(r, &b->imagens, "DATA/IMAGES.INI")) return -1;
	if (tz_recursos_existe(r, "IMAGES/TERRAIN/COMMON.IMAGES.INI") && tz_ini_recurso(r, &b->imagens, "IMAGES/TERRAIN/COMMON.IMAGES.INI")) return -1;
	char nome[300];
	snprintf(nome, sizeof nome, "IMAGES/TERRAIN/%s.IMAGES.INI", bioma);
	if (tz_recursos_existe(r, nome) && tz_ini_recurso(r, &b->imagens, nome)) return -1;
	int cap = 0;
	for (int i = 0; i < r->nnomes; ++i) {
		const char *n = r->nomes[i];
		size_t l = strlen(n);
		if (strncmp(n, "DATA/OBJECTS/", 13) || l < 18 || strcmp(n + l - 4, ".INI") || strchr(n + 13, '/')) continue;
		TzIni ini = { 0 };
		if (tz_ini_recurso(r, &ini, n)) continue;
		const char *c = tz_ini(&ini, "general", "class");
		if (!c) { tz_ini_liberar(&ini); continue; }
		char cl[80];
		snprintf(cl, sizeof cl, "%s", c);
		for (char *q = cl; *q; ++q) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
		if (b->nobjdefs == cap) {
			cap = cap ? cap * 2 : 64;
			TzIni *nd = realloc(b->objdefs, (size_t)cap * sizeof *nd);
			char **nc = realloc(b->objclasse, (size_t)cap * sizeof *nc);
			if (nd) b->objdefs = nd;
			if (nc) b->objclasse = nc;
			if (!nd || !nc) { tz_ini_liberar(&ini); return tz_falha("sin memoria"); }
		}
		/* una clase repetida: se queda la última, como el BTreeMap de ReTzar */
		int at = -1;
		for (int k = 0; k < b->nobjdefs; ++k) if (!strcmp(b->objclasse[k], cl)) at = k;
		if (at >= 0) { tz_ini_liberar(&b->objdefs[at]); b->objdefs[at] = ini; continue; }
		b->objdefs[b->nobjdefs] = ini;
		b->objclasse[b->nobjdefs] = tz_strdup(cl);
		b->nobjdefs++;
	}
	return 0;
}

void tz_biblioteca_liberar(TzBiblioteca *b)
{
	tz_ini_liberar(&b->imagens);
	for (int i = 0; i < b->nobjdefs; ++i) { tz_ini_liberar(&b->objdefs[i]); free(b->objclasse[i]); }
	free(b->objdefs);
	free(b->objclasse);
	memset(b, 0, sizeof *b);
}

static const TzIni *definicao(const TzBiblioteca *b, int classe)
{
	if (classe < 0 || classe >= 144) return NULL;
	for (int i = 0; i < b->nobjdefs; ++i)
		if (!strcmp(b->objclasse[i], tz_classes[classe])) return &b->objdefs[i];
	return NULL;
}

static const char *secao_civ(int civ) { return civ == 2 ? "arab" : civ == 3 ? "asia" : "euro"; }

static const char *propriedade(const TzBiblioteca *b, const TzObjeto *o, const char *chave)
{
	const TzIni *d = definicao(b, o->classe);
	if (!d) return NULL;
	const char *v = tz_ini(d, secao_civ(o->civ), chave);
	if (!v) v = tz_ini(d, "comm", chave);
	return v && *v ? v : NULL;
}

static int tem_imagem(const TzBiblioteca *b, const char *secao) { return secao && tz_ini(&b->imagens, secao, "image") != NULL; }

static unsigned palavra(const TzObjeto *o, uint32_t at) { return at + 2 <= o->ndados ? tz_u16(o->dados + at) : 0; }
static unsigned byte_(const TzObjeto *o, uint32_t at) { return at < o->ndados ? o->dados[at] : 0; }
static uint32_t dpalavra(const TzObjeto *o, uint32_t at) { return at + 4 <= o->ndados ? tz_u32(o->dados + at) : 0; }
static int muro_danado(const TzObjeto *o) { return (o->classe == C_WALL_STONE || o->classe == C_WALL_WOOD) && o->vida_max > 0 && o->vida < o->vida_max; }

/* El dibujo del objeto terminado (ReTzar: final_image). Devuelve un puntero a un búfer estático o al INI. */
static const char *imagem_final(const TzBiblioteca *b, const TzObjeto *o)
{
	static char buf[80];
	if (o->classe == C_WALL_STONE || o->classe == C_WALL_WOOD) {
		unsigned m = palavra(o, 56);
		snprintf(buf, sizeof buf, "walls_%s_%s%s", o->classe == C_WALL_STONE ? "stone" : "wood",
		         m >= 24 ? "vert" : m >= 16 ? "horz" : "tower", muro_danado(o) ? "_dest" : "");
		return buf;
	}
	if (o->classe == C_BRIDGE) return byte_(o, 56) == 0 ? "bridge_horz" : "bridge_vert";
	if (o->classe == C_DUMMY) {
		snprintf(buf, sizeof buf, "dummy_%u", o->variante ? o->variante : 1);
		if (tem_imagem(b, buf)) return buf;
	}
	/* un campo sin toda la vida muestra cómo crece */
	if (o->classe == C_FIELD && o->vida < o->vida_max && tem_imagem(b, "field_grow")) return "field_grow";
	if (o->imagem[0] && tem_imagem(b, o->imagem)) return o->imagem;
	const TzIni *d = definicao(b, o->classe);
	if (!d) return NULL;
	static const char *const civs[] = { NULL, "comm", "euro", "asia", "arab" };
	static const char *const chaves[] = { "image_idle", "image" };
	for (int k = 0; k < 2; ++k)
		for (int c = 0; c < 5; ++c) {
			const char *v = tz_ini(d, c ? civs[c] : secao_civ(o->civ), chaves[k]);
			if (v) return *v ? v : NULL;
		}
	return NULL;
}

const char *tz_imagem_objeto(const TzBiblioteca *b, const TzObjeto *o)
{
	if (tz_classe_e_unidade(o->classe)) {
		const char *v = propriedade(b, o, "image_idle");
		if (v) return v;
	}
	return imagem_final(b, o);
}

/* ------------------------------------------------------------------ imágenes de la escena */

typedef struct { char *nome; uint8_t *d; uint32_t n; } Arquivo;
typedef struct {
	TzEscena *e;
	TzBiblioteca *b;
	TzRecursos *r;
	const TzMapa *m;
	Arquivo arq[64];
	int narq, proximo;
	uint32_t *hashes;
} Montador;

static const uint8_t *arquivo(Montador *mt, const char *nome, uint32_t *n)
{
	for (int i = 0; i < mt->narq; ++i)
		if (!strcmp(mt->arq[i].nome, nome)) { *n = mt->arq[i].n; return mt->arq[i].d; }
	uint8_t *d = tz_recursos_ler(mt->r, nome, n);
	if (!d) return NULL;
	int at;
	if (mt->narq < 64) at = mt->narq++;
	else { at = mt->proximo; mt->proximo = (mt->proximo + 1) % 64; free(mt->arq[at].nome); free(mt->arq[at].d); }
	mt->arq[at].nome = tz_strdup(nome);
	mt->arq[at].d = d;
	mt->arq[at].n = *n;
	if (!mt->arq[at].nome) { free(d); mt->arq[at].d = NULL; mt->narq = at; return NULL; }
	return d;
}

/* La hoja de una sección: el fichero (BMP o el RLE del mismo nombre) y su rejilla. */
typedef struct { char arq[200]; int rle, cols, rows, indexado; } Folha;

static int folha(Montador *mt, const char *secao, Folha *f)
{
	const char *img = tz_ini(&mt->b->imagens, secao, "image");
	if (!img) return tz_falha("%s: no está en IMAGES.INI", secao);
	char ruta[200];
	snprintf(ruta, sizeof ruta, "IMAGES/%s", img);
	for (char *q = ruta; *q; ++q) if (*q == '\\') *q = '/';
	if (strstr(ruta, "..")) return tz_falha("%s: ruta no válida", secao);
	memset(f, 0, sizeof *f);
	snprintf(f->arq, sizeof f->arq, "%s", ruta);
	if (!tz_recursos_existe(mt->r, ruta)) {
		size_t l = strlen(ruta);
		if (l > 4 && !strcasecmp(ruta + l - 4, ".bmp")) ruta[l - 4] = 0;
		snprintf(f->arq, sizeof f->arq, "%s.RLE", ruta);
		f->rle = 1;
	}
	uint32_t n;
	const uint8_t *d = arquivo(mt, f->arq, &n);
	if (!d) return -1;
	if (f->rle || !(n >= 2 && d[0] == 'B' && d[1] == 'M')) {
		TzRle h;
		if (tz_rle_cab(d, n, &h)) return -1;
		f->rle = 1;
		f->cols = h.cols; f->rows = h.rows;
		f->indexado = h.formato == 1 && h.cores == 256;
	} else {
		int w, hh;
		if (tz_bmp_info(d, n, &w, &hh, NULL)) return -1;
		const char *c = tz_ini(&mt->b->imagens, secao, "cols"), *rr = tz_ini(&mt->b->imagens, secao, "rows");
		f->cols = c ? atoi(c) : 1;
		f->rows = rr ? atoi(rr) : 1;
		if (f->cols <= 0 || f->rows <= 0 || f->cols > w || f->rows > hh) return tz_falha("%s: rejilla %dx%d no válida", secao, f->cols, f->rows);
		f->indexado = tz_bmp_indexado(d, n);
	}
	return 0;
}

/* Añade una imagen a la escena (la clave puede ser NULL: no se busca después). Libera `img` si falla. */
static int adicionar(Montador *mt, TzImg img, const char *chave, uint32_t h)
{
	TzEscena *e = mt->e;
	if (e->nimgs == e->capimgs) {
		int nc = e->capimgs ? e->capimgs * 2 : 256;
		TzImg *ni = realloc(e->imgs, (size_t)nc * sizeof *ni);
		if (ni) e->imgs = ni;
		char **nk = ni ? realloc(e->chaves, (size_t)nc * sizeof *nk) : NULL;
		if (nk) e->chaves = nk;
		uint32_t *nh = nk ? realloc(mt->hashes, (size_t)nc * sizeof *nh) : NULL;
		if (nh) mt->hashes = nh;
		if (!ni || !nk || !nh) { tz_img_liberar(&img); return tz_falha("sin memoria"); }
		e->capimgs = nc;
	}
	e->imgs[e->nimgs] = img;
	e->chaves[e->nimgs] = chave ? tz_strdup(chave) : NULL;
	mt->hashes[e->nimgs] = h;
	return e->nimgs++;
}

/* El cuadro (col, row) de una hoja en el color `slot`, una sola vez por escena. Devuelve su índice o -1. */
static int quadro(Montador *mt, const Folha *f, int col, int row, int slot)
{
	TzEscena *e = mt->e;
	if (!f->indexado) slot = -1;
	char chave[260];
	snprintf(chave, sizeof chave, "%s|%d|%d|%d", f->arq, slot, col, row);
	uint32_t h = tz_hash(chave);
	for (int i = 0; i < e->nimgs; ++i)
		if (mt->hashes[i] == h && e->chaves[i] && !strcmp(e->chaves[i], chave)) return i;
	uint32_t n;
	const uint8_t *d = arquivo(mt, f->arq, &n);
	if (!d) return -1;
	TzImg img;
	int r;
	if (f->rle) r = tz_rle_quadro(d, n, col, row, slot, &img);
	else {
		int w, hh;
		if (tz_bmp_info(d, n, &w, &hh, NULL)) return -1;
		int fw = w / f->cols, fh = hh / f->rows;
		r = tz_bmp_recorte(d, n, col * fw, row * fh, fw, fh, 1, slot, &img);
	}
	if (r) return -1;
	return adicionar(mt, img, chave, h);
}

static int img_direta(Montador *mt, TzImg img) { return adicionar(mt, img, NULL, 0); }

static int visual(TzEscena *e, int img, int x, int y, int prio, int capa, int chao, float prof)
{
	if (e->nv == e->capv) {
		int nc = e->capv ? e->capv * 2 : 1024;
		TzVisual *nv = realloc(e->v, (size_t)nc * sizeof *nv);
		if (!nv) return tz_falha("sin memoria");
		e->v = nv; e->capv = nc;
	}
	TzVisual *v = &e->v[e->nv];
	v->img = img; v->x = x; v->y = y; v->prioridade = prio; v->capa = capa; v->chao = chao; v->profundidade = prof;
	v->obj = -1; v->anim = -1; v->voo = -1;
	memset(v->sel, 0, sizeof v->sel);
	v->ordem = e->nv++;
	return 0;
}

/* Lugar en el orden del pintor de algo cuya casilla empieza en (x, y): las filas de más abajo delante y, en
 * la misma fila, de derecha a izquierda (el término de la columna no llega a media fila). */
float tz_profundidade(int x, int y, int lado)
{
	float borde = (float)(lado > 0 ? lado : 1) * 32.f;
	float fila = (float)y / borde, coluna = (float)x / borde;
	if (fila < 0) fila = 0;
	if (fila > 1) fila = 1;
	if (coluna < 0) coluna = 0;
	if (coluna > 1) coluna = 1;
	return 10.f + fila * 100.f + (1.f - coluna) * 50.f / (float)(lado > 0 ? lado : 1);
}

static int prioridade(const TzObjeto *o)
{
	if (o->flags & FLAG_MUERTO) return 1;   /* una mina agotada queda en el suelo */
	switch (o->classe) {
	case C_TROLL_BAT: case C_CARPET: case C_DRAGON: case C_BIRD: case C_EAGLE: return 300;
	case C_FIELD: return 50;
	case C_ITEM: case C_ITEM_PLUSPLUS: case C_ITEM_SCROLL: case C_ITEM_MAGIC: case C_ITEM_HEALTH: return 81;
	case C_GROUND_PATCH: case C_DEBRIS: return 1;
	default: return 100;
	}
}

/* Desplazamiento de la sombra de los que vuelan. */
static void sombra_elevada(int classe, int *dx, int *dy)
{
	*dx = *dy = 0;
	if (classe == 15 || classe == 22) { *dx = 20; *dy = 35; }
	else if (classe == 32 || classe == C_EAGLE) { *dx = 40; *dy = 70; }
}

static int primeira_fila(Montador *mt, const TzObjeto *o, const char *secao, int rows)
{
	if (muro_danado(o)) {
		int r = rows - (int)((unsigned)o->vida * (unsigned)rows / o->vida_max) - 1;
		return r < 0 ? 0 : r;
	}
	if (o->classe == C_FIELD && !strcmp(secao, "field_grow")) {
		int r = (int)((unsigned)o->vida * (unsigned)rows / (o->vida_max ? o->vida_max : 1));
		return r < rows - 1 ? r : rows - 1;
	}
	if (o->classe == C_DEBRIS) return (int)o->variante < rows - 1 ? (int)o->variante : rows - 1;
	if ((o->classe == C_RES_GOLD || o->classe == C_RES_STONE) && rows > 1) {
		/* filas 0-2: tres aspectos de la mina llena; la última, el hoyo agotado */
		if (dpalavra(o, 0x3a) == 0 && byte_(o, 0x3f) == 0) return rows - 1;
		int m = rows - 1 < 3 ? rows - 1 : 3;
		return (int)(o->variante % (unsigned)m);
	}
	const char *s = tz_ini(&mt->b->imagens, secao, "startrow");
	int r = s ? atoi(s) : 0;
	if (r < 0) r = 0;
	return r < rows - 1 ? r : (rows > 0 ? rows - 1 : 0);
}

/* El punto del objeto en el mundo: la esquina de su casilla, o el píxel exacto de los parches de suelo y de
 * las animaciones del mapa que tienen punto caliente. */
static void ancora(Montador *mt, const TzObjeto *o, const char *secao, int *x, int *y)
{
	if (o->classe == C_GROUND_PATCH && o->x == 0 && o->y == 0 && o->ndados >= 0x6a) {
		int px = (int16_t)palavra(o, 0x66), py = (int16_t)palavra(o, 0x68);
		*x = px > 0 ? px : 0; *y = py > 0 ? py : 0;
		return;
	}
	if (o->classe == C_ANIMATION || o->classe == C_GROUND_OBJECT) {
		int hot[2], rect[2] = { 0, 0 };
		if (tz_ini_numeros(&mt->b->imagens, secao, "hotspot", hot, 2) >= 2 && hot[0] && hot[1]) {
			tz_ini_numeros(&mt->b->imagens, secao, "object", rect, 2);
			*x = rect[0] + (int16_t)palavra(o, 0x66) - hot[0];
			*y = rect[1] + (int16_t)palavra(o, 0x68) - hot[1];
			return;
		}
	}
	*x = o->x * 32;
	*y = o->y * 32;
}

/* Esquina y tamaño del dibujo y su caja de selección (`selection`, por defecto todo el dibujo), relativos a (ox, oy). */
static void caixa(Montador *mt, const char *secao, int *ox, int *oy, int *fw, int *fh, int sel[4])
{
	int r[4] = { 0, 0, 32, 32 }, h[4] = { 0, 0, 0, 0 };
	tz_ini_numeros(&mt->b->imagens, secao, "object", r, 4);
	int nh = tz_ini_numeros(&mt->b->imagens, secao, "selection", h, 4);
	*ox = r[0]; *oy = r[1]; *fw = r[2] > 0 ? r[2] : 1; *fh = r[3] > 0 ? r[3] : 1;
	sel[0] = nh > 0 ? h[0] : r[0];
	sel[1] = nh > 1 ? h[1] : r[1];
	sel[2] = nh > 2 && h[2] > 0 ? h[2] : *fw;
	sel[3] = nh > 3 && h[3] > 0 ? h[3] : *fh;
}

static int falha_objeto(Montador *mt, const TzObjeto *o, const char *secao)
{
	mt->e->falhas++;
	snprintf(mt->e->ultima_falha, sizeof mt->e->ultima_falha, "%s #%u (%s): %s", tz_classes[o->classe], o->id, secao, tz_erro());
	return 0;
}

/* object_noise de ReTzar (world.rs): número repetible de cada objeto, en lugar de los sorteos del original. */
static uint32_t ruido(uint16_t id, uint32_t sal)
{
	uint32_t x = (uint32_t)id ^ (sal * 0x9E3779B9u);
	x = (x ^ (x >> 16)) * 0x7FEB352Du;
	x = (x ^ (x >> 15)) * 0x846CA68Bu;
	return x ^ (x >> 16);
}

/* ---- animaciones de la escena */

static int nova_anim(Montador *mt, TzTempo *t, const Folha *f, int col, int slot, uint64_t fase)
{
	TzEscena *e = mt->e;
	int *q = tz_malloc((size_t)f->rows * sizeof *q);
	if (!q) { tz_tempo_liberar(t); return -1; }
	for (int k = 0; k < f->rows; ++k) q[k] = quadro(mt, f, col, k, slot);
	if (e->nanims == e->capanims) {
		int nc = e->capanims ? e->capanims * 2 : 64;
		TzAnim *na = realloc(e->anims, (size_t)nc * sizeof *na);
		if (!na) { free(q); tz_tempo_liberar(t); return -1; }
		e->anims = na; e->capanims = nc;
	}
	TzAnim *a = &e->anims[e->nanims];
	a->t = *t; a->q = q; a->nq = f->rows; a->fase = fase;
	memset(t, 0, sizeof *t);
	return e->nanims++;
}

static int copiar_tempo(TzTempo *dst, const TzTempo *src)
{
	memset(dst, 0, sizeof *dst);
	dst->fins = tz_malloc((size_t)src->n * sizeof *dst->fins);
	if (!dst->fins) return -1;
	memcpy(dst->fins, src->fins, (size_t)src->n * sizeof *dst->fins);
	dst->n = src->n; dst->duracao = src->duracao;
	return 0;
}

/* Cómo se mueve un objeto parado, por su clase (ReTzar: ObjectLibrary::timeline): las unidades repiten su hoja de
 * reposo, las animaciones del mapa avanzan a su fps (la palabra 0x58 del registro: 0 y 10 son el de la imagen),
 * lo demás (edificios, recursos, objetos, muros) se queda en un cuadro. 0 = sin animación. */
static int tempo_do_objeto(Montador *mt, const TzObjeto *o, const char *secao, int cols, int rows, TzTempo *t)
{
	if (rows <= 1) return 0;
	const TzIni *ini = &mt->b->imagens;
	switch (o->classe) {
	case C_ITEM_FLAG: return tz_tempo_passos(t, ini, secao, rows, 0) == 0;
	case C_ANIMATION: case C_GROUND_OBJECT: case C_GROUND_PATCH: {
		const char *own = tz_ini(ini, secao, "fps");
		unsigned fps = own ? (unsigned)atoi(own) : 0, w = palavra(o, 0x58);
		if (w != 0 && w != 10) fps = w;
		return fps > 0 && tz_tempo_uniforme(t, rows, 1000u / fps) == 0;
	}
	default:
		if (!tz_classe_e_unidade(o->classe)) return 0;
		if (cols > 1) return tz_tempo_direcional(t, ini, secao, rows) == 0;
		return tz_tempo_passos(t, ini, secao, rows, 0) == 0;
	}
}

/* ---- composición de un objeto (estático o con pose) */

typedef struct {
	int ax, ay;          /* punto del objeto en el mundo (-1000000 = el de siempre) */
	int col, fila;       /* -1 = los de siempre */
	int anima;           /* 1 = al montar la escena: crea las animaciones */
	const uint64_t *ms;  /* con pose: instante para los estandartes animados (sin animación propia) */
	int obra;            /* sección de obra: el estandarte es «<sección>_c» */
} Opcoes;

#define SEM_PONTO (-1000000)

static int objeto(Montador *mt, const TzObjeto *o, const char *secao, int slot, const Opcoes *op)
{
	Folha f;
	if (folha(mt, secao, &f)) return falha_objeto(mt, o, secao);
	int col = op->col >= 0 ? op->col % f.cols : tz_classe_e_unidade(o->classe) && (o->variante & 0x8000) ? (o->variante & 7) * f.cols / 8 : o->variante % f.cols;
	int ax, ay, ox, oy, fw, fh, sel[4];
	ancora(mt, o, secao, &ax, &ay);
	if (op->ax != SEM_PONTO) { ax = op->ax; ay = op->ay; }
	caixa(mt, secao, &ox, &oy, &fw, &fh, sel);
	int tx = ax - ox, ty = ay - oy;
	int fila = op->fila >= 0 ? (op->fila < f.rows ? op->fila : f.rows - 1) : primeira_fila(mt, o, secao, f.rows);
	int prio = prioridade(o), chao = o->classe == C_GROUND_PATCH ? 1 : 0;
	float prof = tz_profundidade(ax, ay, mt->m->lado);
	int img = quadro(mt, &f, col, fila, slot);
	if (img < 0) return falha_objeto(mt, o, secao);
	/* animación del cuerpo: las unidades empiezan en un punto al azar de su ciclo (id), las animaciones del mapa a
	 * la vez y en su fila de salida */
	TzTempo tp = { 0 };
	int anima = op->anima && tempo_do_objeto(mt, o, secao, f.cols, f.rows, &tp);
	uint64_t fase = 0;
	if (anima) {
		int escenario = o->classe == C_ANIMATION || o->classe == C_GROUND_OBJECT || o->classe == C_GROUND_PATCH;
		fase = (escenario ? 0 : (uint64_t)o->id * 2654435761ull % tp.duracao) + tz_tempo_inicio(&tp, fila);
	}
	/* sombra: hoja «<sección>_s», el mismo lienzo que el cuerpo; en la misma rejilla sigue su cuadro */
	char ss[120];
	snprintf(ss, sizeof ss, "%s_s", secao);
	if (tem_imagem(mt->b, ss)) {
		Folha fs;
		if (!folha(mt, ss, &fs)) {
			int igual = fs.rows == f.rows;
			int sc = col % fs.cols;
			int si = quadro(mt, &fs, sc, igual ? fila : 0, -1);
			int dx, dy;
			sombra_elevada(o->classe, &dx, &dy);
			if (si >= 0) {
				if (visual(mt->e, si, tx + dx, ty + dy, prio, TZ_CAPA_SOMBRA, chao, prof)) { tz_tempo_liberar(&tp); return -1; }
				mt->e->v[mt->e->nv - 1].obj = (int)(o - mt->m->obj);
				if (anima && igual) {
					TzTempo c2;
					if (!copiar_tempo(&c2, &tp)) mt->e->v[mt->e->nv - 1].anim = nova_anim(mt, &c2, &fs, sc, -1, fase);
				}
			}
		}
	}
	if (visual(mt->e, img, tx, ty, prio, TZ_CAPA_CORPO, chao, prof)) { tz_tempo_liberar(&tp); return -1; }
	TzVisual *corpo = &mt->e->v[mt->e->nv - 1];
	corpo->obj = (int)(o - mt->m->obj);
	memcpy(corpo->sel, (int[4]){ tx + sel[0], ty + sel[1], sel[2], sel[3] }, sizeof corpo->sel);
	int corpo_idx = mt->e->nv - 1;
	if (anima) mt->e->v[corpo_idx].anim = nova_anim(mt, &tp, &f, col, slot, fase);
	else tz_tempo_liberar(&tp);
	/* el puente horizontal tiene la baranda de delante como segundo dibujo, debajo de su fila */
	if (o->classe == C_BRIDGE && !strcmp(secao, "bridge_horz") && tem_imagem(mt->b, "bridge_horz1")) {
		Folha fb;
		if (!folha(mt, "bridge_horz1", &fb)) {
			int bi = quadro(mt, &fb, 0, 0, slot), d[2] = { 0, 0 };
			tz_ini_numeros(&mt->b->imagens, "bridge_horz1", "object", d, 2);
			if (bi >= 0 && visual(mt->e, bi, tx + d[0], ty + d[1], prio, TZ_CAPA_ESTANDARTE, chao, tz_profundidade(ax + d[0], ay + d[1], mt->m->lado))) return -1;
		}
	}
	/* estandartes y banderas del color del reino (image_c), desde la esquina del cuerpo; en los muros, solo
	 * en las torres. Giran a su fps (10 por segundo sin fps), como los del original. Una obra lleva el suyo. */
	char obra_c[130];
	const char *ov;
	if (op->obra) { snprintf(obra_c, sizeof obra_c, "%s_c", secao); ov = obra_c; }
	else ov = propriedade(mt->b, o, "image_c");
	int muro = o->classe == C_WALL_STONE || o->classe == C_WALL_WOOD;
	if (ov && tem_imagem(mt->b, ov) && !(muro && (palavra(o, 56) >= 16 || fila != 0))) {
		Folha fo;
		if (!folha(mt, ov, &fo)) {
			int oc = col % fo.cols, d[2] = { 0, 0 };
			TzTempo to;
			int oanim = tz_tempo_passos(&to, &mt->b->imagens, ov, fo.rows, 10) == 0;
			int orow = 0;
			if (oanim && !op->anima && op->ms) orow = tz_tempo_quadro(&to, *op->ms, 0);
			int oi = quadro(mt, &fo, oc, orow, slot);
			tz_ini_numeros(&mt->b->imagens, ov, "object", d, 2);
			if (oi >= 0 && !visual(mt->e, oi, tx + d[0], ty + d[1], prio, TZ_CAPA_ESTANDARTE, chao, prof)) {
				mt->e->v[mt->e->nv - 1].obj = (int)(o - mt->m->obj);
				if (oanim && op->anima) mt->e->v[mt->e->nv - 1].anim = nova_anim(mt, &to, &fo, oc, slot, 0);
				else tz_tempo_liberar(&to);
			} else tz_tempo_liberar(&to);
		}
	}
	return 0;
}

/* Parches de peces: uno de los cinco saltos fish_N al azar, a un cuadro cada 100 ms desde un cuadro al azar. Las redes
 * de los barcos que pescan ahí las pone la simulación. */
static int peixe(Montador *mt, const TzObjeto *o)
{
	char secao[16];
	snprintf(secao, sizeof secao, "fish_%u", ruido(o->id, 2) % 5 + 1);
	if (!tem_imagem(mt->b, secao)) { mt->e->sem_imagem++; return 0; }
	Folha f;
	if (folha(mt, secao, &f)) return falha_objeto(mt, o, secao);
	int ox, oy, fw, fh, sel[4];
	caixa(mt, secao, &ox, &oy, &fw, &fh, sel);
	int ax = o->x * 32, ay = o->y * 32;
	int img = quadro(mt, &f, 0, 0, -1);
	if (img < 0) return falha_objeto(mt, o, secao);
	if (visual(mt->e, img, ax - ox, ay - oy, 1, TZ_CAPA_CORPO, 1, tz_profundidade(ax, ay, mt->m->lado))) return -1;
	TzVisual *v = &mt->e->v[mt->e->nv - 1];
	v->obj = (int)(o - mt->m->obj);
	if (f.rows > 1) {
		TzTempo t;
		if (!tz_tempo_uniforme(&t, f.rows, 100)) v->anim = nova_anim(mt, &t, &f, 0, -1, (uint64_t)(ruido(o->id, 3) % (uint32_t)f.rows) * 100);
	}
	return 0;
}

/* Gaviotas y águilas: el cuerpo y su sombra pasan a un TzVoo que los mueve mientras corre la escena. */
static int passaro(Montador *mt, const TzObjeto *o, const char *secao, int slot)
{
	TzEscena *e = mt->e;
	Folha f;
	if (folha(mt, secao, &f)) return falha_objeto(mt, o, secao);
	int ax, ay, ox, oy, fw, fh, sel[4];
	ancora(mt, o, secao, &ax, &ay);
	caixa(mt, secao, &ox, &oy, &fw, &fh, sel);
	int col = f.cols ? o->variante % f.cols : 0;
	TzVoo *nv = realloc(e->voos, (size_t)(e->nvoos + 1) * sizeof *nv);
	if (!nv) return tz_falha("sin memoria");
	e->voos = nv;
	TzVoo *v = &e->voos[e->nvoos];
	memset(v, 0, sizeof *v);
	v->ncol = f.cols; v->nfil = f.rows;
	v->fq = tz_malloc((size_t)f.cols * (size_t)f.rows * sizeof *v->fq);
	if (!v->fq) return -1;
	for (int c = 0; c < f.cols; ++c)
		for (int r = 0; r < f.rows; ++r) v->fq[c * f.rows + r] = quadro(mt, &f, c, r, slot);
	if (v->fq[col * f.rows] < 0) { free(v->fq); return falha_objeto(mt, o, secao); }
	int tx = ax - ox, ty = ay - oy, dx, dy;
	sombra_elevada(o->classe, &dx, &dy);
	float prof = tz_profundidade(ax, ay, mt->m->lado);
	char ss[120];
	snprintf(ss, sizeof ss, "%s_s", secao);
	v->corpo = v->sombra = -1;
	Folha fs;
	if (tem_imagem(mt->b, ss) && !folha(mt, ss, &fs) && fs.cols == f.cols && fs.rows == f.rows) {
		v->fs = tz_malloc((size_t)f.cols * (size_t)f.rows * sizeof *v->fs);
		if (v->fs) {
			for (int c = 0; c < f.cols; ++c)
				for (int r = 0; r < f.rows; ++r) v->fs[c * f.rows + r] = quadro(mt, &fs, c, r, -1);
			if (v->fs[col * f.rows] >= 0 && !visual(e, v->fs[col * f.rows], tx + dx, ty + dy, 300, TZ_CAPA_SOMBRA, 0, prof)) {
				e->v[e->nv - 1].voo = e->nvoos * 2 + 1;
				e->v[e->nv - 1].obj = (int)(o - mt->m->obj);
				v->sombra = 0;
			}
		}
	}
	if (visual(e, v->fq[col * f.rows], tx, ty, 300, TZ_CAPA_CORPO, 0, prof)) return -1;
	e->v[e->nv - 1].voo = e->nvoos * 2;
	e->v[e->nv - 1].obj = (int)(o - mt->m->obj);
	memcpy(e->v[e->nv - 1].sel, (int[4]){ tx + sel[0], ty + sel[1], sel[2], sel[3] }, sizeof e->v[0].sel);
	/* el registro de animación del que vuela: un píxel cada (32000 / move_len) >> 5 ms y un cuadro de alas cada
	 * ciclo * move_len / (filas * 1000) pasos */
	const char *ml = tz_ini(&mt->b->imagens, secao, "move_len");
	int move_len = ml && atoi(ml) > 0 ? atoi(ml) : 32;
	TzTempo tc;
	uint64_t ciclo = 1000;
	if (!tz_tempo_direcional(&tc, &mt->b->imagens, secao, f.rows)) { ciclo = tc.duracao; tz_tempo_liberar(&tc); }
	v->passo_ms = (32000 / move_len) >> 5;
	if (v->passo_ms < 1) v->passo_ms = 1;
	v->passos_quadro = (int)(ciclo * (uint64_t)move_len / ((uint64_t)f.rows * 1000));
	if (v->passos_quadro < 1) v->passos_quadro = 1;
	int borde = (mt->m->lado > 22 ? mt->m->lado : 22) * 32;
	v->faixa[0] = 320; v->faixa[1] = borde - 352;
	v->desvio[0] = tx - ax; v->desvio[1] = ty - ay;
	v->elevacao[0] = dx; v->elevacao[1] = dy;
	v->semente = ruido(o->id, 4);
	tz_voo_iniciar(v, ax, ay, col);
	e->nvoos++;
	return 0;
}

/* Posición de las dos banderas de una puerta (Tzar.exe 0x589770): desde la esquina del dibujo, por columna de bandera
 * y fila (ReTzar, world.rs: GATE_FLAGS, con y hacia arriba; aquí se suma hacia abajo). */
static void bandeira_porta(int col, int fila, int *dx, int *dy)
{
	static const int esq[8][2] = { { 58, 67 }, { 91, 41 }, { 61, 34 }, { 90, 8 }, { 68, 24 }, { 70, 61 }, { 170, 22 }, { 168, 60 } };
	static const int arruina[8][2] = { { 62, 51 }, { 0, 0 }, { 65, 27 }, { 130, 107 }, { 76, 19 }, { 68, 121 }, { 180, 29 }, { 172, 118 } };
	static const int dir_[8][2] = { { 212, 67 }, { 175, 41 }, { 211, 35 }, { 178, 8 }, { 66, 150 }, { 70, 150 }, { 170, 147 }, { 168, 151 } };
	static const int ultima[8][2] = { { 212, 67 }, { 136, 126 }, { 210, 38 }, { 0, 0 }, { 64, 143 }, { 66, 151 }, { 160, 135 }, { 417, 150 } };
	*dx = *dy = 0;
	if (col < 0 || col > 7 || fila < 0 || fila > 15) return;
	const int *p;
	if (fila == 7) p = arruina[col];
	else if (fila == 15) p = ultima[col];
	else if (fila < 7) {
		/* la columna 4 baja la bandera un píxel después de la primera fila del puente */
		static const int c4[7][2] = { { 68, 24 }, { 68, 23 }, { 68, 23 }, { 68, 23 }, { 68, 23 }, { 68, 23 }, { 68, 23 } };
		p = col == 4 ? c4[fila] : esq[col];
	} else p = dir_[col];
	*dx = p[0]; *dy = p[1];
}

/* Puertas: la fila 8 lleva las dos torres y las filas 0-6 el puente levadizo (6 = bajado); dos banderas del color del
 * reino (hoja «gates c») marcan al dueño. Las verticales parten cada cuadro en dos columnas, que se pintan de atrás
 * adelante. */
static int porta(Montador *mt, const TzObjeto *o, int slot, int ponte, int ax, int ay)
{
	char secao[40];
	snprintf(secao, sizeof secao, "g%c%u_anim", o->classe == C_GATE_STONE ? 's' : 'w', byte_(o, 0x38) & 3);
	if (!tem_imagem(mt->b, secao)) { mt->e->sem_imagem++; return 0; }
	Folha f, fs;
	if (folha(mt, secao, &f)) return falha_objeto(mt, o, secao);
	if (f.rows < 9) { tz_falha("puerta sin las torres"); return falha_objeto(mt, o, secao); }
	char ss[48];
	snprintf(ss, sizeof ss, "%s_s", secao);
	int tem_s = tem_imagem(mt->b, ss) && !folha(mt, ss, &fs);
	int ox, oy, fw, fh, sel[4];
	caixa(mt, secao, &ox, &oy, &fw, &fh, sel);
	if (ax == SEM_PONTO) { ax = o->x * 32; ay = o->y * 32; }
	if (ponte < 0 || ponte > 6) ponte = byte_(o, 57) ? 0 : 6;
	int obj = (int)(o - mt->m->obj);
	float prof = tz_profundidade(ax, ay, mt->m->lado);
	for (int col = f.cols - 1; col >= 0; --col) {
		int filas[2] = { 8, ponte };
		for (int k = 0; k < 2; ++k) {
			if (tem_s && col < fs.cols && filas[k] < fs.rows) {
				int si = quadro(mt, &fs, col, filas[k], -1);
				if (si >= 0) {
					if (visual(mt->e, si, ax - ox, ay - oy, 100, TZ_CAPA_SOMBRA, 0, prof)) return -1;
					mt->e->v[mt->e->nv - 1].obj = obj;
				}
			}
			int img = quadro(mt, &f, col, filas[k], -1);
			if (img < 0) return falha_objeto(mt, o, secao);
			if (visual(mt->e, img, ax - ox, ay - oy, 100, TZ_CAPA_CORPO, 0, prof)) return -1;
			TzVisual *v = &mt->e->v[mt->e->nv - 1];
			v->obj = obj;
			if (col == 0 && k == 1) memcpy(v->sel, (int[4]){ ax - ox + sel[0], ay - oy + sel[1], sel[2], sel[3] }, sizeof v->sel);
		}
	}
	/* banderas: columna según la orientación (1, 0, 3, 2 -> 0, 2, 4, 6) y una más si es de piedra */
	if (tem_imagem(mt->b, "gates c")) {
		Folha fg;
		int orient = (int)(byte_(o, 0x38) & 3);
		int coluna = (int[4]){ 2, 0, 6, 4 }[orient] + (o->classe == C_GATE_STONE);
		if (!folha(mt, "gates c", &fg) && coluna < fg.cols)
			for (int k = 0; k < 2; ++k) {
				int linha = ponte + 8 * k;
				if (linha >= fg.rows) continue;
				int gi = quadro(mt, &fg, coluna, linha, slot), dx, dy;
				bandeira_porta(coluna, linha, &dx, &dy);
				if (gi >= 0) {
					if (visual(mt->e, gi, ax - ox + dx, ay - oy + dy, 100, TZ_CAPA_ESTANDARTE, 0, prof)) return -1;
					mt->e->v[mt->e->nv - 1].obj = obj;
				}
			}
	}
	return 0;
}

static int arvores(Montador *mt, TzTerreno *t)
{
	if (!t->arvores.px) return 0;
	int idx[32][2];
	for (int i = 0; i < 32; ++i) idx[i][0] = idx[i][1] = -1;
	int fw = t->arvores.w / 31, sw = t->arvores_s.w / 31;
	const TzMapa *m = mt->m;
	for (int i = 0; i < m->lado * m->lado; ++i) {
		int v = m->casillas[i] >> 11;
		if (!v) continue;
		if (idx[v][0] < 0) {
			for (int k = 0; k < 2; ++k) {
				const TzImg *src = k ? &t->arvores_s : &t->arvores;
				int w = k ? sw : fw;
				TzImg im = { w, src->h, malloc((size_t)w * (size_t)src->h * 4) };
				if (!im.px) return tz_falha("sin memoria");
				for (int y = 0; y < src->h; ++y) memcpy(im.px + y * w, src->px + (size_t)y * src->w + (v - 1) * w, (size_t)w * 4);
				idx[v][k] = img_direta(mt, im);
				if (idx[v][k] < 0) return -1;
			}
		}
		int x = (i % m->lado) * 32, y = (i / m->lado) * 32;
		float prof = tz_profundidade(x, y, m->lado);
		if (visual(mt->e, idx[v][1], x - t->arv_s_anc[0], y - t->arv_s_anc[1], 100, TZ_CAPA_SOMBRA, 0, prof)) return -1;
		if (visual(mt->e, idx[v][0], x - t->arv_anc[0], y - t->arv_anc[1], 100, TZ_CAPA_CORPO, 0, prof)) return -1;
	}
	return 0;
}

static int penhascos(Montador *mt, TzTerreno *t)
{
	const TzMapa *m = mt->m;
	int *cache = malloc(8 * 8 * 16 * sizeof *cache);
	if (!cache) return tz_falha("sin memoria");
	for (int i = 0; i < 8 * 8 * 16; ++i) cache[i] = -1;
	int r = 0;
	for (int i = 0; i < m->lado * m->lado && !r; ++i) {
		uint16_t c = m->casillas[i];
		if (!(c & 0x400)) continue;
		int k = (c >> 7) & 7, col = (c >> 4) & 7, fila = c & 15;
		if (!t->tem_penhasco[k]) { mt->e->sem_imagem++; continue; }
		const TzImg *a = &t->penhascos[k];
		if (col * 32 + 32 > a->w || fila * 32 + 32 > a->h) { mt->e->falhas++; snprintf(mt->e->ultima_falha, sizeof mt->e->ultima_falha, "acantilado fuera del atlas"); continue; }
		int *ci = &cache[(k * 8 + col) * 16 + fila];
		if (*ci < 0) {
			TzImg im = { 32, 32, malloc(32 * 32 * 4) };
			if (!im.px) { r = tz_falha("sin memoria"); break; }
			for (int y = 0; y < 32; ++y) memcpy(im.px + y * 32, a->px + (size_t)(fila * 32 + y) * a->w + col * 32, 32 * 4);
			*ci = img_direta(mt, im);
			if (*ci < 0) { r = -1; break; }
		}
		r = visual(mt->e, *ci, (i % m->lado) * 32, (i / m->lado) * 32, 100, TZ_CAPA_CORPO, 2, 0);
	}
	free(cache);
	return r;
}

static int ordem_pintor(const void *pa, const void *pb)
{
	const TzVisual *a = pa, *b = pb;
	/* parches de suelo, acantilados y después lo demás */
	int ga = a->chao == 1 ? 0 : a->chao == 2 ? 1 : 2, gb = b->chao == 1 ? 0 : b->chao == 2 ? 1 : 2;
	if (ga != gb) return ga - gb;
	if (a->prioridade != b->prioridade) return a->prioridade - b->prioridade;
	if (a->profundidade != b->profundidade) return a->profundidade < b->profundidade ? -1 : 1;
	if (a->capa != b->capa) return a->capa - b->capa;
	return a->ordem - b->ordem;
}

/* Prepara una escena vacía con su montador: las hojas se leen bajo demanda, para tz_dibujar_objeto. */
static int preparar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, const TzMapa *m)
{
	memset(e, 0, sizeof *e);
	Montador *mt = calloc(1, sizeof *mt);
	if (!mt) return tz_falha("sin memoria");
	mt->e = e; mt->b = b; mt->r = r; mt->m = m;
	e->mt = mt;
	e->lado = m->lado;
	return 0;
}

int tz_escena_preparar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, const TzMapa *m) { return preparar(e, b, r, m); }

static void poner_voos(TzEscena *e)
{
	for (int i = 0; i < e->nvoos; ++i) e->voos[i].corpo = e->voos[i].sombra = -1;
	e->animv = malloc((size_t)(e->nv ? e->nv : 1) * sizeof *e->animv);
	if (!e->animv) return;
	for (int i = 0; i < e->nv; ++i) {
		const TzVisual *v = &e->v[i];
		if (v->voo >= 0) {
			TzVoo *f = &e->voos[v->voo / 2];
			if (v->voo & 1) f->sombra = i; else f->corpo = i;
		}
		if (v->anim >= 0) e->animv[e->nanimv++] = i;
	}
	/* los pájaros: los visuales de cada uno se mueven en tz_animar */
}

int tz_escena_montar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, TzTerreno *t, const TzMapa *m)
{
	if (preparar(e, b, r, m)) return -1;
	Montador *mt = e->mt;
	int res = 0;
	for (int i = 0; i < m->nobj && !res; ++i) {
		const TzObjeto *o = &m->obj[i];
		if (o->flags & (FLAG_INACTIVO | 4)) continue;
		/* los objetos de un reino que no juega no se crean al cargar */
		if (o->dono >= 1 && o->dono <= 8 && m->nreinos && !m->reinos[o->dono - 1].ativo) continue;
		int slot = tz_mapa_cor(m, o->dono);
		if (o->classe == C_FISHPATCH) { res = peixe(mt, o); continue; }
		if (o->classe == C_GATE_WOOD || o->classe == C_GATE_STONE) { res = porta(mt, o, slot, -1, SEM_PONTO, 0); continue; }
		if (tz_classe_e_marcador(o->classe)) continue;
		const char *s = tz_imagem_objeto(b, o);
		char secao[120];
		if (!s) { e->sem_imagem++; continue; }
		snprintf(secao, sizeof secao, "%s", s);
		const char *arq = tz_ini(&b->imagens, secao, "image");
		if (!arq) { e->sem_imagem++; continue; }
		if (!*arq || !strcasecmp(secao, "anim_none")) continue;
		if (o->classe == C_BIRD || o->classe == C_EAGLE) { res = passaro(mt, o, secao, slot); continue; }
		Opcoes op = { SEM_PONTO, 0, -1, -1, 1, NULL, 0 };
		res = objeto(mt, o, secao, slot, &op);
	}
	if (!res) res = arvores(mt, t);
	if (!res) res = penhascos(mt, t);
	if (!res && e->nv > 1) qsort(e->v, (size_t)e->nv, sizeof *e->v, ordem_pintor);
	if (!res) poner_voos(e);
	return res;
}

void tz_escena_liberar(TzEscena *e)
{
	Montador *mt = e->mt;
	if (mt) {
		for (int i = 0; i < mt->narq; ++i) { free(mt->arq[i].nome); free(mt->arq[i].d); }
		free(mt->hashes);
		free(mt);
	}
	for (int i = 0; i < e->nimgs; ++i) { tz_img_liberar(&e->imgs[i]); free(e->chaves[i]); }
	for (int i = 0; i < e->nanims; ++i) { tz_tempo_liberar(&e->anims[i].t); free(e->anims[i].q); }
	for (int i = 0; i < e->nvoos; ++i) { free(e->voos[i].fq); free(e->voos[i].fs); }
	free(e->anims);
	free(e->voos);
	free(e->animv);
	free(e->imgs);
	free(e->chaves);
	free(e->v);
	memset(e, 0, sizeof *e);
}

/* ------------------------------------------------------------------ objetos con pose y efectos sueltos */

const char *tz_seccao_pose(const TzBiblioteca *b, const TzObjeto *o, const TzEstado *s, char *buf, size_t n)
{
	const char *chave = "image_idle";
	switch (s->pose) {
	case TZ_POSE_ANDAR: chave = "image"; break;
	case TZ_POSE_ATACAR: chave = s->ataque == 1 ? "image_attack2" : s->ataque == 2 ? "image_attack3" : "image_attack"; break;
	case TZ_POSE_CONSTRUIR: chave = "image_build"; break;
	case TZ_POSE_FEITICO: chave = "image_spell"; break;
	case TZ_POSE_MORRER: chave = "image_die"; break;
	case TZ_POSE_TRABALHAR:
		chave = s->tarefa == TZ_TAREFA_CORTAR ? "image_chop" : s->tarefa == TZ_TAREFA_CAPTURAR ? "image_capture"
		      : s->tarefa == TZ_TAREFA_ORDENHAR ? "image_milk" : s->tarefa == TZ_TAREFA_CAVAR ? "image_dig" : "image_mine";
		break;
	}
	const char *v = NULL;
	if (tz_classe_e_unidade(o->classe) || s->pose == TZ_POSE_MORRER) {
		v = propriedade(b, o, chave);
		/* los ataques 2 y 3 que no existen vuelven al primero */
		if (!v && s->pose == TZ_POSE_ATACAR) v = propriedade(b, o, "image_attack");
		if (v && !tem_imagem(b, v)) v = NULL;
	}
	if (!v) v = tz_imagem_objeto(b, o);
	if (!v) return NULL;
	snprintf(buf, n, "%s", v);
	return buf;
}

/* Sección de la obra de un edificio (ReTzar, world.rs: site_picture): build<w>x<h> por el tamaño en casillas. */
static const char *seccao_obra(Montador *mt, const TzObjeto *o, char *buf, size_t n)
{
	const char *fin = imagem_final(mt->b, o);
	int w = 3, h = 3;
	if (o->classe == C_HARBOR) snprintf(buf, n, "build harbor");
	else if (o->classe == C_WALL_STONE || o->classe == C_WALL_WOOD) snprintf(buf, n, "walls_%s_build", o->classe == C_WALL_STONE ? "stone" : "wood");
	else if (o->classe == C_BRIDGE) snprintf(buf, n, "%s", byte_(o, 56) == 0 ? "bridge_horz_build" : "bridge_vert_build");
	else if (o->classe == C_GATE_WOOD || o->classe == C_GATE_STONE) snprintf(buf, n, "g%c%u_build", o->classe == C_GATE_STONE ? 's' : 'w', byte_(o, 0x38) & 3);
	else {
		int r[4] = { 0, 0, 96, 96 };
		if (fin) tz_ini_numeros(&mt->b->imagens, fin, "object", r, 4);
		w = r[2] / 32; h = r[3] / 32;
		if (w < 1) w = 1;
		if (h < 1) h = 1;
		snprintf(buf, n, "build%dx%d", w, h);
	}
	if (tem_imagem(mt->b, buf)) return buf;
	snprintf(buf, n, "build3x3");
	return tem_imagem(mt->b, buf) ? buf : NULL;
}

static void dibujar_visuais(TzEscena *e, TzLienzo *l, int desde, int camx, int camy)
{
	for (int i = desde; i < e->nv; ++i) {
		const TzVisual *v = &e->v[i];
		/* sombras antes que cuerpos: ya van en ese orden por construcción */
		tz_pintar(l, &e->imgs[v->img], v->x - camx, v->y - camy);
	}
}

int tz_dibujar_objeto(TzEscena *e, TzLienzo *l, const TzObjeto *o, const TzEstado *s, int px, int py, int camx, int camy, uint64_t ms)
{
	Montador *mt = e->mt;
	if (!mt) return tz_falha("la escena no está preparada");
	int desde = e->nv, nvoos = e->nvoos, r = 0;
	int slot = tz_mapa_cor(mt->m, o->dono);
	if ((o->classe == C_GATE_WOOD || o->classe == C_GATE_STONE) && !s->em_obra) r = porta(mt, o, slot, s->linha_ponte, px, py);
	else {
		char buf[130], tmp[130];
		const char *secao;
		Opcoes op = { px, py, -1, -1, 0, &ms, 0 };
		TzObjeto copia = *o;
		if (s->vida_max > 0) { copia.vida = (uint16_t)s->vida; copia.vida_max = (uint16_t)s->vida_max; }
		if (s->em_obra) { secao = seccao_obra(mt, o, buf, sizeof buf); op.obra = 1; }
		else secao = tz_seccao_pose(mt->b, o, s, buf, sizeof buf);
		Folha f;
		if (!secao || folha(mt, secao, &f)) { if (secao) { e->nv = desde; return -1; } return tz_falha("%s sin dibujo", tz_classes[o->classe]); }
		(void)tmp;
		if (tz_classe_e_unidade(o->classe)) op.col = s->direcao * f.cols / 8;
		/* la fila: la obra sigue su avance; los demás, su línea de tiempo desde que empezó la pose */
		if (s->em_obra) {
			int vm = copia.vida_max ? copia.vida_max : 1;
			op.fila = f.rows * copia.vida / vm;
			if (op.fila > f.rows - 1) op.fila = f.rows - 1;
		} else if (f.rows > 1) {
			TzTempo t;
			int ok = 0;
			if (tz_classe_e_unidade(o->classe)) ok = (f.cols > 1 ? tz_tempo_direcional(&t, &mt->b->imagens, secao, f.rows) == 0 : tz_tempo_passos(&t, &mt->b->imagens, secao, f.rows, 0) == 0);
			if (ok) {
				uint64_t dt = ms > s->desde_ms ? ms - s->desde_ms : 0;
				int k = tz_tempo_quadro(&t, dt, 0);
				if (s->pose == TZ_POSE_MORRER && dt >= t.duracao) k = f.rows - 1;   /* el cadáver se queda en el último cuadro */
				op.fila = k;
				tz_tempo_liberar(&t);
			}
		}
		r = objeto(mt, &copia, secao, slot, &op);
	}
	if (!r) dibujar_visuais(e, l, desde, camx, camy);
	/* nada de lo dibujado queda en la escena: solo las imágenes, que se guardan para el cuadro siguiente */
	e->nv = desde;
	e->nvoos = nvoos;
	return r;
}

int tz_dibujar_efecto(TzEscena *e, TzLienzo *l, const char *secao, int x, int y, int camx, int camy, uint64_t desde_ms, uint64_t ms, int fila_fija)
{
	Montador *mt = e->mt;
	if (!mt) return tz_falha("la escena no está preparada");
	Folha f;
	if (!tem_imagem(mt->b, secao)) return tz_falha("%s: no está en IMAGES.INI", secao);
	if (folha(mt, secao, &f)) return -1;
	int fila = 0;
	if (fila_fija >= 0) fila = fila_fija < f.rows ? fila_fija : f.rows - 1;
	else if (f.rows > 1) {
		const char *p = tz_ini(&mt->b->imagens, secao, "fps");
		int fps = p && atoi(p) > 0 ? atoi(p) : 10;
		uint64_t dt = ms > desde_ms ? ms - desde_ms : 0;
		fila = (int)(dt * (uint64_t)fps / 1000 % (uint64_t)f.rows);
	}
	int q = quadro(mt, &f, 0, fila, -1);
	if (q < 0) return -1;
	/* el punto es el punto caliente (hotspot) o la esquina `object` del dibujo */
	int hot[2], obj[2] = { 0, 0 };
	int nh = tz_ini_numeros(&mt->b->imagens, secao, "hotspot", hot, 2);
	tz_ini_numeros(&mt->b->imagens, secao, "object", obj, 2);
	int ax = nh >= 2 ? hot[0] : obj[0], ay = nh >= 2 ? hot[1] : obj[1];
	tz_pintar(l, &e->imgs[q], x - ax - camx, y - ay - camy);
	return 0;
}

/* Lee todos los cuadros de todas las hojas animadas de IMAGES.INI (más de una fila): comprobación de los datos.
 * `ausentes` cuenta las secciones cuyo fichero no viene en la instalación. */
int tz_anim_verificar(TzEscena *e, int *hojas, int *cuadros, int *falhas, int *ausentes, char *ultimo, size_t n)
{
	Montador *mt = e->mt;
	*hojas = *cuadros = *falhas = *ausentes = 0;
	if (n) ultimo[0] = 0;
	const TzIni *ini = &mt->b->imagens;
	for (int i = 0; i < ini->ns; ++i) {
		const TzIniSecao *sec = &ini->s[i];
		if (!tz_ini(ini, sec->nome, "image")) continue;
		Folha f;
		const char *img = tz_ini(ini, sec->nome, "image");
		if (!*img) continue;
		if (folha(mt, sec->nome, &f)) {
			/* una sección cuyo fichero no viene en la instalación (p. ej. «trees» en FX, que usa el atlas del bioma) no es un fallo */
			if (strstr(tz_erro(), "falta el recurso")) (*ausentes)++;
			else { (*falhas)++; if (n) snprintf(ultimo, n, "%s: %s", sec->nome, tz_erro()); }
			continue;
		}
		if (f.rows < 2) continue;
		(*hojas)++;
		int antes = e->nimgs;
		for (int c = 0; c < f.cols; ++c)
			for (int r = 0; r < f.rows; ++r) {
				if (quadro(mt, &f, c, r, f.indexado ? 0 : -1) < 0) { (*falhas)++; if (n) snprintf(ultimo, n, "%s (%d,%d): %s", sec->nome, c, r, tz_erro()); }
				else (*cuadros)++;
			}
		/* tiempos de la sección */
		TzTempo t;
		if (f.cols > 1) { if (tz_tempo_direcional(&t, ini, sec->nome, f.rows) == 0) tz_tempo_liberar(&t); }
		else if (tz_tempo_passos(&t, ini, sec->nome, f.rows, 0) == 0) tz_tempo_liberar(&t);
		/* soltar lo leído: son miles de hojas */
		for (int k = antes; k < e->nimgs; ++k) { tz_img_liberar(&e->imgs[k]); free(e->chaves[k]); e->chaves[k] = NULL; }
		e->nimgs = antes;
	}
	return *falhas ? -1 : 0;
}

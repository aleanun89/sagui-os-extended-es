/* SPDX-License-Identifier: GPL-3.0-or-later */
/* tzar_camp_captura.c - capturas PRIVADAS de campañas de Tzar: sobre la captura del visor pone la ventana de historia o de conversación
 * de una misión con el texto, el hablante y su retrato reales (D417, fase 7; tests/h417_tzar.py privado). Nunca va a la batería ni a
 * capturas públicas: usa datos del juego.
 *   tzar_camp_captura pos <carpeta> <campaña> <misión>                       escribe «x,y» de la casilla donde abrir el visor
 *   tzar_camp_captura historia|conversa <carpeta> <campaña> <misión> <in.bmp> <out.bmp> */
#include "campanha.h"
#include <stdlib.h>
#include <string.h>

extern const unsigned char fonte_sagui[256][16];

typedef struct { int w, h; uint32_t *px; } Img;

static int ler_bmp(const char *ruta, Img *im)
{
	FILE *f = fopen(ruta, "rb");
	if (!f) return -1;
	uint8_t h[54];
	if (fread(h, 1, 54, f) != 54) { fclose(f); return -1; }
	uint32_t off = tz_u32(h + 10);
	int w = (int)tz_u32(h + 18), hh = (int)tz_u32(h + 22), bpp = tz_u16(h + 28) / 8;
	int alto = hh < 0 ? -hh : hh;
	size_t stride = ((size_t)w * (size_t)bpp + 3) / 4 * 4;
	uint8_t *d = malloc(stride * (size_t)alto);
	fseek(f, (long)off, SEEK_SET);
	if (!d || fread(d, 1, stride * (size_t)alto, f) != stride * (size_t)alto) { fclose(f); free(d); return -1; }
	fclose(f);
	im->w = w; im->h = alto; im->px = malloc((size_t)w * (size_t)alto * 4);
	for (int y = 0; y < alto; ++y)
		for (int x = 0; x < w; ++x) {
			const uint8_t *p = d + (size_t)(hh > 0 ? alto - 1 - y : y) * stride + (size_t)x * (size_t)bpp;
			im->px[y * w + x] = (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
		}
	free(d);
	return 0;
}

static void escribir_bmp(const char *ruta, const Img *im)
{
	FILE *f = fopen(ruta, "wb");
	size_t stride = ((size_t)im->w * 3 + 3) / 4 * 4;
	uint8_t h[54];
	memset(h, 0, sizeof h);
	h[0] = 'B'; h[1] = 'M';
	uint32_t tam = (uint32_t)(54 + stride * (size_t)im->h);
	memcpy(h + 2, &tam, 4); h[10] = 54; h[14] = 40;
	memcpy(h + 18, &im->w, 4); memcpy(h + 22, &im->h, 4); h[26] = 1; h[28] = 24;
	fwrite(h, 1, 54, f);
	uint8_t *fila = calloc(1, stride);
	for (int y = im->h - 1; y >= 0; --y) {
		for (int x = 0; x < im->w; ++x) { uint32_t c = im->px[y * im->w + x]; fila[x * 3] = (uint8_t)c; fila[x * 3 + 1] = (uint8_t)(c >> 8); fila[x * 3 + 2] = (uint8_t)(c >> 16); }
		fwrite(fila, 1, stride, f);
	}
	free(fila);
	fclose(f);
}

static void rect(Img *im, int x, int y, int w, int h, uint32_t c)
{
	for (int yy = y; yy < y + h && yy < im->h; ++yy) for (int xx = x; xx < x + w && xx < im->w; ++xx) if (xx >= 0 && yy >= 0) im->px[yy * im->w + xx] = c;
}

static void letra(Img *im, int x, int y, unsigned char ch, uint32_t c)
{
	for (int r = 0; r < 16; ++r)
		for (int k = 0; k < 8; ++k)
			if (fonte_sagui[ch][r] & (0x80 >> k) && x + k < im->w && y + r < im->h) im->px[(y + r) * im->w + x + k] = c;
}

/* escribe texto con salto de línea por palabras en un ancho en píxeles; devuelve las líneas usadas */
static int texto(Img *im, int x, int y, int ancho, const char *s, uint32_t c)
{
	int col = 0, linhas = 1, max = ancho / 8;
	while (*s) {
		const char *p = s;
		int l = 0;
		while (*p && *p != ' ' && *p != '\n') { p++; l++; }
		if (col && col + l > max) { col = 0; linhas++; }
		for (int i = 0; i < l; ++i) letra(im, x + (col + i) * 8, y + (linhas - 1) * 18, (unsigned char)s[i], c);
		col += l;
		s = p;
		if (*s == '\n') { col = 0; linhas++; s++; }
		else if (*s == ' ') { col++; s++; }
	}
	return linhas;
}

int main(int argc, char **argv)
{
	if (argc < 5) return 2;
	TzMapas cat;
	if (tz_mapas_ler(&cat, argv[2], NULL)) return 1;
	int ci = atoi(argv[3]), mi = atoi(argv[4]);
	if (ci >= cat.ncamp || mi >= cat.camp[ci].nmissoes) return 1;
	TzMissaoAberta ab;
	if (tz_missao_abrir(&cat, &cat.camp[ci].missoes[mi], &ab)) return 1;
	TzGuiao *g = &ab.guiao;
	if (!strcmp(argv[1], "pos")) {
		int cx = ab.mapa.lado / 2, cy = ab.mapa.lado / 2;
		for (int i = 0; i < ab.mapa.nobj; ++i) if (ab.mapa.obj[i].dono == g->jogador && ab.mapa.obj[i].classe == TZC_CASTLE) { cx = ab.mapa.obj[i].x; cy = ab.mapa.obj[i].y; break; }
		for (int i = 0; i < ab.mapa.nobj && cx == ab.mapa.lado / 2; ++i) if (ab.mapa.obj[i].dono == g->jogador) { cx = ab.mapa.obj[i].x; cy = ab.mapa.obj[i].y; break; }
		cx -= 16; cy -= 10;
		printf("%d,%d\n%s|%s\n", cx < 0 ? 0 : cx, cy < 0 ? 0 : cy, cat.camp[ci].missoes[mi].fich, cat.camp[ci].missoes[mi].dentro);
		return 0;
	}
	if (argc < 7) return 2;
	Img im;
	if (ler_bmp(argv[5], &im)) return 1;
	int conversa = !strcmp(argv[1], "conversa");
	const char *titulo = "", *corpo = "", *retrato_nome = "";
	if (conversa) {
		for (int i = 0; i < g->nconversas && !corpo[0]; ++i)
			for (int j = 0; j < g->conversas[i].nfrases; ++j)
				if (g->conversas[i].frases[j].inicio && g->conversas[i].frases[j].texto[0]) {
					const TzPersonagem *p = tz_guiao_personagem(g, g->conversas[i].frases[j].fala_de);
					corpo = g->conversas[i].frases[j].texto;
					if (p) { titulo = p->nome; retrato_nome = p->retrato_grande[0] ? p->retrato_grande : p->retrato; }
					break;
				}
	} else {
		for (int i = 0; i < g->nacoes && !corpo[0]; ++i)
			if (g->acoes[i].tipo == TZ_ACC_HISTORIA && g->acoes[i].t[0] && g->acoes[i].t[0][0]) { corpo = g->acoes[i].t[0]; retrato_nome = g->acoes[i].t[1] ? g->acoes[i].t[1] : ""; titulo = cat.camp[ci].missoes[mi].titulo; }
	}
	if (!corpo[0]) { fprintf(stderr, "esta misión no tiene %s\n", argv[1]); return 3; }
	int bw = im.w - 80 < 760 ? im.w - 80 : 760, bh = 220, bx = (im.w - bw) / 2, by = im.h - bh - 30;
	rect(&im, bx - 3, by - 3, bw + 6, bh + 6, 0x704010);
	rect(&im, bx, by, bw, bh, 0x201408);
	int tx = bx + 16;
	/* el retrato, si se lee de la misión */
	TzImg ret;
	memset(&ret, 0, sizeof ret);
	if (retrato_nome[0]) {
		char nome[200];
		snprintf(nome, sizeof nome, "%s", retrato_nome);
		for (char *p = nome; *p; ++p) if (*p == '\\') *p = '/';
		TzHpfs *src = ab.tem_sub ? &ab.sub : &ab.c;
		uint32_t nb = 0;
		uint8_t *d = NULL;
		for (int i = 0; i < src->n && !d; ++i) {
			size_t le = strlen(src->e[i].nome), ln = strlen(nome);
			if (le >= ln && !strcasecmp(src->e[i].nome + le - ln, nome)) d = tz_hpfs_ler(src, src->e[i].nome, &nb);
		}
		if (d && !tz_bmp(d, nb, 0, -1, &ret)) {
			int pw = ret.w < 160 ? ret.w : 160, ph = ret.h < 190 ? ret.h : 190;
			for (int y = 0; y < ph; ++y) for (int x = 0; x < pw; ++x) im.px[(by + 14 + y) * im.w + bx + 14 + x] = ret.px[y * ret.w + x] & 0xffffff;
			tx = bx + 14 + pw + 16;
			tz_img_liberar(&ret);
		}
		free(d);
	}
	texto(&im, tx, by + 12, bx + bw - tx - 14, titulo, 0xf0d060);
	texto(&im, tx, by + 40, bx + bw - tx - 14, corpo, 0xf0e8d0);
	escribir_bmp(argv[6], &im);
	tz_missao_fechar(&ab);
	tz_mapas_liberar(&cat);
	return 0;
}

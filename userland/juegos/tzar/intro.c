/* SPDX-License-Identifier: GPL-3.0-or-later */
/* intro.c - la introducción de Tzar: intro.MPG (MPEG-1) decodificada con video.h de la libsagui (D417, fase 7).
 * ReTzar reproduce los vídeos con Media Foundation; aquí va el decodificador del sistema. Solo existe en SaguiOS (SAGUI_TZAR): en el
 * visor del PC tz_intro_abrir devuelve NULL y la introducción se salta. */
#include "intro.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#ifdef SAGUI_TZAR
#include "video.h"

struct TzIntro {
	video_t *v;
	int w, h, fps, quadros, tem_audio;
	imagem_t img;
};

static int achar(const char *raiz, char *out, size_t n)
{
	DIR *d = opendir(raiz);
	struct dirent *de;
	int r = -1;
	while (d && (de = readdir(d)))
		if (!strcasecmp(de->d_name, "intro.mpg")) { snprintf(out, n, "%s/%s", raiz, de->d_name); r = 0; break; }
	if (d) closedir(d);
	return r;
}

TzIntro *tz_intro_abrir(const char *raiz)
{
	char ruta[700];
	if (achar(raiz, ruta, sizeof ruta)) { tz_falha("no está intro.MPG"); return NULL; }
	int erro = 0;
	video_t *v = video_abrir(ruta, &erro);
	if (!v) { tz_falha("intro.MPG: %s", video_erro_texto(erro)); return NULL; }
	TzIntro *i = calloc(1, sizeof *i);
	if (!i) { video_fechar(v); return NULL; }
	const video_info_t *vi = video_info(v);
	i->v = v; i->w = vi->larg; i->h = vi->alt; i->fps = vi->fps; i->quadros = vi->quadros; i->tem_audio = vi->tem_audio;
	return i;
}

void tz_intro_tamanho(const TzIntro *v, int *w, int *h, int *fps) { *w = v->w; *h = v->h; *fps = v->fps; }
int tz_intro_cuadros(const TzIntro *v) { return v->quadros; }

int tz_intro_quadro(TzIntro *v, uint32_t *px, TzSom *som)
{
	int r = video_quadro(v->v, &v->img);
	if (r == VIDEO_ERRO_FIM) return 1;
	if (r) return -1;
	if (v->img.larg == v->w && v->img.alt == v->h && v->img.px)
		for (int i = 0; i < v->w * v->h; ++i) px[i] = 0xff000000u | v->img.px[i];
	if (som && v->tem_audio) {
		int16_t pcm[4096 * 2];
		int n;
		while ((n = video_audio(v->v, pcm, 4096)) > 0) tz_som_filme_empurrar(som, pcm, n);
	}
	return 0;
}

void tz_intro_fechar(TzIntro *v)
{
	if (!v) return;
	video_fechar(v->v);
	free(v->img.px);
	free(v);
}
#else
TzIntro *tz_intro_abrir(const char *raiz) { (void)raiz; tz_falha("esta compilación no trae el decodificador MPEG-1"); return NULL; }
void tz_intro_tamanho(const TzIntro *v, int *w, int *h, int *fps) { (void)v; *w = *h = *fps = 0; }
int tz_intro_quadro(TzIntro *v, uint32_t *px, TzSom *som) { (void)v; (void)px; (void)som; return 1; }
int tz_intro_cuadros(const TzIntro *v) { (void)v; return 0; }
void tz_intro_fechar(TzIntro *v) { (void)v; }
#endif

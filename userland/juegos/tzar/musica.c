/* SPDX-License-Identifier: GPL-3.0-or-later */
/* musica.c - música MIDI de Tzar con el sintetizador del sistema (libsagui, audio_midi.c) (D417, fase 7).
 *
 * La lista de misión, la pausa entre canciones y qué suena en el menú, en las historias y al acabar siguen a ReTzar
 * (https://github.com/ColCarlo/ReTzar, GPL-3.0-or-later): src/audio.rs (Music, playlist, MusicCheck, STORY_SONG). La música de la
 * instalación está en sounds.wdt como MUSIC\1.MID..10.MID, INTRO, OUTRO, WIN1 y WIN2; las historias traen la suya en la misión.
 * El sintetizador es el de la libsagui (en enteros, sin banco de sonidos): la tarjeta lo llama desde su callback, por eso cada cambio
 * de canción va con el cerrojo del dispositivo. En una compilación sin sintetizador (el visor del PC) no suena música. */
#include "musica.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#if defined(SAGUI_TZAR) || defined(TZ_MIDI)
#include "audio_midi.h"
#define TEM_MIDI 1
#else
typedef struct midi midi_t;
#define TEM_MIDI 0
#endif

enum { M_NADA = 0, M_MENU, M_JOGO, M_RESULTADO };

struct TzMusica {
	TzSom *dono;
	TzRecursos *rec;
	midi_t *atual;
	uint8_t *dados; uint32_t ndados;     /* la canción en memoria, para repetirla */
	char nome[100];
	int repetir;
	volatile int acabou;                 /* la canción terminó (lo pone el callback) */
	int modo;
	int primeira, ultima; uint32_t pausa_ms;
	int lista[100], nlista, pos;
	uint32_t pausa_ate;                  /* 0 = no se espera */
	uint32_t semente;
};

int tz_musica_sintetizador(void) { return TEM_MIDI; }

static uint32_t rnd(TzMusica *m) { m->semente = m->semente * 1664525u + 1013904223u; return m->semente >> 8; }

TzMusica *tz_musica_criar(TzSom *dono, TzRecursos *r, const char *ini_ruta)
{
	TzMusica *m = calloc(1, sizeof *m);
	if (!m) return NULL;
	m->dono = dono; m->rec = r;
	m->primeira = 1; m->ultima = 10; m->pausa_ms = 10000;
	m->semente = 417;
	FILE *f = ini_ruta ? fopen(ini_ruta, "rb") : NULL;
	if (f) {
		uint8_t *buf = malloc(65536);
		size_t n = buf ? fread(buf, 1, 65536, f) : 0;
		fclose(f);
		if (n) {
			TzIni ini;
			memset(&ini, 0, sizeof ini);
			tz_ini_ler(&ini, buf, (uint32_t)n);
			const char *v;
			if ((v = tz_ini(&ini, "sound", "startsong")) != NULL) { int x = atoi(v); m->primeira = x < 1 ? 1 : x > 99 ? 99 : x; }
			if ((v = tz_ini(&ini, "sound", "endsong")) != NULL) { int x = atoi(v); m->ultima = x < 1 ? 1 : x > 99 ? 99 : x; }
			if (m->ultima < m->primeira) m->ultima = m->primeira;
			if ((v = tz_ini(&ini, "sound", "songpause")) != NULL) { long x = atol(v); m->pausa_ms = x < 0 ? 0 : (uint32_t)x; }
			tz_ini_liberar(&ini);
		}
		free(buf);
	}
	return m;
}

static void soltar(TzMusica *m)
{
#if TEM_MIDI
	if (m->atual) midi_fechar(m->atual);
#endif
	m->atual = NULL;
	free(m->dados);
	m->dados = NULL; m->ndados = 0;
	m->nome[0] = 0;
}

void tz_musica_liberar(TzMusica *m)
{
	if (!m) return;
	soltar(m);
	free(m);
}

int tz_musica_tocar_dados(TzMusica *m, const uint8_t *midi, uint32_t n, int repetir)
{
#if TEM_MIDI
	int erro = 0;
	uint8_t *copia = malloc(n ? n : 1);
	if (!copia) return tz_falha("sin memoria para la canción");
	memcpy(copia, midi, n);
	midi_t *nuevo = midi_abrir_memoria(copia, n, &erro);
	if (!nuevo) { free(copia); return tz_falha("canción MIDI no válida (%d)", erro); }
	tz_som_trancar(m->dono);
	midi_t *viejo = m->atual;
	uint8_t *vd = m->dados;
	m->atual = nuevo; m->dados = copia; m->ndados = n; m->repetir = repetir; m->acabou = 0;
	tz_som_destrancar(m->dono);
	if (viejo) midi_fechar(viejo);
	free(vd);
	return 0;
#else
	(void)m; (void)midi; (void)n; (void)repetir;
	return tz_falha("esta compilación no trae el sintetizador MIDI");
#endif
}

int tz_musica_tocar(TzMusica *m, const char *nome, int repetir)
{
	uint32_t n;
	uint8_t *d = tz_som_ler(m->dono, nome, &n);
	if (!d) return tz_falha("no está la canción %s", nome);
	int r = tz_musica_tocar_dados(m, d, n, repetir);
	free(d);
	if (!r) snprintf(m->nome, sizeof m->nome, "%s", nome);
	return r;
}

void tz_musica_parar(TzMusica *m)
{
	tz_som_trancar(m->dono);
#if TEM_MIDI
	midi_t *viejo = m->atual;
	uint8_t *vd = m->dados;
#else
	void *viejo = NULL; uint8_t *vd = m->dados;
#endif
	m->atual = NULL; m->dados = NULL; m->ndados = 0; m->nome[0] = 0; m->modo = M_NADA; m->pausa_ate = 0;
	tz_som_destrancar(m->dono);
#if TEM_MIDI
	if (viejo) midi_fechar(viejo);
#endif
	(void)viejo;
	free(vd);
}

void tz_musica_menu(TzMusica *m)
{
	if (m->modo == M_MENU && m->atual) return;
	m->modo = M_MENU;
	tz_musica_tocar(m, "MUSIC/INTRO.MID", 1);
}

static void seguinte(TzMusica *m)
{
	for (int tent = 0; tent < m->nlista; ++tent) {
		m->pos = (m->pos + 1) % m->nlista;
		char nome[40];
		snprintf(nome, sizeof nome, "MUSIC/%d.MID", m->lista[m->pos]);
		if (!tz_musica_tocar(m, nome, 0)) return;
	}
}

void tz_musica_jogo(TzMusica *m)
{
	if (m->modo == M_JOGO) return;
	m->modo = M_JOGO;
	/* las canciones de STARTSONG a ENDSONG, barajadas una vez (audio.rs playlist) */
	m->nlista = 0;
	for (int i = m->primeira; i <= m->ultima && m->nlista < 100; ++i) m->lista[m->nlista++] = i;
	for (int i = m->nlista - 1; i > 0; --i) { int j = (int)(rnd(m) % (uint32_t)(i + 1)); int t = m->lista[i]; m->lista[i] = m->lista[j]; m->lista[j] = t; }
	m->pos = -1; m->pausa_ate = 0;
	if (m->nlista) seguinte(m);
}

void tz_musica_resultado(TzMusica *m, int vitoria)
{
	m->modo = M_RESULTADO;
	tz_musica_tocar(m, vitoria ? (rnd(m) & 1 ? "MUSIC/WIN2.MID" : "MUSIC/WIN1.MID") : "MUSIC/OUTRO.MID", 0);
}

void tz_musica_tic(TzMusica *m, uint32_t ms)
{
	if (!m->acabou || m->modo != M_JOGO) return;
	/* tras la canción, la pausa (SONGPAUSE) y la siguiente */
	if (!m->pausa_ate) m->pausa_ate = ms + m->pausa_ms + 1;
	if (ms >= m->pausa_ate) { m->pausa_ate = 0; seguinte(m); }
}

const char *tz_musica_atual(const TzMusica *m) { return m->nome; }
int tz_musica_a_tocar(const TzMusica *m) { return m->atual != NULL && !m->acabou; }

void tz_musica_gerar(TzMusica *m, int32_t *mix, int quadros, int volume)
{
#if TEM_MIDI
	if (!m->atual || volume <= 0) return;
	int16_t tmp[2 * 512];
	int feitos = 0;
	while (feitos < quadros && !m->acabou) {
		int pedir = quadros - feitos < 512 ? quadros - feitos : 512;
		int n = midi_ler(m->atual, tmp, (unsigned)pedir);
		if (n <= 0) {
			if (m->repetir) { midi_buscar(m->atual, 0); n = midi_ler(m->atual, tmp, (unsigned)pedir); }
			if (n <= 0) { m->acabou = 1; break; }
		}
		for (int i = 0; i < n * 2; ++i) mix[feitos * 2 + i] += (int32_t)((tmp[i] * volume) >> 8);
		feitos += n;
	}
#else
	(void)m; (void)mix; (void)quadros; (void)volume;
#endif
}

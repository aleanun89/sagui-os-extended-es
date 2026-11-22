/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Música: lee ficheros MIDI estándar y los reproduce con un pequeño sintetizador por software (no necesita banco de sonidos). */
#include "ui.h"
#include "config.h"
#include <ctype.h>
#include "mezcla.h"

#define RATE 44100
#define MAX_MV 40
#define SIN_N 1024

typedef struct { u32 tick; u8 type, ch, a, b; u32 val, order; } Ev; /* tipo: 0 apagado 1 encendido 2 programa 3 control 4 inflexión de tono 5 tempo */
typedef struct { Ev *ev; int n; int division; } Song;

typedef struct {
	int active, ch, note, wave, stage, held;
	float phase, inc, vel, env, att, rel, dec, floor_, age, p1, p2;
	unsigned noise;
} Mv;

static char folder[1100];
static char files[64][260];
static int nfiles;
static int playlist[64], nplay, play_pos = -1;
static int menu_file = -1, won_file = -1, lost_file = -1;
static int mode;                 /* 0 ninguna, 1 menú (en bucle), 2 partida (lista de reproducción), 3 resultado (una vez) */
static char current[260];
static float vol_music = 0.5f;
static float sintab[SIN_N + 1];

/* estado del reproductor, lo toca el hilo de audio */
static Song *song;
static int evpos;
static double tick_pos;
static double us_per_q = 500000.0;
static volatile int ended;
static int prog[16], vol[16], expr[16], pan[16], sustain[16];
static float bend[16];
static Mv mv[MAX_MV];
static unsigned rngs = 12345u;
static unsigned gap_until;

/* D431: el cambio de pista no usa cerrojos. El hilo del juego deja la pista nueva en un buzón (cola de un productor y un
 * consumidor) y el hilo de audio la toma al empezar su siguiente bloque; la pista que se retira vuelve por otro buzón, para
 * que la libere el hilo del juego (el de audio no llama a free). */
#define BUZON_N 4
static char parada;                            /* su dirección es la orden «silencio» */
static Song *buzon[BUZON_N];
static volatile unsigned bz_esc, bz_lec;       /* juego -> audio */
static Song *retiradas[BUZON_N * 2];
static volatile unsigned rt_esc, rt_lec;       /* audio -> juego */

static void song_free(Song *s);
static void liberar_retiradas(void) {
	while (rt_lec != rt_esc) { Song *s = retiradas[rt_lec % (BUZON_N * 2)]; MEZCLA_BARRERA(); rt_lec = rt_lec + 1; song_free(s); }
}
/* hilo del juego: 0 si el buzón está lleno (la pista no se envía) */
static int enviar_pista(Song *s) {
	unsigned e = bz_esc;
	if (e - bz_lec >= BUZON_N) return 0;
	buzon[e % BUZON_N] = s ? s : (Song *)&parada;
	MEZCLA_BARRERA();
	bz_esc = e + 1;
	return 1;
}

static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u32 be16(const u8 *p) { return ((u32)p[0] << 8) | p[1]; }

static int cmp_ev(const void *a, const void *b) {
	const Ev *x = (const Ev *)a, *y = (const Ev *)b;
	if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
	return x->order < y->order ? -1 : x->order > y->order;
}

static Song *song_load(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 22 || sz > 4 * 1024 * 1024) { fclose(f); return NULL; }
	u8 *buf = (u8 *)malloc((size_t)sz);
	if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
	fclose(f);

	long pos = 0;
	if (!memcmp(buf, "RIFF", 4) && sz > 20 && !memcmp(buf + 8, "RMID", 4)) {
		/* MIDI envuelto en RIFF: busca el bloque de datos */
		long p = 12;
		pos = -1;
		while (p + 8 <= sz) {
			u32 len = (u32)buf[p + 4] | ((u32)buf[p + 5] << 8) | ((u32)buf[p + 6] << 16) | ((u32)buf[p + 7] << 24);
			if (!memcmp(buf + p, "data", 4)) { pos = p + 8; break; }
			p += 8 + (long)len + (long)(len & 1);
		}
		if (pos < 0) { free(buf); return NULL; }
	}
	if (pos + 14 > sz || memcmp(buf + pos, "MThd", 4)) { free(buf); return NULL; }

	int ntrk = (int)be16(buf + pos + 10);
	int division = (int)be16(buf + pos + 12);
	if (division & 0x8000 || division == 0) division = 480;
	pos += 8 + (long)be32(buf + pos + 4);

	int cap = 4096, n = 0;
	Ev *ev = (Ev *)malloc(sizeof(Ev) * (size_t)cap);
	u32 order = 0;

	for (int t = 0; t < ntrk && pos + 8 <= sz; ++t) {
		if (memcmp(buf + pos, "MTrk", 4)) break;
		long len = (long)be32(buf + pos + 4);
		long p = pos + 8, end = MIN(sz, pos + 8 + len);
		pos = end;
		u32 tick = 0;
		int running = 0;

		while (p < end) {
			u32 delta = 0;
			int b;
			do { b = buf[p++]; delta = (delta << 7) | (u32)(b & 0x7f); } while ((b & 0x80) && p < end);
			tick += delta;
			if (p >= end) break;

			int st = buf[p];
			if (st < 0x80) st = running; else ++p;
			if (st < 0x80) break;

			if (n + 2 > cap) { cap *= 2; ev = (Ev *)realloc(ev, sizeof(Ev) * (size_t)cap); }

			if (st == 0xff) { /* meta */
				if (p + 1 > end) break;
				int type = buf[p++];
				u32 l = 0;
				do { b = buf[p++]; l = (l << 7) | (u32)(b & 0x7f); } while ((b & 0x80) && p < end);
				if (type == 0x51 && l >= 3 && p + 3 <= end) {
					ev[n].tick = tick; ev[n].type = 5; ev[n].val = ((u32)buf[p] << 16) | ((u32)buf[p + 1] << 8) | buf[p + 2]; ev[n].order = order++;
					++n;
				}
				p += (long)l;
				if (type == 0x2f) break;
			} else if (st == 0xf0 || st == 0xf7) {
				u32 l = 0;
				do { b = buf[p++]; l = (l << 7) | (u32)(b & 0x7f); } while ((b & 0x80) && p < end);
				p += (long)l;
			} else {
				running = st;
				int ch = st & 15, kind = st & 0xf0;
				int two = !(kind == 0xc0 || kind == 0xd0);
				if (p + (two ? 2 : 1) > end) break;
				int a = buf[p++], bb = two ? buf[p++] : 0;
				Ev *e = &ev[n];
				e->tick = tick; e->ch = (u8)ch; e->a = (u8)a; e->b = (u8)bb; e->val = 0; e->order = order++;
				int keep = 1;
				switch (kind) {
				case 0x80: e->type = 0; break;
				case 0x90: e->type = bb ? 1 : 0; break;
				case 0xb0: e->type = 3; break;
				case 0xc0: e->type = 2; break;
				case 0xe0: e->type = 4; e->val = (u32)(a | (bb << 7)); break;
				default: keep = 0; break;
				}
				if (keep) ++n;
			}
		}
	}
	free(buf);

	if (n == 0) { free(ev); return NULL; }
	qsort(ev, (size_t)n, sizeof(Ev), cmp_ev);
	Song *s = (Song *)malloc(sizeof *s);
	s->ev = ev; s->n = n; s->division = division;
	return s;
}

static void song_free(Song *s) { if (s) { free(s->ev); free(s); } }

/* ---------- sintetizador ---------- */

enum { W_PLUCK, W_ORGAN, W_BASS, W_STRING, W_BRASS, W_REED, W_SYNTH, W_KICK, W_SNARE, W_HAT, W_CRASH, W_TOM };

static int wave_for(int program) {
	if (program < 8) return W_PLUCK;       /* pianos */
	if (program < 16) return W_PLUCK;      /* percusión cromática */
	if (program < 24) return W_ORGAN;
	if (program < 32) return W_PLUCK;      /* guitarras */
	if (program < 40) return W_BASS;
	if (program < 56) return W_STRING;     /* cuerdas, conjuntos, coros */
	if (program < 64) return W_BRASS;
	if (program < 80) return W_REED;       /* lengüetas, flautas */
	if (program < 104) return W_SYNTH;
	if (program < 112) return W_PLUCK;     /* étnicos */
	return W_PLUCK;
}

static float next_noise(Mv *v) {
	v->noise = v->noise * 1664525u + 1013904223u;
	return ((int)(v->noise >> 8) & 0xffff) / 32768.0f - 1.0f;
}

static void voice_on(int ch, int note, int velocity) {
	int slot = -1;
	float oldest = -1;
	for (int i = 0; i < MAX_MV; ++i) {
		if (!mv[i].active) { slot = i; break; }
		if (mv[i].age > oldest) { oldest = mv[i].age; slot = i; }
	}
	Mv *v = &mv[slot];
	memset(v, 0, sizeof *v);
	v->active = 1; v->ch = ch; v->note = note; v->vel = velocity / 127.0f;
	v->noise = rngs = rngs * 1103515245u + 12345u;

	if (ch == 9) {
		/* percusión */
		if (note == 35 || note == 36) { v->wave = W_KICK; v->dec = 0.9994f; v->p1 = 130; }
		else if (note == 38 || note == 40 || note == 37 || note == 39) { v->wave = W_SNARE; v->dec = 0.9990f; }
		else if (note == 42 || note == 44) { v->wave = W_HAT; v->dec = 0.9965f; }
		else if (note == 46) { v->wave = W_HAT; v->dec = 0.9992f; }
		else if (note == 49 || note == 57 || note == 51 || note == 52 || note == 55 || note == 59) { v->wave = W_CRASH; v->dec = 0.99992f; }
		else if (note >= 41 && note <= 50) { v->wave = W_TOM; v->dec = 0.9992f; v->p1 = 70.0f + (note - 41) * 14.0f; }
		else { v->wave = W_HAT; v->dec = 0.9970f; }
		v->env = 1.0f;
		v->stage = 1;
		return;
	}

	v->wave = wave_for(prog[ch]);
	float freq = 440.0f * powf(2.0f, (note - 69 + bend[ch]) / 12.0f);
	v->inc = freq / RATE * SIN_N;
	v->att = 1.0f / (RATE * 0.004f);
	v->rel = 0.9996f;
	v->floor_ = 1.0f;
	v->dec = 1.0f;
	switch (v->wave) {
	case W_PLUCK: v->dec = 0.99985f; v->floor_ = 0.18f; v->rel = 0.9995f; break;
	case W_BASS: v->dec = 0.9998f; v->floor_ = 0.35f; break;
	case W_STRING: v->att = 1.0f / (RATE * 0.08f); v->rel = 0.9998f; break;
	case W_BRASS: v->att = 1.0f / (RATE * 0.03f); break;
	case W_REED: v->att = 1.0f / (RATE * 0.025f); break;
	case W_ORGAN: v->att = 1.0f / (RATE * 0.01f); break;
	default: break;
	}
}

static void voice_off(int ch, int note) {
	for (int i = 0; i < MAX_MV; ++i) {
		Mv *v = &mv[i];
		if (!v->active || v->ch != ch || v->note != note || v->stage == 2) continue;
		if (ch == 9) continue; /* los tambores suenan solos */
		if (sustain[ch] >= 64) v->held = 1;
		else v->stage = 2;
		return;
	}
}

static void reset_synth(void) {
	memset(mv, 0, sizeof mv);
	for (int c = 0; c < 16; ++c) { prog[c] = 0; vol[c] = 100; expr[c] = 127; pan[c] = 64; sustain[c] = 0; bend[c] = 0; }
	us_per_q = 500000.0;
	tick_pos = 0;
	evpos = 0;
}

static void apply_event(const Ev *e) {
	switch (e->type) {
	case 0: voice_off(e->ch, e->a); break;
	case 1: voice_on(e->ch, e->a, e->b); break;
	case 2: prog[e->ch] = e->a; break;
	case 3:
		if (e->a == 7) vol[e->ch] = e->b;
		else if (e->a == 11) expr[e->ch] = e->b;
		else if (e->a == 10) pan[e->ch] = e->b;
		else if (e->a == 64) {
			sustain[e->ch] = e->b;
			if (e->b < 64)
				for (int i = 0; i < MAX_MV; ++i)
					if (mv[i].active && mv[i].ch == e->ch && mv[i].held) { mv[i].held = 0; mv[i].stage = 2; }
		} else if (e->a == 123 || e->a == 120) {
			for (int i = 0; i < MAX_MV; ++i) if (mv[i].active && mv[i].ch == e->ch && e->ch != 9) mv[i].stage = 2;
		}
		break;
	case 4:
		bend[e->ch] = ((int)e->val - 8192) / 8192.0f * 2.0f;
		for (int i = 0; i < MAX_MV; ++i)
			if (mv[i].active && mv[i].ch == e->ch && e->ch != 9)
				mv[i].inc = 440.0f * powf(2.0f, (mv[i].note - 69 + bend[e->ch]) / 12.0f) / RATE * SIN_N;
		break;
	case 5: us_per_q = (double)e->val; break;
	}
}

static inline float tsin(float phase) {
	int i = (int)phase;
	float f = phase - i;
	i &= SIN_N - 1;
	return sintab[i] + (sintab[i + 1] - sintab[i]) * f;
}

/* hilo de audio: toma las órdenes de pista pendientes (la última manda) y devuelve la que se retira */
static void tomar_pistas(void) {
	while (bz_lec != bz_esc) {
		Song *n = buzon[bz_lec % BUZON_N];
		MEZCLA_BARRERA();
		bz_lec = bz_lec + 1;
		Song *old = song;
		reset_synth();
		song = n == (Song *)&parada ? NULL : n;
		ended = 0;
		if (old && rt_esc - rt_lec < BUZON_N * 2) { retiradas[rt_esc % (BUZON_N * 2)] = old; MEZCLA_BARRERA(); rt_esc = rt_esc + 1; }
	}
}

void music_render(int16_t *out, int frames) {
	tomar_pistas();
	if (!song || vol_music <= 0.001f) return;

	for (int n = 0; n < frames; ++n) {
		if (!ended) {
			tick_pos += (1e6 / us_per_q) * song->division / RATE;
			while (evpos < song->n && (double)song->ev[evpos].tick <= tick_pos) apply_event(&song->ev[evpos++]);
			if (evpos >= song->n) {
				/* espera a que se apaguen las últimas notas */
				int busy = 0;
				for (int i = 0; i < MAX_MV; ++i) busy |= mv[i].active;
				static int tail;
				if (!busy || ++tail > RATE * 3) { ended = 1; tail = 0; }
			}
		}

		float l = 0, r = 0;
		for (int i = 0; i < MAX_MV; ++i) {
			Mv *v = &mv[i];
			if (!v->active) continue;
			float s = 0, amp;
			v->age += 1.0f / RATE;

			if (v->wave >= W_KICK) {
				/* percusión */
				v->env *= v->dec;
				switch (v->wave) {
				case W_KICK: v->p1 *= 0.9997f; v->phase += v->p1 / RATE * SIN_N; s = tsin(v->phase) * 1.2f; break;
				case W_SNARE: s = next_noise(v) * 0.7f + tsin(v->phase += 190.0f / RATE * SIN_N) * 0.3f; break;
				case W_TOM: v->p1 *= 0.9999f; v->phase += v->p1 / RATE * SIN_N; s = tsin(v->phase); break;
				case W_CRASH: s = next_noise(v) * 0.5f; break;
				default: { float nz = next_noise(v); s = (nz - v->p2) * 0.6f; v->p2 = nz; break; } /* hi-hat: ruido diferenciado */
				}
				amp = v->env * v->vel;
				if (v->env < 0.002f) { v->active = 0; continue; }
			} else {
				switch (v->stage) {
				case 0: v->env += v->att; if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; } break;
				case 1: if (v->dec < 1.0f && v->env > v->floor_) v->env *= v->dec; break;
				default: v->env *= v->rel; if (v->env < 0.002f) { v->active = 0; continue; } break;
				}
				v->phase += v->inc;
				if (v->phase >= SIN_N) v->phase -= SIN_N;
				float p = v->phase;
				switch (v->wave) {
				case W_PLUCK: s = tsin(p) + 0.45f * tsin(p * 2) + 0.2f * tsin(p * 3); break;
				case W_ORGAN: s = tsin(p) + 0.6f * tsin(p * 2) + 0.35f * tsin(p * 4) + 0.2f * tsin(p * 3); break;
				case W_BASS: s = tsin(p) + 0.5f * tsin(p * 2) + 0.2f * tsin(p * 3); break;
				case W_STRING: { float vib = 1.0f + 0.004f * tsin(v->age * 5.2f * SIN_N); s = (tsin(p * vib) + 0.5f * tsin(p * 2 * vib) + 0.33f * tsin(p * 3 * vib) + 0.25f * tsin(p * 4 * vib)) * 0.7f; break; }
				case W_BRASS: s = tsin(p) + 0.7f * tsin(p * 2) + 0.5f * tsin(p * 3) + 0.35f * tsin(p * 4) + 0.2f * tsin(p * 5); break;
				case W_REED: { float vib = 1.0f + 0.003f * tsin(v->age * 5.0f * SIN_N); s = tsin(p * vib) + 0.35f * tsin(p * 3 * vib) + 0.15f * tsin(p * 5 * vib); break; }
				default: s = tsin(p) + 0.5f * tsin(p * 2) + 0.33f * tsin(p * 3) + 0.25f * tsin(p * 4); break;
				}
				amp = v->env * v->vel;
			}

			float g = amp * (vol[v->ch] / 127.0f) * (expr[v->ch] / 127.0f) * 0.085f;
			float pn = (pan[v->ch] - 64) / 64.0f;
			l += s * g * (1.0f - MAX(0.0f, pn));
			r += s * g * (1.0f + MIN(0.0f, pn));
		}

		int li = out[n * 2] + (int)(l * vol_music * 32767.0f), ri = out[n * 2 + 1] + (int)(r * vol_music * 32767.0f);
		out[n * 2] = (int16_t)CLAMP(li, -32768, 32767);
		out[n * 2 + 1] = (int16_t)CLAMP(ri, -32768, 32767);
	}
}

/* ---------- lista de reproducción ---------- */

static int icontains(const char *s, const char *sub) {
	size_t ls = strlen(s), lb = strlen(sub);
	for (size_t i = 0; i + lb <= ls; ++i) {
		size_t k = 0;
		while (k < lb && tolower((unsigned char)s[i + k]) == tolower((unsigned char)sub[k])) ++k;
		if (k == lb) return 1;
	}
	return 0;
}

int music_setup(const char *data_dir) {
	folder[0] = 0;
	nfiles = 0; nplay = 0; menu_file = won_file = lost_file = -1;
	mezcla_musica(music_render);
	for (int i = 0; i <= SIN_N; ++i) sintab[i] = sinf(6.2831853f * i / SIN_N);
	if (!cfg_find_music(data_dir, folder, sizeof folder)) return 0;
	nfiles = cfg_list_files(folder, ".mid", files, 64);

	for (int i = 0; i < nfiles; ++i) {
		if (icontains(files[i], "music")) playlist[nplay++] = i;
		else if (icontains(files[i], "open")) { if (menu_file < 0 || icontains(files[i], "aokx")) menu_file = i; }
		else if (icontains(files[i], "won")) { if (won_file < 0 || icontains(files[i], "aokx")) won_file = i; }
		else if (icontains(files[i], "lost")) { if (lost_file < 0 || icontains(files[i], "aokx")) lost_file = i; }
	}
	if (menu_file < 0 && nplay > 0) menu_file = playlist[0];

	/* baraja la lista de reproducción */
	srand((unsigned)SDL_GetTicks());
	for (int i = nplay - 1; i > 0; --i) { int j = rand() % (i + 1); int t = playlist[i]; playlist[i] = playlist[j]; playlist[j] = t; }
	return nplay;
}

static int start_file(int idx) {
	if (idx < 0 || idx >= nfiles) return 0;
	char p[1500];
	snprintf(p, sizeof p, "%s/%s", folder, files[idx]);
	Song *s = song_load(p);
	if (!s) return 0;

	liberar_retiradas();
	if (!enviar_pista(s)) { song_free(s); return 0; }
	ended = 0;
	snprintf(current, sizeof current, "%s", files[idx]);
	return 1;
}

static void next_in_playlist(void) {
	for (int tries = 0; tries < nplay; ++tries) {
		play_pos = (play_pos + 1) % MAX(1, nplay);
		if (start_file(playlist[play_pos])) return;
	}
}

void music_play_menu(void) {
	if (mode == 1) return;
	mode = 1;
	if (menu_file >= 0) start_file(menu_file);
}

void music_play_game(void) {
	if (mode == 2 || nplay == 0) return;
	mode = 2;
	gap_until = 0;
	next_in_playlist();
}

void music_play_result(int victory) {
	int f = victory ? won_file : lost_file;
	if (f < 0) return;
	mode = 3;
	start_file(f);
}

void music_stop(void) {
	mode = 0;
	liberar_retiradas();
	enviar_pista(NULL);
	current[0] = 0;
}

void music_update(void) {
	liberar_retiradas();
	if (bz_lec != bz_esc) return;   /* hay una pista nueva por tomar: el «ended» que se ve es de la anterior */
	if (!song || !ended) return;
	if (mode == 1) { if (menu_file >= 0) start_file(menu_file); return; }
	if (mode == 2) {
		/* pausa corta entre pistas */
		if (!gap_until) gap_until = SDL_GetTicks() + 4000;
		if (SDL_GetTicks() >= gap_until) { gap_until = 0; next_in_playlist(); }
	}
}

void music_set_volume(float v) { vol_music = CLAMP(v, 0, 1); }
float music_volume(void) { return vol_music; }
int music_count(void) { return nplay + (menu_file >= 0 && !icontains(files[menu_file], "music") ? 1 : 0); }
const char *music_current(void) { return current; }

/* ayuda para desarrollo: genera cada fichero sin conexión y muestra cómo suena (duración, volumen, recorte) */
void music_test(void) {
	static int16_t buf[RATE * 2];
	for (int i = 0; i < nfiles; ++i) {
		if (!start_file(i)) { printf("%-24s cannot be read\n", files[i]); continue; }
		double sum = 0; long samples = 0; int peak = 0, clipped = 0; double seconds = 0;
		while (!ended && seconds < 240) {
			memset(buf, 0, sizeof buf);
			music_render(buf, RATE);
			seconds += 1.0;
			for (int k = 0; k < RATE * 2; ++k) {
				int a = abs(buf[k]);
				peak = MAX(peak, a);
				if (a >= 32767) ++clipped;
				sum += (double)buf[k] * buf[k];
				++samples;
			}
		}
		printf("%-24s %5.0f s  rms %5.0f  peak %5d  clipped %d  events %d\n", files[i], seconds, sqrt(sum / MAX(1, samples)), peak, clipped, song ? song->n : 0);
	}
}

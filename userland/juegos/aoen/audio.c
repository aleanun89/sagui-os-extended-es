/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Sonido: decodifica los WAV originales, mezcla hasta 24 voces por software y las reproduce con el audio de SDL. */
#include "ui.h"
#include "config.h"

#define RATE 44100
#define MAX_VOICES 24
#define MAX_SOUNDS 512

typedef struct { int id; int16_t *pcm; int n; } Sound; /* mono, RATE */
typedef struct { const int16_t *pcm; int n, pos; float vl, vr; int loop; } Voice;

static SDL_AudioDeviceID dev;
static Drs d_sounds, d_ui;
static int have_sounds, have_ui;
static Sound sounds[MAX_SOUNDS];
static int nsounds;
static Voice voices[MAX_VOICES];
static float vol_sfx = 0.8f, vol_amb = 0.4f;
static char amb_dir[1200];
static float amb_timer = 6.0f;
static Sound amb_cache[24]; static char amb_names[24][16]; static int amb_count;

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u32 rd16(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8); }

/* analiza un búfer RIFF/WAVE y lo convierte a mono de 16 bits a RATE (remuestreo lineal) */
static int wav_decode(const u8 *d, int size, int16_t **out, int *outn) {
	if (size < 44 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return 0;
	int pos = 12, channels = 1, bits = 16, rate = 22050;
	const u8 *data = NULL; int dlen = 0;

	while (pos + 8 <= size) {
		u32 len = rd32(d + pos + 4);
		if (!memcmp(d + pos, "fmt ", 4) && pos + 24 <= size) {
			channels = (int)rd16(d + pos + 10);
			rate = (int)rd32(d + pos + 12);
			bits = (int)rd16(d + pos + 22);
		} else if (!memcmp(d + pos, "data", 4)) {
			data = d + pos + 8;
			dlen = (int)MIN((u32)(size - pos - 8), len);
			break;
		}
		pos += 8 + (int)len + (int)(len & 1);
	}
	if (!data || dlen <= 0 || (bits != 8 && bits != 16) || channels < 1 || channels > 2 || rate < 4000) return 0;

	int bytes = bits / 8, frames = dlen / (bytes * channels);
	int n = (int)((long long)frames * RATE / rate);
	int16_t *pcm = (int16_t *)malloc(sizeof(int16_t) * (size_t)(n ? n : 1));

	for (int i = 0; i < n; ++i) {
		double src = (double)i * rate / RATE;
		int i0 = (int)src, i1 = MIN(i0 + 1, frames - 1);
		double f = src - i0;
		double v[2];
		for (int k = 0; k < 2; ++k) {
			int i2 = k ? i1 : i0;
			int sum = 0;
			for (int c = 0; c < channels; ++c) {
				const u8 *p = data + ((size_t)i2 * channels + c) * bytes;
				sum += bits == 8 ? ((int)p[0] - 128) * 256 : (int16_t)rd16(p);
			}
			v[k] = (double)sum / channels;
		}
		pcm[i] = (int16_t)(v[0] + (v[1] - v[0]) * f);
	}
	*out = pcm; *outn = n;
	return 1;
}

static Sound *get_sound(int id) {
	for (int i = 0; i < nsounds; ++i)
		if (sounds[i].id == id) return sounds[i].pcm ? &sounds[i] : NULL;
	if (nsounds >= MAX_SOUNDS) return NULL;

	Sound *s = &sounds[nsounds++];
	s->id = id; s->pcm = NULL; s->n = 0;
	int size = 0;
	u8 *d = NULL;
	if (have_sounds) d = drs_read(&d_sounds, (u32)id, &size);
	if (!d && have_ui) d = drs_read(&d_ui, (u32)id, &size);
	if (!d) return NULL;
	if (!wav_decode(d, size, &s->pcm, &s->n)) s->pcm = NULL;
	free(d);
	return s->pcm ? s : NULL;
}

static void mix(void *ud, Uint8 *stream, int len) {
	int16_t *out = (int16_t *)stream;
	int frames = len / 4;
	for (int i = 0; i < frames * 2; ++i) out[i] = 0;

	music_render(out, frames);

	for (int v = 0; v < MAX_VOICES; ++v) {
		Voice *vo = &voices[v];
		if (!vo->pcm) continue;
		for (int i = 0; i < frames; ++i) {
			if (vo->pos >= vo->n) {
				if (vo->loop) vo->pos = 0;
				else { vo->pcm = NULL; break; }
			}
			int s = vo->pcm[vo->pos++];
			int l = out[i * 2] + (int)(s * vo->vl), r = out[i * 2 + 1] + (int)(s * vo->vr);
			out[i * 2] = (int16_t)CLAMP(l, -32768, 32767);
			out[i * 2 + 1] = (int16_t)CLAMP(r, -32768, 32767);
		}
	}
}

static int open_data_drs(Drs *d, const char *dir, const char *name) {
	char p[1200], low[64];
	size_t n = 0;
	snprintf(p, sizeof p, "%s/%s", dir, name);
	if (drs_open(d, p)) return 1;
	for (; name[n] && n < 63; ++n) low[n] = (char)((name[n] >= 'A' && name[n] <= 'Z') ? name[n] + 32 : name[n]);
	low[n] = 0;
	snprintf(p, sizeof p, "%s/%s", dir, low);
	return drs_open(d, p);
}

static void play(Sound *s, float vol, float pan, int loop, int slot) {
	if (!dev || !s) return;
	SDL_LockAudioDevice(dev);
	int v = slot;
	if (v < 0) {
		for (int i = 0; i < MAX_VOICES; ++i) if (!voices[i].pcm) { v = i; break; }
		/* todas ocupadas: no quita ninguna, descarta el sonido */
	}
	if (v >= 0) {
		voices[v].pcm = s->pcm; voices[v].n = s->n; voices[v].pos = 0; voices[v].loop = loop;
		voices[v].vl = vol * (1.0f - MAX(0.0f, pan)); voices[v].vr = vol * (1.0f + MIN(0.0f, pan));
	}
	SDL_UnlockAudioDevice(dev);
}

int audio_init(const char *data_dir) {
	have_sounds = open_data_drs(&d_sounds, data_dir, "SOUNDS.DRS");
	have_ui = open_data_drs(&d_ui, data_dir, "INTERFAC.DRS");

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;

	SDL_AudioSpec want, got;
	SDL_zero(want);
	want.freq = RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = mix;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
	if (!dev) return 0;
	if (got.freq != RATE || got.format != AUDIO_S16SYS || got.channels != 2) { SDL_CloseAudioDevice(dev); dev = 0; return 0; }
	SDL_PauseAudioDevice(dev, 0);

	/* los clips de ambiente están en la carpeta de sonido del CD, junto a DATA */
	snprintf(amb_dir, sizeof amb_dir, "%s/../SOUND", data_dir);
	return 1;
}

void audio_lock(void) { if (dev) SDL_LockAudioDevice(dev); }
void audio_unlock(void) { if (dev) SDL_UnlockAudioDevice(dev); }

void audio_shutdown(void) {
	if (dev) SDL_CloseAudioDevice(dev);
	dev = 0;
}

void audio_set_volumes(float sfx, float amb) {
	vol_sfx = CLAMP(sfx, 0, 1);
	vol_amb = CLAMP(amb, 0, 1);
}

float audio_sfx_volume(void) { return vol_sfx; }
float audio_ambience_volume(void) { return vol_amb; }

static int pick(const int *ids, int n) { return ids[rand() % n]; }

/* la alarma se genera: tres toques cortos de bocina */
static Sound *alert_sound(void) {
	static Sound s;
	if (s.pcm) return &s;
	int n = (int)(RATE * 0.9);
	int16_t *p = (int16_t *)malloc(sizeof(int16_t) * (size_t)n);
	if (!p) return NULL;
	for (int i = 0; i < n; ++i) {
		float t = (float)i / RATE;
		int blast = (int)(t / 0.3f);
		float local = fmodf(t, 0.3f);
		float env = (local < 0.02f ? local / 0.02f : 1.0f) * (local > 0.22f ? MAX(0.0f, (0.3f - local) / 0.08f) : 1.0f);
		float f = 392.0f * (blast == 2 ? 1.26f : 1.0f);
		float w = sinf(6.2831853f * f * t) + 0.5f * sinf(6.2831853f * f * 2 * t) + 0.25f * sinf(6.2831853f * f * 3 * t);
		p[i] = (int16_t)(w * env * 9000.0f);
	}
	s.pcm = p; s.n = n;
	return &s;
}

static int event_sound(SndEvent ev) {
	static const int vill[] = { 5037, 5053, 5054, 5158, 5171 };
	static const int die[] = { 5055, 5056, 5057, 5058, 5059, 5060, 5061, 5062, 5063, 5064 };
	static const int hit[] = { 5138, 5139, 5140 };
	static const int bld_die[] = { 5077, 5078, 5079 };
	switch (ev) {
	case SND_CLICK: return 50300;
	case SND_ERROR: return 50303;
	case SND_PLACE: return 50301;
	case SND_BUILD_DONE: return 5022;
	case SND_TRAIN_VILL: return 5166;
	case SND_TRAIN_MIL: return 5144;
	case SND_HIT_MELEE: return pick(hit, 3);
	case SND_SHOOT: return 5139;
	case SND_WOOD: return 5048;
	case SND_MINE: return 5159;
	case SND_FORAGE: return 5085;
	case SND_DIE_UNIT: return pick(die, 10);
	case SND_DIE_BLD: return pick(bld_die, 3);
	case SND_SEL_VILL: return pick(vill, 5);
	case SND_SEL_MIL: return 5144;
	case SND_SEL_BLD: return 5022;
	case SND_SEL_TC: return 5044;
	case SND_AGE: return 5044;
	case SND_VICTORY: return 50320;
	case SND_DEFEAT: return 50321;
	case SND_CONVERT: return 5051;
	case SND_HEAL: return 5165;
	default: return 0;
	}
}

void snd_ui(SndEvent ev) {
	if (!dev) return;
	if (ev == SND_ALERT) { play(alert_sound(), vol_sfx, 0, 0, -1); return; }
	int id = event_sound(ev);
	if (id) play(get_sound(id), vol_sfx, 0, 0, -1);
}

void snd_event(SndEvent ev, float wx, float wy) {
	if (!dev || W.w == 0) return;

	float cx, cy;
	screen_to_world(V.sw / 2, V.sh / 2, &cx, &cy);
	float dx = wx - cx, dy = wy - cy, d = sqrtf(dx * dx + dy * dy);
	float range = 26.0f / V.zoom;
	if (d > range) return;

	/* paneo según el desplazamiento horizontal en pantalla */
	float px = ((wx + wy) - (cx + cy)) * 32.0f * V.zoom;
	float pan = CLAMP(px / (V.sw * 0.5f), -1.0f, 1.0f) * 0.7f;
	float vol = vol_sfx * (1.0f - d / range * 0.8f);

	int id = event_sound(ev);
	if (id) play(get_sound(id), vol, pan, 0, -1);
}

/* un clip de la carpeta de sonido, decodificado una sola vez */
static Sound *amb_clip(const char *name) {
	for (int i = 0; i < amb_count; ++i)
		if (!strcmp(amb_names[i], name)) return amb_cache[i].pcm ? &amb_cache[i] : NULL;
	if (amb_count >= 24) return NULL;

	int k = amb_count++;
	snprintf(amb_names[k], sizeof amb_names[k], "%s", name);
	amb_cache[k].pcm = NULL; amb_cache[k].n = 0;

	char p[1400];
	snprintf(p, sizeof p, "%s/%s", amb_dir, name);
	FILE *f = fopen(p, "rb");
	if (!f) { snprintf(p, sizeof p, "%s/../sound/%s", amb_dir, name); f = fopen(p, "rb"); }
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 8 * 1024 * 1024) { fclose(f); return NULL; }
	u8 *buf = (u8 *)malloc((size_t)sz);
	if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz) wav_decode(buf, (int)sz, &amb_cache[k].pcm, &amb_cache[k].n);
	free(buf);
	fclose(f);
	return amb_cache[k].pcm ? &amb_cache[k] : NULL;
}

/* de vez en cuando, un sonido natural que encaja con el terreno alrededor de la cámara: bosque, desierto, hierba o agua */
void audio_ambience_tick(float dt) {
	if (!dev || W.w == 0 || vol_amb <= 0.01f) return;
	amb_timer -= dt;
	if (amb_timer > 0) return;
	amb_timer = 14.0f + (float)(rand() % 22);

	float cx, cy;
	screen_to_world(V.sw / 2, V.sh / 2, &cx, &cy);
	int ix = (int)cx, iy = (int)cy;
	if (ix < 0 || iy < 0 || ix >= W.w || iy >= W.h) return;

	int water = 0, grass = 0, desert = 0, trees = 0;
	for (int dy = -6; dy <= 6; dy += 2)
		for (int dx = -6; dx <= 6; dx += 2) {
			int x = ix + dx, y = iy + dy;
			if (x < 0 || y < 0 || x >= W.w || y >= W.h) continue;
			int t = W.tile[y * W.w + x];
			if (t >= TILE_WATER) ++water; else if (t == TILE_GRASS) ++grass; else ++desert;
		}
	for (int i = 0; i < W.nents; ++i)
		if (W.ents[i].alive && W.ents[i].type == E_TREE && fabsf(W.ents[i].x - cx) < 8 && fabsf(W.ents[i].y - cy) < 8) ++trees;

	static const char *forest[] = { "FOREST1.WAV", "FOREST2.WAV", "FOREST3.WAV", "FOREST4.WAV" };
	static const char *des[] = { "DESERT1.WAV", "DESERT2.WAV", "DESERT3.WAV", "DESERT4.WAV" };
	static const char *gra[] = { "GRASS2.WAV", "GRASS3.WAV", "BIRD.WAV", "BIRDS1.WAV" };
	static const char *wat[] = { "OCEAN1.WAV", "OCEAN2.WAV", "LAKE1.WAV", "STREAM2.WAV" };

	const char *name;
	if (water >= 12) name = wat[rand() % 4];
	else if (trees >= 8) name = forest[rand() % 4];
	else if (grass >= desert) name = gra[rand() % 4];
	else name = des[rand() % 4];

	play(amb_clip(name), vol_amb * 0.8f, 0, 0, -1);
}

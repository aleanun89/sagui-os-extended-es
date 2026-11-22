/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Sonido: decodifica los WAV originales y los dispara en el mezclador (mezcla.c, hasta 24 voces por software). El audio va por
 * su cuenta, no por cuadros (D431): en el PC lo llama la SDL desde su hilo de audio con un búfer de ~93 ms; en SaguiOS lo
 * mezcla un hilo propio al ritmo de la tarjeta, por delante de la latencia del kernel (D436: 60 ms más, como mínimo 150), con
 * «alta prioridad de audio» opcional (PLAN_AUDIO). El hilo del juego solo deja eventos en una cola. */
#include "ui.h"
#include "config.h"
#include "mezcla.h"
#include "gpu.h"
#ifdef SAGUI_AOEN
#include "sagui.h"
#endif

#define RATE MEZCLA_RATE
#define MAX_SOUNDS 512
#define BLOQUE 4096            /* cuadros por llamada de la SDL en el PC (~93 ms) */
#define BLOQUE_HILO 1024       /* cuadros por bloque del hilo propio (~23 ms) */
#define ADELANTO_MS 150        /* lo que el hilo propio deja por delante en el flujo del sistema (como mínimo) */
#define ADELANTO_EXTRA_MS 60   /* D436: lo que va por delante de lo que el kernel ya mezcló en la tarjeta */
#define ADELANTO_EXTRA_GPU_MS 100   /* D436: lo mismo con el dibujo por GPU (el hilo principal pasa casi todo en el kernel) */

typedef struct { int id; int16_t *pcm; int n; } Sound; /* mono, RATE */

#ifndef SAGUI_AOEN
static SDL_AudioDeviceID dev;
#endif
static Drs d_sounds, d_ui;
static int have_sounds, have_ui;
static Sound sounds[MAX_SOUNDS];
static int nsounds;
static float vol_sfx = 0.8f, vol_amb = 0.4f;
static char amb_dir[1200];
static float amb_timer = 6.0f;
static int audio_vivo;                              /* hay salida de audio (tarjeta, dispositivo de la SDL o mudo con hilo) */
static Uint32 est_ult, est_hueco_max;               /* reloj de la última mezcla y mayor hueco entre dos */
static long est_vacios;                             /* veces que la tarjeta se quedó sin datos antes de tiempo */
static const char *grab_ruta;
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

/* medida de continuidad: el mayor hueco entre dos mezclas seguidas */
static void anotar_mezcla(void) {
	Uint32 ahora = SDL_GetTicks();
	if (est_ult && ahora - est_ult > est_hueco_max) est_hueco_max = ahora - est_ult;
	est_ult = ahora;
}

#ifndef SAGUI_AOEN
/* llamada de vuelta de la SDL: el hilo de audio de la SDL, al ritmo del dispositivo */
static void mix(void *ud, Uint8 *stream, int len) {
	(void)ud;
	anotar_mezcla();
	mezcla_render((int16_t *)stream, len / 4);
}
#else
static SDL_Thread *hilo_audio;
static volatile int hilo_parar;
static int fd_audio = -1;

/* D436 (audio con el dibujo por GPU): lo que dice el kernel de su mezclador. audio_pendente() cuenta lo que queda en el
 * flujo MÁS lo que el kernel ya mezcló en el anillo de la tarjeta, y el kernel va «latencia» por delante de la tarjeta:
 * 100 ms, pero 20 ms más tras cada corte (hasta 300). Con un adelanto fijo de 150 ms, en cuanto la latencia pasaba de
 * 150 el flujo se quedaba vacío en cada tic del kernel, que rellenaba con silencio: chasquidos sin parar (en el Amilo,
 * 20 s «sin datos del programa» en 45 s de partida) y el contador de antes no lo veía (miraba pend <= 0). Ahora el
 * adelanto es la latencia del kernel + ADELANTO_EXTRA_MS, y «sin datos» cuenta las veces que el flujo propio estaba
 * vacío (lo pendiente no llegaba ni a la latencia del kernel). */
static int lat_kernel_ms = 100;
static unsigned falhas_kernel0, falhas_kernel;
static Uint32 lat_visto;
static volatile int alta_prio_ok;   /* D436: 0 apagada, 1 activa, -1 pedida y el kernel no la tiene (anterior a la D436) */
static volatile int prio_cfg = -1;  /* «Alta prioridad de audio»: 1 sí, 0 no, -1 automática (sí con el dibujo por GPU) */

static void ler_kernel_audio(void) {
	static info_audio_t ia;   /* grande (los flujos y las salidas): fuera de la pila del hilo */
	if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) return;
	lat_kernel_ms = (int)CLAMP((int)ia.latencia_ms, 10, 1000);
	if (!lat_visto) falhas_kernel0 = ia.falhas;
	falhas_kernel = ia.falhas - falhas_kernel0;
}

int audio_adelanto_ms(void) { return MAX(ADELANTO_MS, lat_kernel_ms + (gpu_pedido ? ADELANTO_EXTRA_GPU_MS : ADELANTO_EXTRA_MS)); }
int audio_latencia_kernel_ms(void) { return lat_kernel_ms; }
long audio_sin_datos(void) { return est_vacios; }
unsigned audio_hueco_max(void) { return est_hueco_max; }
unsigned audio_cortes_kernel(void) { return falhas_kernel; }
int audio_alta_prioridad(void) { return alta_prio_ok; }
int audio_prioridad_opcion(void) { return prio_cfg < 0 ? gpu_pedido : prio_cfg; }
void audio_prioridad_elegir(int si) { prio_cfg = si ? 1 : 0; }

/* el propio hilo de audio la pide o la suelta (PLAN_AUDIO solo vale para el hilo que llama) */
static void prioridad_aplicar(int *aplicada) {
	int quer = audio_prioridad_opcion();
	if (quer == *aplicada) return;
	*aplicada = quer;
	int r = planificador_hilo_audio(quer);
	alta_prio_ok = r < 0 ? (quer ? -1 : 0) : quer;
	AOEN_LOG("[AOEN] audio: alta prioridad del hilo %s", r < 0 ? (quer ? "no disponible (el kernel no la tiene)" : "apagada")
	         : quer ? "encendida" : "apagada");
}

/* cada 10 s, si algo cambió: lo que se escribía solo al salir (con kill no se escribía) */
static void informe_periodico(Uint32 ahora) {
	static Uint32 ult;
	static long vac_ult = -1;
	static unsigned hueco_ult, falhas_ult;
	if (ult && ahora - ult < 10000) return;
	ult = ahora;
	if (vac_ult == est_vacios && hueco_ult == est_hueco_max && falhas_ult == falhas_kernel) return;
	vac_ult = est_vacios; hueco_ult = est_hueco_max; falhas_ult = falhas_kernel;
	AOEN_LOG("[AOEN] audio: hasta ahora sin datos %ld veces, hueco máximo %u ms; kernel: latencia %d ms, %u cortes de la tarjeta; "
	         "por delante %d ms; alta prioridad %s", est_vacios, (unsigned)est_hueco_max, lat_kernel_ms, falhas_kernel,
	         audio_adelanto_ms(), alta_prio_ok > 0 ? "sí" : alta_prio_ok < 0 ? "no disponible" : "no");
}

/* hilo propio: mezcla en cuanto la tarjeta tiene sitio, sin esperar a ningún cuadro del juego */
static int hilo_audio_fn(void *arg) {
	(void)arg;
	static int16_t buf[BLOQUE_HILO * 2];
	Uint32 marca = SDL_GetTicks();
	long credito = 0;   /* sin tarjeta: cuadros que tocaba haber mezclado según el reloj, en milésimas de cuadro */
	int vacio_antes = 0, prio_aplicada = 0, lleno = 0;
	while (!hilo_parar) {
		prioridad_aplicar(&prio_aplicada);
		if (fd_audio >= 0) {
			Uint32 ahora = SDL_GetTicks();
			if (!lat_visto || ahora - lat_visto >= 500) { ler_kernel_audio(); lat_visto = ahora ? ahora : 1; informe_periodico(ahora); }
			int pend = audio_pendente(fd_audio);
			if (pend >= audio_adelanto_ms() || audio_livre(fd_audio) < BLOQUE_HILO * 4) { lleno = 1; SDL_Delay(5); continue; }
			/* el flujo propio, vacío: el kernel ya está metiendo silencio (una vez por racha; al empezar, hasta llenarlo
			 * la primera vez, es lo normal) */
			int vacio = lleno && pend + 5 < lat_kernel_ms;
			if (vacio && !vacio_antes) ++est_vacios;
			vacio_antes = vacio;
			anotar_mezcla();
			mezcla_render(buf, BLOQUE_HILO);
			audio_escrever(fd_audio, buf, BLOQUE_HILO * 4);
		} else {
			/* sin tarjeta: se mezcla (y se tira) al ritmo del reloj, para que lo demás (grabación, eventos) funcione igual */
			Uint32 ahora = SDL_GetTicks();
			credito += (long)(ahora - marca) * RATE / 1000; marca = ahora;
			while (credito >= BLOQUE_HILO) { anotar_mezcla(); mezcla_render(buf, BLOQUE_HILO); credito -= BLOQUE_HILO; }
			SDL_Delay(5);
		}
	}
	if (prio_aplicada) planificador_hilo_audio(0);
	return 0;
}
#endif
#ifndef SAGUI_AOEN
int audio_adelanto_ms(void) { return 0; }
int audio_latencia_kernel_ms(void) { return 0; }
long audio_sin_datos(void) { return 0; }
unsigned audio_hueco_max(void) { return est_hueco_max; }
unsigned audio_cortes_kernel(void) { return 0; }
int audio_alta_prioridad(void) { return 0; }
int audio_prioridad_opcion(void) { return 0; }
void audio_prioridad_elegir(int si) { (void)si; }
#endif

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

/* hilo del juego: deja el efecto en la cola del mezclador (sin cerrojos); si está llena, se descarta */
static void play(Sound *s, float vol, float pan, int loop) {
	if (!audio_vivo || !s) return;
	mezcla_voz(s->pcm, s->n, vol * (1.0f - MAX(0.0f, pan)), vol * (1.0f + MIN(0.0f, pan)), loop);
}

int audio_init(const char *data_dir) {
	have_sounds = open_data_drs(&d_sounds, data_dir, "SOUNDS.DRS");
	have_ui = open_data_drs(&d_ui, data_dir, "INTERFAC.DRS");

	/* AOEN_AUDIO_WAV=ruta graba lo que mezcla el hilo de audio (hasta AOEN_AUDIO_SEG segundos, 60 por defecto): las pruebas
	 * comparan su duración y su continuidad (D431) */
	grab_ruta = getenv("AOEN_AUDIO_WAV");
	if (grab_ruta && *grab_ruta) {
		int seg = getenv("AOEN_AUDIO_SEG") ? atoi(getenv("AOEN_AUDIO_SEG")) : 60;
		mezcla_grabar((CLAMP(seg, 1, 600) + 2) * RATE);
	} else grab_ruta = NULL;

#ifdef SAGUI_AOEN
	/* en SaguiOS la SDL mueve el audio desde el hilo principal, una vez por cuadro: con pocos FPS se queda sin datos.
	 * Se abre la tarjeta directamente y un hilo la alimenta al ritmo del reloj de la tarjeta (audio_escrever espera cuando
	 * el flujo está lleno). Sin tarjeta, el hilo mezcla por el reloj del sistema. */
	{
		const char *e = getenv("AOEN_AUDIO_PRIO");   /* D436: 1/0 para las pruebas; si no, aoen.cfg (-1: automática) */
		prio_cfg = e && *e ? (atoi(e) > 0) : CLAMP(cfg_get_int("audio_prioridad", -1), -1, 1);
	}
	fd_audio = audio_abrir(RATE, 2, 16);
	if (fd_audio < 0) fd_audio = -1;
	hilo_parar = 0;
	hilo_audio = SDL_CreateThread(hilo_audio_fn, "aoen-audio", NULL);
	if (!hilo_audio) { if (fd_audio >= 0) audio_fechar(fd_audio, 0); fd_audio = -1; return 0; }
	audio_vivo = 1;
	AOEN_LOG("[AOEN] audio: hilo propio, %s, bloque %d, por delante %d ms", fd_audio >= 0 ? "con tarjeta" : "mudo", BLOQUE_HILO, ADELANTO_MS);
#else
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return 0;

	SDL_AudioSpec want, got;
	SDL_zero(want);
	want.freq = RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = BLOQUE; want.callback = mix;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
	if (!dev) return 0;
	if (got.freq != RATE || got.format != AUDIO_S16SYS || got.channels != 2) { SDL_CloseAudioDevice(dev); dev = 0; return 0; }
	SDL_PauseAudioDevice(dev, 0);
	audio_vivo = 1;
#endif

	/* los clips de ambiente están en la carpeta de sonido del CD, junto a DATA */
	snprintf(amb_dir, sizeof amb_dir, "%s/../SOUND", data_dir);
	return 1;
}

void audio_shutdown(void) {
#ifdef SAGUI_AOEN
	hilo_parar = 1;
	if (hilo_audio) SDL_WaitThread(hilo_audio, NULL);
	hilo_audio = NULL;
	if (fd_audio >= 0) audio_fechar(fd_audio, 0);
	fd_audio = -1;
#else
	if (dev) SDL_CloseAudioDevice(dev);
	dev = 0;
#endif
	if (audio_vivo) {
		MezclaEst e = mezcla_estadisticas();
		AOEN_LOG("[AOEN] audio: %lld cuadros en %ld bloques, %ld efectos, %ld perdidos, hueco máximo %u ms, sin datos %ld veces",
		         e.frames, e.llamadas, e.eventos, e.perdidos, (unsigned)est_hueco_max, est_vacios);
#ifdef SAGUI_AOEN
		AOEN_LOG("[AOEN] audio: kernel: latencia %d ms, %u cortes de la tarjeta; por delante %d ms; alta prioridad %s",
		         lat_kernel_ms, falhas_kernel, audio_adelanto_ms(), alta_prio_ok > 0 ? "sí" : alta_prio_ok < 0 ? "no disponible" : "no");
#endif
	}
	audio_vivo = 0;
	if (grab_ruta) {
		AOEN_LOG("[AOEN] audio: grabación %s %s", grab_ruta, mezcla_grabar_guardar(grab_ruta) ? "guardada" : "no se pudo guardar");
		grab_ruta = NULL;
	}
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
	if (!audio_vivo) return;
	if (ev == SND_ALERT) { play(alert_sound(), vol_sfx, 0, 0); return; }
	int id = event_sound(ev);
	if (id) play(get_sound(id), vol_sfx, 0, 0);
}

void snd_event(SndEvent ev, float wx, float wy) {
	if (!audio_vivo || W.w == 0) return;

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
	if (id) play(get_sound(id), vol, pan, 0);
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
	if (!audio_vivo || W.w == 0 || vol_amb <= 0.01f) return;
	/* el ambiente cuenta tiempo real, no el de la simulación (que con pocos FPS se arrastra, D431) */
	{
		static Uint32 ult;
		Uint32 ahora = SDL_GetTicks();
		if (ult) dt = MIN((ahora - ult) / 1000.0f, 1.0f);
		ult = ahora;
	}
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

	play(amb_clip(name), vol_amb * 0.8f, 0, 0);
}

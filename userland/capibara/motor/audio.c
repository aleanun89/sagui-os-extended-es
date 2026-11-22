/* SPDX-License-Identifier: MIT */
/* audio.c - mezclador de efectos y música. No usa la SDL ni sabe de cuadros: lo llama el hilo de audio (el de la tarjeta en
 * SaguiOS, la llamada de vuelta de la SDL en el PC) y el hilo del juego solo le deja órdenes en una cola de un productor y
 * un consumidor, sin cerrojos y sin bloquear nunca a quien mezcla. Así el sonido sigue al ritmo de la tarjeta aunque el
 * juego vaya a pocos cuadros por segundo. */
#include "capi.h"

#ifdef __GNUC__
#define BARRERA() __sync_synchronize()
#else
#define BARRERA() ((void)0)
#endif

#define COLA 256
enum { E_VOZ, E_DETENER, E_MUSICA, E_DETENER_MUSICA, E_VOL };
typedef struct { int tipo, id, bucle; const Sonido *s; float vl, vr; } Orden;
static Orden cola[COLA]; static volatile unsigned c_esc, c_lec;

typedef struct { const Sonido *s; int pos, bucle, id; float vl, vr; } Voz;
static Voz voces[MEZ_VOCES]; static Voz musica;
static volatile float maestro = 1.0f;
static int sig_id = 1;
static MezEst est;
static int16_t *grab; static int grab_max, grab_n;

static int poner(const Orden *o) {
	unsigned e = c_esc;
	if (e - c_lec >= COLA) { ++est.perdidas; return 0; }
	cola[e % COLA] = *o; BARRERA(); c_esc = e + 1;
	return 1;
}

int mez_voz(const Sonido *s, float vol, float pan, int bucle, int musica_) {
	(void)musica_;
	if (!s || !s->pcm) return -1;
	Orden o = { E_VOZ, sig_id, bucle, s, vol * (1.f - (pan > 0 ? pan : 0)), vol * (1.f + (pan < 0 ? pan : 0)) };
	if (!poner(&o)) return -1;
	int id = sig_id; sig_id = sig_id >= 0x3FFFFFFF ? 1 : sig_id + 1;
	return id;
}
void mez_detener(int id) { if (id < 0) return; Orden o = { E_DETENER, id, 0, NULL, 0, 0 }; poner(&o); }
void mez_musica(const Sonido *s, float vol, int bucle) { Orden o = { E_MUSICA, 0, bucle, s, vol, vol }; poner(&o); }
void mez_detener_musica(void) { Orden o = { E_DETENER_MUSICA, 0, 0, NULL, 0, 0 }; poner(&o); }
void mez_volumen(float v) { maestro = v < 0 ? 0 : v > 2 ? 2 : v; }
int mez_voz_activa(int id) { for (int i = 0; i < MEZ_VOCES; ++i) if (voces[i].s && voces[i].id == id) return 1; return 0; }

static void aplicar(const Orden *o) {
	switch (o->tipo) {
	case E_VOZ: {
		int l = -1; for (int i = 0; i < MEZ_VOCES; ++i) if (!voces[i].s) { l = i; break; }
		if (l < 0) { ++est.perdidas; return; }
		voces[l] = (Voz){ o->s, 0, o->bucle, o->id, o->vl, o->vr }; ++est.voces; break;
	}
	case E_DETENER: for (int i = 0; i < MEZ_VOCES; ++i) if (voces[i].s && voces[i].id == o->id) voces[i].s = NULL; break;
	case E_MUSICA: musica = (Voz){ o->s, 0, o->bucle, -1, o->vl, o->vr }; break;
	case E_DETENER_MUSICA: musica.s = NULL; break;
	}
}

static void sumar(Voz *v, int32_t *acc, int n) {
	for (int i = 0; i < n && v->s; ++i) {
		if (v->pos >= v->s->n) { if (v->bucle) v->pos = 0; else { v->s = NULL; break; } }
		acc[2 * i] += (int32_t)(v->s->pcm[2 * v->pos] * v->vl); acc[2 * i + 1] += (int32_t)(v->s->pcm[2 * v->pos + 1] * v->vr);
		++v->pos;
	}
}

void mez_renderizar(int16_t *sal, int cuadros) {
	static int32_t acc[8192];
	++est.llamadas;
	while (c_lec != c_esc) { BARRERA(); aplicar(&cola[c_lec % COLA]); c_lec = c_lec + 1; }
	while (cuadros > 0) {
		int n = cuadros > 4096 ? 4096 : cuadros;
		memset(acc, 0, sizeof(int32_t) * 2 * (size_t)n);
		int act = 0;
		for (int i = 0; i < MEZ_VOCES; ++i) if (voces[i].s) { sumar(&voces[i], acc, n); ++act; }
		if (musica.s) { sumar(&musica, acc, n); ++act; }
		est.activas = act;
		float m = maestro;
		for (int i = 0; i < 2 * n; ++i) { int32_t v = (int32_t)(acc[i] * m); sal[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
		if (grab && grab_n + n <= grab_max) { memcpy(grab + 2 * grab_n, sal, sizeof(int16_t) * 2 * (size_t)n); grab_n += n; }
		est.cuadros += n; sal += 2 * n; cuadros -= n;
	}
}

MezEst mez_estadisticas(void) { return est; }
int mez_grabar(int max) { free(grab); grab = calloc((size_t)max, 4); grab_max = grab ? max : 0; grab_n = 0; return grab != NULL; }
int mez_grabar_guardar(const char *ruta) {
	if (!grab) return 0;
	FILE *f = fopen(ruta, "wb"); if (!f) return 0;
	uint32_t datos = (uint32_t)grab_n * 4, riff = 36 + datos, tasa = MEZ_TASA, byter = MEZ_TASA * 4; uint16_t fmt = 1, ch = 2, al = 4, bits = 16; uint32_t l16 = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&l16, 4, 1, f);
	fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&tasa, 4, 1, f); fwrite(&byter, 4, 1, f); fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&datos, 4, 1, f); fwrite(grab, 4, (size_t)grab_n, f); fclose(f);
	return 1;
}

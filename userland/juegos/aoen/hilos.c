/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Reparto del trabajo en varios núcleos (D393).
 *
 * Un grupo fijo de hilos de trabajo (SDL_CreateThread; en SaguiOS, hilos de verdad que con --smp corren en las otras
 * CPU) espera tareas. hilos_repartir() publica una tarea partida en trozos y la reparte: cada hilo, y también el
 * principal, toma el siguiente trozo libre hasta que no quedan; el principal vuelve cuando están todos hechos.
 *
 * Todo pasa por un cerrojo y dos condiciones de la SDL (nada de atómicas del compilador: así compila igual con gcc,
 * con TCC dentro de SaguiOS y en Windows). Con unas pocas partes por tarea y unas pocas tareas por cuadro el cerrojo
 * no se nota. Cada tarea lleva un número (gen): un hilo que despierta tarde no toma trozos de otra tarea.
 *
 * El resultado no depende del reparto: cada trozo escribe solo lo suyo (sus filas de la pantalla, el plan de su
 * jugador). Con un hilo (un núcleo, o AOEN_HILOS=1) no se crea ninguno y hilos_repartir() es un bucle. */
#include "aoen.h"
#include "config.h"
#include "hilos.h"

static int n_hilos = 1;          /* hilos que trabajan, contando el principal */
static int iniciado;
static SDL_Thread *hilo[HILOS_MAX];
static SDL_mutex *cerrojo;
static SDL_cond *hay_trabajo, *trabajo_hecho;
static int salir;

/* la tarea en curso (todo bajo el cerrojo) */
static unsigned gen;
static TareaFn t_fn;
static void *t_ctx;
static int t_partes, t_siguiente, t_hechas;

/* toma el siguiente trozo de la tarea «g» (-1 si ya no quedan o la tarea es otra); con el cerrojo tomado */
static int tomar(unsigned g) {
	if (g != gen || t_siguiente >= t_partes) return -1;
	return t_siguiente++;
}

static int trabajador(void *arg) {
	(void)arg;
	unsigned visto = 0;
	SDL_LockMutex(cerrojo);
	for (;;) {
		while (!salir && gen == visto) SDL_CondWait(hay_trabajo, cerrojo);
		if (salir) break;
		visto = gen;
		TareaFn fn = t_fn; void *ctx = t_ctx; int partes = t_partes;
		int k;
		while ((k = tomar(visto)) >= 0) {
			SDL_UnlockMutex(cerrojo);
			fn(ctx, k, partes);
			SDL_LockMutex(cerrojo);
			if (++t_hechas == partes) SDL_CondSignal(trabajo_hecho);
		}
	}
	SDL_UnlockMutex(cerrojo);
	return 0;
}

int hilos_iniciar(void) {
	if (iniciado) return n_hilos;
	iniciado = 1;
	int nucleos = SDL_GetCPUCount();
	int quiero = nucleos;
	const char *e = getenv("AOEN_HILOS");
	int conf = cfg_get_int("hilos", 0);   /* 0: tantos como núcleos */
	if (e && *e) quiero = atoi(e);
	else if (conf > 0) quiero = conf;
	quiero = CLAMP(quiero, 1, HILOS_MAX);
	n_hilos = 1;
	if (quiero > 1) {
		cerrojo = SDL_CreateMutex();
		hay_trabajo = SDL_CreateCond();
		trabajo_hecho = SDL_CreateCond();
		if (cerrojo && hay_trabajo && trabajo_hecho) {
			for (int i = 1; i < quiero; ++i) {
				hilo[i] = SDL_CreateThread(trabajador, "aoen", NULL);
				if (!hilo[i]) break;
				++n_hilos;
			}
		}
	}
	AOEN_LOG("[AOEN] hilos: %d (núcleos %d%s)", n_hilos, nucleos, e && *e ? ", AOEN_HILOS" : conf > 0 ? ", configuración" : "");
	return n_hilos;
}

int hilos_n(void) { return n_hilos; }

int hilos_partes(void) { return n_hilos > 1 ? n_hilos * 4 : 1; }

void hilos_parar(void) {
	if (n_hilos <= 1) return;
	SDL_LockMutex(cerrojo);
	salir = 1;
	SDL_CondBroadcast(hay_trabajo);
	SDL_UnlockMutex(cerrojo);
	for (int i = 1; i < n_hilos; ++i) { SDL_WaitThread(hilo[i], NULL); hilo[i] = NULL; }
	n_hilos = 1;
}

void hilos_repartir(TareaFn fn, void *ctx, int partes) {
	if (partes <= 0) return;
	if (n_hilos <= 1 || partes == 1) {
		for (int k = 0; k < partes; ++k) fn(ctx, k, partes);
		return;
	}
	SDL_LockMutex(cerrojo);
	unsigned g = ++gen;
	if (!g) g = gen = 1;   /* el 0 es «nada visto» */
	t_fn = fn; t_ctx = ctx; t_partes = partes; t_siguiente = 0; t_hechas = 0;
	SDL_CondBroadcast(hay_trabajo);
	int k;
	while ((k = tomar(g)) >= 0) {
		SDL_UnlockMutex(cerrojo);
		fn(ctx, k, partes);
		SDL_LockMutex(cerrojo);
		++t_hechas;
	}
	while (t_hechas < partes) SDL_CondWait(trabajo_hecho, cerrojo);
	SDL_UnlockMutex(cerrojo);
}

/* ---------- medidas (AOEN_PERFIL=1) ---------- */

int perf_activo;
static unsigned long long perf_acum[PERF_N];

unsigned long long perf_reloj(void) {
#if defined(SAGUI_AOEN)
	unsigned lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
#else
	return (unsigned long long)SDL_GetPerformanceCounter();
#endif
}

void perf_sumar(int fase, unsigned long long desde) {
	if (fase >= 0 && fase < PERF_N) perf_acum[fase] += perf_reloj() - desde;
}

/* el reloj de alta resolución se pasa a milisegundos con el tiempo total medido con SDL_GetTicks */
static unsigned long long perf_t0;
static unsigned perf_ms0;

void perf_informe(int cuadros) {
	if (!perf_activo || cuadros <= 0) return;
	unsigned long long ciclos = perf_reloj() - perf_t0;
	unsigned ms = SDL_GetTicks() - perf_ms0;
	if (!ciclos || !ms) return;
	static const char *nombre[PERF_N] = { "simulación", "ia", "terreno", "entidades", "interfaz", "presentar", "eventos", "(entidades: recoger", "lista)" };
	/* las fases van anidadas: la simulación incluye la IA y la interfaz incluye el terreno y las entidades */
	double v[PERF_N];
	for (int f = 0; f < PERF_N; ++f) v[f] = (double)perf_acum[f] * ms / (double)ciclos / cuadros;
	v[PERF_SIM] -= v[PERF_IA];
	v[PERF_INTERFAZ] -= v[PERF_TERRENO] + v[PERF_ENTIDADES];
	char b[300];
	int n = snprintf(b, sizeof b, "por cuadro (ms):");
	for (int f = 0; f < PERF_N && n < (int)sizeof b; ++f)
		n += snprintf(b + n, sizeof b - (size_t)n, " %s %.2f", nombre[f], v[f] < 0 ? 0.0 : v[f]);
	AOEN_LOG("[AOEN] perfil: %s; hilos %d", b, n_hilos);
}

/* empieza (o vuelve a empezar) a medir: lo llama el banco al empezar a contar cuadros */
void perf_empezar(void) {
	memset(perf_acum, 0, sizeof perf_acum);
	perf_t0 = perf_reloj();
	perf_ms0 = SDL_GetTicks();
}

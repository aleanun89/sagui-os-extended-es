/* SPDX-License-Identifier: MIT */
/* plataforma_sdl.c - lo que une el motor con la SDL 2: ventana, renderizador (acelerado si hay GPU), entrada, hilo de
 * audio, bucle de paso fijo con dibujo interpolado, HUD de rendimiento (F3) y la línea de órdenes.
 *
 *   capibara [opciones] proyecto/ | juego.capi
 *   capibara --empaquetar proyecto/ juego.capi
 */
#include "capi.h"
#include "dibujo_sdl.h"
#include <SDL.h>
#ifdef CAPI_SAGUI
#include "sagui.h"
#include <SDL_image.h>   /* IMG_Load_Mem: sin la declaración, GCC 14+ da error (D464) */
#else
#include <SDL_image.h>
#endif
#include <ctype.h>

static SDL_Window *W; static SDL_Renderer *R;
static int g_acel, g_completa, g_gpu_modo = -1;   /* -1 automático, 0 nunca, 1 siempre */
static char g_nombre_rend[40] = "";
static float g_esc = 1, g_ox, g_oy;
static int g_cuadros_fijos, g_bench;              /* --cuadros N / --bench N */
static const char *g_volcar, *g_grabar_audio, *g_escena_arg;   /* g_escena_arg: --escena (D460: el editor juega la escena abierta) */
static int g_audio_prio = -1;                     /* -1 automática (con GPU), 0 no, 1 sí */
static int g_salir_esc = 1;

/* ---------------------------------------------------------------------------------------------------- imágenes */
static uint32_t *decodificar(const unsigned char *d, size_t n, int *w, int *h) {
#ifdef CAPI_SAGUI
	SDL_Surface *s = IMG_Load_Mem(d, (int)n);
#else
	SDL_Surface *s = IMG_Load_RW(SDL_RWFromConstMem(d, (int)n), 1);
	if (s && s->format->format != SDL_PIXELFORMAT_ARGB8888) { SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ARGB8888, 0); SDL_FreeSurface(s); s = c; }
#endif
	if (!s) return NULL;
	uint32_t *px = malloc((size_t)s->w * (size_t)s->h * 4);
	if (px) for (int y = 0; y < s->h; ++y) memcpy(px + (size_t)y * (size_t)s->w, (char *)s->pixels + (size_t)y * (size_t)s->pitch, (size_t)s->w * 4);
	*w = s->w; *h = s->h; SDL_FreeSurface(s);
	return px;
}

/* ---------------------------------------------------------------------------------------------------- audio */
static volatile int a_parar; static int a_activo;
#ifdef CAPI_SAGUI
static SDL_Thread *a_hilo;
static int fd_audio = -1, lat_kernel_ms = 100; static volatile int prio_ok;
static int gpu_activa_ahora(void) { unsigned long d = 0, r = 0; return R ? sagui_sdl2_gpu_estado(R, &d, &r) : 0; }
static void leer_kernel_audio(void) {
	static info_audio_t ia;
	if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) return;
	lat_kernel_ms = (int)ia.latencia_ms; if (lat_kernel_ms < 10) lat_kernel_ms = 10; if (lat_kernel_ms > 1000) lat_kernel_ms = 1000;
}
static int hilo_audio(void *arg) {
	(void)arg;
	static int16_t buf[1024 * 2]; int aplicada = 0; Uint32 visto = 0, marca = SDL_GetTicks(); long credito = 0;
	while (!a_parar) {
		/* alta prioridad (PLAN_AUDIO, D436): solo la pide el propio hilo; automática: sí cuando se dibuja por la GPU */
		int quer = g_audio_prio < 0 ? gpu_activa_ahora() : g_audio_prio;
		if (quer != aplicada) { int r = planificador_hilo_audio(quer); prio_ok = r < 0 ? (quer ? -1 : 0) : quer; aplicada = quer;
			capi_log("audio: alta prioridad del hilo %s", r < 0 ? "no disponible" : quer ? "encendida" : "apagada"); }
		if (fd_audio >= 0) {
			Uint32 ahora = SDL_GetTicks();
			if (!visto || ahora - visto >= 500) { leer_kernel_audio(); visto = ahora ? ahora : 1; }
			int adelanto = lat_kernel_ms + (gpu_activa_ahora() ? 160 : 60); if (adelanto < 150) adelanto = 150;
			if (audio_pendente(fd_audio) >= adelanto || audio_livre(fd_audio) < 4096) { SDL_Delay(5); continue; }
			mez_renderizar(buf, 1024); audio_escrever(fd_audio, buf, 1024 * 4);
		} else {   /* sin tarjeta: se mezcla y se tira al ritmo del reloj, para que lo demás funcione igual */
			Uint32 ahora = SDL_GetTicks(); credito += (long)(ahora - marca) * MEZ_TASA / 1000; marca = ahora;
			while (credito >= 1024) { mez_renderizar(buf, 1024); credito -= 1024; }
			SDL_Delay(5);
		}
	}
	if (aplicada) planificador_hilo_audio(0);
	return 0;
}
static int audio_abrir_plat(void) {
	fd_audio = audio_abrir(MEZ_TASA, 2, 16); if (fd_audio < 0) fd_audio = -1;
	a_parar = 0; a_hilo = SDL_CreateThread(hilo_audio, "capibara-audio", NULL);
	if (!a_hilo) { if (fd_audio >= 0) audio_fechar(fd_audio, 0); fd_audio = -1; return -1; }
	capi_log("audio: hilo propio, %s", fd_audio >= 0 ? "con tarjeta" : "mudo (sin tarjeta)");
	return 0;
}
static void audio_cerrar_plat(void) {
	a_parar = 1; if (a_hilo) SDL_WaitThread(a_hilo, NULL); a_hilo = NULL;
	if (fd_audio >= 0) audio_fechar(fd_audio, 0); fd_audio = -1;
}
#else
static SDL_AudioDeviceID a_dev;
static void a_cb(void *ud, Uint8 *s, int len) { (void)ud; mez_renderizar((int16_t *)s, len / 4); }
static int audio_abrir_plat(void) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return -1;
	SDL_AudioSpec want, got; SDL_zero(want);
	want.freq = MEZ_TASA; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 2048; want.callback = a_cb;
	a_dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
	if (!a_dev) return -1;
	SDL_PauseAudioDevice(a_dev, 0); capi_log("audio: dispositivo de la SDL (%d Hz)", got.freq);
	return 0;
}
static void audio_cerrar_plat(void) { if (a_dev) SDL_CloseAudioDevice(a_dev); a_dev = 0; }
#endif

/* ---------------------------------------------------------------------------------------------------- entrada */
static const char *nombre_mando(int b) {
	switch (b) {
	case SDL_CONTROLLER_BUTTON_A: return "a"; case SDL_CONTROLLER_BUTTON_B: return "b"; case SDL_CONTROLLER_BUTTON_X: return "x"; case SDL_CONTROLLER_BUTTON_Y: return "y";
	case SDL_CONTROLLER_BUTTON_BACK: return "atras"; case SDL_CONTROLLER_BUTTON_START: return "inicio";
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "lb"; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "rb";
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return "arriba"; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "abajo";
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "izquierda"; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "derecha";
	}
	return "";
}
static const char *nombre_eje(int a) {
	switch (a) { case SDL_CONTROLLER_AXIS_LEFTX: return "leftx"; case SDL_CONTROLLER_AXIS_LEFTY: return "lefty";
	             case SDL_CONTROLLER_AXIS_RIGHTX: return "rightx"; case SDL_CONTROLLER_AXIS_RIGHTY: return "righty"; }
	return "";
}

static void a_logico(int wx, int wy, int *lx, int *ly) {
	*lx = (int)floorf(((float)wx - g_ox) / g_esc); *ly = (int)floorf(((float)wy - g_oy) / g_esc);
	if (*lx < 0) *lx = 0;
	if (*ly < 0) *ly = 0;
	if (*lx >= g_proy.ancho) *lx = g_proy.ancho - 1;
	if (*ly >= g_proy.alto) *ly = g_proy.alto - 1;
}

static void poner_completa(int si) { g_completa = si; if (W) SDL_SetWindowFullscreen(W, si ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }

static void tratar_eventos(void) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT: g_salir = 1; break;
		case SDL_KEYDOWN: case SDL_KEYUP: {
			if (e.key.repeat) break;
			int abajo = e.type == SDL_KEYDOWN; SDL_Keycode k = e.key.keysym.sym;
			if (abajo && k == SDLK_F3) { g_hud = !g_hud; break; }
			if (abajo && k == SDLK_F4) { g_dep_col = !g_dep_col; break; }
			if (abajo && (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)))) { poner_completa(!g_completa); break; }
			if (abajo && k == SDLK_ESCAPE && g_salir_esc) { g_salir = 1; break; }
			entrada_tecla(SDL_GetKeyName(k), abajo);
			break;
		}
		case SDL_MOUSEMOTION: { int x, y; a_logico(e.motion.x, e.motion.y, &x, &y); entrada_raton_pos(x, y); break; }
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: {
			int x, y; a_logico(e.button.x, e.button.y, &x, &y); g_raton_x = x; g_raton_y = y;
			entrada_raton_boton(e.button.button, e.type == SDL_MOUSEBUTTONDOWN); break;
		}
		case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP: entrada_mando_boton(nombre_mando(e.cbutton.button), e.type == SDL_CONTROLLERBUTTONDOWN); break;
		case SDL_CONTROLLERAXISMOTION: { float v = e.caxis.value / 32767.0f; if (fabsf(v) < 0.2f) v = 0; entrada_mando_eje(nombre_eje(e.caxis.axis), v); break; }
		default: break;
		}
	}
}

static void abrir_mandos(void) {
	for (int i = 0; i < SDL_NumJoysticks(); ++i) if (SDL_IsGameController(i)) { if (SDL_GameControllerOpen(i)) { capi_log("mando %d abierto", i); break; } }
}

/* ---------------------------------------------------------------------------------------------------- vista y dibujo */
static void calcular_vista(void) {
	int ow = 0, oh = 0; SDL_GetRendererOutputSize(R, &ow, &oh);
	if (ow <= 0 || oh <= 0) { ow = g_proy.ancho; oh = g_proy.alto; }
	float e = fminf((float)ow / g_proy.ancho, (float)oh / g_proy.alto);
	if (g_proy.entera && e >= 1) e = floorf(e);
	g_esc = e; g_ox = floorf(((float)ow - g_proy.ancho * e) * 0.5f); g_oy = floorf(((float)oh - g_proy.alto * e) * 0.5f);
}

static void dibujar_cuadro(float alpha) {
	calcular_vista();
	g_vista_w = g_proy.ancho; g_vista_h = g_proy.alto;
	nodo_fijar_cuadro_dibujo(g_mundo.cuadro, alpha);
	dib_empezar_cuadro(g_mundo.arbol, alpha);
	dib_hud();
	SDL_RenderSetClipRect(R, NULL);
	SDL_SetRenderDrawColor(R, 0, 0, 0, 255); SDL_RenderClear(R);
	SDL_Rect vr = { (int)g_ox, (int)g_oy, (int)(g_proy.ancho * g_esc), (int)(g_proy.alto * g_esc) };
	SDL_SetRenderDrawColor(R, g_proy.fondo[0], g_proy.fondo[1], g_proy.fondo[2], 255); SDL_RenderFillRect(R, &vr);
	SDL_RenderSetClipRect(R, &vr);
	dibsdl_enviar(g_esc, g_ox, g_oy);
}

/* ---------------------------------------------------------------------------------------------------- HUD */
#ifdef CAPI_SAGUI
static info_nucleo_t nuc_ant[8]; static int nnuc_ant;
static void hud_sistema(char l[][100], int *n) {
	info_nucleo_t nuc[8]; int nn = info_sistema(INFO_NUCLEOS, nuc, 8);
	if (nn > 0) {
		int tu = 0, tt = 0, c = nn > 8 ? 8 : nn;
		for (int i = 0; i < c; ++i) { unsigned dt = i < nnuc_ant ? nuc[i].tiques - nuc_ant[i].tiques : 0, di = i < nnuc_ant ? nuc[i].tiques_ociosos - nuc_ant[i].tiques_ociosos : 0; tu += (int)(dt > di ? dt - di : 0); tt += (int)dt; }
		memcpy(nuc_ant, nuc, sizeof(info_nucleo_t) * (size_t)c); nnuc_ant = c;
		if (tt) snprintf(l[(*n)++], 100, "CPU %d%% (%d nucleos)", tu * 100 / tt, c); else snprintf(l[(*n)++], 100, "CPU - (%d nucleos)", c);
	}
	info_memoria_t m; memset(&m, 0, sizeof m);
	if (info_sistema(INFO_MEMORIA, &m, sizeof m) == 0 && m.gerida_kb) snprintf(l[(*n)++], 100, "RAM %u MB usada, %u libre", m.usada_kb / 1024, m.livre_kb / 1024);
	unsigned long d = 0, r = 0; int g = R ? sagui_sdl2_gpu_estado(R, &d, &r) : 0;
	snprintf(l[(*n)++], 100, "Dibujo por %s; presentacion directa %lu, por RAM %lu", g ? "GPU" : "CPU", d, r);
}
#else
static void hud_sistema(char l[][100], int *n) { snprintf(l[(*n)++], 100, "CPU %d nucleos; RAM -", SDL_GetCPUCount()); }
#endif

static struct { double acum, ocupado, peor; int n; } hs;
static double fps_actual, ms_medio, ms_peor;
/* ms: lo que tardó el cuadro trabajando; pared: tiempo transcurrido desde el cuadro anterior (con el tope de fps, mayor) */
static void hud_cuadro(double ms, double pared) {
	hs.acum += pared; hs.ocupado += ms; ++hs.n; if (ms > hs.peor) hs.peor = ms;
	if (hs.acum >= 500 || (g_hud && !g_hud_n)) {
		if (hs.acum > 0) { fps_actual = hs.n * 1000.0 / hs.acum; ms_medio = hs.ocupado / hs.n; ms_peor = hs.peor; }
		if (hs.acum >= 500) { hs.acum = hs.ocupado = hs.peor = 0; hs.n = 0; }
		if (g_hud) {
			int n = 0; MezEst a = mez_estadisticas();
			snprintf(g_hud_txt[n++], 100, "Capibara %s  FPS %.1f  cuadro %.1f ms (peor %.1f)", CAPI_VERSION, fps_actual, ms_medio, ms_peor);
			snprintf(g_hud_txt[n++], 100, "render %s (%s)  lotes %ld  tri %ld  copias %ld", g_nombre_rend, g_acel ? "geometria" : "copias", dibsdl_lotes, dibsdl_triangulos, dibsdl_copias);
			snprintf(g_hud_txt[n++], 100, "quads %d  texturas %d  nodos %d  fisica %ldp", g_nquads, rec_numero_texturas(), nodo_contar(g_mundo.arbol), g_fis_pares);
			snprintf(g_hud_txt[n++], 100, "audio: %d voces, %lld cuadros, %ld perdidas", a.activas, a.cuadros, a.perdidas);
			hud_sistema(g_hud_txt, &n);
			g_hud_n = n;
		}
	}
}

/* ---------------------------------------------------------------------------------------------------- entrada guionada (pruebas) */
typedef struct { int cuadro; char accion[24]; int abajo; } Paso;
static Paso pasos[256]; static int npasos;
static void cargar_pasos(const char *s) {   /* «12:derecha+;40:saltar+;41:saltar-» */
	char b[2048]; snprintf(b, sizeof b, "%s", s);
	for (char *t = strtok(b, ";"); t && npasos < 256; t = strtok(NULL, ";")) {
		char *c = strchr(t, ':'); if (!c) continue; *c = 0;
		Paso *p = &pasos[npasos]; p->cuadro = atoi(t);
		size_t l = strlen(c + 1); if (!l) continue;
		p->abajo = c[l] != '-'; if (c[l] == '+' || c[l] == '-') c[l] = 0;
		snprintf(p->accion, sizeof p->accion, "%s", c + 1); ++npasos;
	}
}
static void aplicar_pasos(long cuadro) { for (int i = 0; i < npasos; ++i) if (pasos[i].cuadro == cuadro) entrada_pulsa(pasos[i].accion, pasos[i].abajo); }

/* ---------------------------------------------------------------------------------------------------- hash y volcado */
static uint32_t hash_cuadro(const char *volcar) {
	int ow, oh; SDL_GetRendererOutputSize(R, &ow, &oh);
	uint32_t *px = malloc((size_t)ow * (size_t)oh * 4); uint32_t h = 2166136261u;
	if (!px || SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ARGB8888, px, ow * 4) != 0) { free(px); return 0; }
	for (long i = 0; i < (long)ow * oh; ++i) { h ^= px[i] & 0xFFFFFF; h *= 16777619u; }
	if (volcar) {
		FILE *f = fopen(volcar, "wb");
		if (f) { fprintf(f, "P6\n%d %d\n255\n", ow, oh); for (long i = 0; i < (long)ow * oh; ++i) { uint8_t c[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] }; fwrite(c, 1, 3, f); } fclose(f); }
	}
	free(px);
	return h;
}

/* ---------------------------------------------------------------------------------------------------- principal */
static void uso(void) {
	printf("Capibara %s - motor de juegos 2D\n"
	       "uso: capibara [opciones] proyecto/ | juego.capi\n"
	       "     capibara --empaquetar proyecto/ juego.capi\n"
	       "opciones: --pantalla-completa  --gpu=auto|si|no  --hud  --cuadros N (sin ventana visible, determinista; imprime CAPI_HASH)\n"
	       "          --escena ruta.esc  (empieza por esa escena en vez de la principal)\n"
	       "          --bench N  --volcar f.ppm  --entrada \"12:derecha+;40:saltar+\"  --grabar-audio f.wav  --audio-prio=auto|si|no\n", CAPI_VERSION);
}

int capibara_principal(int argc, char **argv) {
	const char *proyecto = NULL; char err[256] = "";
	const char *pasos_txt = getenv("CAPI_ENTRADA");
	if (pasos_txt) cargar_pasos(pasos_txt);
	if (getenv("CAPI_GPU")) g_gpu_modo = atoi(getenv("CAPI_GPU"));
	setvbuf(stdout, NULL, _IOLBF, 0);
	for (int i = 1; i < argc; ++i) {
		const char *a = argv[i];
		if (!strcmp(a, "--empaquetar") && i + 2 < argc) {
			int n = cap_empaquetar(argv[i + 1], argv[i + 2], err, sizeof err);
			if (n < 0) { printf("capibara: %s\n", err); return 1; }
			printf("capibara: %d ficheros empaquetados en %s\n", n, argv[i + 2]); return 0;
		}
		else if (!strcmp(a, "--pantalla-completa")) g_completa = 1;
		else if (!strcmp(a, "--hud")) g_hud = 1;
		else if (!strncmp(a, "--gpu=", 6)) g_gpu_modo = !strcmp(a + 6, "si") ? 1 : !strcmp(a + 6, "no") ? 0 : -1;
		else if (!strncmp(a, "--audio-prio=", 13)) g_audio_prio = !strcmp(a + 13, "si") ? 1 : !strcmp(a + 13, "no") ? 0 : -1;
		else if (!strcmp(a, "--cuadros") && i + 1 < argc) g_cuadros_fijos = atoi(argv[++i]);
		else if (!strcmp(a, "--bench") && i + 1 < argc) g_bench = atoi(argv[++i]);
		else if (!strcmp(a, "--volcar") && i + 1 < argc) g_volcar = argv[++i];
		else if (!strcmp(a, "--escena") && i + 1 < argc) g_escena_arg = argv[++i];
		else if (!strcmp(a, "--grabar-audio") && i + 1 < argc) g_grabar_audio = argv[++i];
		else if (!strcmp(a, "--entrada") && i + 1 < argc) cargar_pasos(argv[++i]);
		else if (!strcmp(a, "--ayuda") || !strcmp(a, "-h") || !strcmp(a, "--help")) { uso(); return 0; }
		else if (a[0] != '-') proyecto = a;
		else { printf("capibara: opción desconocida: %s\n", a); uso(); return 2; }
	}
	if (!proyecto) { uso(); return 2; }
	if (cap_abrir_proyecto(proyecto, err, sizeof err) != 0) { printf("capibara: %s\n", err); return 1; }
	if (proyecto_cargar(err, sizeof err) != 0) { printf("capibara: %s\n", err); return 1; }
	if (g_escena_arg) snprintf(g_proy.principal, sizeof g_proy.principal, "%s", g_escena_arg);
	if (guion_iniciar() != 0) { printf("capibara: no se pudo iniciar Lua\n"); return 1; }
	rec_poner_decodificador(decodificar);
	g_vista_w = g_proy.ancho; g_vista_h = g_proy.alto;

	Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER;
	if (SDL_Init(flags) != 0 && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { printf("capibara: SDL_Init: %s\n", SDL_GetError()); return 1; }
	int f = (640 + g_proy.ancho - 1) / g_proy.ancho; if (f < 1) f = 1;
	int sin_ventana = g_cuadros_fijos > 0;
	W = SDL_CreateWindow(g_proy.nombre, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_proy.ancho * f, g_proy.alto * f,
	                     (sin_ventana ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) | SDL_WINDOW_RESIZABLE | (g_completa ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (!W) { printf("capibara: SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
	/* GPU: automática = solo a pantalla completa (en ventana, traer el cuadro de la GPU a la RAM puede costar mucho; D432) */
	int quiere_gpu = g_gpu_modo == 1 || (g_gpu_modo < 0 && g_completa);
	if (sin_ventana && g_gpu_modo < 0) quiere_gpu = 0;
	R = SDL_CreateRenderer(W, -1, quiere_gpu ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
	if (!R) R = SDL_CreateRenderer(W, -1, SDL_RENDERER_SOFTWARE);
	if (!R) { printf("capibara: SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
	SDL_RendererInfo ri; memset(&ri, 0, sizeof ri); SDL_GetRendererInfo(R, &ri);
	snprintf(g_nombre_rend, sizeof g_nombre_rend, "%s", ri.name ? ri.name : "?");
	g_acel = (ri.flags & SDL_RENDERER_ACCELERATED) != 0;
	dibsdl_iniciar(R, g_acel);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	capi_log("CAPI_ARRANQUE proyecto=\"%s\" pantalla=%dx%d renderizador=%s gpu=%d", g_proy.nombre, g_proy.ancho, g_proy.alto, g_nombre_rend, g_acel);
	abrir_mandos();

	if (g_grabar_audio) mez_grabar(MEZ_TASA * 120);
	if (!sin_ventana) { if (audio_abrir_plat() != 0) capi_log("audio: sin dispositivo (el juego sigue en silencio)"); else a_activo = 1; }
	if (mundo_iniciar(err, sizeof err) != 0) { printf("capibara: %s\n", err); return 1; }
	capi_log("CAPI_ESCENA %s nodos=%d", g_proy.principal, nodo_contar(g_mundo.arbol));

	double hz = g_hz, dt = 1.0 / hz;
	Uint64 frec = SDL_GetPerformanceFrequency(), t_ant = SDL_GetPerformanceCounter();
	double acum = 0, bench_t0 = 0, bench_peor = 0; long bench_n = 0; Uint64 bench_ini = 0;
	long cuadro_fijo = 0;
	int salir_por_cuadros = 0;
	while (!g_salir && !salir_por_cuadros) {
		Uint64 t_ini = SDL_GetPerformanceCounter();
		tratar_eventos();
		double fdt;
		if (g_cuadros_fijos) fdt = dt;
		else { fdt = (double)(t_ini - t_ant) / (double)frec; if (fdt > 0.25) fdt = 0.25; if (fdt < 0) fdt = 0; }
		t_ant = t_ini;
		aplicar_pasos(g_cuadros_fijos ? cuadro_fijo : (long)g_mundo.cuadro);
		int pasos_n = 0;
		acum += fdt;
		while (acum >= dt - 1e-9 && pasos_n < 5) { mundo_paso_fisica((float)dt); acum -= dt; ++pasos_n; }
		if (pasos_n == 5) acum = 0;
		mundo_proceso((float)fdt);
		if (g_cuadros_fijos) { int16_t tmp[2 * 2048]; int k = (int)(MEZ_TASA / hz); mez_renderizar(tmp, k > 2048 ? 2048 : k); }
		dibujar_cuadro((float)(acum / dt));
		SDL_RenderPresent(R);
		mundo_fin_cuadro();
		Uint64 t_fin = SDL_GetPerformanceCounter();
		double ms = (double)(t_fin - t_ini) * 1000.0 / (double)frec;
		hud_cuadro(ms, g_cuadros_fijos ? ms : fdt * 1000.0);
		++cuadro_fijo;
		if (g_cuadros_fijos && cuadro_fijo >= g_cuadros_fijos) salir_por_cuadros = 1;
		if (g_bench) {
			if (!bench_n) { bench_ini = t_ini; bench_t0 = 0; }
			++bench_n; if (ms > bench_peor) bench_peor = ms;
			if (bench_n >= g_bench) salir_por_cuadros = 1;
		} else if (!g_cuadros_fijos) {
			/* tope de cuadros: hz (la física y el dibujo interpolado siguen igual); el resto del tiempo se cede */
			double resto = 1000.0 / hz - ms;
			if (resto > 1.0) SDL_Delay((Uint32)resto);
		}
	}
	(void)bench_t0;
	if (g_bench && bench_n) {
		double tot = (double)(SDL_GetPerformanceCounter() - bench_ini) * 1000.0 / (double)frec;
		printf("CAPI_BENCH cuadros %ld tiempo_ms %.0f fps_x10 %.0f peor_cuadro_ms %.1f quads %d lotes %ld render %s gpu %d\n", bench_n, tot, bench_n * 10000.0 / tot, bench_peor, g_nquads, dibsdl_lotes, g_nombre_rend, g_acel);
	}
	if (g_cuadros_fijos) printf("CAPI_HASH %08x cuadros %ld\n", hash_cuadro(g_volcar), cuadro_fijo);
	else if (g_volcar) hash_cuadro(g_volcar);
	if (a_activo) audio_cerrar_plat();
	if (g_grabar_audio) { MezEst e = mez_estadisticas(); printf("CAPI_AUDIO voces %ld cuadros %lld grabado %d\n", e.voces, e.cuadros, mez_grabar_guardar(g_grabar_audio)); }
	capi_log("CAPI_FIN cuadros=%llu", g_mundo.cuadro);
	dibsdl_soltar();
	mundo_cerrar();
	SDL_DestroyRenderer(R); SDL_DestroyWindow(W); SDL_Quit();
	return 0;
}

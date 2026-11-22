/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* HUD de rendimiento (D436): Ctrl+F o Opciones > Gráficos; apagado de serie. Una caja semitransparente arriba a la izquierda
 * con los fps (actual y media) y el tiempo de cuadro, la CPU (total, por núcleo y frecuencia), la RAM (del sistema y de AoEN),
 * la GPU (cuadros y triángulos por segundo del 3D, cuánto espera AoEN a la GPU, VRAM), el camino de dibujo y de presentación,
 * y las temperaturas. Los datos salen de las mismas llamadas que usan htop, sensores, infopc y el Administrador de tareas
 * (INFO_NUCLEOS, INFO_MEMORIA, INFO_PROCESSOS, INFO_SENSORES, INFO_GPU3D, INFO_GPU_ENERGIA), dos veces por segundo como
 * mucho. Lo que no hay en la máquina sale como «—». Se dibuja como el resto de la interfaz (por la CPU o, con el dibujo por
 * GPU, en su capa), encima de todo. */
#include "interfaz.h"
#include "gpu.h"
#include "hilos.h"
#include "config.h"
#ifdef SAGUI_AOEN
#include "sagui.h"
#include "sensores.h"
#endif

int hud_rend_visible;

#define NUC_MAX 8
static struct {
	int ok;                               /* ya se midió una vez */
	char lin[8][96];
	int nlin;
} H;

/* medidas de los cuadros: tiempo de cada uno (ms) y la media desde que se encendió el HUD */
static Uint32 t_ant, t_medida;
static float ms_cuadro, ms_suma;
static int cuadros_medida, fps_atual;
static unsigned long long cuadros_total;
static double fps_soma_total;
static unsigned long long espera_ant, reloj_ant;

#ifdef SAGUI_AOEN
static info_nucleo_t nuc_ant[NUC_MAX];
static int nnuc_ant;
static unsigned g3d_q_ant, g3d_t_ant;
static Uint32 g3d_ms_ant;
#endif

static const char *traco = "\xe2\x80\x94";   /* «—» (la fuente lo dibuja como raya) */

static void medir(void) {
	char cpu[96], ram[96], gpu[96], vram[96], dib[96], tmp[96];
	snprintf(cpu, sizeof cpu, "CPU %s", traco);
	snprintf(ram, sizeof ram, "RAM %s", traco);
	snprintf(gpu, sizeof gpu, "GPU %s", traco);
	snprintf(vram, sizeof vram, "VRAM %s", traco);
	snprintf(tmp, sizeof tmp, "Temp. CPU %s  GPU %s", traco, traco);
	/* espera a la GPU: lo que AoEN pasa mandando triángulos y presentando, sobre el tiempo total */
	unsigned long long esp = gpu_espera_ciclos(), rel = perf_reloj();
	int espera_pct = -1;
	if (reloj_ant && rel > reloj_ant) espera_pct = (int)((esp - espera_ant) * 100 / (rel - reloj_ant));
	espera_ant = esp; reloj_ant = rel;
#ifdef SAGUI_AOEN
	/* CPU: tiques en uso de cada núcleo desde la última medida (como htop) */
	info_nucleo_t nuc[NUC_MAX];
	int nn = info_sistema(INFO_NUCLEOS, nuc, NUC_MAX);
	if (nn > 0) {
		int total_u = 0, total_t = 0, n = MIN(nn, NUC_MAX);
		char por[64] = "";
		int pl = 0;
		for (int i = 0; i < n; ++i) {
			unsigned dtq = i < nnuc_ant ? nuc[i].tiques - nuc_ant[i].tiques : 0, dio = i < nnuc_ant ? nuc[i].tiques_ociosos - nuc_ant[i].tiques_ociosos : 0;
			int uso = dtq ? (int)((dtq > dio ? dtq - dio : 0) * 100 / dtq) : 0;
			total_u += (int)(dtq > dio ? dtq - dio : 0); total_t += (int)dtq;
			if (n > 1 && pl < (int)sizeof por - 6) pl += snprintf(por + pl, sizeof por - (size_t)pl, "%s%d", i ? " " : " [", uso);
		}
		if (n > 1 && pl < (int)sizeof por - 2) snprintf(por + pl, sizeof por - (size_t)pl, "]");
		memcpy(nuc_ant, nuc, sizeof(info_nucleo_t) * (size_t)n); nnuc_ant = n;
		info_sensores_t s;
		char f[32];
		const char *o;
		unsigned khz = sensores_ler(&s) ? sensores_freq_atual(&s, &o) : 0;
		if (khz) snprintf(f, sizeof f, "%u MHz", khz / 1000); else snprintf(f, sizeof f, "%s MHz", traco);
		if (total_t) snprintf(cpu, sizeof cpu, "CPU %d%%%s  %s", total_u * 100 / total_t, por, f);
		else snprintf(cpu, sizeof cpu, "CPU %s%s  %s", traco, por, f);
		int tc = s.qtd_temp ? sensores_temp_maior(&s) : -1;
		char tcs[16], tgs[16];
		if (khz || s.qtd_temp) { if (tc >= -100000 && s.qtd_temp) snprintf(tcs, sizeof tcs, "%d °C", (tc + 500) / 1000); else snprintf(tcs, sizeof tcs, "%s", traco); }
		else snprintf(tcs, sizeof tcs, "%s", traco);
		snprintf(tgs, sizeof tgs, "%s", traco);
		info_gpu_energia_t ge;
		memset(&ge, 0, sizeof ge);
		int gok = info_sistema(INFO_GPU_ENERGIA, &ge, sizeof ge) == 0 && ge.versao == 1 && ge.soporte != GPU_EN_SIN;
		if (gok && ge.temp_mili_c != GPU_EN_SIN_SENSOR) snprintf(tgs, sizeof tgs, "%d °C", (ge.temp_mili_c + 500) / 1000);
		snprintf(tmp, sizeof tmp, "Temp. CPU %s  GPU %s", tcs, tgs);
		/* VRAM: la de la tarjeta (energía de la GPU, D395) y la de la zona 3D (gpu3d) */
		info_gpu3d_t g;
		memset(&g, 0, sizeof g);
		int g3 = info_sistema(INFO_GPU3D, &g, sizeof g) == 0 && g.versao == 1;
		char v1[40], v2[48];
		if (gok && ge.vram_kb) snprintf(v1, sizeof v1, "%u/%u MB", (ge.vram_usada_kb + 1023) / 1024, ge.vram_kb / 1024);
		else snprintf(v1, sizeof v1, "%s", traco);
		if (g3 && g.memoria_kb) snprintf(v2, sizeof v2, "3D %u MB usada, %u libre", (g.memoria_kb - g.livre_kb) / 1024, g.livre_kb / 1024);
		else snprintf(v2, sizeof v2, "3D %s", traco);
		snprintf(vram, sizeof vram, "VRAM %s  %s", v1, v2);
		/* GPU: cuadros y triángulos por segundo del 3D (Administrador de tareas) y la espera de AoEN */
		Uint32 ahora = SDL_GetTicks();
		if (g3 && g.estado == GPU3D_ATIVO) {
			unsigned qs = 0, ts = 0;
			if (g3d_ms_ant && ahora > g3d_ms_ant) {
				qs = (unsigned)((g.quadros + g.reservado[3] - g3d_q_ant) * 1000ull / (ahora - g3d_ms_ant));
				ts = (unsigned)((g.triangulos - g3d_t_ant) * 1000ull / (ahora - g3d_ms_ant));
			}
			char e[24];
			if (espera_pct >= 0 && gpu_ativo_ultimo()) snprintf(e, sizeof e, "%d%%", espera_pct); else snprintf(e, sizeof e, "%s", traco);
			snprintf(gpu, sizeof gpu, "GPU %s: %u cuadros/s, %u mil tri/s, espera %s", g.nome, qs, ts / 1000, e);
		} else if (g3) snprintf(gpu, sizeof gpu, "GPU 3D %s", g.estado == GPU3D_SEM_GPU ? "sin driver" : "apagado");
		g3d_q_ant = g.quadros + g.reservado[3]; g3d_t_ant = g.triangulos; g3d_ms_ant = ahora;
	}
	/* RAM: del sistema (free) y la de este proceso (ps) */
	info_memoria_t m;
	memset(&m, 0, sizeof m);
	if (info_sistema(INFO_MEMORIA, &m, sizeof m) == 0 && m.gerida_kb) {
		static info_processo_t pv[128];
		int np = info_sistema(INFO_PROCESSOS, pv, 128), eu = meu_pid();
		unsigned mio = 0;
		for (int i = 0; i < np && i < 128; ++i) if (pv[i].pid == eu) mio = pv[i].memoria_kb;
		char a[24];
		if (mio) snprintf(a, sizeof a, "%u MB", (mio + 1023) / 1024); else snprintf(a, sizeof a, "%s", traco);
		snprintf(ram, sizeof ram, "RAM %u MB usada, %u libre; AoEN %s", m.usada_kb / 1024, m.livre_kb / 1024, a);
	}
#else
	snprintf(cpu, sizeof cpu, "CPU %s (%d núcleos)", traco, SDL_GetCPUCount());
	if (espera_pct >= 0 && gpu_ativo_ultimo()) snprintf(gpu, sizeof gpu, "GPU espera %d%%", espera_pct);
#endif
	int pres = 0;
	const char *camino = gpu_estado(&pres);
	snprintf(dib, sizeof dib, "Dibujo por %s; presentación %s", gpu_ativo_ultimo() ? "GPU" : camino, pres ? "por la GPU (D432)" : "por la CPU");
	snprintf(H.lin[1], sizeof H.lin[1], "%s", cpu);
	snprintf(H.lin[2], sizeof H.lin[2], "%s", ram);
	snprintf(H.lin[3], sizeof H.lin[3], "%s", gpu);
	snprintf(H.lin[4], sizeof H.lin[4], "%s", vram);
	snprintf(H.lin[5], sizeof H.lin[5], "%s", dib);
	snprintf(H.lin[6], sizeof H.lin[6], "%s", tmp);
	{
		/* D436: el sonido, por si se corta: alta prioridad, veces sin datos y la latencia del kernel */
		int ap = audio_alta_prioridad();
		snprintf(H.lin[7], sizeof H.lin[7], "Audio: alta prioridad %s; sin datos %ld; hueco %u ms; kernel %d ms, %u cortes",
		         ap > 0 ? "sí" : ap < 0 ? "no disponible" : "no", audio_sin_datos(), audio_hueco_max(),
		         audio_latencia_kernel_ms(), audio_cortes_kernel());
	}
	H.nlin = 8;
	H.ok = 1;
	static int vezes;
	if (++vezes == 3) for (int i = 1; i < H.nlin; ++i) AOEN_LOG("[AOEN] HUD: %s", H.lin[i]);   /* para las pruebas, una vez */
}

/* una vez por cuadro (con el HUD visible o no: así la media es buena al encenderlo) */
void hud_rend_cuadro(void) {
	Uint32 ahora = SDL_GetTicks();
	if (t_ant) {
		float ms = (float)(ahora - t_ant);
		ms_cuadro = ms_cuadro ? ms_cuadro * 0.8f + ms * 0.2f : ms;
		ms_suma += ms; ++cuadros_medida;
	}
	t_ant = ahora;
	if (!hud_rend_visible) return;
	if (!t_medida || ahora - t_medida >= 500) {   /* dos veces por segundo */
		if (t_medida && cuadros_medida && ms_suma > 0) {
			fps_atual = (int)(cuadros_medida * 1000.0f / ms_suma + 0.5f);
			fps_soma_total += fps_atual; ++cuadros_total;
		}
		ms_suma = 0; cuadros_medida = 0;
		t_medida = ahora;
		medir();
	}
}

void hud_rend_alternar(void) {
	hud_rend_visible = !hud_rend_visible;
	cfg_set_int("hud_rendimiento", hud_rend_visible);
	fps_soma_total = 0; cuadros_total = 0; t_medida = 0; H.ok = 0;
	AOEN_LOG("[AOEN] HUD de rendimiento %s", hud_rend_visible ? "encendido" : "apagado");
}

void hud_rend_dibujar(void) {
	if (!hud_rend_visible) return;
	if (!H.ok) medir();
	float media = cuadros_total ? (float)(fps_soma_total / (double)cuadros_total) : (float)fps_atual;
	snprintf(H.lin[0], sizeof H.lin[0], "FPS %d (media %.1f)  %.1f ms por cuadro", fps_atual, media, ms_cuadro);
	int w = 0;
	for (int i = 0; i < H.nlin; ++i) w = MAX(w, gfx_t_w(H.lin[i], 1));
	int x = 6, y = top_h * app.us + 6, alto = H.nlin * 17 + 8;
	gfx_fill_a(x, y, w + 12, alto, gfx_rgb(0, 0, 0), 150);
	gfx_rect(x, y, w + 12, alto, gfx_rgb(120, 100, 60));
	for (int i = 0; i < H.nlin; ++i) gfx_ts(x + 6, y + 4 + i * 17, H.lin[i], i == 0 ? gfx_rgb(255, 230, 120) : gfx_rgb(235, 230, 215), 1);
	static int registrado;
	if (!registrado && cuadros_total >= 2) { registrado = 1; AOEN_LOG("[AOEN] HUD: %s", H.lin[0]); }
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* vigia.c - vigilante del bucle principal de Tzar (D417). Código propio.
 *
 * El bucle principal marca un latido por cuadro y, alrededor de cada parte grande (eventos, simulación, guion, interfaz,
 * sonido, dibujo, presentar, carga de un mapa), entra y sale de un «módulo». Un hilo de la SDL mira cada 250 ms: si el latido no
 * cambia en el tiempo pedido, para el audio con la función que le dieron y escribe en el registro la pila de módulos (la parte
 * donde se quedó el bucle) y el último dato. Solo lee lo que escribe el bucle (palabras de 32 bits y punteros a textos
 * estáticos): no necesita cerrojo. */
#include "vigia.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define NIVEIS 8

static volatile uint32_t latido_ms, latidos;
static const char *volatile pilha[NIVEIS];
static volatile int npilha;
static volatile uint32_t dado;
static volatile int sair, ativo;
static int limite_ms;
static TzVigiaParar f_parar;
static TzVigiaRegistro f_reg;
static SDL_Thread *hilo;
static int avisos, parado;
static uint32_t visto_latidos, visto_ms, parado_desde;
static char ultimo[300];

void tz_vigia_latido(void)
{
	latido_ms = SDL_GetTicks();
	latidos++;
}

void tz_vigia_entrar(const char *m)
{
	int n = npilha;
	if (n < NIVEIS) pilha[n] = m;
	npilha = n + 1;
}

void tz_vigia_sair(void)
{
	if (npilha > 0) npilha--;
}

void tz_vigia_dado(uint32_t v) { dado = v; }
int tz_vigia_avisos(void) { return avisos; }
const char *tz_vigia_ultimo(void) { return ultimo; }

void tz_vigia_mirar(uint32_t agora)
{
	uint32_t l = latidos;
	if (l != visto_latidos || !visto_ms) {
		visto_latidos = l; visto_ms = agora;
		if (parado) {
			parado = 0;
			snprintf(ultimo, sizeof ultimo, "[TZAR] vigilante: el juego vuelve tras %u ms; sigue el audio", agora - parado_desde);
			if (f_reg) f_reg(ultimo);
			if (f_parar) f_parar(0);
		}
		return;
	}
	if (parado || limite_ms <= 0 || agora - visto_ms < (uint32_t)limite_ms) return;
	parado = 1;
	parado_desde = visto_ms;
	avisos++;
	char p[200] = "";
	int n = npilha;
	for (int i = 0; i < n && i < NIVEIS; ++i) {
		const char *m = pilha[i];
		snprintf(p + strlen(p), sizeof p - strlen(p), "%s%s", i ? " > " : "", m ? m : "?");
	}
	snprintf(ultimo, sizeof ultimo, "[TZAR] vigilante: el juego no avanza desde hace %u ms (en: %s; dato %u); se para el audio",
	         agora - visto_ms, p[0] ? p : "bucle principal", (unsigned)dado);
	if (f_parar) f_parar(1);
	if (f_reg) f_reg(ultimo);
}

static int correr(void *x)
{
	(void)x;
	while (!sair) {
		SDL_Delay(250);
		if (ativo) tz_vigia_mirar(SDL_GetTicks());
	}
	return 0;
}

int tz_vigia_iniciar(int segundos, TzVigiaParar parar, TzVigiaRegistro registrar)
{
	f_parar = parar; f_reg = registrar;
	limite_ms = segundos * 1000;
	avisos = 0; parado = 0; visto_ms = 0; visto_latidos = 0;
	tz_vigia_latido();
	if (segundos <= 0) return 0;
	sair = 0; ativo = 1;
	hilo = SDL_CreateThread(correr, "tzar-vigia", NULL);
	return hilo ? 0 : -1;
}

void tz_vigia_terminar(void)
{
	ativo = 0;
	sair = 1;
	if (hilo) SDL_WaitThread(hilo, NULL);
	hilo = NULL;
}

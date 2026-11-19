/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Reparto del trabajo en varios núcleos (hilos.c, D393). */
#ifndef AOEN_HILOS_H
#define AOEN_HILOS_H

/* Una tarea se parte en «partes» trozos independientes (franjas de la pantalla, jugadores de la máquina...).
 * Cada parte solo escribe lo suyo, así que el resultado no depende de qué hilo haga cada una ni en qué orden:
 * con 1 hilo o con 8 sale lo mismo, bit a bit. */
typedef void (*TareaFn)(void *ctx, int parte, int partes);

/* Crea los hilos de trabajo: tantos como núcleos (SDL_GetCPUCount), hasta HILOS_MAX. AOEN_HILOS=N (variable de
 * entorno) o «hilos=N» en la configuración fuerzan N (1: sin hilos, el camino de siempre). Devuelve cuántos hay
 * en total contando el principal. Se puede llamar varias veces (solo crea la primera). */
int hilos_iniciar(void);
int hilos_n(void);                 /* hilos que trabajan (1 si no se iniciaron o no hay más núcleos) */
void hilos_parar(void);            /* espera a que acaben los hilos de trabajo y los libera */

/* Ejecuta fn(ctx, k, partes) para k = 0..partes-1, repartido entre los hilos (el principal también trabaja), y
 * vuelve cuando han acabado todas. Con un hilo es un bucle normal. No se puede anidar. */
void hilos_repartir(TareaFn fn, void *ctx, int partes);

/* partes recomendadas para trocear un trabajo homogéneo (unas cuantas por hilo, para repartir bien la carga) */
int hilos_partes(void);

#define HILOS_MAX 8

/* ---- medidas (AOEN_PERFIL=1): tiempo por fase del cuadro, para el banco (main.c) ---- */
/* RECOGER (elegir y ordenar las entidades visibles) y LISTA (reproducir la lista de dibujo) van dentro de ENTIDADES */
enum { PERF_SIM, PERF_IA, PERF_TERRENO, PERF_ENTIDADES, PERF_INTERFAZ, PERF_PRESENTAR, PERF_EVENTOS, PERF_RECOGER, PERF_LISTA, PERF_N };
extern int perf_activo;
unsigned long long perf_reloj(void);         /* contador de alta resolución (TSC en SaguiOS) */
void perf_sumar(int fase, unsigned long long desde);
void perf_empezar(void);                     /* pone a cero las sumas (el banco, al empezar a contar cuadros) */
void perf_informe(int cuadros);              /* escribe en el registro el reparto medio por cuadro */
#define PERF_INI() unsigned long long _perf_t0 = perf_activo ? perf_reloj() : 0
#define PERF_FIN(f) do { if (perf_activo) perf_sumar((f), _perf_t0); } while (0)

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* vigia.h - vigilante del bucle principal de Tzar (D417): un hilo aparte mira que el juego avance. Si el bucle no da señales
 * en un tiempo (TZAR_VIGIA segundos, 8 por defecto; 0 lo apaga), para el audio (para que no se quede sonando lo último) y deja
 * en el registro en qué parte estaba el bucle (la pila de módulos que marca el juego) y cuánto lleva así. Cuando el juego
 * vuelve, el audio sigue y lo apunta. Código propio. */
#ifndef TZAR_VIGIA_H
#define TZAR_VIGIA_H

#include <stdint.h>

/* parar(1) se llama desde el hilo del vigilante al detectar el cuelgue y parar(0) cuando el juego vuelve */
typedef void (*TzVigiaParar)(int parar);
/* registrar: cómo escribir una línea en el registro (desde el hilo del vigilante) */
typedef void (*TzVigiaRegistro)(const char *linha);

int tz_vigia_iniciar(int segundos, TzVigiaParar parar, TzVigiaRegistro registrar);
void tz_vigia_terminar(void);
/* el bucle principal: «sigo vivo» (una vez por cuadro) */
void tz_vigia_latido(void);
/* pila de módulos: entrar en uno (nombre estático) y salir; hasta 8 niveles */
void tz_vigia_entrar(const char *modulo);
void tz_vigia_sair(void);
/* un dato del sitio (el tic del mundo, el reino de la IA...) que sale en el registro */
void tz_vigia_dado(uint32_t v);
/* para las pruebas: cuántas veces saltó y el texto del último aviso */
int tz_vigia_avisos(void);
const char *tz_vigia_ultimo(void);
/* para las pruebas sin hilo: mira una vez como lo haría el hilo con el reloj dado (ms) */
void tz_vigia_mirar(uint32_t agora_ms);

#endif

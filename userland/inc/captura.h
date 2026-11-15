// SPDX-License-Identifier: GPL-2.0-or-later
/* captura.h - entrada de sonido (micrófono) de la libsagui (D292).
 *
 * Interfaz para cualquier programa (documentada en docs/microfono.md):
 *   info_captura_t i;
 *   captura_estado(CAP_FUENTE_REAL, &i);        // i.presente, i.nome, i.n_entradas, i.entradas[k], i.entrada, i.pico...
 *   captura_abrir(CAP_FUENTE_REAL, 44100, 1);   // PCM de 16 bits, tasa de 8000 a 48000 Hz, 1 o 2 canales
 *   n = captura_ler(buf, tam);                  // bytes ya convertidos; 0 = aún no hay (no bloquea)
 *   n = captura_ler_espera(buf, tam, ms);       // espera hasta ms milisegundos a que haya algo
 *   captura_cerrar();
 * Los servicios (de_servico) no pueden grabar (ERRO_ACESSO). Una captura a la vez en todo el sistema. */
#ifndef SAGUI_CAPTURA_H
#define SAGUI_CAPTURA_H
#include "sagui.h"

/* Lo que devuelven las funciones: >= 0 bien; < 0 un ERRO_* del sistema. */
int  captura_estado(unsigned fuente, info_captura_t *i);
int  captura_abrir(unsigned fuente, unsigned taxa, unsigned canais);
int  captura_ler(void *buf, unsigned n);
int  captura_ler_espera(void *buf, unsigned n, unsigned ms);
int  captura_cerrar(void);
int  captura_entrada(unsigned indice);                    /* elige la entrada (valida con la captura cerrada) */
int  captura_ganancia(unsigned pct, bool mudo);           /* 0-100 */

/* Cabecera WAV (PCM de 16 bits) de 44 bytes para "bytes" de datos. */
void captura_wav_cabecera(uint8_t h[44], unsigned taxa, unsigned canais, unsigned bytes);

/* Graba "ms" milisegundos de la fuente en un WAV; 0 = bien, < 0 error. progreso (opcional) recibe los ms grabados y el pico. */
int  captura_gravar_wav(const char *caminho, unsigned fuente, unsigned taxa, unsigned canais, unsigned ms,
                        void (*progreso)(unsigned ms, unsigned pico));
#endif

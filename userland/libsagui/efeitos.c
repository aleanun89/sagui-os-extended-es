// SPDX-License-Identifier: GPL-2.0-or-later
/* efeitos.c - efectos sonoros de los juegos (D090).
 *
 * Los juegos describen cada efecto como una lista de notas (frecuencia y
 * duración), la misma del bip (B3). Con tarjeta de sonido, la lista se vuelve una onda
 * cuadrada suave (que decae a lo largo de cada nota) en un flujo de audio del
 * propio programa, abierto la primera vez; el sonido se mezcla con el resto del
 * sistema. Sin tarjeta, la lista va al altavoz del PC como antes. Un
 * efecto nuevo corta el anterior, como BIP_SUBSTITUIR. */
#include "sagui.h"

#define TAXA 22050

static int fluxo = -2;   /* -2 = todavía no lo intentó; < 0 = sin tarjeta */

int efeito_tocar(const nota_t *notas, int n)
{
    if (fluxo == -2) fluxo = audio_abrir(TAXA, 1, 16);
    if (fluxo < 0) return tocar(notas, n, BIP_SUBSTITUIR);
    audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    static int16_t buf[1024];
    uint32_t fase = 0;
    for (int k = 0; k < n; k++) {
        uint32_t total = (uint32_t)notas[k].ms * TAXA / 1000, passo = (uint32_t)notas[k].freq * 65536u / TAXA;
        for (uint32_t i = 0; i < total;) {
            uint32_t m = 0;
            for (; m < 1024 && i < total; m++, i++) {
                int a = notas[k].freq ? 7000 - (int)(5000 * i / total) : 0;
                buf[m] = (int16_t)((fase >> 15) & 1 ? a : -a);
                fase += passo;
            }
            if (audio_escrever_sem_bloquear(fluxo, buf, m * 2) < (int)(m * 2)) return n;   /* cola llena: corta */
        }
    }
    return n;
}

void efeito_parar(void)
{
    if (fluxo >= 0) audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    else silenciar();
}

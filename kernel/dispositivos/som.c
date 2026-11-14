// SPDX-License-Identifier: GPL-3.0-only
/* som.c - pitido por el altavoz del PC (bonus B3).
 *
 * El altavoz interno del PC se conecta a la salida del canal 2 del PIT, el mismo
 * chip que da el latido del sistema en el canal 0. Programando el canal 2 como
 * generador de onda cuadrada con divisor N, la salida oscila a 1.193.182 / N Hz;
 * el bit 0 del puerto 0x61 libera el reloj del canal 2 y el bit 1 conecta la salida
 * al altavoz. Desactivar el bit 1 calla el sonido.
 *
 * Los programas envían una lista de notas (frecuencia y duración) y vuelven al
 * momento: quien toca es el tic del reloj, que cada 10 ms descuenta el tiempo de la
 * nota actual y pasa a la siguiente. Así un juego no se bloquea mientras el pitido
 * suena. Una lista nueva sustituye a la que esté sonando (un efecto nuevo es
 * más importante que el fin del anterior), a no ser que el programa pida
 * encolar.
 *
 * Diagnóstico: en la primera nota del arranque, el kernel lee la salida del canal 2 por el
 * bit 5 del puerto 0x61 varias veces; si cambia, el canal está oscilando. Eso
 * va al log y muestra que el lado del sistema funcionó, aunque el
 * portátil no conecte esa señal al altavoz. */
#include "som.h"
#include "cpu.h"
#include "portas.h"
#include "console.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define PIT_FREQ    1193182u
#define PIT_CANAL2  0x42
#define PIT_CMD     0x43
#define PORTA_B     0x61
#define MAX_NOTAS   64

static nota_t fila[MAX_NOTAS];
static volatile int inicio, quantas;
static volatile int restante_tiques;   /* de la nota que suena ahora; 0 = parado */
static bool conferido;

static void soar(uint32_t freq)
{
    if (freq < 20 || freq > 20000) {           /* 0 = pausa; fuera de eso ni es audible */
        porta_escreve8(PORTA_B, porta_le8(PORTA_B) & ~3u);
        return;
    }
    uint32_t divisor = PIT_FREQ / freq;
    porta_escreve8(PIT_CMD, 0xB6);              /* canal 2, byte bajo y alto, modo 3 (onda cuadrada) */
    porta_escreve8(PIT_CANAL2, divisor & 0xFF);
    porta_escreve8(PIT_CANAL2, (divisor >> 8) & 0xFF);
    porta_escreve8(PORTA_B, porta_le8(PORTA_B) | 3u);
}

void som_parar(void)
{
    porta_escreve8(PORTA_B, porta_le8(PORTA_B) & ~3u);
    restante_tiques = 0;
}

/* Empieza la siguiente nota de la cola (o calla, si se acabó). Interrupciones desactivadas. */
static void proxima(void)
{
    if (quantas == 0) { som_parar(); klog("[BIP] silencio\n"); return; }
    nota_t n = fila[inicio];
    inicio = (inicio + 1) % MAX_NOTAS;
    quantas--;
    soar(n.freq);
    uint32_t t = (n.ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    restante_tiques = t ? (int)t : 1;
    klog("[BIP] %u Hz %u ms\n", n.freq, n.ms);
}

void som_tique(void)
{
    if (restante_tiques == 0) return;
    if (--restante_tiques == 0) proxima();
}

/* Lee la salida del canal 2 (bit 5 del puerto 0x61) durante 5 ms y cuenta los cambios.
 * El tiempo se mide con el contador del canal 0 (que baja de ~11932 a 0 cada
 * 10 ms a 1,19 MHz), y no por número de vueltas: una lectura de puerto tarda
 * ~1 µs en un PC de verdad, pero bastante menos en un emulador. */
static uint32_t canal0_agora(void)
{
    porta_escreve8(0x43, 0x00);                  /* fija el valor actual del canal 0 */
    uint32_t lo = porta_le8(0x40);
    uint32_t hi = porta_le8(0x40);
    return lo | (hi << 8);
}

static void conferir_canal2(void)
{
    int viradas = 0;
    uint32_t passou = 0, antes_c = canal0_agora();
    uint8_t antes = porta_le8(PORTA_B) & 0x20;
    for (uint32_t guarda = 0; passou < 5966 && guarda < 2000000; guarda++) {   /* 5966 = 5 ms */
        uint8_t agora = porta_le8(PORTA_B) & 0x20;
        if (agora != antes) viradas++;
        antes = agora;
        uint32_t c = canal0_agora();
        passou += c <= antes_c ? antes_c - c : antes_c + 11932 - c;   /* dio la vuelta */
        antes_c = c;
    }
    klog("[BIP] canal 2 do PIT oscilando: %s (%d viradas em 5 ms)\n", viradas >= 2 ? "sim" : "NAO", viradas);
}

/* SIS_BIP (notas, cantidad, modo): modo 0 sustituye lo que suena, 1 encola. */
int32_t sis_bip(uint32_t pnotas, uint32_t n, uint32_t modo, uint32_t d, uint32_t e)
{
    (void)d; (void)e;
    if (n == 0) { bool i0 = int_salva_desliga(); quantas = 0; som_parar(); int_restaura(i0); return 0; }
    if (n > MAX_NOTAS) n = MAX_NOTAS;
    if (!usuario_ok((const void *)pnotas, n * sizeof(nota_t), false)) return ERRO_INVALIDO;
    nota_t copia[MAX_NOTAS];
    memcpy(copia, (const void *)pnotas, n * sizeof(nota_t));
    bool i0 = int_salva_desliga();
    if (modo != BIP_ENFILEIRAR) { inicio = 0; quantas = 0; restante_tiques = 0; }
    for (uint32_t i = 0; i < n && quantas < MAX_NOTAS; i++) {
        fila[(inicio + quantas) % MAX_NOTAS] = copia[i];
        quantas++;
    }
    if (restante_tiques == 0) proxima();
    int_restaura(i0);
    if (!conferido) {
        conferido = true;
        if (copia[0].freq >= 20) conferir_canal2();
    }
    return (int32_t)n;
}

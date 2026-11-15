// SPDX-License-Identifier: GPL-2.0-or-later
/* grabar archivo.wav [segundos] [opciones] - graba el micrófono en un WAV (D292).
 *
 *   grabar nota.wav            5 segundos, 44100 Hz, mono, 16 bits
 *   grabar nota.wav 10         10 segundos
 *   grabar --info              la tarjeta, las entradas, la elegida, la ganancia y el nivel
 *   opciones:
 *     -p, --prueba      usa la fuente de prueba (un tono de 1 kHz del kernel) en vez del micrófono
 *     -r HZ             tasa (8000 a 48000; 44100 por defecto)
 *     -e                estéreo (mono por defecto)
 *     -t MS             la duración en milisegundos (en vez de segundos; para pruebas cortas)
 *     -i N              elige la entrada N (mira --info) y la recuerda
 *     -g N              ganancia de grabación de 0 a 100
 *     --silencio / --activar   calla o vuelve a activar la entrada
 * Sin tarjeta con entrada de sonido dice que no hay micrófono. Los servicios no pueden grabar. */
#include "sagui.h"
#include "captura.h"

static unsigned pico_max;

static void progreso(unsigned ms, unsigned pico)
{
    (void)ms;
    if (pico > pico_max) pico_max = pico;
}

static int info(unsigned fuente)
{
    info_captura_t i;
    int r = captura_estado(fuente, &i);
    if (r < 0 || !i.presente) {
        imprimir("Micrófono: no hay entrada de sonido (%s)\n", r == ERRO_ACESSO ? "no permitido a los servicios" : "sin tarjeta o sin driver");
        return 1;
    }
    imprimir("Tarjeta: %s, %u Hz nativos\n", i.nome, i.taxa);
    for (unsigned k = 0; k < i.n_entradas && k < CAP_ENTRADAS_MAX; k++)
        imprimir("%c %u: %s\n", k == i.entrada ? '*' : ' ', k, i.entradas[k]);
    imprimir("Ganancia: %u %%%s\n", i.ganancia, i.mudo ? " (en silencio)" : "");
    return 0;
}

static int uso(void)
{
    imprimir_em(ERROS, "uso: grabar archivo.wav [segundos] [-p] [-r HZ] [-e] [-i N] [-g N]\n"
                       "     grabar --info [-p]\n");
    return 1;
}

int principal(int argc, char **argv)
{
    unsigned fuente = CAP_FUENTE_REAL, taxa = 44100, canais = 1, seg = 5, ms = 0;
    const char *arq = NULL;
    bool solo_info = false;
    int entrada = -1, ganancia = -1, mudo = -1;
    int n_pos = 0;
    for (int a = 1; a < argc; a++) {
        const char *s = argv[a];
        if (!compara_texto(s, "-p") || !compara_texto(s, "--prueba")) fuente = CAP_FUENTE_PRUEBA;
        else if (!compara_texto(s, "--info")) solo_info = true;
        else if (!compara_texto(s, "-e")) canais = 2;
        else if (!compara_texto(s, "-r") && a + 1 < argc) taxa = (unsigned)texto_para_int(argv[++a]);
        else if (!compara_texto(s, "-t") && a + 1 < argc) ms = (unsigned)texto_para_int(argv[++a]);
        else if (!compara_texto(s, "-i") && a + 1 < argc) entrada = texto_para_int(argv[++a]);
        else if (!compara_texto(s, "-g") && a + 1 < argc) ganancia = texto_para_int(argv[++a]);
        else if (!compara_texto(s, "--silencio")) mudo = 1;
        else if (!compara_texto(s, "--activar")) mudo = 0;
        else if (s[0] == '-') return uso();
        else if (n_pos == 0) { arq = s; n_pos++; }
        else if (n_pos == 1) { seg = (unsigned)texto_para_int(s); n_pos++; }
        else return uso();
    }
    if (entrada >= 0 && fuente == CAP_FUENTE_REAL) {
        int r = captura_entrada((unsigned)entrada);
        if (r < 0) { imprimir_em(ERROS, "grabar: no existe la entrada %d (grabar --info)\n", entrada); return 1; }
    }
    if (ganancia >= 0 || mudo >= 0) {
        info_captura_t i;
        captura_estado(fuente, &i);
        captura_ganancia(ganancia >= 0 ? (unsigned)ganancia : i.ganancia, mudo >= 0 ? mudo != 0 : i.mudo != 0);
    }
    if (solo_info || !arq) return arq || solo_info || entrada >= 0 || ganancia >= 0 || mudo >= 0 ? info(fuente) : uso();
    if (!ms) ms = seg * 1000;
    if (ms < 10 || ms > 3600000 || taxa < 8000 || taxa > 48000) return uso();
    imprimir("Grabando %u ms a %u Hz, %s, en %s...\n", ms, taxa, canais == 1 ? "mono" : "estéreo", arq);
    int r = captura_gravar_wav(arq, fuente, taxa, canais, ms, progreso);
    if (r == ERRO_NAO_EXISTE) { imprimir_em(ERROS, "grabar: no hay micrófono (ninguna tarjeta con entrada de sonido)\n"); return 1; }
    if (r == ERRO_ACESSO) { imprimir_em(ERROS, "grabar: los servicios no pueden grabar\n"); return 1; }
    if (r == ERRO_OCUPADO) { imprimir_em(ERROS, "grabar: otro programa está grabando\n"); return 1; }
    if (r < 0) { imprimir_em(ERROS, "grabar: error %d (%s)\n", r, nome_erro(r)); return 1; }
    imprimir("Listo: %s (pico %u %%)\n", arq, pico_max * 100 / 32767);
    return 0;
}

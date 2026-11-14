// SPDX-License-Identifier: GPL-2.0-or-later
/* volumen [0-100 | +N | -N | silencio | activar] - volumen general del sonido (D088).
 *
 * Sin argumentos dice el volumen y la tarjeta. Un número fija el volumen (y
 * quita el silencio); "silencio" calla todo sin olvidar el volumen;
 * "activar" vuelve a sonar. Es el volumen maestro del mezclador del kernel:
 * cada programa tiene además el de su flujo. */
#include "sagui.h"

static void mostrar(void)
{
    info_audio_t i;
    if (info_sistema(INFO_AUDIO, &i, sizeof i) < 0 || i.estado != AUDIO_FUNCIONANDO) {
        imprimir("Volumen: no hay tarjeta de sonido (solo el altavoz del PC)\n");
        return;
    }
    imprimir("Volumen: %u %%%s\n", i.volume, i.silencio ? " (silencio)" : "");
    imprimir("Tarjeta: %s (%s), %s\n", i.placa, i.driver, i.codec);
    if (i.nfluxos) imprimir("Sonando: %u flujo(s)\n", i.nfluxos);
}

static bool numero(const char *s, int *v)
{
    int n = 0;
    if (!*s) return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        n = n * 10 + (*s - '0');
        if (n > 1000) return false;
    }
    *v = n;
    return true;
}

int principal(int argc, char **argv)
{
    if (argc < 2) { mostrar(); return 0; }
    const char *a = argv[1];
    int v;
    if (audio_volume_mestre(-1) < 0) {
        imprimir_em(ERROS, "volumen: no hay tarjeta de sonido (solo el altavoz del PC)\n");
        return 1;
    }
    if (compara_texto(a, "silencio") == 0) {
        audio_silencio(1);
        imprimir("Volumen: silencio\n");
        return 0;
    }
    if (compara_texto(a, "activar") == 0) {
        audio_silencio(0);
        imprimir("Volumen: %d %%\n", audio_volume_mestre(-1));
        return 0;
    }
    bool rel = a[0] == '+' || a[0] == '-';
    if (!numero(a + rel, &v) || (!rel && v > 100)) {
        imprimir_em(ERROS, "uso: volumen [0-100 | +N | -N | silencio | activar]\n");
        return 1;
    }
    if (rel) v = audio_volume_mestre(-1) + (a[0] == '-' ? -v : v);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    audio_volume_mestre(v);
    audio_silencio(0);
    imprimir("Volumen: %d %%\n", v);
    return 0;
}

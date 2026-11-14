// SPDX-License-Identifier: GPL-2.0-or-later
/* ecualizador - ecualizador de 5 bandas para todo el sonido del sistema (D201).
 *
 *   ecualizador                       muestra las bandas y el preajuste
 *   ecualizador plano|graves|voz|agudos
 *   ecualizador 60=+3 1k=-2 ...       cambia solo esas bandas (60, 250, 1k, 4k, 12k; -12 a +12 dB)
 *   ecualizador --ayuda
 *
 * Los coeficientes se calculan aquí (coma flotante) y el mezclador del kernel los aplica en
 * punto fijo. El ajuste se guarda en /home/.config/sagui.conf (clave ecualizador=) y
 * "configuracion --aplicar" lo restablece al arrancar. */
#include "sagui.h"

static void ayuda(void)
{
    imprimir("uso: ecualizador [plano | graves | voz | agudos | BANDA=DB ...]\n"
             "  Bandas: 60 (graves), 250, 1k, 4k, 12k (agudos), cada una de -12 a +12 dB.\n"
             "  Ejemplos: ecualizador voz        ecualizador 60=+3 1k=-2\n"
             "  Sin argumentos muestra el estado. Se guarda en /home/.config/sagui.conf.\n");
}

static void mostrar(const int *db)
{
    static const char *const nom[EQ_BANDAS] = { "60 Hz", "250 Hz", "1 kHz", "4 kHz", "12 kHz" };
    const char *p = eq_nome_preajuste(db);
    imprimir("Ecualizador: %s\n", p ? p : "personalizado");
    for (int i = 0; i < EQ_BANDAS; i++) { char v[4]; imprimir("  %-7s %s dB\n", nom[i], eq_db_texto(v, db[i])); }
}

/* "60", "250", "1k", "4k", "12k" -> índice de banda, o -1. */
static int banda(const char *s)
{
    for (int i = 0; i < EQ_BANDAS; i++) {
        int f = eq_freq_banda(i);
        char t[12];
        if (f >= 1000) formatar_texto(t, sizeof t, "%dk", f / 1000);
        else formatar_texto(t, sizeof t, "%d", f);
        if (compara_texto(s, t) == 0) return i;
    }
    return -1;
}

int principal(int argc, char **argv)
{
    int db[EQ_BANDAS] = { 0 };
    info_audio_t ia;
    memset(&ia, 0, sizeof ia);
    info_sistema(INFO_AUDIO, &ia, sizeof ia);
    for (int i = 0; i < EQ_BANDAS; i++) db[i] = ia.eq_db[i];
    if (argc >= 2 && (compara_texto(argv[1], "--ayuda") == 0 || compara_texto(argv[1], "-h") == 0)) { ayuda(); return 0; }
    if (argc < 2) {
        if (ia.estado != AUDIO_FUNCIONANDO) { int g[EQ_BANDAS]; if (eq_conf_ler(g) == 0) memcpy(db, g, sizeof db); }
        mostrar(db);
        return 0;
    }
    if (argc == 2 && eq_preajuste(argv[1], db) == 0) { /* preajuste entero */ }
    else {
        for (int a = 1; a < argc; a++) {
            char *eq = acha_char(argv[a], '=');
            if (!eq) { imprimir_em(ERROS, "ecualizador: no entiendo \"%s\" (ecualizador --ayuda)\n", argv[a]); return 2; }
            *eq = 0;
            int b = banda(argv[a]);
            const char *v = eq + 1;
            int neg = 0, n = 0, dig = 0;
            if (*v == '+' || *v == '-') neg = *v++ == '-';
            for (; *v >= '0' && *v <= '9'; v++, dig++) { n = n * 10 + (*v - '0'); if (n > 99) break; }
            if (b < 0 || !dig || *v || n > 12) {
                imprimir_em(ERROS, "ecualizador: \"%s=%s\": banda 60, 250, 1k, 4k o 12k y valor de -12 a +12\n", argv[a], eq + 1);
                return 2;
            }
            db[b] = neg ? -n : n;
        }
    }
    int r = eq_aplicar(db);
    if (r < 0) {
        imprimir_em(ERROS, "ecualizador: no hay tarjeta de sonido (solo el altavoz del PC)\n");
        return 1;
    }
    mostrar(db);
    int g = eq_conf_gravar(db);
    if (g == 1) imprimir("Aviso: /home no es un disco persistente; el cambio vale hasta apagar y no se recordará.\n");
    else if (g < 0) imprimir("Aviso: no se pudo guardar en /home/.config/sagui.conf: %s\n", nome_erro(g));
    else imprimir("Guardado en /home/.config/sagui.conf: se aplicará en cada arranque.\n");
    return 0;
}

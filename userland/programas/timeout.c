// SPDX-License-Identifier: GPL-2.0-or-later
/* timeout - ejecuta una orden y la termina si pasa de un tiempo (D232). */
#include "execucao.h"

static const char *AYUDA =
    "uso: timeout [-k SEG] DURACIÓN ORDEN [argumentos...]\n"
    "  Ejecuta la orden y la termina si sigue viva al pasar la duración (segundos; con s, m o h: 90s, 2m, 1h).\n"
    "  Código de salida: el de la orden; 124 si se agotó el tiempo; 127 si no se encontró la orden.\n"
    "  -k SEG  se acepta por compatibilidad (la orden se termina de golpe)\n";

static bool duracion(const char *s, unsigned *ms)
{
    unsigned v = 0, dec = 0;
    bool punto = false;
    if (!*s) return false;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s - '0'); s++; }
    if (*s == '.') { punto = true; s++; if (*s >= '0' && *s <= '9') dec = (unsigned)(*s++ - '0') * 100; while (*s >= '0' && *s <= '9') s++; }
    unsigned mult = 1000;
    if (*s == 's') { s++; } else if (*s == 'm') { mult = 60000; s++; } else if (*s == 'h') { mult = 3600000; s++; }
    if (*s) return false;
    (void)punto;
    *ms = v * mult + dec * (mult / 1000);
    return true;
}

int principal(int argc, char **argv)
{
    int i = 1;
    if (i < argc && (compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0)) { escrever_texto(SAIDA, AYUDA); return 0; }
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (compara_texto(argv[i], "-k") == 0) i += 2; else if (compara_texto(argv[i], "-s") == 0) i += 2; else i++;
    }
    if (argc - i < 2) { imprimir_em(ERROS, "uso: timeout DURACIÓN ORDEN [argumentos...]  (timeout --help)\n"); return 2; }
    unsigned ms;
    if (!duracion(argv[i], &ms)) { imprimir_em(ERROS, "timeout: %s: duración no válida (ej.: 5, 2.5, 30s, 2m, 1h)\n", argv[i]); return 2; }
    int pid = lanzar_orden("timeout", &argv[i + 1]);
    if (pid < 0) return 127;
    unsigned t0 = tempo_ms();
    for (;;) {
        int codigo = 0;
        int hijo = recolher(&codigo);
        if (hijo == pid) return codigo;
        if (ms && tempo_ms() - t0 >= ms) {
            matar(pid);
            esperar(pid, &codigo);
            return 124;
        }
        dormir(10);
    }
}

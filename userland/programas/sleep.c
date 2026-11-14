// SPDX-License-Identifier: GPL-2.0-or-later
/* sleep (dormir) - espera unos segundos (D097). Acepta decimales (0.5) y sufijos s, m, h. */
#include "sagui.h"

int principal(int argc, char **argv)
{
    if (argc != 2 || compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0) {
        escrever_texto(argc == 2 ? SAIDA : ERROS, "uso: sleep SEGUNDOS[s|m|h]   (ej.: sleep 2, sleep 0.5, sleep 1m)\n");
        return argc == 2 ? 0 : 2;
    }
    const char *s = argv[1];
    unsigned ent = 0, dec = 0, escala = 100;
    if (*s < '0' || *s > '9') { imprimir_em(ERROS, "sleep: %s: tiempo no válido\n", s); return 2; }
    while (*s >= '0' && *s <= '9') ent = ent * 10 + (unsigned)(*s++ - '0');
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { dec += escala * (unsigned)(*s++ - '0'); escala /= 10; }
    }
    unsigned mult = 1;
    if (*s == 'm') { mult = 60; s++; } else if (*s == 'h') { mult = 3600; s++; } else if (*s == 's') s++;
    if (*s) { imprimir_em(ERROS, "sleep: %s: tiempo no válido\n", argv[1]); return 2; }
    unsigned ms = (ent * 1000 + dec) * mult;
    dormir(ms);
    return 0;
}

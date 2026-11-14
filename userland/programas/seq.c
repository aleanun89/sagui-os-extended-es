// SPDX-License-Identifier: GPL-2.0-or-later
/* seq - escribe una secuencia de números enteros (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: seq [-s SEP] [-w] ÚLTIMO | PRIMERO ÚLTIMO | PRIMERO PASO ÚLTIMO\n"
    "  -s SEP  separador entre números (por defecto, salto de línea)\n"
    "  -w      rellena con ceros a la izquierda hasta el mismo ancho\n";

static bool entero(const char *s, int *v)
{
    int sg = 1, r = 0;
    if (*s == '-') { sg = -1; s++; } else if (*s == '+') s++;
    if (!*s) return false;
    for (; *s; s++) { if (*s < '0' || *s > '9') return false; r = r * 10 + (*s - '0'); }
    *v = r * sg;
    return true;
}

int principal(int argc, char **argv)
{
    const char *sep = "\n";
    bool ancho_igual = false;
    opcoes_t o = { 0 };
    int r;
    /* "-3" es un número, no una opción: se reordena a mano */
    char *nums[3];
    int nn = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-s") == 0 && i + 1 < argc) { sep = argv[++i]; continue; }
        if (compara_texto(argv[i], "-w") == 0) { ancho_igual = true; continue; }
        if (compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0 || compara_texto(argv[i], "-h") == 0) {
            escrever_texto(SAIDA, AYUDA);
            return 0;
        }
        int t;
        if (argv[i][0] == '-' && !entero(argv[i], &t)) { o.errada = argv[i]; return opcao_invalida("seq", '?', &o); }
        if (nn < 3) nums[nn++] = argv[i];
        else { imprimir_em(ERROS, "seq: demasiados argumentos (seq --help)\n"); return 2; }
    }
    (void)r;
    int a = 1, paso = 1, b;
    if (nn == 0) { imprimir_em(ERROS, "seq: falta un número (seq --help)\n"); return 2; }
    bool ok = true;
    if (nn == 1) ok = entero(nums[0], &b);
    else if (nn == 2) ok = entero(nums[0], &a) && entero(nums[1], &b);
    else ok = entero(nums[0], &a) && entero(nums[1], &paso) && entero(nums[2], &b);
    if (!ok) { imprimir_em(ERROS, "seq: número no válido (solo se admiten enteros)\n"); return 2; }
    if (paso == 0) { imprimir_em(ERROS, "seq: el paso no puede ser 0\n"); return 2; }
    int ancho = 0;
    if (ancho_igual) {
        char t[16];
        int x = formatar_texto(t, sizeof t, "%d", a), y = formatar_texto(t, sizeof t, "%d", b);
        ancho = x > y ? x : y;
    }
    bool primero = true;
    for (int v = a; paso > 0 ? v <= b : v >= b; v += paso) {
        char t[40];
        int k = 0;
        if (!primero) k += formatar_texto(t, sizeof t, "%s", sep);
        if (ancho) k += formatar_texto(t + k, sizeof t - (size_t)k, "%0*d", ancho, v);
        else k += formatar_texto(t + k, sizeof t - (size_t)k, "%d", v);
        if (escrever_tudo(SAIDA, t, (unsigned)k) < 0) return 0;
        primero = false;
    }
    if (!primero) escrever_tudo(SAIDA, "\n", 1);
    return 0;
}

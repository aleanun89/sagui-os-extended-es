// SPDX-License-Identifier: GPL-2.0-or-later
/* chmod - cambia los permisos de archivos y carpetas (D282, mejora 098).
 * Solo el dueño o root. En FAT32 y en los CD no se puede: sus permisos son los
 * del montaje (montar ... uid=, gid=, umask=). */
#include "permisos_comum.h"

static const char *AYUDA =
    "uso: chmod [-R] MODO archivo...\n"
    "  MODO en octal (755, 644, 1777) o con letras: [ugoa][+-=][rwxt], separados por comas\n"
    "       (u+x, go-w, a=r, u=rwx,go=rx, +t). Sin quién, para todos.\n"
    "  -R  también lo que hay dentro de las carpetas\n"
    "Solo el dueño o root cambian los permisos (ls -l los muestra).\n";

/* Aplica "m" (octal o letras) a "modo". -1 si no se entiende. */
static int novo_modo(const char *m, unsigned modo)
{
    if (*m >= '0' && *m <= '7') {
        unsigned x = 0;
        for (const char *p = m; *p; p++) {
            if (*p < '0' || *p > '7') return -1;
            x = x * 8 + (unsigned)(*p - '0');
        }
        return x > 07777 ? -1 : (int)(x & MODO_PERMISOS);
    }
    const char *p = m;
    while (*p) {
        unsigned quem = 0;
        for (; *p == 'u' || *p == 'g' || *p == 'o' || *p == 'a'; p++)
            quem |= *p == 'u' ? 0700u : *p == 'g' ? 070u : *p == 'o' ? 07u : 0777u;
        if (!quem) quem = 0777;
        char op = *p;
        if (op != '+' && op != '-' && op != '=') return -1;
        p++;
        unsigned bits = 0, peg = 0;
        for (; *p && *p != ','; p++) {
            if (*p == 'r') bits |= 0444;
            else if (*p == 'w') bits |= 0222;
            else if (*p == 'x') bits |= 0111;
            else if (*p == 't') peg = MODO_PEGAJOSO;
            else return -1;
        }
        bits &= quem;
        if (op == '+') modo |= bits | peg;
        else if (op == '-') modo &= ~(bits | peg);
        else modo = (modo & ~quem & ~(unsigned)MODO_PEGAJOSO) | bits | peg;
        if (*p == ',') p++;
    }
    return (int)(modo & MODO_PERMISOS);
}

static int aplicar(const char *c, void *dados)
{
    info_permisos_t p;
    int r = permisos_arquivo(c, &p);
    if (r < 0) return r;
    int m = novo_modo((const char *)dados, p.modo);
    if (m < 0) return ERRO_INVALIDO;
    return mudar_permisos(c, (unsigned)m);
}

int principal(int argc, char **argv)
{
    bool recursivo = false;
    const char *modo = NULL;
    int primeiro = 0, erros = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--help") == 0 || compara_texto(argv[i], "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(argv[i], "-R") == 0 && !modo) { recursivo = true; continue; }
        if (!modo) { modo = argv[i]; continue; }
        if (!primeiro) primeiro = i;
    }
    if (!modo || !primeiro) { imprimir_em(ERROS, "chmod: falta el modo o el archivo (chmod --help)\n"); return 2; }
    if (novo_modo(modo, 0) < 0) { imprimir_em(ERROS, "chmod: modo no válido: %s\n", modo); return 2; }
    for (int i = primeiro; i < argc; i++)
        erros += aplicar_em("chmod", argv[i], recursivo, aplicar, (void *)modo, 0);
    return erros ? 1 : 0;
}

// SPDX-License-Identifier: GPL-3.0-only
/* caminho.c - resolución de rutas.
 *
 * En vez de manipular texto con ".." en medio, la ruta se trata como una
 * pila de componentes: cada nombre apila, ".." desapila, "." y barras
 * repetidas se ignoran. La pila es la propia cadena de salida, y
 * "desapilar" es volver hasta la barra anterior. */
#include "caminho.h"
#include "texto.h"

static bool empilhar_componentes(char *saida, size_t max, size_t *tam, const char *s)
{
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *ini = s;
        while (*s && *s != '/') s++;
        size_t n = (size_t)(s - ini);
        if (n == 1 && ini[0] == '.')
            continue;
        if (n == 2 && ini[0] == '.' && ini[1] == '.') {
            while (*tam > 0 && saida[*tam - 1] != '/') (*tam)--;   /* borra el nombre */
            if (*tam > 0) (*tam)--;                                 /* y la barra */
            continue;
        }
        if (*tam + 1 + n + 1 > max)
            return false;
        saida[(*tam)++] = '/';
        memcpy(saida + *tam, ini, n);
        *tam += n;
    }
    return true;
}

bool caminho_resolver(const char *atual, const char *alvo, char *saida, size_t max)
{
    if (max < 2)
        return false;
    size_t tam = 0;
    if (alvo[0] != '/' && !empilhar_componentes(saida, max, &tam, atual))
        return false;
    if (!empilhar_componentes(saida, max, &tam, alvo))
        return false;
    if (tam == 0)
        saida[tam++] = '/';
    saida[tam] = 0;
    return true;
}

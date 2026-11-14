// SPDX-License-Identifier: GPL-2.0-or-later
/* ambiente.c - variables de entorno (D037).
 *
 * El kernel copia el vector "NOME=valor" de quien llamó a executar a la pila
 * del programa nuevo; _inicio (inicio.asm) guarda la dirección en environ.
 * executar() pasa adelante el environ actual, así que quien cambia environ (la
 * shell) cambia el entorno de los programas que inicia. */
#include "sagui.h"

char **environ;

const char *obter_env(const char *nome)
{
    if (!environ || !nome) return NULL;
    size_t n = tam_texto(nome);
    for (char **e = environ; *e; e++)
        if (compara_texto_n(*e, nome, n) == 0 && (*e)[n] == '=')
            return *e + n + 1;
    return NULL;
}

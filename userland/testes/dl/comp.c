// SPDX-License-Identifier: GPL-3.0-only
/* comp.c - biblioteca de prueba libcomp.so de las páginas compartidas (D378, fase 3; h390).
 *
 * Se compila dos veces, con VERSAO "A" y "B": las dos .so miden lo mismo y solo cambia una
 * letra de un texto de solo lectura. Así la prueba puede sustituir el fichero en el disco
 * por otro del mismo tamaño (y con la misma fecha) y comprobar que el kernel no da a un
 * proceso nuevo las páginas viejas. Lleva 256 KB de datos de solo lectura (compartidos) y un
 * contador en .data (privado de cada proceso). */
#ifndef VERSAO
#define VERSAO "A"
#endif

const char comp_versao[] = "COMP-VERSAO-" VERSAO;
const unsigned comp_grande[65536] = { [0 ... 65535] = 0x5A5A5A5Au };
int comp_contador = 7;

const char *comp_texto(void) { return comp_versao; }

int comp_inc(void) { return ++comp_contador; }

/* Lee todas las páginas de comp_grande (una palabra de cada 16). */
unsigned comp_soma(void)
{
    unsigned s = 0;
    for (unsigned i = 0; i < 65536; i += 16) s += comp_grande[i];
    return s;
}

void *comp_endereco_codigo(void) { return (void *)comp_inc; }

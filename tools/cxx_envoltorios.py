#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""cxx_envoltorios.py - las cabeceras de la libc común vistas desde C++ (D228).

La libc común (userland/libc, D210) está escrita para C: sus cabeceras no
llevan «extern "C"», y en C++ sus funciones se declararían con nombres
decorados (memcpy pasaría a ser _Z6memcpyPvPKvj) y no enlazarían. En vez de
tocar cada cabecera, el Makefile genera con este guion, en build/, una
cabecera envoltorio por cada una de la libc, con el mismo nombre:

    #pragma GCC system_header
    extern "C" {
    #include_next <string.h>
    }

y la pone en el -I del código C++ antes que la libc. #include_next sigue
buscando a partir de la carpeta del envoltorio, así que encuentra la de la
libc. Si una cabecera de la libc incluye otra, entra por su envoltorio (un
«extern "C"» dentro de otro no cambia nada).

Uso: cxx_envoltorios.py CARPETA_LIBC CARPETA_SALIDA
"""
import os
import sys


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    origem, destino = sys.argv[1], sys.argv[2]
    n = 0
    for raiz, _, ficheiros in os.walk(origem):
        for f in sorted(ficheiros):
            if not f.endswith('.h'):
                continue
            rel = os.path.relpath(os.path.join(raiz, f), origem).replace(os.sep, '/')
            saida = os.path.join(destino, rel)
            os.makedirs(os.path.dirname(saida), exist_ok=True)
            texto = ('/* Generado por tools/cxx_envoltorios.py (D228): <%s> de la libc\n'
                     ' * común con enlace de C para el código C++. No editar. */\n'
                     '#pragma GCC system_header\n'
                     'extern "C" {\n'
                     '#include_next <%s>\n'
                     '}\n') % (rel, rel)
            # Solo se reescribe si cambia: así make no recompila todo el C++.
            try:
                with open(saida, encoding='utf-8') as fh:
                    if fh.read() == texto:
                        n += 1
                        continue
            except OSError:
                pass
            with open(saida, 'w', encoding='utf-8') as fh:
                fh.write(texto)
            n += 1
    print('cxx_envoltorios: %d cabeceras en %s' % (n, destino))
    return 0


if __name__ == '__main__':
    sys.exit(main())

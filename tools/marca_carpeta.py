#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""marca_carpeta.py - una marca que cambia cuando cambia el contenido de unas carpetas (D228).

    marca_carpeta.py MARCA TEXTO CARPETA [CARPETA...]

Escribe en MARCA el TEXTO y la lista de ficheros de las CARPETAS (ruta, tamaño
y fecha), pero solo si es distinta de lo que ya tenía: así su fecha cambia
solo cuando algo cambió y el Makefile puede depender de ella. Lo usa la
partición de juegos (input_share/games y los datos de OpenJazz), cuyos
nombres de fichero pueden llevar espacios, que make no sabe poner como
dependencias. Una carpeta que no existe cuenta como vacía.
"""
import os
import sys


def main():
    if len(sys.argv) < 3:
        sys.stderr.write(__doc__)
        return 2
    marca, texto, pastas = sys.argv[1], sys.argv[2], sys.argv[3:]
    linhas = [texto]
    for pasta in pastas:
        linhas.append('# ' + pasta)
        if not pasta or not os.path.isdir(pasta):
            continue
        for raiz, dirs, ficheiros in os.walk(pasta):
            dirs.sort()
            for f in sorted(ficheiros):
                c = os.path.join(raiz, f)
                try:
                    st = os.stat(c)
                except OSError:
                    continue
                linhas.append('%s\t%d\t%d' % (os.path.relpath(c, pasta), st.st_size, int(st.st_mtime)))
    novo = '\n'.join(linhas) + '\n'
    try:
        with open(marca, encoding='utf-8', errors='surrogateescape') as f:
            if f.read() == novo:
                return 0
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(marca)), exist_ok=True)
    with open(marca, 'w', encoding='utf-8', errors='surrogateescape') as f:
        f.write(novo)
    return 0


if __name__ == '__main__':
    sys.exit(main())

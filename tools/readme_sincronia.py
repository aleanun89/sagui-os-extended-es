#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Comprueba que las traducciones del README están al día con README.md.

README.md (español) es la fuente. Cada traducción (README_PT.md, README_EN.md) lleva en su
primera línea un comentario HTML con el hash del último commit que tocó README.md cuando se
tradujo:

    <!-- Traducción de README.md en el commit a8bf0167 (sincronía: tools/readme_sincronia.py) -->

Este guion compara ese hash con el último commit que toca README.md (`git log -1 -- README.md`).
Si difieren, imprime qué traducción está desfasada y el resumen de cambios (`git diff --stat`);
con `--diff` imprime también el diff completo, que es lo que hay que traducir. Sale con código 1
si alguna traducción está desactualizada y con 0 si todas están al día.

Solo mira commits: los cambios en README.md sin commitear no cuentan.
"""
import io
import os
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUENTE = 'README.md'
TRADUCCIONES = ['README_PT.md', 'README_EN.md']
PATRON = re.compile(r'Traducción de README\.md en el commit ([0-9a-f]{7,40})')


def git(*argumentos, comprobar=True):
    resultado = subprocess.run(['git', '-C', RAIZ] + list(argumentos),
                               capture_output=True, text=True)
    if comprobar and resultado.returncode != 0:
        raise RuntimeError(resultado.stderr.strip() or 'fallo de git')
    return resultado.stdout


def ultimo_commit_readme():
    return git('log', '-1', '--format=%h', '--', FUENTE).strip()


def hash_de_traduccion(ruta):
    with io.open(ruta, encoding='utf-8') as f:
        primera = f.readline()
    coincidencia = PATRON.search(primera)
    return coincidencia.group(1) if coincidencia else None


def mismo_commit(a, b):
    n = min(len(a), len(b))
    return n > 0 and a[:n] == b[:n]


def main():
    ver_diff = '--diff' in sys.argv[1:]
    actual = ultimo_commit_readme()
    if not actual:
        print(f'{FUENTE} no tiene commits: no hay nada que comparar.')
        return 1

    desfasadas = 0
    for nombre in TRADUCCIONES:
        ruta = os.path.join(RAIZ, nombre)
        if not os.path.exists(ruta):
            print(f'{nombre} no existe.')
            desfasadas += 1
            continue
        origen = hash_de_traduccion(ruta)
        if origen is None:
            print(f'{nombre}: falta el hash de sincronía en la primera línea.')
            desfasadas += 1
            continue
        if mismo_commit(origen, actual):
            print(f'{nombre} al día (traducido de {origen}).')
            continue

        desfasadas += 1
        print(f'{nombre} desactualizado: traducido de {origen}, {FUENTE} va por {actual}')
        try:
            print(git('diff', '--stat', f'{origen}..HEAD', '--', FUENTE), end='')
            if ver_diff:
                print(git('diff', f'{origen}..HEAD', '--', FUENTE), end='')
        except RuntimeError as error:
            print(f'  (no se puede calcular el diff desde {origen}: {error})')

    if desfasadas:
        print(f'{desfasadas} traducción(es) por actualizar. Ver tools/readme_sincronia.py --diff.')
        return 1
    print('Traducciones del README al día.')
    return 0


if __name__ == '__main__':
    sys.exit(main())

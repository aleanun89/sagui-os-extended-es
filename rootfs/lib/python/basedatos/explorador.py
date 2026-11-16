# SPDX-License-Identifier: GPL-2.0-or-later
# Explorador de bases de datos (mejora 127, D304): desde la v8.4.4 (mejora 131, D333) son dos aplicaciones,
# `explorador_sqlite.py` (relacional) y `explorador_litemonkey.py` (documental). Este archivo queda como alias
# para no romper los lanzadores, las pruebas ni los ejemplos antiguos:
#
#     python /lib/python/basedatos/explorador.py [fichero.db | litemonkey://host:7979/base]
#
# Una dirección litemonkey:// (o lmk://) abre el Explorador lite-monkey; todo lo demás, el Explorador SQLite.
import sys


def principal(argv):
    if any(a.startswith("litemonkey://") or a.startswith("lmk://") for a in argv[1:]):
        from basedatos import explorador_litemonkey
        explorador_litemonkey.principal(argv)
    else:
        from basedatos import explorador_sqlite
        explorador_sqlite.principal(argv)


if __name__ == "__main__":
    principal(sys.argv)

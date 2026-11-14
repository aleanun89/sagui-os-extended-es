#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# hola.sh - el primer script de SaguiOS.
# Ejecútalo con:  /ejemplos/hola.sh Tito Ana   (o: sagsh /ejemplos/hola.sh)

nombre=${1:-mundo}
echo "¡Hola, $nombre!"
echo "Este script se llama $0 y ha recibido $# argumentos."

n=1
for a in "$@"; do
    echo "  argumento $n: $a"
    n=$((n + 1))
done

programas=0
for p in /bin/*; do
    programas=$((programas + 1))
done
echo "En /bin hay $programas programas."

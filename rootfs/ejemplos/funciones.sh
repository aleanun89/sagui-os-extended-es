#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# funciones.sh - funciones, recursión, while, case y aritmética.

factorial() {
    if [ "$1" -le 1 ]; then
        echo 1
    else
        local resto=$(factorial $(($1 - 1)))
        echo $(($1 * resto))
    fi
}

tipo() {
    case $1 in
        *.c)   echo "$1: programa en C" ;;
        *.py)  echo "$1: programa en Python" ;;
        *.sh)  echo "$1: script de sagsh" ;;
        *)     echo "$1: otra cosa" ;;
    esac
}

i=1
while [ $i -le 6 ]; do
    echo "$i! = $(factorial $i)"
    i=$((i + 1))
done

for f in /ejemplos/*; do
    tipo "$f"
done

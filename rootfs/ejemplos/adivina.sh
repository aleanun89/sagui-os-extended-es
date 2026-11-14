#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# adivina.sh - un juego: lee números con "read" hasta acertar.
# El número secreto sale de los segundos del reloj (o del primer argumento).

fecha=$(data)              # p. ej. "viernes, 06/11/2026 12:34:56"
segundos=${fecha##*:}      # lo que va tras los últimos dos puntos: "56"
segundos=${segundos#0}     # "07" -> "7" (con un 0 delante sería octal)
secreto=${1:-$(( segundos % 50 + 1 ))}
intentos=0
echo "He pensado un número del 1 al 50. ¿Cuál es?"
while read -p "> " n; do
    intentos=$((intentos + 1))
    case $n in
        ''|*[!0-9]*) echo "Eso no es un número." ; continue ;;
    esac
    if [ "$n" -lt "$secreto" ]; then
        echo "Más alto."
    elif [ "$n" -gt "$secreto" ]; then
        echo "Más bajo."
    else
        echo "¡Acertaste en $intentos intentos!"
        exit 0
    fi
done
echo "Hasta otra."

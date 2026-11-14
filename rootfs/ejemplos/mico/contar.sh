#!/bin/sh
# contar.sh - cuenta hasta $1 (por defecto 3)
fin=${1:-3}
i=1
while [ $i -le $fin ]; do
    echo "vuelta $i de $fin"
    i=$((i + 1))
done

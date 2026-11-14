#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Compila cc y pascal como programas del anfitrión (64 bits) en el directorio $1,
# para probar el generador sin QEMU. Uso: tools/hospedeiro/compilar.sh DESTINO
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)
D=${1:-/tmp/hospedeiro}
mkdir -p "$D"
for c in cc pascal; do
  gcc -w -O1 -I "$R/tools/hospedeiro" -I "$R/kernel/inc" -I "$R/userland/programas" \
      -o "$D/h_$c" "$R/userland/programas/$c.c" "$R/tools/hospedeiro/fonte.c" \
      -Dprincipal=main
done

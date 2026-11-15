#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# firmware_r5xx.sh - baja y comprueba el microcódigo del procesador de órdenes (CP)
# de las Radeon R5xx (X1300 a X1950) para el 3D por la GPU (D287).
#
# El archivo (R520_cp.bin, 2048 bytes; vale para RV515, R520, RV530, RV560, RV570 y
# R580) es de AMD y viene de linux-firmware: se puede redistribuir sin cambios, solo
# en binario (LICENSE.radeon), pero no es libre. Va en git, en firmware/radeon/, con
# su licencia al lado (D073 punto 5, D287), para que la imagen se construya sin red;
# este guion sirve para rehacerlo o comprobarlo desde la etiqueta fija de
# linux-firmware, con su SHA-256. `make SEM_FIRMWARE=1` no lo mete en la imagen y el
# 3D de las R5xx dice que falta el microcódigo.
#
#   tools/firmware_r5xx.sh [--comprobar]
#
#   --comprobar  solo comprueba los SHA-256 de lo que hay en firmware/radeon/
set -euo pipefail

ETIQUETA=20241110
BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/firmware/radeon"

# archivo de destino, ruta en linux-firmware, SHA-256
LISTA="
R520_cp.bin radeon/R520_cp.bin 830ce72c50d53c0f324242cdffede681deae05e9b9778d7e75764ffc37647d22
LICENSE.radeon.txt LICENSE.radeon fc6223d4bfe9f2f9e2eddc44b9fe5721d0caf49f01cb08d602906add686d8c6f
"

comprobar() {
    local f="$1" esperado="$2"
    [ -f "$f" ] || { echo "firmware_r5xx.sh: falta $f" >&2; return 1; }
    local real
    real="$(sha256sum "$f" | cut -d' ' -f1)"
    if [ "$real" != "$esperado" ]; then
        echo "firmware_r5xx.sh: SHA-256 de $f no cuadra ($real)" >&2
        return 1
    fi
}

if [ "${1:-}" = "--comprobar" ]; then
    echo "$LISTA" | while read -r nome ruta sha; do
        [ -n "$nome" ] || continue
        comprobar "$DEST/$nome" "$sha"
    done
    echo "firmware_r5xx.sh: microcódigo y licencia correctos"
    exit 0
fi

mkdir -p "$DEST"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
echo "$LISTA" | while read -r nome ruta sha; do
    [ -n "$nome" ] || continue
    curl -sfL -o "$tmp/$nome" "$BASE/$ruta?h=$ETIQUETA"
    comprobar "$tmp/$nome" "$sha"
    cp "$tmp/$nome" "$DEST/"
done
echo "firmware_r5xx.sh: microcódigo de linux-firmware $ETIQUETA en $DEST"

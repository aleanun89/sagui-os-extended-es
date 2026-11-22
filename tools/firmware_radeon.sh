#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# firmware_radeon.sh - baja y comprueba el microcódigo del procesador de órdenes
# (CP) de las Radeon R7xx (D284), R6xx (D357) y Evergreen/NI sin Cayman (D427) para el 3D por la GPU.
#
# Los archivos (RV710/RV730/RV770 _pfp.bin y _me.bin, unos 27 KB; R600/RV610/RV630/
# RV670 _pfp.bin y _me.bin, unos 94 KB; D427: CEDAR, REDWOOD, JUNIPER, CYPRESS, BARTS,
# TURKS y CAICOS _pfp.bin y _me.bin, unos 70 KB) son de
# AMD y vienen de linux-firmware: se pueden redistribuir sin cambios, solo en
# binario (LICENSE.radeon), pero no son libres. Van en git, en firmware/radeon/,
# con su licencia al lado (D073 punto 5, D284), para que la imagen se construya
# sin red; este guion sirve para rehacerlos o comprobarlos desde la etiqueta fija
# de linux-firmware, con su SHA-256. `make SEM_FIRMWARE=1` no los mete en la
# imagen y el 3D por la GPU dice que falta el microcódigo.
#
#   tools/firmware_radeon.sh [--comprobar]
#
#   --comprobar  solo comprueba los SHA-256 de lo que hay en firmware/radeon/
set -euo pipefail

ETIQUETA=20241110
BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/firmware/radeon"

# archivo de destino, ruta en linux-firmware, SHA-256
LISTA="
RV710_pfp.bin radeon/RV710_pfp.bin 7fd6822683b36a31173b42faa12a1cf2ab99b90db6bbca5fa8ca72ecedc833d5
RV710_me.bin radeon/RV710_me.bin 91f43071fd815b2d89722338923b6dc7e201752a2cab28d33b3b5152d251aa13
RV730_pfp.bin radeon/RV730_pfp.bin 7fd6822683b36a31173b42faa12a1cf2ab99b90db6bbca5fa8ca72ecedc833d5
RV730_me.bin radeon/RV730_me.bin 8e8434837a357e183bb7d6ec6b055bf3b99d83ce3bffb076acf575f49d6ef15c
RV770_pfp.bin radeon/RV770_pfp.bin bdaefcfb54b5fec41aa8b3de8fe98ecfab57616db40cf6107d0609ebe15dc966
RV770_me.bin radeon/RV770_me.bin 99c54f6a5a8e30cf57135cdd44e11d7252f5274ac1f5145e5f8c3b2995f629d0
R600_pfp.bin radeon/R600_pfp.bin 87e8cf139b918d34389e425cde183e6f45d6aa5692b7f6ea56c9657539af81eb
R600_me.bin radeon/R600_me.bin 94ee102cbffdd94d34c823f72280f0455fa814bb2251087a92de01c4a891b014
RV610_pfp.bin radeon/RV610_pfp.bin c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246
RV610_me.bin radeon/RV610_me.bin 566508c94ab890868242298a338e9362e893115a349f8548d26b9280ef9f4b4f
RV630_pfp.bin radeon/RV630_pfp.bin c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246
RV630_me.bin radeon/RV630_me.bin 2a7e310ab6fd2d8c466ddbb394ae0da26303b1da9f924c04d7cbc4f47836e9e0
RV670_pfp.bin radeon/RV670_pfp.bin c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246
RV670_me.bin radeon/RV670_me.bin 7630b5499e2f73b81098dac53b047480cb58bcba07e6604d2cee481ed1c7bc7b
CEDAR_pfp.bin radeon/CEDAR_pfp.bin bf614bcdd72fd23a919d2bf39ec065824f0c327e50b9ba3d9d39cb675764fcf3
CEDAR_me.bin radeon/CEDAR_me.bin 86cec272c18673356c0e8d774d7ed48976baa631f79734225522a928e9089db4
REDWOOD_pfp.bin radeon/REDWOOD_pfp.bin bf614bcdd72fd23a919d2bf39ec065824f0c327e50b9ba3d9d39cb675764fcf3
REDWOOD_me.bin radeon/REDWOOD_me.bin bc98d6afb1fa179dc2113e0c16bb609f4d395020cfb7e47652304ccf5206cade
JUNIPER_pfp.bin radeon/JUNIPER_pfp.bin 5f4e98e1bd41bbbba68fb4c1b8761ab0fd7c2f9f4f088c44b1464b7c2e08a9e2
JUNIPER_me.bin radeon/JUNIPER_me.bin 3890437894d09dbb0b99900e661a8a0d3ad264a650b6ad2d84e5b8e1156cc9a0
CYPRESS_pfp.bin radeon/CYPRESS_pfp.bin 5f4e98e1bd41bbbba68fb4c1b8761ab0fd7c2f9f4f088c44b1464b7c2e08a9e2
CYPRESS_me.bin radeon/CYPRESS_me.bin 3890437894d09dbb0b99900e661a8a0d3ad264a650b6ad2d84e5b8e1156cc9a0
BARTS_pfp.bin radeon/BARTS_pfp.bin c9cfed97f54c24190761524e2fbbab80ff1ac07f09d84bb3c955c2337ea7e4f6
BARTS_me.bin radeon/BARTS_me.bin 37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b
TURKS_pfp.bin radeon/TURKS_pfp.bin 65d98665384252ddd627c365dbfd2f238b0538570d9e1dad6d4382bb93723089
TURKS_me.bin radeon/TURKS_me.bin 37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b
CAICOS_pfp.bin radeon/CAICOS_pfp.bin 210b9c42cf7866fcacf9239016142e799f52901ef3b001f7081fc92ff2c43401
CAICOS_me.bin radeon/CAICOS_me.bin 37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b
LICENSE.radeon.txt LICENSE.radeon fc6223d4bfe9f2f9e2eddc44b9fe5721d0caf49f01cb08d602906add686d8c6f
"

comprobar() {
    local f="$1" esperado="$2"
    [ -f "$f" ] || { echo "firmware_radeon.sh: falta $f" >&2; return 1; }
    local real
    real="$(sha256sum "$f" | cut -d' ' -f1)"
    if [ "$real" != "$esperado" ]; then
        echo "firmware_radeon.sh: SHA-256 de $f no cuadra ($real)" >&2
        return 1
    fi
}

if [ "${1:-}" = "--comprobar" ]; then
    echo "$LISTA" | while read -r nome ruta sha; do
        [ -n "$nome" ] || continue
        comprobar "$DEST/$nome" "$sha"
    done
    echo "firmware_radeon.sh: microcódigo y licencia correctos"
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
echo "firmware_radeon.sh: microcódigo de linux-firmware $ETIQUETA en $DEST"

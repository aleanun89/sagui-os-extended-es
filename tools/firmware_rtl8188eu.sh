#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# firmware_rtl8188eu.sh - baja y comprueba el firmware del pincho Wi-Fi
# Realtek RTL8188EU (D215).
#
# El firmware (rtl8188eufw.bin, unos 15 KB) es de Realtek y viene de
# linux-firmware: se puede redistribuir sin cambios (LICENCE.rtlwifi_firmware.txt)
# pero no es libre. Va en git, en firmware/realtek/, con su licencia al lado
# (D073, D215), para que la imagen se construya sin red; este guion sirve para
# rehacerlo o comprobarlo desde la etiqueta fija de linux-firmware, con su
# SHA-256. `make SEM_FIRMWARE=1` no lo mete en la imagen.
#
#   tools/firmware_rtl8188eu.sh [--comprobar]
#
#   --comprobar  solo comprueba los SHA-256 de lo que hay en firmware/realtek/
set -euo pipefail

ETIQUETA=20241110
BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"
SHA_FW=2ff74315287529dec2e50eb57d6e0c97d2116f28ae166773ccdf93b6360000c4
SHA_LIC=a61351665b4f264f6c631364f85b907d8f8f41f8b369533ef4021765f9f3b62e

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/firmware/realtek"

comprobar() {
    local f="$1" esperado="$2"
    [ -f "$f" ] || { echo "firmware_rtl8188eu.sh: falta $f" >&2; return 1; }
    local real
    real="$(sha256sum "$f" | cut -d' ' -f1)"
    if [ "$real" != "$esperado" ]; then
        echo "firmware_rtl8188eu.sh: SHA-256 de $f no cuadra ($real)" >&2
        return 1
    fi
}

if [ "${1:-}" = "--comprobar" ]; then
    comprobar "$DEST/rtl8188eufw.bin" "$SHA_FW"
    comprobar "$DEST/LICENCE.rtlwifi_firmware.txt" "$SHA_LIC"
    echo "firmware_rtl8188eu.sh: firmware y licencia correctos"
    exit 0
fi

mkdir -p "$DEST"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
curl -sfL -o "$tmp/rtl8188eufw.bin" "$BASE/rtlwifi/rtl8188eufw.bin?h=$ETIQUETA"
curl -sfL -o "$tmp/LICENCE.rtlwifi_firmware.txt" "$BASE/LICENCE.rtlwifi_firmware.txt?h=$ETIQUETA"
comprobar "$tmp/rtl8188eufw.bin" "$SHA_FW"
comprobar "$tmp/LICENCE.rtlwifi_firmware.txt" "$SHA_LIC"
cp "$tmp/rtl8188eufw.bin" "$tmp/LICENCE.rtlwifi_firmware.txt" "$DEST/"
echo "firmware_rtl8188eu.sh: firmware de linux-firmware $ETIQUETA en $DEST"

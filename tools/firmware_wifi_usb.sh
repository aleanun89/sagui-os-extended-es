#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# firmware_wifi_usb.sh - baja y comprueba el firmware de los pinchos Wi-Fi
# USB experimentales (D409): Realtek RTL8188CU/8192CU y Ralink RT2870/RT3070/RT5370.
#
# Vienen de linux-firmware (etiqueta fija), se pueden redistribuir sin cambios con
# su licencia pero no son libres. Van en git, en firmware/<fabricante>/, con la
# licencia al lado (D073), para que la imagen se construya sin red; este guion
# sirve para rehacerlos o comprobarlos. `make SEM_FIRMWARE=1` no los mete.
# El del Atheros AR9271 (htc_9271.fw de open-ath9k-htc-firmware) es libre y no se
# integra todavía (D409).
#
#   tools/firmware_wifi_usb.sh [--comprobar]
set -euo pipefail

ETIQUETA=20241110
BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"

# destino|origen en linux-firmware|SHA-256
LISTA="
realtek/rtl8192cufw_A.bin|rtlwifi/rtl8192cufw_A.bin|32846065b017c8b1d2d448e8c0ea2f5e741424809be49f2c6a6261339cd0b0cf
realtek/rtl8192cufw_TMSC.bin|rtlwifi/rtl8192cufw_TMSC.bin|c083f2db36ef33d0ffffc979aba1d40f55fa9dc38dce50f26e0108a21c080286
ralink/rt2870.bin|rt2870.bin|251b8918391eac6415d60dca239e415aad0177e885376f2a17782e64fcbbe317
ralink/LICENCE.ralink-firmware.txt|LICENCE.ralink-firmware.txt|d7bec70668ddd4aae8fb4aa32870e54b49fcdb0b9b007aa9f54b53a1ac7461bd
"

comprobar() {
    local f="$1" esperado="$2"
    [ -f "$f" ] || { echo "firmware_wifi_usb.sh: falta $f" >&2; return 1; }
    local real
    real="$(sha256sum "$f" | cut -d' ' -f1)"
    [ "$real" = "$esperado" ] || { echo "firmware_wifi_usb.sh: SHA-256 de $f no cuadra ($real)" >&2; return 1; }
}

if [ "${1:-}" = "--comprobar" ]; then
    while IFS='|' read -r dest orig sha; do
        [ -n "$dest" ] || continue
        comprobar "$RAIZ/firmware/$dest" "$sha"
    done <<< "$LISTA"
    "$RAIZ/tools/firmware_rtl8188eu.sh" --comprobar
    echo "firmware_wifi_usb.sh: firmware y licencias correctos"
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
while IFS='|' read -r dest orig sha; do
    [ -n "$dest" ] || continue
    mkdir -p "$RAIZ/firmware/$(dirname "$dest")"
    curl -sfL -o "$tmp/f" "$BASE/$orig?h=$ETIQUETA"
    comprobar "$tmp/f" "$sha"
    cp "$tmp/f" "$RAIZ/firmware/$dest"
done <<< "$LISTA"
echo "firmware_wifi_usb.sh: firmware de linux-firmware $ETIQUETA en $RAIZ/firmware"

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# firmware_bt.sh - baja y comprueba el firmware de los controladores Bluetooth (D411):
#   Realtek (rtl_bt/*_fw.bin y *_config.bin: RTL8723B/D, RTL8761BU, RTL8821A/C) en firmware/realtek/rtl_bt/
#   Atheros (ar3k/*.dfu del AR3012 y ath3k-1.fw del AR3011) en firmware/atheros/
# de linux-firmware, etiqueta fija, con el SHA-256 de cada archivo. Son redistribuibles sin cambios
# pero no libres (D073); van en git con su licencia al lado y `make SEM_FIRMWARE=1` los deja fuera.
# Los parches .hcd de Broadcom NO están en linux-firmware y no se distribuyen: el usuario los pone en
# /lib/firmware/brcm/ (docs/bluetooth.md).
#
#   tools/firmware_bt.sh [--comprobar]
set -euo pipefail

ETIQUETA=20241110
BASE="https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/firmware"

# SHA-256 y ruta dentro de firmware/ (la ruta en linux-firmware es la misma sin el fabricante)
TABLA="
    802b7014b26c606cf6248ae8b0ab1ce6d2d1b0db236d38dd269e676cd70710f2  atheros/LICENCE.atheros_firmware
    24844c16de86a54b44d34c83c78b67ef561c09a228769d8cb6975a0a8db455db  atheros/ar3k/AthrBT_0x01020001.dfu
    c064839aea2f8c5ea11da08ed1814adf26c0b74f0da5c7b92f9576bf1b43773b  atheros/ar3k/AthrBT_0x01020200.dfu
    b7dad005c0d019f2785b302e470df7f483879af971a3dde16c4993eada4d4a26  atheros/ar3k/AthrBT_0x01020201.dfu
    a15ce5593f65a3c099a70efed1238f7e0f87ca374d70c72fbdfe397dd1ab3bb3  atheros/ar3k/AthrBT_0x11020000.dfu
    0504043d9d17ce09ac829e5ae7dba6af739e136160c842e76d772d5fcb02a019  atheros/ar3k/AthrBT_0x11020100.dfu
    d0bc433c0ff69e53c54cb728f6852dd5467f417c615813b2e72b5b7dd556439d  atheros/ar3k/AthrBT_0x31010000.dfu
    d0e20e32c84b42684829743475a53225e7116089393ac4c4970be136b8f3a2f6  atheros/ar3k/AthrBT_0x31010100.dfu
    0807e12526dc8e0d6e0655710afb245ea7b097cd5b25495c86b7100429a703a3  atheros/ar3k/AthrBT_0x41020000.dfu
    515e51ac2b21afba8ba9a87ac8b53713b72f6461f316f1da0eb405f580e36ead  atheros/ar3k/ramps_0x01020001_26.dfu
    57a67ed2e45b4c826b7a3cafcee5328d511d25643105f15f82f310071ba339e3  atheros/ar3k/ramps_0x01020200_26.dfu
    162f381675f859e75e79a3d0327167e87f4c6e326a85972a12abefd569ed2ea4  atheros/ar3k/ramps_0x01020200_40.dfu
    871b701f2734a8bb9a45c4c369a60719252bea01f3eb8108699fa8ea8f16fafd  atheros/ar3k/ramps_0x01020201_26.dfu
    871b701f2734a8bb9a45c4c369a60719252bea01f3eb8108699fa8ea8f16fafd  atheros/ar3k/ramps_0x01020201_40.dfu
    7e776dab8b7f74f830cb2b19629751717b5316c05a160a6430ee842d4dc666f6  atheros/ar3k/ramps_0x11020000_40.dfu
    d10b989e90b1b18fa015df3c88d3beac6dba04c586663c0a7132e1fa2feb3f59  atheros/ar3k/ramps_0x11020100_40.dfu
    c8404e5abc29ab8adb310fb13109009ca744f6145e3e7c9bd49b84680c5db615  atheros/ar3k/ramps_0x31010000_40.dfu
    a2f99c2d9565d55040c6956427d0662911c6c9dd6f12965f1470169b75b274ae  atheros/ar3k/ramps_0x31010100_40.dfu
    4c20a37cb8af73948c2247eb1947bce127a0956c6b91b446104a413e4151c546  atheros/ar3k/ramps_0x41020000_40.dfu
    e51feca60698858fdf8150135360a26fb4742323eea73a4d42f15410f00e7683  atheros/ath3k-1.fw
    c9c8a9f6ebd90b7449a7e4d38ee39952dc7cc47d6c1d054e42cfbb5e4d818853  realtek/rtl_bt/rtl8723b_fw.bin
    6b70723b2db9b658cd9535fe36f1494b4494699e1ad07920c9356f8bd90d9263  realtek/rtl_bt/rtl8723d_fw.bin
    6c28a3f07c6a30ed208c4b64862a23f02b7d93543ea980edd24df16bab45095f  realtek/rtl_bt/rtl8761bu_config.bin
    1d7a9597349ad89344fa16c1913d3e39e9a12e966e417ca16871bc79bbe59edb  realtek/rtl_bt/rtl8761bu_fw.bin
    3ed5077f8f02ff2b06be257691dc72061efac976cd2dc174057833624c820b9d  realtek/rtl_bt/rtl8821a_fw.bin
    292717b0610ce920f66b734c299d349ea59a4c4400044078d8fc65625f17e382  realtek/rtl_bt/rtl8821c_config.bin
    dfe7ef772d078ec09490c02190a32fcd40b3eaf3b040049d8930eb80e6d5743c  realtek/rtl_bt/rtl8821c_fw.bin
"

# ruta en linux-firmware a partir de la ruta local
origen() {
    case "$1" in
        realtek/rtl_bt/*) echo "${1#realtek/}" ;;
        atheros/LICENCE.atheros_firmware) echo "LICENCE.atheros_firmware" ;;
        atheros/ath3k-1.fw) echo "ath3k-1.fw" ;;
        atheros/ar3k/*) echo "${1#atheros/}" ;;
    esac
}

comprobar() {
    local f="$1" esperado="$2" real
    [ -f "$DEST/$f" ] || { echo "firmware_bt.sh: falta $f" >&2; return 1; }
    real="$(sha256sum "$DEST/$f" | cut -d' ' -f1)"
    [ "$real" = "$esperado" ] || { echo "firmware_bt.sh: SHA-256 de $f no cuadra ($real)" >&2; return 1; }
}

if [ "${1:-}" = "--comprobar" ]; then
    mal=0
    while read -r h f; do
        [ -n "$h" ] || continue
        comprobar "$f" "$h" || mal=1
    done <<< "$TABLA"
    [ "$mal" = 0 ] || exit 1
    echo "firmware_bt.sh: firmware Bluetooth correcto"
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
while read -r h f; do
    [ -n "$h" ] || continue
    mkdir -p "$tmp/$(dirname "$f")"
    curl -sfL -o "$tmp/$f" "$BASE/$(origen "$f")?h=$ETIQUETA"
    real="$(sha256sum "$tmp/$f" | cut -d' ' -f1)"
    [ "$real" = "$h" ] || { echo "firmware_bt.sh: SHA-256 de $f no cuadra ($real)" >&2; exit 1; }
done <<< "$TABLA"
while read -r h f; do
    [ -n "$h" ] || continue
    mkdir -p "$DEST/$(dirname "$f")"
    cp "$tmp/$f" "$DEST/$f"
done <<< "$TABLA"
echo "firmware_bt.sh: firmware de linux-firmware $ETIQUETA en $DEST"

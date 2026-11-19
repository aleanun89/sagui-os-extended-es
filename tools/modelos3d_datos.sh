#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# modelos3d_datos.sh - baja los modelos 3D libres (CC0) que usa «sgldemo -p --modelo» (D404).
#
# Van FUERA de git: este guion los baja con SHA-256 fijo a userland/terceros/modelos3d/datos/
# (ignorada por git). Sin ellos, la escena usa un avión hecho por código. Licencia de cada
# fichero comprobada en su página de OpenGameArt el 2026-11-19 (ver docs/terceros.md):
#   biplane.obj + biplane.png  «Low-Poly Biplane», mfep, CC0 1.0
#                              https://opengameart.org/content/low-poly-biplane (biplane.zip)
#   funky.obj                  «Funky aircraft» (low.obj), Savino, CC0 1.0
#                              https://opengameart.org/content/funky-aircraft
#   ww1.obj                    «WW1 (WWI) Airplane model obj.», Kala1987, CC0 1.0 (sin MTL)
#                              https://opengameart.org/content/ww1-wwi-airplane-model-obj
#
#   tools/modelos3d_datos.sh [--forzar]
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${MODELOS3D_DEST:-$RAIZ/userland/terceros/modelos3d/datos}"
BASE="https://opengameart.org/sites/default/files"
# fichero remoto | SHA-256
ARQS=(
    "biplane.zip|18ba0e37b66d5c15fb774a5e1f8e685cafaca767239a9aaac57a0a338929ebd4"
    "low_0.obj|b10aabd72ecea4e9bd702b32316bee00fef260d62ae48a36299f956ff7c1c1a5"
    "WWIairplane.obj|16bbd50d14b5b5921c45724d8fef7da305e990dd297afbf00662bfb6c59ce91c"
)
FORZAR=0
while [ $# -gt 0 ]; do
    case "$1" in
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,15p' "$0"; exit 0 ;;
        *) echo "modelos3d_datos.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
if [ -f "$DEST/.preparado" ] && [ "$FORZAR" = 0 ]; then
    echo "modelos3d_datos.sh: ya preparado en $DEST (--forzar para rehacer)"; exit 0
fi
for h in curl sha256sum unzip; do
    command -v "$h" >/dev/null 2>&1 || { echo "modelos3d_datos.sh: falta $h" >&2; exit 1; }
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
for a in "${ARQS[@]}"; do
    f="${a%%|*}"; sha="${a##*|}"
    echo "modelos3d_datos.sh: descargando $BASE/$f"
    # OpenGameArt rechaza los clientes sin nombre de navegador
    curl -fsSL -A "Mozilla/5.0" -o "$TMP/$f" "$BASE/$f"
    echo "$sha  $TMP/$f" | sha256sum -c --quiet - || { echo "modelos3d_datos.sh: SHA-256 inesperado en $f" >&2; exit 1; }
done
unzip -q -o "$TMP/biplane.zip" -d "$TMP/bip"
rm -rf "$DEST"; mkdir -p "$DEST"
cp "$TMP/bip/biplane.obj" "$DEST/biplane.obj"
cp "$TMP/bip/diffuse_512.png" "$DEST/biplane.png"   # sin MTL: sgl_obj_cargar usa <nombre>.png
cp "$TMP/low_0.obj" "$DEST/funky.obj"
cp "$TMP/WWIairplane.obj" "$DEST/ww1.obj"
cat > "$DEST/LICENCIA.txt" <<'EOF'
Modelos 3D de OpenGameArt.org, todos con licencia CC0 1.0 (dominio público):
https://creativecommons.org/publicdomain/zero/1.0/

biplane.obj, biplane.png  "Low-Poly Biplane", de mfep (2015)
                          https://opengameart.org/content/low-poly-biplane
                          (biplane.zip: biplane.obj y diffuse_512.png, renombrado)
funky.obj                 "Funky aircraft" (low.obj), de Savino (2012)
                          https://opengameart.org/content/funky-aircraft
ww1.obj                   "WW1 (WWI) Airplane model obj.", de Kala1987 (2018)
                          https://opengameart.org/content/ww1-wwi-airplane-model-obj

Orientación en sgldemo (morro hacia +x): biplane --giro 90; funky y ww1 --giro 90.
EOF
echo "1" > "$DEST/.preparado"
echo "modelos3d_datos.sh: listo en $DEST"

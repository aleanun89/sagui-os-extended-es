#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# doom_datos.sh - baja Freedoom 0.13.0 (BSD-3-Clause, https://freedoom.github.io) para Chocolate Doom (D383).
#
# freedoom1.wad y freedoom2.wad son contenido libre y van en las imágenes públicas; NO van en git:
# este guion baja el zip de la versión (SHA-256 fijo) a userland/terceros/chocolatedoom/datos/
# (ignorada por git). Los WAD comerciales (doom.wad, doom2.wad...) NUNCA van aquí: solo por
# input_share/games/DOOM en las imágenes privadas.
#
#   tools/doom_datos.sh [--zip FICHERO] [--forzar]
set -euo pipefail
VER=0.13.0
URL="https://github.com/freedoom/freedoom/releases/download/v${VER}/freedoom-${VER}.zip"
SHA256=3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${DOOM_DATOS_DEST:-$RAIZ/userland/terceros/chocolatedoom/datos}"
ZIP=""; FORZAR=0
while [ $# -gt 0 ]; do
    case "$1" in
        --zip) ZIP="$2"; shift 2 ;;
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "doom_datos.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
if [ -f "$DEST/.preparado" ] && [ "$FORZAR" = 0 ]; then
    echo "doom_datos.sh: ya preparado en $DEST (--forzar para rehacer)"; exit 0
fi
for h in curl sha256sum unzip; do
    command -v "$h" >/dev/null 2>&1 || { echo "doom_datos.sh: falta $h" >&2; exit 1; }
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$ZIP" ]; then
    ZIP="$TMP/freedoom.zip"
    echo "doom_datos.sh: descargando $URL"
    curl -fsSL -o "$ZIP" "$URL"
fi
echo "$SHA256  $ZIP" | sha256sum -c --quiet - || { echo "doom_datos.sh: SHA-256 inesperado" >&2; exit 1; }
unzip -q -o "$ZIP" -d "$TMP/x"
rm -rf "$DEST"; mkdir -p "$DEST"
cp "$TMP/x/freedoom-$VER/freedoom1.wad" "$TMP/x/freedoom-$VER/freedoom2.wad" \
   "$TMP/x/freedoom-$VER/COPYING.txt" "$TMP/x/freedoom-$VER/CREDITS.txt" "$TMP/x/freedoom-$VER/CREDITS-MUSIC.txt" "$DEST/"
echo "$VER" > "$DEST/.preparado"
echo "doom_datos.sh: listo en $DEST"
